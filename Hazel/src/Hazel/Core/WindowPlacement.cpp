#include "hzpch.h"
#include "WindowPlacement.h"
#include <algorithm>
#include <cmath>
namespace Hazel {
    WindowPlacement FitWindow(WindowPlacement p, const std::vector<DisplayArea> &displays) {
        if (displays.empty())
            return p; // A compositor may own placement entirely.
        const DisplayArea *chosen = &displays.front();
        long long best = 0;
        for (const auto &d : displays) {
            if (d.Width < 1 || d.Height < 1)
                continue;
            const auto w = std::max(0ll, std::min(static_cast<long long>(p.X) + p.Width,
                                                  static_cast<long long>(d.X) + d.Width) -
                                             std::max<long long>(p.X, d.X));
            const auto h = std::max(0ll, std::min(static_cast<long long>(p.Y) + p.Height,
                                                  static_cast<long long>(d.Y) + d.Height) -
                                             std::max<long long>(p.Y, d.Y));
            if (w * h > best) {
                best = w * h;
                chosen = &d;
            }
        }
        const auto &d = *chosen;
        const float scale = std::isfinite(d.Scale) && d.Scale > 0 ? d.Scale : 1;
        const float ratio =
            std::isfinite(p.Scale) && p.Scale > 0 ? std::clamp(scale / p.Scale, .5f, 2.f) : 1;
        p.Width = std::clamp(static_cast<int>(std::clamp(double(p.Width) * ratio, 1., 100000.)),
                             std::min(640, d.Width), d.Width);
        p.Height = std::clamp(static_cast<int>(std::clamp(double(p.Height) * ratio, 1., 100000.)),
                              std::min(360, d.Height), d.Height);
        if (!best) {
            p.X = d.X + (d.Width - p.Width) / 2;
            p.Y = d.Y + (d.Height - p.Height) / 2;
        } else {
            p.X = std::clamp(p.X, d.X, d.X + d.Width - p.Width);
            p.Y = std::clamp(p.Y, d.Y, d.Y + d.Height - p.Height);
        }
        p.Scale = scale;
        return p;
    }
} // namespace Hazel
