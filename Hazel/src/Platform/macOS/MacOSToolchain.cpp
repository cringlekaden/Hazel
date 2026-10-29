#include "hzpch.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Utils/Toolchain.h"
namespace Hazel
{
std::vector<std::filesystem::path> Toolchain::PlatformPythonCandidates()
{
	std::vector<std::filesystem::path> paths = {"/usr/bin/python3", "/usr/local/bin/python3"};
	for (int minor = 14; minor >= 9; --minor)
		paths.emplace_back("/opt/python/3." + std::to_string(minor) + "/bin/python3");
	const char *environment = std::getenv("PATH");
	std::istringstream stream(environment ? environment : "");
	std::string part;
	while (std::getline(stream, part, ':'))
		if (!part.empty() && std::filesystem::path(part).is_absolute())
			paths.push_back(std::filesystem::path(part) / "python3");
	return paths;
}
} // namespace Hazel
