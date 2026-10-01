#include "hzpch.h"
#include "Hazel/Utils/FileWatcher.h"
namespace Hazel {
#ifdef HZ_PLATFORM_LINUX
    Scope<FileWatcher> CreateLinuxFileWatcher(const std::filesystem::path&, FileWatcher::Callback);
#elif defined(HZ_PLATFORM_WINDOWS)
    Scope<FileWatcher> CreateWindowsFileWatcher(const std::filesystem::path&, FileWatcher::Callback);
#endif
    Scope<FileWatcher> FileWatcher::Create(const std::filesystem::path& path, Callback callback) {
#ifdef HZ_PLATFORM_LINUX
        return CreateLinuxFileWatcher(path, std::move(callback));
#elif defined(HZ_PLATFORM_WINDOWS)
        return CreateWindowsFileWatcher(path, std::move(callback));
#else
        throw std::runtime_error("File watching is unavailable on this platform");
#endif
    }
}
