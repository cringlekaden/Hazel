#include "MetalRendererAPI.h"
#include "MetalRenderer2DFrame.h"
#include "MetalFramebuffer.h"
#include "MetalDevice.h"
#include "MetalShader.h"
#include "Hazel/Renderer/Drawing/Renderer2DVertices.h"
#include <cstring>
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
void MetalRendererAPI::BindFramebuffer(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    Check(!m_Stopped && m_Renderer2DFrame.expired(), "Unbind the current Metal framebuffer before binding another");
    m_Renderer2DFrame = frame; m_Renderer2DScene = false;
    m_Raster.Viewport={0,0,frame->Width,frame->Height};
}
bool MetalRendererAPI::IsFramebufferBound(const std::shared_ptr<MetalRenderer2DFrame>& frame) const
{
    return frame && m_Renderer2DFrame.lock() == frame;
}
void MetalRendererAPI::UnbindFramebuffer(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    Check(IsFramebufferBound(frame) && !m_Renderer2DScene, "End Renderer2D scene before unbinding/cancelling its framebuffer");
    m_Renderer2DFrame.reset();
}
void MetalRendererAPI::CheckFramebufferClear(const std::shared_ptr<MetalRenderer2DFrame>& frame) const
{
    Check(IsFramebufferBound(frame) && !m_Renderer2DScene && frame->Draws.empty(), "Clear requires a bound framebuffer before scene submission");
}
void MetalRendererAPI::BeginRenderer2DScene(const glm::mat4& camera)
{
    Check(!m_Stopped && !m_Renderer2DFrame.expired() && !m_Renderer2DScene, "Bind a qualified framebuffer before Renderer2D::BeginScene");
    Check(Finite(&camera[0][0], 16), "Quad camera must be finite");
    m_Renderer2DCamera = camera; m_Renderer2DScene = true;
}
void MetalRendererAPI::EnqueueRenderer2DBatch(const std::vector<MetalRenderer2DRun>& runs,
    const std::array<std::shared_ptr<const MetalTextureVersion>, 32>& textures, uint32_t textureCount)
{
    std::vector<MetalRenderer2DDraw> captured;
    captured.reserve(runs.size());
    for (const auto& run : runs)
    {
        const auto& shader=run.Shader;
        const void* vertices=run.Vertices;
        const uint32_t first=run.First, count=run.Count;
        const bool picking=run.Picking;
        const float lineWidth=run.LineWidth;
        const auto& font=run.Font;
        auto frame = m_Renderer2DFrame.lock();
        Check(!m_Stopped && frame && m_Renderer2DScene && vertices && shader, "Renderer2D run requires an active scene and stream");
        auto native = std::dynamic_pointer_cast<MetalShader>(shader);
        Check(bool(native), "Renderer2D requires generated Metal shader artifacts");
        const auto& name = native->Artifact().Name;
        const bool lines = name == "Renderer2D_Line";
        Check(lines || name == "Renderer2D_Quad" || name == "Renderer2D_Circle" || name == "Renderer2D_Text", "Invalid Renderer2D shader");
        Check(count && first <= 120000 && count <= 120000 - first && first % (lines ? 2 : 6) == 0 && count % (lines ? 2 : 6) == 0,
            "Invalid Renderer2D run range");
        Check(textureCount && textureCount <= 32, "Invalid Renderer2D texture slots");
        MetalRenderer2DDraw draw;
        draw.Raster = m_Raster;
        if (!picking) draw.Raster.PickingMask = glm::bvec4(false);
        // Pipeline identity includes only immutable attachment state, not dynamic depth/viewport.
        uint32_t state = draw.Raster.Blend ? 1 : 0;
        for (int i = 0; i < 4; ++i) {
            if (draw.Raster.ColorMask[i]) state |= 1u << (i + 1);
            if (draw.Raster.PickingMask[i]) state |= 1u << (i + 5);
        }
        const auto key = name + "/raster/" + std::to_string(state);
        {
            std::lock_guard<std::mutex> lock(m_ShaderMutex);
            auto found = m_Shaders.find(key);
            if (found == m_Shaders.end())
                found = m_Shaders.emplace(key, std::make_shared<MetalShader>(m_Device, name, 32, false, &draw.Raster)).first;
            draw.Shader = std::static_pointer_cast<MetalShader>(found->second);
        }
        draw.Count = count; draw.Camera = m_Renderer2DCamera; draw.Lines = lines;
        const size_t vertexFirst = lines ? first : first / 6 * 4;
        const size_t vertexCount = lines ? count : count / 6 * 4;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        draw.Vertices.assign(bytes + vertexFirst * native->VertexStride(), bytes + (vertexFirst + vertexCount) * native->VertexStride());
        const size_t idOffset = (name == "Renderer2D_Quad" ? offsetof(QuadVertex,EntityID) : name == "Renderer2D_Circle" ? offsetof(CircleVertex,EntityID) :
            lines ? offsetof(LineVertex,EntityID) : offsetof(TextVertex,EntityID)) / sizeof(float);
        for (size_t i = 0; i < vertexCount; ++i) {
            const auto* vertex = draw.Vertices.data() + i * native->VertexStride();
            for (size_t j = 0; j < native->VertexStride() / 4; ++j) if (j != idOffset) {
                float value; std::memcpy(&value, vertex + j * 4, 4);
                Check(std::isfinite(value), "Renderer2D requires finite vertices");
            }
            if (name == "Renderer2D_Quad") {
                float slot; std::memcpy(&slot, vertex + offsetof(QuadVertex,TexIndex), 4);
                Check(slot >= 0 && slot < textureCount && std::floor(slot) == slot, "Invalid quad texture index");
            }
        }
        Check(std::isfinite(lineWidth) && lineWidth > 0, "Invalid Renderer2D line width");
        // Native Metal lines and the maintained Apple GL 4.1 context both
        // expose a one-pixel maximum. Honor that capability without repacking.
        for (uint32_t i = 0; i < 32; ++i)
        {
            draw.Textures[i] = textures[i < textureCount ? i : 0];
            Check(bool(draw.Textures[i]), "Renderer2D requires captured Metal texture versions");
        }
        if (font) {
            auto atlas = std::dynamic_pointer_cast<MetalTexture2D>(font);
            Check(bool(atlas), "Text requires a Metal font atlas");
            draw.Textures[0] = atlas->Snapshot();
        }
        captured.push_back(std::move(draw));
    }
    auto frame=m_Renderer2DFrame.lock();
    uint64_t bytes=0;
    for (const auto& draw : frame->Draws) bytes+=draw.Vertices.size();
    for (const auto& draw : captured) bytes+=draw.Vertices.size();
    Check(bytes <= 256ULL * 1024 * 1024, "Metal Renderer2D CPU packet exceeds 256 MiB");
    frame->Draws.reserve(frame->Draws.size()+captured.size());
    for (auto& draw : captured) {
        frame->Draws.push_back(std::move(draw));
        RenderCommand::RecordDrawSubmission();
    }
}
void MetalRendererAPI::EndRenderer2DScene()
{
    Check(m_Renderer2DScene, "No offscreen quad scene to end");
    m_Renderer2DScene = false;
}
}
