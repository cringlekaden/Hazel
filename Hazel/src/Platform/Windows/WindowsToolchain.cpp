#include "hzpch.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Utils/Toolchain.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
namespace Hazel
{
static std::wstring RegistryString(HKEY key, const wchar_t *name)
{
	DWORD bytes = 0, type = 0;
	if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS || type != REG_SZ ||
		bytes > 65536)
		return {};
	std::wstring value(bytes / sizeof(wchar_t), L'\0');
	if (RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<BYTE *>(value.data()), &bytes) !=
		ERROR_SUCCESS)
		return {};
	while (!value.empty() && value.back() == L'\0')
		value.pop_back();
	return value;
}
std::vector<std::filesystem::path> Toolchain::PlatformPythonCandidates()
{
	std::vector<std::filesystem::path> paths;
	for (auto hive : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE})
		for (auto view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY})
		{
			HKEY root = nullptr;
			if (RegOpenKeyExW(hive, L"Software\\Python\\PythonCore", 0, KEY_READ | view, &root) !=
				ERROR_SUCCESS)
				continue;
			std::vector<std::wstring> versions;
			wchar_t name[256];
			DWORD size;
			for (DWORD i = 0;; ++i)
			{
				size = 256;
				if (RegEnumKeyExW(root, i, name, &size, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
					break;
				if (std::wstring(name).rfind(L"3.", 0) == 0)
					versions.emplace_back(name);
			}
			auto minor = [](const std::wstring &v) {
				try
				{
					return std::stoi(v.substr(2));
				}
				catch (...)
				{
					return 0;
				}
			};
			std::sort(versions.begin(), versions.end(),
					  [&](auto &a, auto &b) { return minor(a) == minor(b) ? a > b : minor(a) > minor(b); });
			for (auto &version : versions)
			{
				HKEY install = nullptr;
				if (RegOpenKeyExW(root, (version + L"\\InstallPath").c_str(), 0, KEY_READ | view, &install) !=
					ERROR_SUCCESS)
					continue;
				auto exe = RegistryString(install, L"ExecutablePath");
				auto directory = RegistryString(install, nullptr);
				if (!exe.empty())
					paths.emplace_back(exe);
				else if (!directory.empty())
					paths.push_back(std::filesystem::path(directory) / "python.exe");
				RegCloseKey(install);
			}
			RegCloseKey(root);
		}
	for (int minor = 14; minor >= 9; --minor)
	{
		auto folder = "Python3" + std::to_string(minor);
		for (auto base : {FileSystem::GetEnvironmentPath("LOCALAPPDATA") / "Programs/Python",
						  FileSystem::GetEnvironmentPath("ProgramFiles"), std::filesystem::path("C:/")})
			if (base.is_absolute())
				paths.push_back(base / folder / "python.exe");
	}
	auto env = FileSystem::GetEnvironmentPath("PATH").u8string();
	std::istringstream stream(env);
	std::string part;
	while (std::getline(stream, part, ';'))
	{
		auto path = std::filesystem::u8path(part);
		if (path.is_absolute())
			paths.push_back(path / "python.exe");
	}
	return paths;
}
} // namespace Hazel
