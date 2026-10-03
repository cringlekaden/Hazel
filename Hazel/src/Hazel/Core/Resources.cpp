#include "hzpch.h"
#include "Resources.h"
#include "FileSystem.h"
#include <cstdlib>

namespace Hazel {
    static ApplicationResourceSpecification s_Resources;
    static std::filesystem::path ConfiguredPath(const char* variable, const std::filesystem::path& standard) {
        const auto value = FileSystem::GetEnvironmentPath(variable);
        return !value.empty() ? std::filesystem::absolute(value) : standard;
    }
    ApplicationResourceSpecification Resources::Defaults(const std::string& application) {
        const auto executable = FileSystem::GetExecutablePath().parent_path();
        auto data = FileSystem::GetEnvironmentPath("HAZEL_DATA");
        if (data.empty()) data = FileSystem::GetUserDataDirectory() / "Hazel" / application;
        return { ConfiguredPath("HAZEL_RESOURCES", executable / "Resources"),
                 ConfiguredPath("HAZEL_MONO", executable / "mono"),
                 std::filesystem::absolute(data) };
    }
    void Resources::Configure(const ApplicationResourceSpecification& specification) {
        if (specification.Root.empty() || specification.MonoRoot.empty() || specification.UserData.empty())
            throw std::invalid_argument("Application resource, Mono and user-data roots must be configured");
        s_Resources = { std::filesystem::absolute(specification.Root),
            std::filesystem::absolute(specification.MonoRoot), std::filesystem::absolute(specification.UserData) };
        std::filesystem::create_directories(s_Resources.UserData);
    }
    const ApplicationResourceSpecification& Resources::Get() {
        if (s_Resources.Root.empty()) throw std::logic_error("Configure application resources before initialization");
        return s_Resources;
    }
    std::filesystem::path Resources::Resolve(const std::filesystem::path& relative) {
        if (relative.is_absolute()) throw std::invalid_argument("Engine resource references must be relative");
        const auto normalized = relative.lexically_normal();
        if (normalized.empty() || *normalized.begin() == "..") throw std::invalid_argument("Engine resource reference escapes its root");
        return Get().Root / normalized;
    }
    std::filesystem::path Resources::CacheDirectory() { return Get().UserData / "cache/shader/opengl"; }
}
