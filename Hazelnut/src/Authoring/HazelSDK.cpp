#include "HazelSDK.h"
#include "Hazel/Core/FileSystem.h"
#include <fstream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>
namespace Hazel
{
namespace
{
#ifdef HZ_PLATFORM_WINDOWS
constexpr const char *Platform = "windows";
constexpr const char *Premake = "build/tools/premake-core/bin/release/premake5.exe";
#else
constexpr const char *Platform = "linux";
constexpr const char *Premake = "build/tools/premake-core/bin/release/premake5";
#endif
YAML::Node Metadata(const std::filesystem::path &path)
{
    if (std::filesystem::file_size(path) > 65536)
        throw std::runtime_error("Tooling metadata exceeds 64 KiB: " + path.generic_u8string());
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read tooling metadata: " + path.generic_u8string());
    auto data = YAML::Load(input); // Existing JSON manifests are also valid YAML.
    if (!data.IsMap())
        throw std::runtime_error("Expected a tooling metadata object: " + path.generic_u8string());
    return data;
}
bool File(const std::filesystem::path &path)
{
    return std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) != 0;
}
} // namespace
const char *SDKSelection::Status() const
{
    switch (State)
    {
    case SDKState::Ready: return "Ready (SDK files checked)";
    case SDKState::Missing: return "Missing";
    case SDKState::Incompatible: return "Incompatible";
    default: return "Not configured";
    }
}
const char *HazelSDK::SetupInstructions()
{
    return "Select the Hazel source checkout folder containing premake5.lua and scripts/hazel.py, "
           "not bin/Hazelnut or the application Resources folder. Clone cringlekaden/Hazel with "
           "--recurse-submodules --branch feature/editor-usability for this editor, install the "
           "README prerequisites, then run scripts/setup.sh "
           "(Linux) or scripts/setup.ps1 (Windows) to prepare Debug ScriptCore and pinned Premake. "
           "Check Readiness verifies Python and host compilers; export also needs the native "
           "build prerequisites. Runtime application packages do not include this source SDK.";
}
SDKSelection HazelSDK::Validate(const std::filesystem::path &root, const std::string &source)
{
    SDKSelection result{SDKState::NotConfigured, root, source, {}};
    if (root.empty())
    {
        result.Diagnostic = "No Hazel source SDK selected or discovered.";
        return result;
    }
    try
    {
        if (!root.is_absolute())
        {
            result.State = SDKState::Incompatible;
            result.Diagnostic = "SDK override must be an absolute folder; it is never resolved against the working directory.";
            return result;
        }
        result.State = SDKState::Missing;
        if (!std::filesystem::is_directory(root))
        {
            result.Diagnostic = "SDK folder is missing or unavailable: " + root.generic_u8string();
            return result;
        }
        result.Root = std::filesystem::weakly_canonical(root);
        // These are the current canonical CLI/preflight/template inputs, not a new SDK schema.
        for (const char *file : {"premake5.lua", "scripts/hazel.py", "scripts/internal/toolchain.json",
                                 "scripts/internal/authoring.py", "scripts/internal/packaging.py",
                                 "scripts/internal/child_tools.py", "scripts/internal/templates/project.lua"})
            if (!File(result.Root / file))
            {
                result.Diagnostic = "SDK is incomplete; missing " + (result.Root / file).generic_u8string();
                return result;
            }
        result.State = SDKState::Incompatible;
        const auto pins = Metadata(result.Root / "scripts/internal/toolchain.json");
        if (pins["premake"].as<std::string>() != HAZEL_TOOLCHAIN_PREMAKE ||
            pins["pyyaml"].as<std::string>() != HAZEL_TOOLCHAIN_PYYAML)
        {
            result.Diagnostic = "SDK pinned Premake/PyYAML contract differs from this editor. Select a compatible checkout and rebuild its tools.";
            return result;
        }
        result.State = SDKState::Missing;
        const auto core = result.Root / "bin" / (std::string("Debug-") + Platform + "-x86_64") /
                          "Hazel-ScriptCore/Hazel-ScriptCore.dll";
        for (const auto &file : {result.Root / Premake, core})
            if (!File(file))
            {
                result.Diagnostic = "SDK needs setup; missing " + file.generic_u8string();
                return result;
            }
        result.State = SDKState::Ready;
        result.Diagnostic = "Canonical SDK files and pinned tooling match. Check Readiness verifies Python, script compiler/targeting pack, and native export prerequisites.";
    }
    catch (const std::exception &error)
    {
        result.Diagnostic = error.what();
    }
    return result;
}
SDKSelection HazelSDK::Discover(const std::filesystem::path &configured)
{
    if (!configured.empty()) return Validate(configured);
    try
    {
        return Discover({}, FileSystem::GetExecutablePath());
    }
    catch (const std::exception &error)
    {
        return {SDKState::NotConfigured, {}, "Application location", error.what()};
    }
}
SDKSelection HazelSDK::Discover(const std::filesystem::path &configured,
                                const std::filesystem::path &executable)
{
    if (!configured.empty())
        return Validate(configured); // Invalid explicit choices never silently fall back.
    try
    {
        if (!executable.is_absolute())
            return {SDKState::Incompatible, {}, "Application location", "Application location must be absolute."};
        const auto app = std::filesystem::weakly_canonical(executable).parent_path();
        const auto metadata = app / "build.json";
        if (std::filesystem::is_regular_file(metadata))
        {
            const auto data = Metadata(metadata);
            if (data["hazel_sdk"])
            {
                // Optional explicit package/application locator; current runtime packages omit it.
                auto root = std::filesystem::u8path(data["hazel_sdk"].as<std::string>());
                if (root.empty())
                    throw std::runtime_error("Application hazel_sdk locator is empty");
                if (!root.is_absolute()) root = app / root;
                return Validate(root, "Application build.json locator");
            }
            // A runtime package is not a development checkout, even if placed under bin/.
            return {SDKState::NotConfigured, {}, "Runtime application package",
                    "This package does not advertise a source SDK. Content editing, saving and prepared-project Play remain available."};
        }
        const auto configuration = app.parent_path().filename().u8string();
        const bool development = configuration == std::string("Debug-") + Platform + "-x86_64" ||
                                 configuration == std::string("Release-") + Platform + "-x86_64" ||
                                 configuration == std::string("Dist-") + Platform + "-x86_64";
        if (development && app.parent_path().parent_path().filename() == "bin" &&
            (app.filename() == "Hazelnut" || app.filename() == "MigrationEditorSmoke"))
            return Validate(app.parent_path().parent_path().parent_path(), "Development executable layout");
        return {SDKState::NotConfigured, {}, "Automatic discovery",
                "No SDK locator or supported development executable layout. Select a prepared Hazel source checkout."};
    }
    catch (const std::exception &error)
    {
        return {SDKState::Incompatible, {}, "Application build.json locator", error.what()};
    }
}
} // namespace Hazel
