#pragma once
#include "Hazel/Assets/Environment.h"
#include "Hazel/Renderer/Resources/Texture.h"
namespace Hazel
{
// Render-thread realization; accepted CPU definitions have no backend identities.
class TextureCube
{
  public:
    virtual ~TextureCube() = default;
    virtual void Bind(uint32_t slot) const = 0;
    static Ref<TextureCube> Create(const LightingImage &image);
};
class EnvironmentResources
{
  public:
    Ref<TextureCube> Background, Irradiance, Specular;
    Ref<Texture2D> BRDF;
    explicit EnvironmentResources(const PreparedEnvironment &accepted);
};
} // namespace Hazel
