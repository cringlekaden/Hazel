#include "MetalRendererAPI.h"
#include "MetalRenderer2DFrame.h"
#include "MetalDevice.h"
#include "MetalShader.h"
#include "MetalBuffer.h"
#include "MetalComputePipeline.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/Drawing/Renderer3DUniforms.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath>
namespace Hazel {
namespace { [[noreturn]] void Unavailable() { throw std::logic_error("Operation unavailable in the Metal 4 Renderer2D runtime"); } }
MetalRendererAPI::MetalRendererAPI():m_Device(std::make_shared<MetalDevice>()) {
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    m_Caps.Metal=true; m_Caps.Compute=true; m_Caps.TemporalAA=true;
    m_Caps.Features += ", full-resolution rigid-mesh HDR TAA, XeGTAO";
    m_Caps.LimitsSource="Metal 4 family guarantees capped by Hazel binding/attachment contracts; sample count queried on active device";
    m_Caps.ShaderLoading="Offline Metal libraries (required); no runtime developer shader compiler";
    m_Caps.TargetFormats="RGBA32F HDR / R32Sint ID / Depth32F; one sample, <=4096 per axis / <=8M pixels";
    for (uint32_t samples : {1u, 2u, 4u, 8u, 16u})
        if (m_Device->NativeDevice()->supportsTextureSampleCount(samples)) m_Caps.MaxSamples=samples;
    m_Caps.MaxColorSamples=m_Caps.MaxIntegerSamples=m_Caps.MaxDepthSamples=1;
    m_Caps.MaxUniformBindings=8; m_Caps.MaxUniformBlockBytes=16384; m_Caps.MaxVertexAttributes=16; m_Caps.MaxTextureBindings=32; m_Caps.MaxCubeSize=16384;
    m_Caps.MaxTextureSlots=32; m_Caps.MaxTextureSize=16384; m_Caps.MaxColorAttachments=2; m_Caps.MaxDrawBuffers=2; m_Caps.Vendor="Apple"; m_Caps.Device=m_Device->NativeDevice()->name()->utf8String(); m_Caps.Driver=std::string("Metal 4 / ")+NS::ProcessInfo::processInfo()->operatingSystemVersionString()->utf8String();
}
MetalRendererAPI::~MetalRendererAPI() { Shutdown(); }
void MetalRendererAPI::Init(const RendererSettings& settings) {
    m_Resolution=RendererPolicy::Resolve(settings,m_Caps);
}
void MetalRendererAPI::Shutdown() {
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    if(m_Stopped) return;
    m_Renderer2DFrame.reset(); m_Renderer2DScene=false;
    m_Device->Drain();
    m_LocalLightCull.reset();m_LocalMetrics.reset();m_LocalLights.reset();
    m_Shaders.clear(); m_Textures = {}; m_Uniforms = {}; m_BoundShader.clear();
    m_Stopped=true;
}
std::shared_ptr<Shader> MetalRendererAPI::LoadShader(const std::string& name, uint32_t slots) {
    MetalShaderArtifact::ValidateName(name, slots);
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    if(m_Stopped) throw std::logic_error("Cannot load Metal shaders after backend shutdown");
    const auto key=name+"-"+std::to_string(slots);
    auto found=m_Shaders.find(key);
    if(found!=m_Shaders.end()) return found->second;
    auto shader=std::make_shared<MetalShader>(m_Device,name,slots);
    m_Shaders.emplace(key,shader); // Publish only after library/pipeline/reflection validation.
    return shader;
}
void MetalRendererAPI::EnableEditorReadbackForTesting() { m_Device->EnableEditorReadback(); }
std::shared_ptr<MetalEditorCapture> MetalRendererAPI::CaptureEditorFrameForTesting() { return m_Device->CaptureEditorFrame(); }
void MetalRendererAPI::PushState() { m_StateStack.push_back(m_Raster); }
void MetalRendererAPI::PopState() { if(m_StateStack.empty())throw std::logic_error("Metal raster state stack underflow"); m_Raster=m_StateStack.back(); m_StateStack.pop_back(); }
void MetalRendererAPI::SetPickingMask(glm::bvec4 mask) { m_Raster.PickingMask=mask; }
RasterState MetalRendererAPI::GetRasterState() const { return m_Raster; }
void MetalRendererAPI::SetRasterState(const RasterState& state) { m_Raster=state; }
void MetalRendererAPI::BeginTextCompositing() { Unavailable(); }
void MetalRendererAPI::SetTextCompositing(bool) { Unavailable(); }
void MetalRendererAPI::EndTextCompositing() { Unavailable(); }
void MetalRendererAPI::SetViewport(uint32_t x,uint32_t y,uint32_t width,uint32_t height) {
    if (!m_Renderer2DFrame.expired()) throw std::logic_error("Metal framebuffer viewport is fixed by its attachment extent");
    m_Raster.Viewport={x,y,width,height};
}
void MetalRendererAPI::SetClearColor(const glm::vec4& color) {
    if (!m_Renderer2DFrame.expired()) throw std::logic_error("Use Framebuffer::Clear for an explicit Metal offscreen pass");
    m_Device->SetClear(color);
}
void MetalRendererAPI::Clear() {
    if (!m_Renderer2DFrame.expired()) throw std::logic_error("Use Framebuffer::Clear for an explicit Metal offscreen pass");
    // Explicit Metal 4 load-action clear occurs at presentation.
}
void MetalRendererAPI::RetainResource(std::shared_ptr<void> resource)
{
    auto frame = m_Renderer2DFrame.lock();
    if (!frame || !resource) throw std::logic_error("Resource retention requires a bound Metal frame");
    if (std::find(frame->ResourceOwners.begin(), frame->ResourceOwners.end(), resource) == frame->ResourceOwners.end())
        frame->ResourceOwners.push_back(std::move(resource));
}
void MetalRendererAPI::BindUniform(uint32_t binding, const std::shared_ptr<const std::vector<uint8_t>>& bytes)
{
    if (binding >= m_Uniforms.size() || m_Stopped) throw std::logic_error("Invalid Metal uniform binding");
    m_Uniforms[binding] = bytes;
}
void MetalRendererAPI::CaptureLocalLights(const std::shared_ptr<LocalLightingMetrics>& metrics)
{
    auto bytes=m_Uniforms[7].lock();
    if(!bytes || bytes->size()!=sizeof(Renderer3DUniforms::LocalLightData))
        throw std::logic_error("Missing local light uniform block");
    // UniformBuffer::SetData mutates its CPU allocation. Freeze it once per
    // submission, before another view or queued frame can overwrite that data.
    m_LocalLights=std::make_shared<const std::vector<uint8_t>>(*bytes);
    m_LocalMetrics=metrics;
}
void MetalRendererAPI::BindTexture(uint32_t slot, std::shared_ptr<const MetalTextureVersion> texture)
{
    if (slot >= m_Textures.size() || !texture || m_Stopped) throw std::logic_error("Invalid Metal material texture binding");
    m_Textures[slot] = std::move(texture);
}
void MetalRendererAPI::BindShader(const std::string& name)
{
    if (m_Stopped) throw std::logic_error("Cannot bind Metal shader after backend shutdown");
    if (name != "ForwardPBR" && name != "DepthPBR" && name != "MotionPBR" && name != "EnvironmentSky") throw std::logic_error("Metal indexed drawing does not support this shader");
    m_BoundShader = name;
}
void MetalRendererAPI::DrawIndexed(const Ref<VertexArray>& array, uint32_t count, uint32_t first)
{
    auto frame = m_Renderer2DFrame.lock();
    if (m_Stopped || !frame || m_Renderer2DScene || m_BoundShader.empty() || !array)
        throw std::logic_error("Metal indexed draw requires a bound framebuffer and production shader");
    auto indices = std::dynamic_pointer_cast<MetalIndexBuffer>(array->GetIndexBuffer());
    if (!indices || first > indices->GetCount()) throw std::invalid_argument("Invalid native index range");
    if (!count) count = indices->GetCount() - first;
    if (!count || count > indices->GetCount() - first) throw std::invalid_argument("Native index range exceeds geometry");
    MetalRenderer2DDraw draw;
    draw.IndexedMesh = true; draw.Count = count; draw.FirstIndex = first;
    draw.MeshIndices = indices->Snapshot(); draw.Raster = m_Raster;
    for (size_t i = 0; i < m_Uniforms.size(); ++i)
        if (i!=7) if (auto bytes = m_Uniforms[i].lock()) draw.Uniforms[i] = *bytes;
    for (size_t i = 0; i < m_Textures.size(); ++i) draw.Textures[i] = m_Textures[i];
    if (m_BoundShader=="ForwardPBR") {
        auto bytes=m_LocalLights;
        if (!bytes || bytes->size()!=sizeof(Renderer3DUniforms::LocalLightData))
            throw std::logic_error("Missing local light uniform block");
        for (auto &pass:frame->LocalLights) if(pass->Lights==bytes && pass->Viewport==draw.Raster.Viewport) draw.LocalLights=pass;
        if (!draw.LocalLights) {
            auto pass=std::make_shared<MetalLocalLightPass>();pass->Lights=bytes;pass->Camera=draw.Uniforms[2];pass->Viewport=draw.Raster.Viewport;pass->Timing=m_LocalMetrics;
            float count;std::memcpy(&count,bytes->data(),sizeof(count));pass->Count=unsigned(count);
            float fullLoop;std::memcpy(&fullLoop,bytes->data()+8,sizeof(fullLoop));
            pass->Tiled=(pass->Count>8 || fullLoop<0) && fullLoop<.5f;
            float tileSize;std::memcpy(&tileSize,bytes->data()+12,sizeof(tileSize));pass->TileSize=unsigned(tileSize);
            if (pass->Tiled) {
                if (!m_LocalLightCull) m_LocalLightCull=std::make_shared<MetalComputePipeline>(m_Device,
                    Resources::Resolve("shaders/metal/LocalLightCull.metallib"),"cullLocalLights");
                pass->Pipeline=m_LocalLightCull;
            }
            if(bytes->size()>256ULL*1024*1024-frame->PacketBytes)
                throw std::runtime_error("Metal local light packet exceeds 256 MiB");
            draw.LocalLights=pass;frame->LocalLights.push_back(std::move(pass));
            frame->PacketBytes+=bytes->size();
        }
    }
    if(m_BoundShader=="DepthPBR") {
        for(size_t i=0;i<draw.Uniforms.size();++i)if(i<2 || i>4)draw.Uniforms[i].clear();
        for(size_t i=1;i<draw.Textures.size();++i)draw.Textures[i].reset();
    }
    if(m_BoundShader=="MotionPBR") {
        for(size_t i=0;i<draw.Uniforms.size();++i)if(i<2 || i>5)draw.Uniforms[i].clear();
        for(size_t i=1;i<draw.Textures.size();++i)draw.Textures[i].reset();
    }
    if ((m_BoundShader == "ForwardPBR" || m_BoundShader == "DepthPBR" || m_BoundShader == "MotionPBR")) {
        if (array->GetVertexBuffers().size() != 1) throw std::invalid_argument("PBR requires one native vertex stream");
        auto vertices = std::dynamic_pointer_cast<MetalVertexBuffer>(array->GetVertexBuffers()[0]);
        if (!vertices || vertices->GetLayout().GetStride() != 64) throw std::invalid_argument("Invalid PBR vertex layout");
        draw.MeshVertices = vertices->Snapshot();
    }
    uint32_t state = draw.Raster.Blend ? 1 : 0;
    for (int i = 0; i < 4; ++i) {
        if (draw.Raster.ColorMask[i]) state |= 1u << (i + 1);
        if (draw.Raster.PickingMask[i]) state |= 1u << (i + 5);
    }
    const auto key = m_BoundShader + "/raster/" + std::to_string(state);
    {
        std::lock_guard<std::mutex> lock(m_ShaderMutex);
        auto found = m_Shaders.find(key);
        if (found == m_Shaders.end())
            found = m_Shaders.emplace(key, std::make_shared<MetalShader>(m_Device, m_BoundShader, 32, false, &draw.Raster)).first;
        draw.Shader = std::static_pointer_cast<MetalShader>(found->second);
    }
    if (frame->Draws.size() >= 65536) throw std::runtime_error("Metal frame exceeds 65536 draws");
    uint64_t bytes = sizeof(draw);
    for (const auto& uniform : draw.Uniforms) bytes += uniform.size();
    if (bytes > 256ULL * 1024 * 1024 - frame->PacketBytes) throw std::runtime_error("Metal frame packet exceeds 256 MiB");
    frame->Draws.push_back(std::move(draw));
    frame->PacketBytes += bytes;
}
void MetalRendererAPI::DrawLines(const Ref<VertexArray>&,uint32_t,uint32_t) { Unavailable(); }
void MetalRendererAPI::SetLineWidth(float width) {
    if(!std::isfinite(width)||width<=0)throw std::invalid_argument("Line width must be finite and positive");
    m_Raster.LineWidth=std::clamp(width,m_Caps.MinLineWidth,m_Caps.MaxLineWidth);
}
std::shared_ptr<MetalProofObserver> MetalRendererAPI::Observe() const { return m_Device->Observer(); }
void MetalRendererAPI::EnableTransferProof() { m_Device->EnableTransferProof(); }
void MetalRendererAPI::SkipNextDrawableForTesting() { m_Device->SkipNextDrawable(); }
void MetalRendererAPI::OverrideNextExtentForTesting(uint32_t w,uint32_t h) { m_Device->OverrideNextExtent(w,h); }
std::function<void()> MetalRendererAPI::GateQueueForTesting() { return m_Device->GateQueue(); }
void MetalRendererAPI::StartCapture(const std::filesystem::path& path) { m_Device->StartCapture(path); }
void MetalRendererAPI::StopCapture() { m_Device->StopCapture(); }
void MetalRendererAPI::FailNextStartupForTesting(MetalStartupFailure point) { MetalDevice::FailNextStartup(point); }
}
