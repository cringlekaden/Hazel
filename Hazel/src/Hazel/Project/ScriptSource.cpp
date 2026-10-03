#include "hzpch.h"
#include "ScriptSource.h"
#include "Hazel/Core/FileSystem.h"
#include <regex>
#include <set>
namespace Hazel
{
bool ScriptSource::ValidIdentifier(const std::string &value)
{
	static const std::set<std::string> reserved = {
		"abstract",	 "as",		   "base",		"bool",		"break",	"byte",		 "case",	"catch",
		"char",		 "checked",	   "class",		"const",	"continue", "decimal",	 "default", "delegate",
		"do",		 "double",	   "else",		"enum",		"event",	"explicit",	 "extern",	"false",
		"finally",	 "fixed",	   "float",		"for",		"foreach",	"goto",		 "if",		"implicit",
		"in",		 "int",		   "interface", "internal", "is",		"lock",		 "long",	"namespace",
		"new",		 "null",	   "object",	"operator", "out",		"override",	 "params",	"private",
		"protected", "public",	   "readonly",	"ref",		"return",	"sbyte",	 "sealed",	"short",
		"sizeof",	 "stackalloc", "static",	"string",	"struct",	"switch",	 "this",	"throw",
		"true",		 "try",		   "typeof",	"uint",		"ulong",	"unchecked", "unsafe",	"ushort",
		"using",	 "virtual",	   "void",		"volatile", "while",	"Hazel",	 "Entity",	"Prefab"};
	return std::regex_match(value, std::regex("[A-Za-z_][A-Za-z0-9_]*")) && !reserved.count(value);
}
bool ScriptSource::ValidNamespace(const std::string &value)
{
	std::istringstream input(value);
	std::string part;
	if (value.empty() || value.back() == '.')
		return false;
	while (std::getline(input, part, '.'))
		if (!ValidIdentifier(part))
			return false;
	return true;
}
std::filesystem::path ScriptSource::Create(const std::filesystem::path &root, const std::string &name,
										   const std::string &space)
{
	if (!ValidIdentifier(name) || !ValidNamespace(space))
		throw std::invalid_argument("Use valid C# class and namespace identifiers (avoid keywords)");
	const auto directory = root / "Scripts/Source";
	std::filesystem::create_directories(directory);
	const auto path = directory / (name + ".cs");
	auto canonical =
		std::filesystem::weakly_canonical(path).lexically_relative(std::filesystem::canonical(root));
	if (canonical.empty() || *canonical.begin() == "..")
		throw std::runtime_error("Script source folder must remain inside project Assets");
	const std::string text =
		"using Hazel;\n\nnamespace " + space + " {\n    public class " + name +
		" : Entity {\n        public Prefab SpawnAsset;\n        private Entity spawned;\n        void "
		"OnCreate() { }\n        void OnUpdate(float dt) {\n            // Select SpawnAsset in the "
		"Inspector after Build Scripts.\n            if (Input.IsKeyDown(KeyCode.Space) && spawned == null "
		"&& SpawnAsset != null && SpawnAsset.IsAssigned)\n                spawned = "
		"Entity.Instantiate(SpawnAsset, Translation);\n            if (Input.IsKeyDown(KeyCode.R) && spawned "
		"!= null) { spawned.Destroy(); spawned = null; }\n        }\n        void OnDestroy() { /* Cleanup "
		"runs once, outside update iteration. */ if (spawned != null) spawned.Destroy(); }\n    }\n}\n";
	FileSystem::WriteNewFile(path, text);
	return path;
}
std::filesystem::path ScriptSource::Find(const std::filesystem::path &root, const std::string &fullClass)
{
	auto last = fullClass.find_last_of('.');
	auto name = fullClass.substr(last == std::string::npos ? 0 : last + 1);
	if (!ValidIdentifier(name))
		throw std::runtime_error("Class name is unavailable");
	auto space = last == std::string::npos ? std::string{} : fullClass.substr(0, last);
	if (!space.empty() && !ValidNamespace(space))
		throw std::runtime_error("Namespace is unavailable");
	std::string namespacePattern;
	for (auto c : space)
	{
		if (c == '.')
			namespacePattern += "\\";
		namespacePattern += c;
	}
	std::vector<std::filesystem::path> paths, matches;
	std::error_code error;
	for (auto it = std::filesystem::recursive_directory_iterator(root / "Scripts/Source", error);
		 !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
		if (it->path().extension() == ".cs")
			paths.push_back(it->path());
	std::sort(paths.begin(), paths.end());
	for (auto &path : paths)
	{
		std::ifstream input(path);
		std::string text{std::istreambuf_iterator<char>(input), {}};
		if (std::regex_search(text, std::regex("\\bclass\\s+" + name + "\\b")) &&
			(space.empty() ||
			 std::regex_search(text, std::regex("\\bnamespace\\s+" + namespacePattern + "\\s*[;{]"))))
			matches.push_back(path);
	}
	if (matches.size() == 1)
		return matches.front();
	if (matches.size() > 1)
		throw std::runtime_error(
			"Multiple source files match this class. Open the desired .cs asset in Content Browser.");
	throw std::runtime_error("No source file found. Select a .cs asset in Content Browser, or create one in "
							 "Project > Create Script.");
}
} // namespace Hazel
