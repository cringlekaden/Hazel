#pragma once
#include <filesystem>

namespace Hazel {
// Read-only installation locations. Writable profile/cache/save ownership stays
// with Resources and the platform data-directory services.
struct InstallationLayout {
    std::filesystem::path Executables, ResourceRoot, MonoRoot, RuntimeProject, EditorProject, Metadata;
    bool Bundled=false;
    static InstallationLayout FromExecutable(const std::filesystem::path& executable);
    static const InstallationLayout& Current();
};
}
