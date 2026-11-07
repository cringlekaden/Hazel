#include "hzpch.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Renderer.h"
namespace Hazel
{
Ref<TextureCube> TextureCube::Create(const LightingImage &d)
{
    d.Validate();
    if (d.Kind == LightingImageKind::BRDF)
        throw std::runtime_error("Cubemap requires six RGB faces");
    if (Renderer::GetAPI() != RendererAPI::API::OpenGL)
        throw std::runtime_error("Cubemap backend unavailable");
    return CreateRef<OpenGLTextureCube>(d);
}
EnvironmentResources::EnvironmentResources(const PreparedEnvironment &a)
{
    Background = TextureCube::Create(a.Background);
    Irradiance = TextureCube::Create(a.Irradiance);
    Specular = TextureCube::Create(a.Specular);
    a.BRDF.Validate();
    if (a.BRDF.Kind != LightingImageKind::BRDF)
        throw std::runtime_error("Expected BRDF data LUT");
    TextureSpecification spec;
    spec.Width = spec.Height = a.BRDF.Side;
    spec.Format = ImageFormat::RG32F;
    spec.GenerateMips = false;
    spec.MinFilter = spec.MagFilter = TextureFilter::Linear;
    spec.WrapS = spec.WrapT = TextureWrap::ClampToEdge;
    BRDF = Texture2D::Create(spec);
    BRDF->SetData(a.BRDF.Mips[0].data(), uint32_t(a.BRDF.Bytes()));
}
} // namespace Hazel
