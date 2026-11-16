#include "MetalRendererAPI.h"
#include "MetalQuadFrame.h"
#include "MetalDevice.h"
#include "MetalShader.h"
#include <cmath>
#include <stdexcept>

namespace Hazel {
namespace {
void Check(bool value, const char* message) { if (!value) throw std::logic_error(message); }
bool Finite(const float* values, size_t count)
{
    for (size_t i = 0; i < count; ++i) if (!std::isfinite(values[i])) return false;
    return true;
}
}
std::shared_ptr<Texture2D> MetalRendererAPI::CreateTexture(const TextureSpecification& specification)
{
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    Check(!m_Stopped, "Cannot create textures after Metal backend shutdown");
    return std::make_shared<MetalTexture2D>(m_Device, specification);
}
void MetalRendererAPI::BeginOffscreenQuadFrame(uint32_t width, uint32_t height, glm::vec4 clear)
{
    Check(!m_Stopped && !m_QuadFrame, "Offscreen quad frame already active or backend stopped");
    Check(width && height && width <= 32 && height <= 32 && Finite(&clear[0], 4), "Offscreen profile requires finite clear and 1..32 extents");
    auto shader = std::static_pointer_cast<MetalShader>(LoadShader("Renderer2D_Quad"));
    auto frame = std::make_shared<MetalQuadFrame>();
    frame->Width = width; frame->Height = height; frame->Clear = clear; frame->Shader = std::move(shader);
    m_QuadFrame = std::move(frame);
}
void MetalRendererAPI::BeginQuadScene(const glm::mat4& camera)
{
    Check(!m_Stopped && m_QuadFrame && !m_QuadScene, "Begin a bounded offscreen frame before Renderer2D::BeginScene");
    Check(Finite(&camera[0][0], 16), "Quad camera must be finite");
    m_QuadCamera = camera; m_QuadScene = true;
}
void MetalRendererAPI::EnqueueQuadRun(const QuadVertex* vertices, uint32_t vertexCount, uint32_t first, uint32_t count,
    const std::array<std::shared_ptr<const MetalTextureVersion>, 32>& textures, uint32_t textureCount)
{
    Check(!m_Stopped && m_QuadFrame && m_QuadScene, "Quad run requires an active Renderer2D scene/offscreen frame");
    uint32_t used = 0;
    for (const auto& draw : m_QuadFrame->Draws) used += draw.Count / 6;
    Check(vertices && count && count <= 48 && first <= 48 && vertexCount % 4 == 0 && first % 6 == 0 && count % 6 == 0 && vertexCount <= 32 &&
        (first + count) / 6 * 4 <= vertexCount && used + count / 6 <= 8,
        "Offscreen profile supports at most eight quads per frame and valid run offsets");
    Check(textureCount && textureCount <= 32, "Invalid quad texture slot count");
    MetalQuadDraw draw;
    draw.First = first; draw.Count = count; draw.Camera = m_QuadCamera;
    draw.Vertices.assign(vertices, vertices + vertexCount);
    for (uint32_t i = 0; i < 32; ++i)
    {
        draw.Textures[i] = textures[i < textureCount ? i : 0];
        Check(bool(draw.Textures[i]), "Quad run requires captured Metal texture versions");
    }
    for (const auto& vertex : draw.Vertices)
        Check(Finite(&vertex.Position[0], 3) && Finite(&vertex.Color[0], 4) && Finite(&vertex.TexCoord[0], 2) &&
            std::isfinite(vertex.TilingFactor) && std::isfinite(vertex.TexIndex) && vertex.TexIndex >= 0 &&
            vertex.TexIndex < textureCount && std::floor(vertex.TexIndex) == vertex.TexIndex && vertex.Color.a == 1 &&
            vertex.ClipRect == glm::vec4(-1), "Quad run requires finite opaque vertices, valid slots and no clipping");
    m_QuadFrame->Draws.push_back(std::move(draw)); // publish only after all validation/snapshots succeed
}
void MetalRendererAPI::EndQuadScene()
{
    Check(m_QuadFrame && m_QuadScene, "No offscreen quad scene to end");
    m_QuadScene = false;
}
std::shared_ptr<const MetalQuadReadback> MetalRendererAPI::SubmitOffscreenQuadFrame()
{
    Check(!m_Stopped && m_QuadFrame && !m_QuadScene, "End Renderer2D scene before submitting offscreen quad frame");
    try
    {
        auto result = m_Device->SubmitQuadFrame(m_QuadFrame);
        if (result) m_QuadFrame.reset();
        return result;
    }
    catch (...) { m_QuadFrame.reset(); throw; }
}
void MetalRendererAPI::CancelOffscreenQuadFrame()
{
    Check(bool(m_QuadFrame), "No unsubmitted quad frame to cancel");
    m_QuadFrame.reset(); m_QuadScene = false;
}
void MetalRendererAPI::PollOffscreenQuads()
{
    m_Device->Poll();
    const auto proof = m_Device->Observer()->Snapshot();
    if (proof.Errors) throw std::runtime_error(proof.Error);
}
}
