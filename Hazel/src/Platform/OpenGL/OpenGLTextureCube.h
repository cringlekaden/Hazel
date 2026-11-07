#pragma once
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
namespace Hazel
{
class OpenGLTextureCube final : public TextureCube
{
    uint32_t m_ID = 0;

  public:
    explicit OpenGLTextureCube(const LightingImage &image);
    ~OpenGLTextureCube() override;
    void Bind(uint32_t slot) const override;
};
} // namespace Hazel
