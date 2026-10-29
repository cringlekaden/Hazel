#include "hzpch.h"
#include "Dropdown.h"
#include "Hazel/Utils/UTF8.h"
#include <cmath>
namespace Hazel::GameUI
{
void Validate(const DropdownConfig &c)
{
    if (!std::isfinite(c.MinimumWidth) || c.MinimumWidth < 1 || c.MinimumWidth > 4096 ||
        !std::isfinite(c.MaximumHeight) || c.MaximumHeight < 1 || c.MaximumHeight > 4096 ||
        !std::isfinite(c.Gap) || c.Gap < 0 || c.Gap > 64 || c.Placeholder.size() > 1024)
        throw std::invalid_argument("Dropdown needs width/height 1..4096, gap "
                                    "0..64 and placeholder <=1024 UTF-8 bytes");
    UTF8::Validate(c.Placeholder);
}
} // namespace Hazel::GameUI
