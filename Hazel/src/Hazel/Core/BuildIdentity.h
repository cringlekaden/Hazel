#pragma once
#include <filesystem>
#include <string>

namespace Hazel {
// Supplied by Premake from scripts/internal/toolchain.json, never inferred from
// the machine running an installed application. No native OS API belongs here.
struct BuildIdentity {
    std::string System, Architecture, Configuration, ExecutableSuffix;
    bool ExportSupported;
    static const BuildIdentity& Current() {
        static const BuildIdentity identity{HZ_TARGET_SYSTEM, HZ_TARGET_ARCHITECTURE, HZ_BUILD_CONFIGURATION,
                HZ_TARGET_EXECUTABLE_SUFFIX, HZ_TARGET_EXPORT_SUPPORTED != 0};
        return identity;
    }
    std::string ToolConfiguration() const { return Configuration == "Dist" ? "Release" : Configuration; }
    std::string OutputName(const std::string& configuration) const { return configuration + "-" + System + "-" + Architecture; }
    std::filesystem::path Products(const std::filesystem::path& checkout, const std::string& configuration) const {
        return checkout / "bin" / OutputName(configuration);
    }
    std::filesystem::path NativeTool(const std::filesystem::path& checkout, const std::string& configuration, const std::string& name) const {
        return Products(checkout, configuration) / name / (name + ExecutableSuffix);
    }
    std::filesystem::path Premake(const std::filesystem::path& checkout) const {
        return checkout / "build/tools/premake-core/bin/release" / ("premake5" + ExecutableSuffix);
    }
};
}
