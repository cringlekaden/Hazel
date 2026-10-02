#include "hzpch.h"
#include "Hazel/Core/FileSystem.h"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>

namespace Hazel {
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
