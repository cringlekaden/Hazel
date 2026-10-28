#include "hzpch.h"
#include "ManagedString.h"
#include <mono/metadata/object.h>
#include <cstdint>
#include <stdexcept>

namespace Hazel::ManagedStrings {
    std::string Read(MonoString* value, size_t maxUnits, size_t maxBytes,
                     const char* subject, NulPolicy nul) {
        if (!value)
            return {};
        const auto length = mono_string_length(value);
        auto reject = [subject](const char* reason) {
            throw std::invalid_argument(std::string(subject) + reason);
        };
        if (length < 0 || size_t(length) > maxUnits)
            reject(" exceeds its UTF-16 input limit");
        const auto* chars = mono_string_chars(value);
        auto scalar = [&](int32_t& index) -> uint32_t {
            uint32_t c = chars[index++];
            if (!c && nul == NulPolicy::Reject)
                reject(" cannot contain embedded NUL");
            if (c >= 0xd800 && c <= 0xdbff) {
                if (index == length || chars[index] < 0xdc00 || chars[index] > 0xdfff)
                    reject(" contains an unpaired UTF-16 surrogate");
                return 0x10000 + ((c - 0xd800) << 10) + (chars[index++] - 0xdc00);
            }
            if (c >= 0xdc00 && c <= 0xdfff)
                reject(" contains an unpaired UTF-16 surrogate");
            return c;
        };
        size_t bytes = 0;
        for (int32_t i = 0; i < length;) {
            const auto c = scalar(i);
            const size_t count = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
            if (count > maxBytes - bytes)
                reject(" exceeds its UTF-8 byte limit");
            bytes += count;
        }
        std::string result;
        result.reserve(bytes);
        for (int32_t i = 0; i < length;) {
            const auto c = scalar(i);
            if (c < 0x80)
                result.push_back(char(c));
            else if (c < 0x800) {
                result.push_back(char(0xc0 | (c >> 6)));
                result.push_back(char(0x80 | (c & 0x3f)));
            } else if (c < 0x10000) {
                result.push_back(char(0xe0 | (c >> 12)));
                result.push_back(char(0x80 | ((c >> 6) & 0x3f)));
                result.push_back(char(0x80 | (c & 0x3f)));
            } else {
                result.push_back(char(0xf0 | (c >> 18)));
                result.push_back(char(0x80 | ((c >> 12) & 0x3f)));
                result.push_back(char(0x80 | ((c >> 6) & 0x3f)));
                result.push_back(char(0x80 | (c & 0x3f)));
            }
        }
        return result;
    }
}
