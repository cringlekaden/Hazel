#pragma once
#include <glm/glm.hpp>
#include <array>

namespace Hazel {
struct SceneSubmission;
struct DirectionalShadowSettings
{
    bool Enabled = false;
    unsigned Resolution = 1024; // Per cascade; four viewports in a depth atlas.
    float Distance = 100, SplitLambda = .65f;
    float NormalBias = .2f, DepthBias = .05f; // World texels, independent of depth span.
    float Blend = .1f; // Fraction of each interval, including final distance fade.
    void Validate() const;
    bool operator==(const DirectionalShadowSettings& b) const {
        return Enabled==b.Enabled && Resolution==b.Resolution && Distance==b.Distance &&
            SplitLambda==b.SplitLambda && NormalBias==b.NormalBias && DepthBias==b.DepthBias && Blend==b.Blend;
    }
};
// std140 binding 5 in forward shading; motion uses this binding in a separate pass.
struct alignas(16) DirectionalShadowData
{
    std::array<glm::mat4,4> VP{};
    glm::vec4 Splits{0}, Eye{0}, Forward{0}, Parameters{0}, Range{0};
    std::array<glm::vec4,4> TexelDepth{}; // world texel width, inverse light-depth span.
};
static_assert(sizeof(DirectionalShadowData)==400,"Directional shadow std140 ABI");
DirectionalShadowData FitDirectionalShadows(const SceneSubmission& frame);
bool ShadowCasterVisible(const glm::vec3& minimum, const glm::vec3& maximum,
                         const glm::mat4& clip);
}
