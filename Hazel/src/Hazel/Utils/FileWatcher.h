#pragma once
// Local OS adapter for the target filewatch/reload architecture. Native paths
// and watcher resources stay in Platform; this API has no native OS types.
#include "Hazel/Core/Base.h"
#include <filesystem>
#include <functional>
namespace Hazel {
    enum class FileWatchEvent { added, removed, modified, renamed_old, renamed_new };
    class FileWatcher {
    public:
        using Callback = std::function<void(const std::filesystem::path&, FileWatchEvent)>;
        virtual ~FileWatcher() = default;
        static Scope<FileWatcher> Create(const std::filesystem::path& path, Callback callback);
    };
}
