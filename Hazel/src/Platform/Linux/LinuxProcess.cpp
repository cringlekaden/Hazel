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
ProcessResult Process::Run(const std::filesystem::path &exe, const std::vector<std::string> &args,
						   const std::filesystem::path &directory, std::chrono::milliseconds timeout,
						   const std::function<void(const std::string &)> &onOutput)
{
	if (!exe.is_absolute())
		throw std::invalid_argument("Process executable must be absolute");
	int pipefd[2];
	if (pipe2(pipefd, O_CLOEXEC))
		throw std::runtime_error("Cannot create process output pipe");
	std::vector<std::string> owned{exe.u8string()};
	owned.insert(owned.end(), args.begin(), args.end());
	std::vector<char *> argv;
	for (auto &s : owned)
		argv.push_back(s.data());
	argv.push_back(nullptr);
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
	posix_spawn_file_actions_addclose(&actions, pipefd[0]);
	auto setupError = posix_spawn_file_actions_addclosefrom_np(&actions, 3);
	if (!directory.empty())
		posix_spawn_file_actions_addchdir_np(&actions, directory.c_str());
	posix_spawnattr_t attr;
	posix_spawnattr_init(&attr);
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&attr, 0);
	pid_t pid;
	int error = setupError ? setupError : posix_spawn(&pid, exe.c_str(), &actions, &attr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attr);
	close(pipefd[1]);
	if (error)
	{
		close(pipefd[0]);
		return {-1, false, std::string("Launch failed: ") + std::strerror(error)};
	}
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	ProcessResult result;
	int status = 0;
	auto start = std::chrono::steady_clock::now();
	auto drain = [&] {
		char data[4096];
		ssize_t n;
		while ((n = read(pipefd[0], data, sizeof(data))) > 0)
		{
			result.Output.append(data, n);
			if (result.Output.size() > 2 * 1024 * 1024)
				result.Output.erase(0, result.Output.size() - 2 * 1024 * 1024);
			if (onOutput)
				onOutput(std::string(data, n));
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
			if (std::chrono::steady_clock::now() - start > timeout)
			{
				result.TimedOut = true;
				kill(-pid, SIGKILL);
				while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
				{
				}
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
		drain();
	}
	catch (...)
	{
		kill(-pid, SIGKILL);
		while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		{
		}
		close(pipefd[0]);
		throw;
	}
	close(pipefd[0]);
	result.ExitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	return result;
}
} // namespace Hazel
