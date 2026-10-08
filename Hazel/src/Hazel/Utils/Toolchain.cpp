#include "hzpch.h"
#include "Toolchain.h"
#include "Process.h"
#include <yaml-cpp/yaml.h>
namespace Hazel
{
PythonSelection Toolchain::ProbePython(const std::filesystem::path &path, const std::string &source)
{
	PythonSelection result;
	result.Executable = path;
	result.Source = source;
	try
	{
		if (!path.is_absolute() || !std::filesystem::is_regular_file(path))
			throw std::runtime_error("Select an existing absolute Python executable");
#ifdef HZ_PLATFORM_WINDOWS
		auto parent = path.parent_path().filename().u8string();
		std::transform(parent.begin(), parent.end(), parent.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (parent == "windowsapps")
			throw std::runtime_error(
				"Windows Store execution aliases are not interpreters; select the installed python.exe");
#endif
		auto probe = Process::Run(path,
								  {"-I", "-c",
								   "import sys,json,pathlib,subprocess; "
								   "print(json.dumps({'version':list(sys.version_info[:3]),'executable':str("
								   "pathlib.Path(sys.executable).resolve()),'ok':sys.version_info>=(3,9)}))"},
								  {}, std::chrono::seconds(5));
		if (probe.TimedOut)
			throw std::runtime_error("Python validation timed out after five seconds");
		if (probe.ExitCode)
			throw std::runtime_error("Python validation failed: " + probe.Output);
		if (probe.Output.size() > 8192)
			throw std::runtime_error("Python probe output exceeded its expected size");
		auto data = YAML::Load(probe.Output);
		auto version = data["version"];
		if (!data["ok"].as<bool>() || version[0].as<int>() != 3 || version[1].as<int>() < 9)
			throw std::runtime_error("Python 3.9 or newer (Python 3) is required");
		result.Version = std::to_string(version[0].as<int>()) + "." + std::to_string(version[1].as<int>()) +
						 "." + std::to_string(version[2].as<int>());
		result.Executable = std::filesystem::u8path(data["executable"].as<std::string>());
		if (!result.Executable.is_absolute() || !std::filesystem::is_regular_file(result.Executable))
			throw std::runtime_error("Probe returned an invalid interpreter executable");
	}
	catch (const std::exception &error)
	{
		result.Error = std::string(error.what()) + ". Configure Python in Edit > Editor Preferences.";
	}
	return result;
}
PythonSelection Toolchain::DiscoverPython(const std::filesystem::path &configured,
										  const std::filesystem::path &sdk)
{
	if (!configured.empty())
		return ProbePython(configured, "Configured in Editor Preferences");
	if (!sdk.empty())
	{
#ifdef HZ_PLATFORM_WINDOWS
		const auto owned = sdk / "build/tools/python-runtime/python.exe";
#else
		const auto owned = sdk / "build/tools/python-runtime/bin/python3";
#endif
		if (std::filesystem::is_regular_file(owned))
			return ProbePython(owned, "SDK-owned tooling interpreter");
	}
	return SelectPythonCandidates(PlatformPythonCandidates());
}
PythonSelection Toolchain::SelectPythonCandidates(const std::vector<std::filesystem::path> &candidates)
{
	std::string diagnostics;
	for (const auto &path : candidates)
	{
		if (!std::filesystem::is_regular_file(path))
			continue;
		auto candidate = ProbePython(path, "Platform discovery");
		if (candidate)
			return candidate;
		diagnostics += path.generic_u8string() + ": " + candidate.Error + "\n";
	}
	return {{},
			"Platform discovery",
			{},
			"No compatible Python found. Install Python 3.9+ or configure an existing interpreter in Edit > "
			"Editor Preferences.\n" +
				diagnostics};
}
} // namespace Hazel
