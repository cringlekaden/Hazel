#pragma once
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace MetalM2A {
inline void Check(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}
inline std::string Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Check(bool(input), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
inline std::vector<uint32_t> Words(const std::filesystem::path& path)
{
    const auto bytes = Read(path);
    Check(!bytes.empty() && bytes.size() % 4 == 0, "Invalid SPIR-V file");
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}
int Translate(const std::filesystem::path& output);
}
