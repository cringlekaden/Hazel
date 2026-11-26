#include "hzpch.h"
#include "ReflectionProbeCapture.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Scene/Scene.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Hazel
{
ReflectionProbeCapture::ReflectionProbeCapture(Ref<Scene> scene, const glm::vec3 &position, unsigned side)
    : m_Scene(std::move(scene)), m_Position(position)
{
    ReflectionProbe settings;
    settings.BakeResolution = side;
    settings.Validate();
    settings.ValidatePosition(position);
    if (!m_Scene || !m_Scene->GetAssets() || !m_Scene->Presentation().SceneReferred)
        throw std::runtime_error("Probe capture requires a project scene with scene-referred rendering");

    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    FramebufferSpecification spec;
    spec.Width = spec.Height = side;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    m_Target = Framebuffer::Create(spec);
    m_Image.Kind = LightingImageKind::Background;
    m_Image.Side = side;
    m_Image.Mips.emplace_back().reserve(size_t(side) * side * 18);
    m_Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
}

ReflectionProbeCapture::~ReflectionProbeCapture()
{
    if (m_Recorded)
        m_Target->Cancel();
}

bool ReflectionProbeCapture::Advance()
{
    if (m_Face == 6)
        return true;
    if (std::chrono::steady_clock::now() >= m_Deadline)
        throw std::runtime_error("Probe capture timed out waiting for a render face");

    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    if (!m_Recorded)
    {
        SceneCameraView camera;
        const auto forward = glm::vec3(CubeDirection(m_Face, .5, .5));
        const auto up = glm::vec3(CubeDirection(m_Face, .5, 0));
        camera.Position = m_Position;
        camera.ToCamera = -forward;
        // Use a rotation about the origin so that position + forward never
        // loses the unit offset at large coordinates.
        camera.View = glm::translate(glm::lookAt(glm::vec3(0), forward, up), -m_Position);
        camera.Projection = glm::perspective(glm::radians(90.f), 1.f, .05f, 1000.f);
        camera.ViewProjection = camera.Projection * camera.View;
        const auto frame = SceneRenderer::Collect(*m_Scene, camera, glm::uvec2(m_Image.Side), true);
        RenderCommand::StateScope state;
        m_Target->Bind();
        try
        {
            if (metal)
                m_Target->Clear(frame.Presentation.Background);
            else
            {
                auto raster = RenderCommand::GetRasterState();
                raster.SRGB = raster.ClipEnabled = false;
                raster.ColorMask = raster.PickingMask = glm::bvec4(true);
                raster.DepthWrite = true;
                RenderCommand::SetRasterState(raster);
                RenderCommand::SetClearColor(frame.Presentation.Background);
                RenderCommand::Clear();
            }
            m_Target->ClearAttachment(1, -1);
            SceneRenderer::Render(frame, m_View, m_Target, [] {}, [] {});
        }
        catch (...)
        {
            if (metal) m_Target->Cancel();
            else m_Target->Unbind();
            throw;
        }
        m_Target->Unbind();
        if (!metal)
        {
            StoreFace(m_Target->ReadColorPixels());
            return m_Face == 6;
        }
        m_Recorded = true;
    }

    m_Target->Poll();
    if (!m_Pending)
        m_Pending = m_Target->Submit();
    if (!m_Pending || !m_Pending->Completed)
        return false;
    if (!m_Pending->Error.empty())
        throw std::runtime_error(m_Pending->Error);

    StoreFace(m_Pending->Color);
    m_Pending.reset();
    m_Recorded = false;
    return m_Face == 6;
}

void ReflectionProbeCapture::StoreFace(const std::vector<glm::vec4> &pixels)
{
    const auto side = m_Image.Side;
    if (pixels.size() != size_t(side) * side)
        throw std::runtime_error("Incomplete probe capture face");
    auto &cube = m_Image.Mips[0];
    // Framebuffers are bottom-up. Cube t=0 is at the camera's top and
    // increasing cube s points opposite the outward-looking camera's right.
    for (unsigned y = side; y > 0; --y)
        for (unsigned x = 0; x < side; ++x)
        {
            const auto pixel = pixels[size_t(y - 1) * side + (side - 1 - x)];
            for (int channel = 0; channel < 3; ++channel)
            {
                const float radiance = pixel[channel];
                if (!std::isfinite(radiance) || radiance < 0 || radiance > 1e10f)
                    throw std::runtime_error("Probe capture contains radiance outside the supported HDR range");
                cube.push_back(radiance);
            }
        }
    ++m_Face;
}

const LightingImage &ReflectionProbeCapture::Image() const
{
    if (m_Face != 6)
        throw std::logic_error("Probe capture is incomplete");
    return m_Image;
}
}
