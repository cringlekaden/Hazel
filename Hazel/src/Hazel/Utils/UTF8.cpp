#include "hzpch.h"
#include "UTF8.h"
#include <stdexcept>

namespace Hazel::UTF8
{
void Decode(std::string_view text, std::vector<uint32_t> &scalars, std::vector<size_t> *offsets)
{
    scalars.clear();
    if (offsets)
        offsets->clear();
    for (size_t i = 0; i < text.size();)
    {
        const auto start = i;
        auto lead = static_cast<uint8_t>(text[i++]);
        uint32_t value = lead, minimum = 0;
        size_t remaining = 0;
        if (lead >= 0xc2 && lead <= 0xdf)
        {
            value = lead & 0x1f;
            remaining = 1;
            minimum = 0x80;
        }
        else if (lead >= 0xe0 && lead <= 0xef)
        {
            value = lead & 0x0f;
            remaining = 2;
            minimum = 0x800;
        }
        else if (lead >= 0xf0 && lead <= 0xf4)
        {
            value = lead & 7;
            remaining = 3;
            minimum = 0x10000;
        }
        else if (lead >= 0x80)
            throw std::invalid_argument("Text contains an invalid UTF-8 lead byte");
        if (remaining > text.size() - i)
            throw std::invalid_argument("Text contains truncated UTF-8");
        for (size_t j = 0; j < remaining; ++j)
        {
            auto next = static_cast<uint8_t>(text[i++]);
            if ((next & 0xc0) != 0x80)
                throw std::invalid_argument("Text contains an invalid UTF-8 continuation");
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::invalid_argument("Text contains an invalid Unicode scalar/overlong UTF-8");
        if (!value)
            throw std::invalid_argument("Text cannot contain embedded NUL");
        if (start == 0 && value == 0xfeff)
            continue;
        if (offsets)
            offsets->push_back(start);
        scalars.push_back(value);
    }
    if (offsets)
        offsets->push_back(text.size());
}
void Validate(std::string_view text)
{
    thread_local std::vector<uint32_t> scalars;
    Decode(text, scalars);
}
} // namespace Hazel::UTF8
