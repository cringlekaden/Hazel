#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace Hazel
{
struct PythonSelection
{
	std::filesystem::path Executable;
	std::string Source, Version, Error;
	explicit operator bool() const
	{
		return Error.empty() && !Executable.empty();
	}
};
class Toolchain
{
  public:
	static PythonSelection DiscoverPython(const std::filesystem::path &configured = {},
										  const std::filesystem::path &sdk = {});
	// Ordered platform candidates are supplied by each OS implementation; also supports isolated tests.
	static PythonSelection SelectPythonCandidates(const std::vector<std::filesystem::path> &candidates);
	static PythonSelection ProbePython(const std::filesystem::path &executable, const std::string &source);

  private:
	static std::vector<std::filesystem::path> PlatformPythonCandidates();
};
} // namespace Hazel
