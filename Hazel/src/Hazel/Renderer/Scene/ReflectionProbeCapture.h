#pragma once

#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include <chrono>

namespace Hazel
{
// A private offline mesh/sky view. Its caller owns an immutable scene snapshot
// through the six faces and consumes the completed CPU cube on the render thread.
class ReflectionProbeCapture
{
  public:
    ReflectionProbeCapture(Ref<Scene> scene, const glm::vec3 &position, unsigned side);
    ~ReflectionProbeCapture();

    bool Advance();
    unsigned FacesComplete() const { return m_Face; }
    const LightingImage &Image() const;

  private:
    void StoreFace(const std::vector<glm::vec4> &pixels);

    Ref<Scene> m_Scene;
    glm::vec3 m_Position;
    LightingImage m_Image;
    SceneRenderView m_View;
    Ref<Framebuffer> m_Target;
    Ref<const FramebufferReadback> m_Pending;
    unsigned m_Face = 0;
    bool m_Recorded = false;
    std::chrono::steady_clock::time_point m_Deadline;
};
}
