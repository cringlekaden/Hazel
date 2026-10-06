#include "Hazel/Core/FileLease.h"
#include "hzpch.h"
#include <fcntl.h>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>
namespace Hazel {
    std::unique_ptr<FileLease> FileLease::Try(const std::filesystem::path &path) {
        std::filesystem::create_directories(path.parent_path());
        const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), "Open settings lock");
        if (flock(fd, LOCK_EX | LOCK_NB)) {
            const int error = errno;
            close(fd);
            if (error == EWOULDBLOCK || error == EAGAIN)
                return {};
            throw std::system_error(error, std::generic_category(), "Lock settings");
        }
        return std::unique_ptr<FileLease>(new FileLease(fd));
    }
    FileLease::~FileLease() { close(static_cast<int>(m_Handle)); }
} // namespace Hazel
