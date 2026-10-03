#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
namespace Hazel
{
struct ProcessResult
{
	int ExitCode = -1;
	bool TimedOut = false;
	std::string Output;
};
// Structured argv, no shell. Captures bounded output; owns and reaps children.
class Process
{
  public:
	static bool Launch(const std::filesystem::path &executable, const std::vector<std::string> &arguments);
	static ProcessResult Run(const std::filesystem::path &executable,
							 const std::vector<std::string> &arguments,
							 const std::filesystem::path &directory,
							 std::chrono::milliseconds timeout = std::chrono::minutes(60),
							 const std::function<void(const std::string &)> &onOutput = {});
};
} // namespace Hazel
