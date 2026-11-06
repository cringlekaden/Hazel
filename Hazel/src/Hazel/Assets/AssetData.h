#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace Hazel
{
using ContentDigest = std::array<uint8_t, 32>;
ContentDigest AssetSHA256(const void *data, size_t bytes);
std::string AssetHash(std::string_view bytes);
std::string DigestText(const ContentDigest &digest);
ContentDigest ReadDigest(const std::string &text);
std::string ReadAssetBytes(const std::filesystem::path &path, size_t limit);
void AssetPath(const std::filesystem::path &path, const char *extension = nullptr,
               bool optional = false);
void AssetName(const std::string &name);
void AssetNotice(const std::string &notice); // UTF-8 attribution, up to 64 KiB; preserves lines.
std::string AssetKey(uint64_t key);
uint64_t ReadAssetKey(const std::string &text);
} // namespace Hazel
