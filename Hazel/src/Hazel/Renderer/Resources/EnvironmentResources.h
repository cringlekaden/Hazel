#pragma once
#include "Hazel/Assets/Environment.h"
#include "Hazel/Renderer/Resources/Texture.h"
#include <vector>
namespace Hazel
{
// RuntimeBRDF_v2 RGBA: A/B plus exact cosine averages of the mu=t^4
// endpoint-grid interpolant. Historical environment LUT artifacts are not rewritten.
LightingImage ReadRuntimeBRDF();
std::vector<float> PackBRDFTable(const LightingImage &image);

// Render-thread realization; accepted CPU definitions have no backend identities.
class TextureCube
{
  public:
    virtual ~TextureCube() = default;
    virtual void Bind(uint32_t slot) const = 0;
    static Ref<TextureCube> Create(const LightingImage &image);
};
// Both kernels belong to local specular prefiltering. The dense cosine mip
// is evaluated through the same irradiance-over-pi endpoint as global IBL.
struct ReflectionProbeResources
{
    Ref<TextureCube> Specular, Cosine;
    explicit ReflectionProbeResources(const PreparedEnvironment &environment);
};
class EnvironmentResources
{
  public:
    Ref<TextureCube> Background, Irradiance, Specular;
    Ref<Texture2D> BRDF;
    explicit EnvironmentResources(const PreparedEnvironment &accepted);
};
} // namespace Hazel
