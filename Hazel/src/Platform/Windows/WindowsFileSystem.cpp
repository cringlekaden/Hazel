#include "hzpch.h"
#include "Hazel/Core/FileSystem.h"
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <cerrno>
#include <system_error>
#include <shlobj.h>

namespace Hazel {
std::filesystem::path FileSystem::GetExecutablePath() {
    std::vector<wchar_t> buffer(32768);
    DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!count || count >= buffer.size()) throw std::runtime_error("Cannot locate executable");
    return std::filesystem::path(std::wstring(buffer.data(), count));
}
std::filesystem::path FileSystem::GetUserDataDirectory() {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
        throw std::runtime_error("Cannot locate LocalAppData");
    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}
std::FILE* FileSystem::OpenExclusiveOutput(const std::filesystem::path& path) {
    const int descriptor = _wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
    if (descriptor < 0) {
        if (errno == EEXIST) return nullptr;
        throw std::system_error(errno, std::generic_category(), "Create save temporary " + path.generic_u8string());
    }
    auto* file = _fdopen(descriptor, "wb");
    if (!file) {
        const int error = errno; _close(descriptor);
        std::error_code ignored; std::filesystem::remove(path, ignored);
        throw std::system_error(error, std::generic_category(), "Open save temporary stream");
    }
    return file;
}
void FileSystem::ReplaceFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
        throw std::system_error(GetLastError(), std::system_category(), "Replace saved file " + destination.generic_u8string());
}
}
