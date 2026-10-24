#include "hzpch.h"

#include "Slider.h"
#include <cmath>
#include <limits>
namespace Hazel::GameUI
{
void SteppedRange::Validate() const
{
    if (!std::isfinite(Minimum) || !std::isfinite(Maximum) || !std::isfinite(Step) ||
        Minimum >= Maximum || std::abs(Minimum) > 1e9f || std::abs(Maximum) > 1e9f ||
        Step < 1e-6f || !std::isfinite(Maximum - Minimum))
        throw std::invalid_argument("Slider range: finite increasing endpoints "
                                    "within +/-1e9 and step >= 1e-6 required");
    float extent = std::max(std::abs(Minimum), std::abs(Maximum));
    if (Step < std::nextafter(extent, std::numeric_limits<float>::infinity()) - extent ||
        std::ceil((double(Maximum) - Minimum) / Step) > MaxSteps)
        throw std::invalid_argument("Slider range: step is below float precision "
                                    "or exceeds 65536 intervals");
}
uint32_t SteppedRange::LastIndex() const
{
    Validate();
    double span = double(Maximum) - Minimum;
    auto regular = uint32_t(std::floor(span / Step));
    // A rounded regular position may coincide with the exact endpoint.
    return regular + (float(double(Minimum) + double(regular) * Step) < Maximum ? 1 : 0);
}
float SteppedRange::At(uint32_t index) const
{
    auto last = LastIndex();
    if (index >= last)
        return Maximum;
    return float(double(Minimum) + double(index) * Step);
}
uint32_t SteppedRange::Index(float value) const
{
    if (!std::isfinite(value))
        throw std::invalid_argument("Slider value must be finite");
    auto last = LastIndex();
    if (value <= Minimum)
        return 0;
    if (value >= Maximum)
        return last;
    auto low = std::min(uint32_t(std::floor((double(value) - Minimum) / Step)), last - 1);
    const double a = At(low), b = At(low + 1);
    return double(value) - a < b - double(value) ? low : low + 1; // ties toward larger value
}
float SliderGeometry::Position(glm::vec2 point, float grab) const
{
    if (Travel <= 0)
        return 0;
    float t = std::clamp((point[Axis] - grab - Start) / Travel, 0.f, 1.f);
    return Reverse ? 1 - t : t;
}
bool SliderGeometry::operator==(const SliderGeometry &b) const
{
    return Track.Position == b.Track.Position && Track.Size == b.Track.Size &&
           Thumb.Size == b.Thumb.Size && Thumb.Position[1 - Axis] == b.Thumb.Position[1 - b.Axis] &&
           Start == b.Start && Travel == b.Travel && Axis == b.Axis && Reverse == b.Reverse;
}
SliderGeometry ArrangeSlider(Rect content, Rect track, glm::vec2 size, float normalized,
                             FillDirection direction)
{
    if (unsigned(direction) > 3 || !std::isfinite(normalized))
        throw std::invalid_argument("Invalid slider direction/position");
    SliderGeometry g;
    g.Track = track.Intersect(content);
    g.Axis = unsigned(direction) >= 2 ? 1 : 0;
    g.Reverse = direction == FillDirection::Left || direction == FillDirection::Up;
    size = glm::min(glm::max(size, glm::vec2(0)), content.Size);
    size[g.Axis] = std::min(size[g.Axis], g.Track.Size[g.Axis]);
    g.Start = g.Track.Position[g.Axis] + size[g.Axis] * .5f;
    g.Travel = g.Track.Size[g.Axis] - size[g.Axis];
    float t = std::clamp(normalized, 0.f, 1.f);
    if (g.Reverse)
        t = 1 - t;
    g.Thumb = {g.Track.Position + g.Track.Size * .5f - size * .5f, size};
    g.Thumb.Position[g.Axis] = g.Start + g.Travel * t - size[g.Axis] * .5f;
    int cross = 1 - g.Axis;
    g.Thumb.Position[cross] =
        std::clamp(g.Thumb.Position[cross], content.Position[cross],
                   content.Position[cross] + content.Size[cross] - size[cross]);
    g.Fill = g.Track;
    float extent = g.Track.Size[g.Axis] * std::clamp(normalized, 0.f, 1.f);
    if (g.Reverse)
        g.Fill.Position[g.Axis] += g.Fill.Size[g.Axis] - extent;
    g.Fill.Size[g.Axis] = extent;
    return g;
}
} // namespace Hazel::GameUI
