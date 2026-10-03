#pragma once
#include <filesystem>
#include <string>
namespace Hazel
{
class ScriptSource
{
  public:
	static bool ValidIdentifier(const std::string &value);
	static bool ValidNamespace(const std::string &value);
	static std::filesystem::path Create(const std::filesystem::path &root, const std::string &name,
										const std::string &nameSpace);
	static std::filesystem::path Find(const std::filesystem::path &root, const std::string &fullClass);
};
} // namespace Hazel
