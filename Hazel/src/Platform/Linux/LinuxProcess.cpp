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
    // External editors must not keep the engine's sockets, watchers or files alive.
    auto error = posix_spawn_file_actions_addclosefrom_np(&actions, 3);
    if (!error)
        error = posix_spawn(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
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
    if (pipe2(pipes[0], O_CLOEXEC))
        throw std::runtime_error("Cannot create process output pipe");
    if (pipe2(pipes[1], O_CLOEXEC))
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
    setup(posix_spawn_file_actions_addclosefrom_np(&actions, 3));
    if (!directory.empty())
        setup(posix_spawn_file_actions_addchdir_np(&actions, directory.c_str()));
    posix_spawnattr_t attr;
    const auto attrError = posix_spawnattr_init(&attr);
    if (attrError)
    {
        posix_spawn_file_actions_destroy(&actions);
        closePipes();
        return {-1, false, "Cannot initialize spawn attributes"};
    }
    setup(posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP));
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
