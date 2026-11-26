#include "hzpch.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Renderer/Scene/ReflectionProbe.h"
#include "Hazel/Core/Resources.h"
#include <cmath>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalTexture.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Hazel/Renderer/RenderCommand.h"
#endif
namespace Hazel
{
Ref<TextureCube> TextureCube::Create(const LightingImage &d)
{
    d.Validate();
    if (d.Kind == LightingImageKind::BRDF)
        throw std::runtime_error("Cubemap requires six RGB faces");
#ifdef HZ_PLATFORM_MACOS
    if (Renderer::GetAPI() == RendererAPI::API::Metal)
        return CreateRef<MetalTextureCube>(static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device(), d);
#endif
    if (Renderer::GetAPI() != RendererAPI::API::OpenGL)
        throw std::runtime_error("Cubemap backend unavailable");
    return CreateRef<OpenGLTextureCube>(d);
}
LightingImage ReadRuntimeBRDF()
{
    auto image = DecodeLightingImage(ReadAssetBytes(Resources::Get().Root / RuntimeBRDFResource,
                                                   LightingArtifactByteLimit));
    if (image.Kind != LightingImageKind::BRDF || image.Side < 2)
        throw std::runtime_error("RuntimeBRDF_v2 requires a BRDF endpoint grid with side >=2");
    return image;
}
std::vector<float> PackBRDFTable(const LightingImage &image)
{
    image.Validate();
    if (image.Kind != LightingImageKind::BRDF || image.Side < 2)
        throw std::runtime_error("RuntimeBRDF_v2 requires a BRDF endpoint grid with side >=2");
    const size_t side = image.Side;
    const auto &source = image.Mips[0];
    std::vector<float> packed(side * side * 4);
    const double step = 1. / double(side - 1);
    for (size_t y = 0; y < side; ++y)
    {
        glm::dvec2 average(0);
        auto at = [&](size_t x) {
            const size_t p = (y * side + x) * 2;
            return glm::dvec2(source[p], source[p + 1]);
        };
        for (size_t x = 0; x + 1 < side; ++x)
        {
            const double left = double(x) * step, right = double(x + 1) * step;
            // 2*mu*dmu = 8*t^7*dt. Integrate each linear t segment exactly.
            const double area = std::pow(right, 8) - std::pow(left, 8);
            const double rightWeight = (8 * (std::pow(right, 9) - std::pow(left, 9)) / 9 - left * area) / step;
            average += at(x) * (area - rightWeight) + at(x + 1) * rightWeight;
        }
        for (size_t x = 0; x < side; ++x)
        {
            const size_t p = y * side + x;
            packed[p * 4] = source[p * 2];
            packed[p * 4 + 1] = source[p * 2 + 1];
            packed[p * 4 + 2] = float(average.x);
            packed[p * 4 + 3] = float(average.y);
        }
    }
    return packed;
}
ReflectionProbeResources::ReflectionProbeResources(const PreparedEnvironment &environment)
{
    ValidateReflectionProbeEnvironment(environment);
    Specular = TextureCube::Create(environment.Specular);
    Cosine = TextureCube::Create(environment.Irradiance);
}

EnvironmentResources::EnvironmentResources(const PreparedEnvironment &a)
{
    Background = TextureCube::Create(a.Background);
    Irradiance = TextureCube::Create(a.Irradiance);
    Specular = TextureCube::Create(a.Specular);
    a.BRDF.Validate();
    if (a.BRDF.Kind != LightingImageKind::BRDF)
        throw std::runtime_error("Expected BRDF data LUT");
    // Accepted environment A/B remains validated historical source data. Current
    // lighting uses the same engine-owned RuntimeBRDF_v2 for every environment.
    const auto runtime = ReadRuntimeBRDF();
    TextureSpecification spec;
    spec.Width = spec.Height = runtime.Side;
    spec.Format = ImageFormat::RGBA32F;
    spec.GenerateMips = false;
    spec.MinFilter = spec.MagFilter = TextureFilter::Linear;
    spec.WrapS = spec.WrapT = TextureWrap::ClampToEdge;
    BRDF = Texture2D::Create(spec);
    const auto packed = PackBRDFTable(runtime);
    BRDF->SetData(packed.data(), uint32_t(packed.size() * sizeof(float)));
}
} // namespace Hazel
