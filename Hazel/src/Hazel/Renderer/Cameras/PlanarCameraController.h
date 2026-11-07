#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
namespace Hazel
{
// Orthographic pan/zoom policy in logical points. Hosts own bindings and projection.
class PlanarCameraController
{
  public:
    glm::vec2 Focus{0};
    float Size = 10;
    void Restore(glm::vec2 focus, float size)
    {
        if (!std::isfinite(focus.x) || !std::isfinite(focus.y) || !std::isfinite(size) ||
            size < .01f || size > 100000)
            return;
        Focus = focus;
        Size = size;
    }
    void Pan(glm::vec2 points, float height)
    {
        if (height > 0)
            Focus += glm::vec2(-points.x, points.y) * (Size / height);
    }
    void Zoom(float steps) { Size = std::clamp(Size * std::exp(-steps * .15f), .01f, 100000.f); }
    void Frame(glm::vec3 minimum, glm::vec3 maximum, float aspect)
    {
        auto span = maximum - minimum;
        Restore(glm::vec2((minimum + maximum) * .5f),
                std::clamp(std::max(span.y, span.x / aspect) * 1.2f, .01f, 100000.f));
    }
};
} // namespace Hazel
