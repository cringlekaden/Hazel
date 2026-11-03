#include "hzpch.h"
#include "Hazel/Utils/Process.h"
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace Hazel
{
bool Process::Launch(const std::filesystem::path &exe, const std::vector<std::string> &args)
{
    if (!exe.is_absolute())
        return false;
    std::vector<std::string> owned{exe.u8string()};
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char *> argv;
    for (auto &arg : owned)
        argv.push_back(arg.data());
    argv.push_back(nullptr);
    pid_t pid;
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions))
        return false;
    // Darwin closes every descriptor except explicitly inherited spawn actions.
    posix_spawnattr_t attributes;
    auto error = posix_spawnattr_init(&attributes);
    if (error) { posix_spawn_file_actions_destroy(&actions); return false; }
    error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);
    if (!error) error = posix_spawn(&pid, exe.c_str(), &actions, &attributes, argv.data(), environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    if (error)
        return false;
    std::thread([pid] {
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        {
        }
    }).detach();
    return true;
}
ProcessResult Process::RunStreams(const std::filesystem::path &exe, const std::vector<std::string> &args,
                                  const std::filesystem::path &directory, std::chrono::milliseconds timeout,
                                  const ProcessCallbacks &callbacks)
{
    if (!exe.is_absolute())
        throw std::invalid_argument("Process executable must be absolute");
    std::vector<std::string> owned{exe.u8string()};
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char *> argv;
    for (auto &s : owned)
        argv.push_back(s.data());
    argv.push_back(nullptr);
    int pipes[2][2];
    if (pipe(pipes[0]))
        throw std::runtime_error("Cannot create process output pipe");
    if (pipe(pipes[1]))
    {
        close(pipes[0][0]);
        close(pipes[0][1]);
        throw std::runtime_error("Cannot create stderr pipe");
    }
    auto closePipes = [&] {
        for (auto &pipe : pipes)
        {
            close(pipe[0]);
            close(pipe[1]);
        }
    };
    for (auto& pipe : pipes)
        for (int descriptor : pipe)
            if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
                closePipes();
                throw std::runtime_error("Cannot protect process pipe descriptors");
            }
    posix_spawn_file_actions_t actions;
    int setupError = posix_spawn_file_actions_init(&actions);
    if (setupError)
    {
        closePipes();
        return {-1, false, "Cannot initialize spawn actions"};
    }
    const auto setup = [&](int error) {
        if (!setupError)
            setupError = error;
    };
    setup(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0));
    setup(posix_spawn_file_actions_adddup2(&actions, pipes[0][1], STDOUT_FILENO));
    setup(posix_spawn_file_actions_adddup2(&actions, pipes[1][1], STDERR_FILENO));
    setup(posix_spawn_file_actions_addclose(&actions, pipes[0][0]));
    setup(posix_spawn_file_actions_addclose(&actions, pipes[1][0]));
    if (!directory.empty())
        setup(posix_spawn_file_actions_addchdir(&actions, directory.c_str()));
    posix_spawnattr_t attr;
    const auto attrError = posix_spawnattr_init(&attr);
    if (attrError)
    {
        posix_spawn_file_actions_destroy(&actions);
        closePipes();
        return {-1, false, "Cannot initialize spawn attributes"};
    }
    setup(posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT));
    setup(posix_spawnattr_setpgroup(&attr, 0));
    pid_t pid;
    int error =
        setupError ? setupError : posix_spawn(&pid, exe.c_str(), &actions, &attr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    close(pipes[0][1]);
    close(pipes[1][1]);
    if (error)
    {
        close(pipes[0][0]);
        close(pipes[1][0]);
        return {-1, false, std::string("Launch failed: ") + std::strerror(error)};
    }
    for (auto &pipe : pipes)
        fcntl(pipe[0], F_SETFL, O_NONBLOCK);
    ProcessResult result;
    int status = 0;
    auto start = std::chrono::steady_clock::now();
    auto drain = [&] {
        // Alternating streams and finite reads keep exit/cancel/deadline checks fair under floods.
        char data[4096];
        for (int pass = 0; pass < 8; ++pass)
            for (int stream = 0; stream < 2; ++stream)
            {
                const auto n = read(pipes[stream][0], data, sizeof(data));
                if (n <= 0)
                    continue;
                result.Output.append(data, static_cast<size_t>(n));
                if (result.Output.size() > 2 * 1024 * 1024)
                {
                    const auto dropped = result.Output.size() - 2 * 1024 * 1024;
                    result.DroppedBytes += dropped;
                    result.Output.erase(0, dropped);
                }
                if (callbacks.Output)
                    callbacks.Output(static_cast<ProcessStream>(stream),
                                     std::string(data, static_cast<size_t>(n)));
            }
    };
    try
    {
        for (;;)
        {
            drain();
            auto state = waitpid(pid, &status, WNOHANG);
            if (state == pid)
                break;
            if (state < 0 && errno != EINTR)
            {
                throw std::runtime_error("Cannot reap process");
            }
            const bool cancelled = callbacks.Cancel && callbacks.Cancel();
            if (cancelled || std::chrono::steady_clock::now() - start > timeout)
            {
                result.Cancelled = cancelled;
                result.TimedOut = !result.Cancelled;
                kill(-pid, SIGKILL);
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
                {
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        kill(-pid, SIGKILL);
        for (int pass = 0; pass < 4; ++pass)
            drain();
    }
    catch (...)
    {
        kill(-pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        {
        }
        close(pipes[0][0]);
        close(pipes[1][0]);
        throw;
    }
    close(pipes[0][0]);
    close(pipes[1][0]);
    result.ExitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}
} // namespace Hazel
