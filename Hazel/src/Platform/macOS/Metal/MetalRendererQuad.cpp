#include "MetalRendererAPI.h"
#include "MetalQuadFrame.h"
#include "MetalFramebuffer.h"
#include "MetalDevice.h"
#include "MetalShader.h"
#include "Hazel/Renderer/RenderCommand.h"
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
std::shared_ptr<Framebuffer> MetalRendererAPI::CreateFramebuffer(const FramebufferSpecification& specification)
{
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    Check(!m_Stopped, "Cannot create framebuffers after Metal backend shutdown");
    return std::make_shared<MetalFramebuffer>(m_Device, specification);
}
std::shared_ptr<Texture2D> MetalRendererAPI::CreateTexture(const TextureSpecification& specification, const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    Check(!m_Stopped, "Cannot create textures after Metal backend shutdown");
    return std::make_shared<MetalTexture2D>(m_Device, specification, path);
}
void MetalRendererAPI::BindFramebuffer(const std::shared_ptr<MetalQuadFrame>& frame)
{
    Check(!m_Stopped && m_QuadFrame.expired(), "Unbind the current Metal framebuffer before binding another");
    m_QuadFrame = frame; m_QuadScene = false;
}
bool MetalRendererAPI::IsFramebufferBound(const std::shared_ptr<MetalQuadFrame>& frame) const
{
    return frame && m_QuadFrame.lock() == frame;
}
void MetalRendererAPI::UnbindFramebuffer(const std::shared_ptr<MetalQuadFrame>& frame)
{
    Check(IsFramebufferBound(frame) && !m_QuadScene, "End Renderer2D scene before unbinding/cancelling its framebuffer");
    m_QuadFrame.reset();
}
void MetalRendererAPI::CheckFramebufferClear(const std::shared_ptr<MetalQuadFrame>& frame) const
{
    Check(IsFramebufferBound(frame) && !m_QuadScene && frame->Draws.empty(), "Clear requires a bound framebuffer before scene submission");
}
void MetalRendererAPI::BeginQuadScene(const glm::mat4& camera)
{
    Check(!m_Stopped && !m_QuadFrame.expired() && !m_QuadScene, "Bind a qualified framebuffer before Renderer2D::BeginScene");
    Check(!m_QuadFrame.lock()->World, "World conversion destination cannot accept additional quad scenes");
    Check(Finite(&camera[0][0], 16), "Quad camera must be finite");
    m_QuadCamera = camera; m_QuadScene = true;
}
void MetalRendererAPI::EnqueueQuadRun(const QuadVertex* vertices, uint32_t vertexCount, uint32_t first, uint32_t count,
    const std::array<std::shared_ptr<const MetalTextureVersion>, 32>& textures, uint32_t textureCount)
{
    auto frame = m_QuadFrame.lock();
    Check(!m_Stopped && frame && m_QuadScene, "Quad run requires an active Renderer2D scene/offscreen frame");
    uint32_t used = 0;
    for (const auto& draw : frame->Draws) used += draw.Count / 6;
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
    frame->Draws.push_back(std::move(draw)); // publish only after all validation/snapshots succeed
    RenderCommand::RecordDrawSubmission();
}
void MetalRendererAPI::EndQuadScene()
{
    Check(m_QuadScene, "No offscreen quad scene to end");
    m_QuadScene = false;
}
}
