#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Hazel::UTF8
{
// Strict Unicode scalars. One leading BOM is ignored; embedded NUL is rejected.
// Optional offsets contain each retained scalar's byte start and the end offset.
// Caller-owned buffers retain capacity; malformed input throws invalid_argument.
void Decode(std::string_view text, std::vector<uint32_t> &scalars,
            std::vector<size_t> *offsets = nullptr);
void Validate(std::string_view text);
} // namespace Hazel::UTF8
