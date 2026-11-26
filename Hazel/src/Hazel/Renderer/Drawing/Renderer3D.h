#pragma once
#include "Hazel/Renderer/Scene/SceneSubmission.h"
namespace Hazel
{
// Forward static meshes, material/light bindings and the ordered environment background.
// Destination/presentation and world-2D/UI sequencing belong to SceneRenderer.
class Renderer3D
{
  public:
    static void Render(const SceneSubmission &frame, bool drawBackground, bool depthOnly = false, bool motionOnly = false,
                       const ViewReflectionProbes *probes = nullptr);
    static void Shutdown();
};
}
