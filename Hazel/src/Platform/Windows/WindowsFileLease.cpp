#include "hzpch.h"
#include "Hazel/Core/FileLease.h"
#include <Windows.h>
#include <system_error>
namespace Hazel {
    std::unique_ptr<FileLease> FileLease::Try(const std::filesystem::path &path) {
        std::filesystem::create_directories(path.parent_path());
        const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(GetLastError(), std::system_category(), "Open settings lock");
        OVERLAPPED range{};
        if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                        &range)) {
            const auto error = GetLastError();
            CloseHandle(handle);
            if (error == ERROR_LOCK_VIOLATION)
                return {};
            throw std::system_error(error, std::system_category(), "Lock settings");
        }
        return std::unique_ptr<FileLease>(new FileLease(reinterpret_cast<intptr_t>(handle)));
    }
    FileLease::~FileLease() {
        OVERLAPPED range{};
        auto handle = reinterpret_cast<HANDLE>(m_Handle);
        UnlockFileEx(handle, 0, 1, 0, &range);
        CloseHandle(handle);
    }
} // namespace Hazel
