#pragma once
// Local portability adaptation: UTF-8 path bytes, independent of wchar_t size.
#include <filesystem>
#include <cstring>
#include <stdexcept>
namespace Hazel {
inline std::filesystem::path ContentBrowserPath(const void* data, int size) {
    if (!data || size < 1) throw std::invalid_argument("Empty content-browser payload");
    const auto* bytes = static_cast<const char*>(data);
    if (bytes[size - 1] != '\0' || std::memchr(bytes, '\0', size - 1))
        throw std::invalid_argument("Invalid content-browser path payload");
    return std::filesystem::u8path(std::string(bytes, size - 1));
}
}
