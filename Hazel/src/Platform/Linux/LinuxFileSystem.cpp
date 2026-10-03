#include "hzpch.h"
#include "Hazel/Core/FileSystem.h"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>

namespace Hazel {
std::filesystem::path FileSystem::GetEnvironmentPath(const char* name) {
    const char* value = std::getenv(name);
    return value && *value ? std::filesystem::u8path(value) : std::filesystem::path{};
}
std::filesystem::path FileSystem::GetExecutablePath() {
    return std::filesystem::read_symlink("/proc/self/exe");
}
std::filesystem::path FileSystem::GetUserDataDirectory() {
    if (const char* value = std::getenv("XDG_DATA_HOME"); value && *value) {
        auto root = std::filesystem::u8path(value);
        if (!root.is_absolute()) throw std::runtime_error("XDG_DATA_HOME must be absolute");
        return root;
    }
    const char* value = std::getenv("HOME");
    if (!value || !*value) throw std::runtime_error("HOME is required to locate writable application data");
    return std::filesystem::u8path(value) / ".local/share";
}
std::FILE* FileSystem::OpenExclusiveOutput(const std::filesystem::path& path) {
    const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (descriptor < 0) {
        if (errno == EEXIST) return nullptr;
        throw std::system_error(errno, std::generic_category(), "Create save temporary " + path.generic_u8string());
    }
    auto* file = fdopen(descriptor, "wb");
    if (!file) {
        const int error = errno; close(descriptor);
        std::error_code ignored; std::filesystem::remove(path, ignored);
        throw std::system_error(error, std::generic_category(), "Open save temporary stream");
    }
    return file;
}
void FileSystem::ReplaceFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::filesystem::rename(source, destination);
}
}
