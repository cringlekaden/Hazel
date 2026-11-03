#include "hzpch.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Utils/Toolchain.h"
namespace Hazel
{
std::filesystem::path Toolchain::OwnedPythonExecutable(const std::filesystem::path& sdk) {return sdk/"build/tools/python-runtime/bin/python3";}
std::vector<std::filesystem::path> Toolchain::PlatformPythonCandidates()
{
	// Stable installer entry points, independent of Terminal's PATH. Avoid
	// Cellar/version paths: ProbePython resolves and validates the real executable.
	std::vector<std::filesystem::path> paths = {"/opt/homebrew/bin/python3", "/usr/local/bin/python3"};
	const std::filesystem::path frameworks="/Library/Frameworks/Python.framework/Versions";
	std::vector<std::filesystem::path> installed;
	std::error_code error;
	for (std::filesystem::directory_iterator it(frameworks,error),end; !error && it!=end; it.increment(error))
		if (it->path().filename().u8string().rfind("3.",0)==0) installed.push_back(it->path()/"bin/python3");
	std::sort(installed.rbegin(),installed.rend());
	paths.insert(paths.end(),installed.begin(),installed.end());
	paths.emplace_back("/usr/bin/python3");
	const char *environment = std::getenv("PATH");
	std::istringstream stream(environment ? environment : "");
	std::string part;
	while (std::getline(stream, part, ':'))
		if (!part.empty() && std::filesystem::path(part).is_absolute())
			paths.push_back(std::filesystem::path(part) / "python3");
	return paths;
}
} // namespace Hazel
