#include "hzpch.h"
#include "AssetData.h"
#include "Hazel/Utils/UTF8.h"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cctype>
namespace Hazel
{
namespace
{
constexpr uint32_t constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
uint32_t rotate(uint32_t x, unsigned bits)
{
    return (x >> bits) | (x << (32 - bits));
}
void block(uint32_t *state, const uint8_t *input)
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t(input[i * 4]) << 24) | (uint32_t(input[i * 4 + 1]) << 16) |
               (uint32_t(input[i * 4 + 2]) << 8) | input[i * 4 + 3];
    for (int i = 16; i < 64; ++i)
    {
        auto x = w[i - 15], y = w[i - 2];
        w[i] = w[i - 16] + (rotate(x, 7) ^ rotate(x, 18) ^ (x >> 3)) + w[i - 7] +
               (rotate(y, 17) ^ rotate(y, 19) ^ (y >> 10));
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5],
             g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i)
    {
        auto t = h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) +
                 constants[i] + w[i];
        auto u = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t;
        d = c;
        c = b;
        b = a;
        a = t + u;
    }
    const uint32_t values[]{a, b, c, d, e, f, g, h};
    for (int i = 0; i < 8; ++i)
        state[i] += values[i];
}
int digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    throw std::runtime_error("Asset identity requires canonical lowercase hexadecimal");
}
} // namespace
ContentDigest AssetSHA256(const void *data, size_t bytes)
{
    if (bytes > UINT64_MAX / 8 || (!data && bytes))
        throw std::runtime_error("Invalid SHA-256 input size");
    uint32_t state[]{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    auto input = static_cast<const uint8_t *>(data);
    size_t consumed = 0;
    while (bytes - consumed >= 64)
    {
        block(state, input + consumed);
        consumed += 64;
    }
    uint8_t tail[128]{};
    const auto remaining = bytes - consumed;
    if (remaining)
        std::memcpy(tail, input + consumed, remaining);
    tail[remaining] = 128;
    const size_t padded = remaining < 56 ? 64 : 128;
    const uint64_t bits = uint64_t(bytes) * 8;
    for (unsigned i = 0; i < 8; ++i)
        tail[padded - 1 - i] = uint8_t(bits >> (i * 8));
    block(state, tail);
    if (padded == 128)
        block(state, tail + 64);
    ContentDigest result{};
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 4; ++j)
            result[i * 4 + j] = uint8_t(state[i] >> (24 - j * 8));
    return result;
}
std::string DigestText(const ContentDigest &digest)
{
    const char *digits = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (auto byte : digest)
    {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}
std::string AssetHash(std::string_view bytes)
{
    return DigestText(AssetSHA256(bytes.data(), bytes.size()));
}
ContentDigest ReadDigest(const std::string &text)
{
    if (text.size() != 64)
        throw std::runtime_error("Asset digest requires 64 hexadecimal characters");
    ContentDigest result{};
    for (size_t i = 0; i < 32; ++i)
        result[i] = uint8_t(digit(text[i * 2]) * 16 + digit(text[i * 2 + 1]));
    return result;
}
std::string ReadAssetBytes(const std::filesystem::path &path, size_t limit)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot read asset: " + path.generic_u8string());
    auto length = file.tellg();
    if (length < 0 || uint64_t(length) > limit)
        throw std::runtime_error("Asset exceeds supported byte limit: " + path.generic_u8string());
    std::string bytes(size_t(length), '\0');
    file.seekg(0);
    if (!bytes.empty() && !file.read(bytes.data(), std::streamsize(bytes.size())))
        throw std::runtime_error("Cannot read complete asset: " + path.generic_u8string());
    return bytes;
}
void AssetPath(const std::filesystem::path &path, const char *extension, bool optional)
{
    const auto text = path.generic_u8string();
    if (optional && text.empty())
        return;
    if (text.empty() || text.size() > 1024 || path.has_root_path() ||
        text.find_first_of(":\\\r\n<>\"|?*") != std::string::npos ||
        (extension && path.extension() != extension))
        throw std::runtime_error("Select a portable asset-relative " +
                                 std::string(extension ? extension : "file") + " path");
    UTF8::Validate(text);
    for (auto c : text)
        if (uint8_t(c) < 32 || uint8_t(c) == 127)
            throw std::runtime_error("Asset path contains a control character");
    for (const auto &part : path)
    {
        if (part == "." || part == ".." || part.empty())
            throw std::runtime_error("Asset path contains traversal or empty components");
        auto name = part.generic_u8string();
        if (name.back() == '.' || name.back() == ' ')
            throw std::runtime_error("Asset path has a trailing dot/space");
        auto stem = name.substr(0, name.find('.'));
        for (auto &c : stem)
            c = char(std::toupper(static_cast<unsigned char>(c)));
        if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
            (stem.size() == 4 && (stem.substr(0, 3) == "COM" || stem.substr(0, 3) == "LPT") &&
             stem[3] >= '1' && stem[3] <= '9'))
            throw std::runtime_error("Asset path uses a reserved device name");
    }
}
void AssetName(const std::string &name)
{
    if (name.empty() || name.size() > 256)
        throw std::runtime_error("Asset name requires 1–256 UTF-8 bytes");
    UTF8::Validate(name);
    for (auto c : name)
        if (uint8_t(c) < 32 || uint8_t(c) == 127)
            throw std::runtime_error("Asset name contains a control character");
}
std::string AssetKey(uint64_t key)
{
    if (!key)
        throw std::runtime_error("Asset/slot identity cannot be zero");
    std::string text(16, '0');
    const char *digits = "0123456789abcdef";
    for (int i = 15; i >= 0; --i)
    {
        text[i] = digits[key & 15];
        key >>= 4;
    }
    return text;
}
uint64_t ReadAssetKey(const std::string &text)
{
    if (text.size() != 16)
        throw std::runtime_error("Asset identity requires 16 hexadecimal characters");
    uint64_t value = 0;
    for (auto c : text)
        value = (value << 4) | uint64_t(digit(c));
    if (!value)
        throw std::runtime_error("Asset/slot identity cannot be zero");
    return value;
}
} // namespace Hazel
