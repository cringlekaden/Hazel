#pragma once
#include <filesystem>
#include <string>

namespace Hazel {
    struct ApplicationResourceSpecification {
        std::filesystem::path Root;
        std::filesystem::path MonoRoot;
        std::filesystem::path UserData;
    };

    // One configuration for the current application. Configure before renderer/Mono
    // initialization; keep it unchanged until both have shut down. Test hosts may
    // configure explicitly without creating an Application.
    class Resources {
    public:
        // Explicit fields win; resolve defaults only for roots the host omitted.
        static ApplicationResourceSpecification Defaults(const std::string& application,
            ApplicationResourceSpecification specification = {});
        static void Configure(const ApplicationResourceSpecification& specification);
        static const ApplicationResourceSpecification& Get();
        static std::filesystem::path Resolve(const std::filesystem::path& relative);
        static std::filesystem::path CacheDirectory();
    };
}
