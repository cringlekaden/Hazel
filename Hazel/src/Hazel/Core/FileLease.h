#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
namespace Hazel {
    // Nonblocking, process-owned lock. The persistent lock file is never unlinked:
    // replacing its inode would allow two owners. Handles do not reach child tools.
    class FileLease {
      public:
        static std::unique_ptr<FileLease> Try(const std::filesystem::path &path);
        ~FileLease();
        FileLease(const FileLease &) = delete;
        FileLease &operator=(const FileLease &) = delete;

      private:
        explicit FileLease(intptr_t handle) : m_Handle(handle) {}
        intptr_t m_Handle;
    };
} // namespace Hazel
