#include "MetalFramebuffer.h"
#include "MetalRendererAPI.h"
#include "MetalDevice.h"
#include "MetalRenderer2DFrame.h"
#include "MetalShader.h"
#include "MetalComputePipeline.h"
#include "MetalAmbientOcclusion.h"
#include "Hazel/Renderer/Scene/Bloom.h"
#include "MetalExposureMeter.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Hazel {
namespace {
void Check(bool value, const char* message) { if (!value) throw std::logic_error(message); }
[[noreturn]] void Unsupported() { throw std::logic_error("Operation unavailable on the Metal Renderer2D framebuffer profile"); }
}
void MetalFramebuffer::Validate(const FramebufferSpecification& specification)
{
    const auto& attachments = specification.Attachments.Attachments;
    const bool defocusLayers = specification.Scratch == FramebufferSpecification::CameraScratch::Defocus &&
        attachments.size() == 2 && attachments[0].TextureFormat == FramebufferTextureFormat::RGBA32F &&
        attachments[1].TextureFormat == FramebufferTextureFormat::RGBA32F;
    const bool depthOnly=attachments.size()==1 && attachments[0].TextureFormat==FramebufferTextureFormat::DEPTH32F;
    const bool reflectionSurface=attachments.size()==6 &&
        attachments[3].TextureFormat==FramebufferTextureFormat::RGBA16F &&
        attachments[4].TextureFormat==FramebufferTextureFormat::RGBA16F &&
        attachments[5].TextureFormat==FramebufferTextureFormat::RGBA32F;
    Check(specification.Width && specification.Height && specification.Width <= 4096 && specification.Height <= 4096 && uint64_t(specification.Width) * specification.Height <= (depthOnly?16777216u:8388608u) &&
        specification.Samples == 1 && !specification.SwapChainTarget && (defocusLayers || (attachments.size() == 1 && (attachments[0].TextureFormat == FramebufferTextureFormat::RGBA8 || attachments[0].TextureFormat == FramebufferTextureFormat::RGBA32F || attachments[0].TextureFormat == FramebufferTextureFormat::RGBA16F || attachments[0].TextureFormat == FramebufferTextureFormat::DEPTH32F)) || ((attachments.size() == 3 || reflectionSurface) &&
        attachments[0].TextureFormat == FramebufferTextureFormat::RGBA32F &&
        attachments[1].TextureFormat == FramebufferTextureFormat::RED_INTEGER &&
        attachments[2].TextureFormat == FramebufferTextureFormat::DEPTH32F)),
        "Metal framebuffer requires 1..4096 extent / at most 8M color or 16M depth-only pixels, one sample, ordered RGBA32F/R32Sint/Depth32F, optional reflection surface data, depth-only preparation, two RGBA32F defocus layers, RGBA16F bloom or RGBA8 output and no swapchain target");
}
MetalFramebuffer::MetalFramebuffer(std::shared_ptr<MetalDevice> device, const FramebufferSpecification& specification)
    : m_Device(std::move(device)), m_Specification(specification)
{
    Validate(specification);
    Check(bool(m_Device), "Metal framebuffer requires its renderer device");
    m_Attachments = Allocate(specification.Width, specification.Height);
}
std::shared_ptr<const MetalFramebufferAttachments> MetalFramebuffer::Allocate(uint32_t width, uint32_t height, bool temporalHistory) const
{
    Check(!m_Device->m_Stopped, "Cannot allocate framebuffer attachments after Metal device shutdown");
    m_Device->CheckMemoryGrowth("framebuffer");
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto generation = std::make_shared<MetalFramebufferAttachments>();
    generation->Width = width; generation->Height = height;
    auto descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D); descriptor->setWidth(width); descriptor->setHeight(height);
    descriptor->setStorageMode(MTL::StorageModePrivate); descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    const MTL::PixelFormat formats[] = {MTL::PixelFormatRGBA32Float, MTL::PixelFormatR32Sint, MTL::PixelFormatDepth32Float,
        MTL::PixelFormatRGBA16Float, MTL::PixelFormatRGBA16Float, MTL::PixelFormatRGBA32Float};
    const bool depthOnly=m_Specification.Attachments.Attachments.size()==1 && m_Specification.Attachments.Attachments[0].TextureFormat==FramebufferTextureFormat::DEPTH32F;
    const bool singleFloat = m_Specification.Attachments.Attachments.size() == 1 &&
        m_Specification.Attachments.Attachments[0].TextureFormat == FramebufferTextureFormat::RGBA32F;
    const bool defocusLayers = m_Specification.Attachments.Attachments.size() == 2;
    if (m_Specification.Scratch == FramebufferSpecification::CameraScratch::Exposure) generation->Role = "exposure_meter";
    else if (m_Specification.Scratch == FramebufferSpecification::CameraScratch::Defocus)
        generation->Role = defocusLayers ? "defocus_layers" : "defocus_scratch";
    else if (m_Specification.Scratch == FramebufferSpecification::CameraScratch::MotionBlur)
        generation->Role = "shutter_scratch";
    else if (temporalHistory) generation->Role = "taa_history";
    else if (depthOnly) generation->Role = "depth_prepass";
    else if (singleFloat) generation->Role = "float_scratch";
    else if (m_Specification.Attachments.Attachments.size() == 6) generation->Role = "ssr_surface";
    else if (m_Specification.Attachments.Attachments.size() == 1)
        generation->Role = m_Specification.Attachments.Attachments[0].TextureFormat == FramebufferTextureFormat::RGBA16F ? "bloom" : "encoded_output";
    for (size_t i = depthOnly?2u:0u; i < (depthOnly?3u:temporalHistory?2u:m_Specification.Attachments.Attachments.size()); ++i)
    {
        descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead |
            ((i == 0 && m_Specification.Attachments.Attachments.size() == 3) || temporalHistory ||
             (singleFloat && m_Specification.Scratch != FramebufferSpecification::CameraScratch::Defocus) ? MTL::TextureUsageShaderWrite : 0));
        auto format = formats[i];
        if (defocusLayers) format = MTL::PixelFormatRGBA32Float;
        if (!depthOnly && !singleFloat && m_Specification.Attachments.Attachments.size() == 1)
            format = m_Specification.Attachments.Attachments[0].TextureFormat == FramebufferTextureFormat::RGBA16F
                ? MTL::PixelFormatRGBA16Float : MTL::PixelFormatBGRA8Unorm;
        descriptor->setPixelFormat(format);
        generation->Targets[i] = NS::TransferPtr(m_Device->NativeDevice()->newTexture(descriptor.get()));
        if (!generation->Targets[i])
            throw MetalMemoryAdmissionError("Metal could not allocate framebuffer attachments at " +
                std::to_string(width) + "×" + std::to_string(height));
    }
    auto& live = m_Device->m_AttachmentGenerations;
    live.erase(std::remove_if(live.begin(), live.end(), [](const auto& weak) { return weak.expired(); }), live.end());
    live.push_back(generation);
    m_Device->Account(nullptr, true);
    { std::lock_guard<std::mutex> lock(m_Device->m_Observer->Mutex); ++m_Device->m_Observer->Value.AttachmentAllocations; }
    return generation;
}
MetalRendererAPI& MetalFramebuffer::Backend() const
{
    auto* backend = dynamic_cast<MetalRendererAPI*>(&RenderCommand::Backend());
    Check(backend && backend->Device() == m_Device, "Framebuffer belongs to a different or released Metal backend");
    return *backend;
}
void MetalFramebuffer::PrepareAmbientOcclusion(const Ref<Framebuffer>& depth, const SceneCameraView& camera, const AmbientOcclusionSettings& settings, unsigned debugView, const std::shared_ptr<AmbientOcclusionTiming>& timing, unsigned temporalPhase)
{
    auto source=std::dynamic_pointer_cast<MetalFramebuffer>(depth);
    Check(source && source.get()!=this && source->m_Device==m_Device && source->m_Frame && m_Frame &&
        !Backend().IsFramebufferBound(source->m_Frame) && Backend().IsFramebufferBound(m_Frame) &&
        source->m_Frame->Width==m_Frame->Width && source->m_Frame->Height==m_Frame->Height &&
        source->m_Attachments->Targets[2] && m_Attachments->Targets[1],
        "XeGTAO requires an unbound depth producer and bound equal-size lighting target");
    auto constants=AmbientOcclusionConstants(camera,settings,m_Frame->Width,m_Frame->Height,temporalPhase);
    constants.Quality.w=temporalPhase?288u*((temporalPhase-1)%64):0u;
    if(!source->m_AOPipelines)source->m_AOPipelines=std::make_shared<MetalAOPipelines>(*m_Device);
    auto& scratch = source->m_AOResources;
    if (!scratch || scratch->Width != m_Frame->Width || scratch->Height != m_Frame->Height) {
        m_Device->CheckMemoryGrowth("ambient occlusion scratch");
        auto resources=std::make_shared<MetalAOResources>(*m_Device,source->m_AOPipelines,m_Frame->Width,m_Frame->Height);
        m_Device->m_AOGenerations.push_back(resources);
        m_Device->Account(nullptr, true);
        scratch = std::move(resources);
    }
    auto pass=std::make_shared<MetalAOPass>();pass->DebugView=debugView;pass->Timing=timing;pass->Resources=scratch;pass->Constants=constants;
    for (const auto& candidate : source->m_AOCounters)
        if (candidate.use_count() == 1) { pass->CounterOwner = candidate; break; }
    if (!pass->CounterOwner) {
        Check(source->m_AOCounters.size() < 4, "XeGTAO counter heaps exceed three slots and a CPU packet");
        pass->CounterOwner = std::make_shared<MetalAOCounters>(*m_Device);
        source->m_AOCounters.push_back(pass->CounterOwner);
    }
    m_Frame->AO=std::move(pass);m_Frame->AOSource=std::move(source->m_Frame);
    // The private depth and compute producers order writes after earlier readers.
    source->m_DepthProducer = true;
    m_Frame->AOSource->QueueOrderedProducer = true;
    m_Frame->AOSource->Readback=false;source->m_SubmittedGeneration=true;
}
void MetalFramebuffer::Bind()
{
    Check(!m_Frame, "Submit or cancel the prior framebuffer packet before binding again");
    auto& backend = Backend();
    Check(!backend.m_Stopped, "Cannot bind framebuffer after Metal backend shutdown");
    m_Device->Poll();
    if (m_SubmittedGeneration && (m_TemporalProducer || m_WorldProducer || m_DepthProducer)) {
        m_SubmittedGeneration=false;
        std::lock_guard<std::mutex> lock(m_Device->m_Observer->Mutex);++m_Device->m_Observer->Value.AttachmentReuses;
    }
    if (m_SubmittedGeneration) {
        m_Available.push_back(std::move(m_Attachments));
        m_SubmittedGeneration = false;
    }
    std::shared_ptr<const MetalFramebufferAttachments> generation = m_Attachments;
    if (!generation) {
        auto free = std::find_if(m_Available.begin(), m_Available.end(), [](const auto& value) { return value.use_count() == 1; });
        if (free != m_Available.end()) {
            generation = std::move(*free); m_Available.erase(free);
            std::lock_guard<std::mutex> lock(m_Device->m_Observer->Mutex); ++m_Device->m_Observer->Value.AttachmentReuses;
        }
        else {
            Check(m_Available.size() < 4, "All bounded Metal framebuffer generations are still in use");
            generation = Allocate(m_Specification.Width, m_Specification.Height);
        }
    }
    if (m_Device->TrimIdleMemory())
        m_Available.erase(std::remove_if(m_Available.begin(), m_Available.end(),
            [](const auto& candidate) { return candidate.use_count() == 1; }), m_Available.end());
    auto frame = std::make_shared<MetalRenderer2DFrame>();
    frame->Width = generation->Width; frame->Height = generation->Height;
    frame->Attachments = generation;
    frame->Readback = bool(generation->Targets[0]);
    frame->QueueOrderedProducer=m_TemporalProducer || m_WorldProducer || m_DepthProducer;
    backend.BindFramebuffer(frame);
    m_Frame = std::move(frame); m_Attachments = std::move(generation); m_SubmittedGeneration = false;
    m_Readback.reset(); // this pass must not expose an older generation as current pixels
}
void MetalFramebuffer::Unbind() { Backend().UnbindFramebuffer(m_Frame); }
void MetalFramebuffer::PrepareShadowDepth(const Ref<Framebuffer>& depth, ShadowMapKind kind)
{
    auto source=std::dynamic_pointer_cast<MetalFramebuffer>(depth);
    Check(bool(m_Frame), "Shadows require a bound lighting view");
    auto& producer = kind == ShadowMapKind::Directional ? m_Frame->ShadowSource :
        kind == ShadowMapKind::Spot ? m_Frame->SpotShadowSource : m_Frame->PointShadowSource;
    Check(source && source.get()!=this && source->m_Device==m_Device && source->m_Frame &&
        !Backend().IsFramebufferBound(source->m_Frame) && Backend().IsFramebufferBound(m_Frame) &&
        source->m_Specification.Attachments.Attachments.size()==1 && source->m_Attachments->Targets[2] &&
        !source->m_Attachments->Targets[0] && !producer,
        "Shadows require an unbound depth-only producer and bound lighting view");
    producer = std::move(source->m_Frame);
    producer->Attachments->Role = kind == ShadowMapKind::Directional ? "directional_shadow" :
        kind == ShadowMapKind::Spot ? "spot_shadow" : "point_shadow";
    // A private atlas has no direct readback/editor consumers. Each captured
    // parent encodes its own depth producer immediately before sampling it on
    // the same queue. Reuse storage with the existing all-stage queue barrier;
    // packets, uniforms and resized generations still retain their own lifetime.
    source->m_DepthProducer = true;
    producer->QueueOrderedProducer = true;
    producer->Readback = false;
    source->m_SubmittedGeneration = true;
}
void MetalFramebuffer::Resume()
{
    Check(m_Frame && !m_Frame->Result && !Backend().IsFramebufferBound(m_Frame),
        "Resume requires an unbound, unsubmitted framebuffer packet");
    Backend().BindFramebuffer(m_Frame);
}
void MetalFramebuffer::Resume(glm::vec4 clearColor)
{
    Check(m_Frame && m_Frame->Draws.empty(), "Inspection clear requires an empty pending framebuffer packet");
    for(int i=0;i<4;++i)Check(std::isfinite(clearColor[i]), "Framebuffer clear color must be finite");
    Resume();
    m_Frame->Clear=clearColor;
}
void MetalFramebuffer::Clear(glm::vec4 color)
{
    Backend().CheckFramebufferClear(m_Frame);
    for (int i = 0; i < 4; ++i) Check(std::isfinite(color[i]), "Framebuffer clear color must be finite");
    m_Frame->Clear = color; m_Frame->ClearEntity = -1; // all attachments clear on pass begin; depth is one
}
void MetalFramebuffer::ClearAttachment(uint32_t attachment, int value)
{
    Check(attachment == 1 && m_Attachments->Targets[1], "Only an existing signed entity attachment one supports integer clear");
    Backend().CheckFramebufferClear(m_Frame);
    m_Frame->ClearEntity = value;
}
Ref<const FramebufferReadback> MetalFramebuffer::Submit()
{
    Check(!m_TemporalProducer && !m_WorldProducer,"Private scratch must be consumed by its resolve or world composite");
    Check(m_Frame && !Backend().IsFramebufferBound(m_Frame), "Unbind framebuffer after EndScene before submitting");
    Check(bool(m_Attachments->Targets[0]), "Depth-only preparation targets must be consumed by a lighting view");
    // Busy leaves this exact CPU packet/generation available for retry or cancellation.
    try
    {
        auto result = m_Device->SubmitRenderer2DFrame(m_Frame);
        if (result) { m_Readback = result; m_Frame.reset(); m_SubmittedGeneration = true; }
        return result;
    }
    catch (...) { m_Frame.reset(); throw; }
}
void MetalFramebuffer::Poll()
{
    m_Device->Poll();
    for (const auto& pending : m_EditorPendingReadbacks) if (pending->Completed && pending->Error.empty() &&
        (!m_EditorReadback || pending->Serial > m_EditorReadback->Serial)) m_EditorReadback = pending;
    m_EditorPendingReadbacks.erase(std::remove_if(m_EditorPendingReadbacks.begin(), m_EditorPendingReadbacks.end(), [](const auto& pending) { return pending->Completed; }), m_EditorPendingReadbacks.end());
    const auto proof = m_Device->Observer()->Snapshot();
    if (proof.Errors) throw std::runtime_error(proof.Error);
}
void MetalFramebuffer::Cancel()
{
    if (m_Frame && Backend().IsFramebufferBound(m_Frame)) Backend().UnbindFramebuffer(m_Frame);
    m_Frame.reset(); m_Readback.reset(); // never abort or release a submitted generation
}
void MetalFramebuffer::Resize(uint32_t width, uint32_t height)
{
    Check(!m_Frame, "Submit or cancel the framebuffer packet before resize");
    auto specification = m_Specification;
    specification.Width = width; specification.Height = height;
    Validate(specification);
    if (width == m_Specification.Width && height == m_Specification.Height) return;
    // Retired caches are disposable; keep the current target intact if allocation fails.
    m_Available.erase(std::remove_if(m_Available.begin(),m_Available.end(),[](const auto& value){return value.use_count()==1;}),m_Available.end());
    auto generation = Allocate(width, height); // publish only on complete allocation success
    m_TemporalPrevious.reset();m_TemporalColors.clear();
    m_Available.clear();
    m_Attachments = std::move(generation); m_Specification = specification;
    m_SubmittedGeneration = false; m_Readback.reset(); ResetEditorReadback();
}
size_t MetalFramebuffer::ReadIndex(int x, int y)
{
    Poll();
    Check(m_Readback && m_Readback->Completed && m_Readback->Error.empty(), "Framebuffer readback is unavailable or pending; poll without a GPU wait");
    Check(x >= 0 && y >= 0 && uint32_t(x) < m_Readback->Width && uint32_t(y) < m_Readback->Height, "Framebuffer readback pixel is out of bounds");
    return size_t(y) * m_Readback->Width + uint32_t(x);
}
glm::vec4 MetalFramebuffer::ReadColorPixel(uint32_t attachment, int x, int y)
{
    Check(attachment == 0, "Color readback requires RGBA32F attachment zero");
    const auto index = ReadIndex(x, y); return m_Readback->Color[index];
}
int MetalFramebuffer::ReadPixel(uint32_t attachment, int x, int y)
{
    Check(attachment == 1, "Integer readback requires signed attachment one");
    const auto index = ReadIndex(x, y); return m_Readback->Entity[index];
}
float MetalFramebuffer::ReadDepthPixel(int x, int y) { const auto index = ReadIndex(x, y); return m_Readback->Depth[index]; }
void MetalFramebuffer::EncodeOutput(const Ref<Framebuffer>& destination)
{
    Check(!m_TemporalProducer && !m_WorldProducer,"Private scratch cannot be presented directly");
    Check(m_Specification.Attachments.Attachments.size()==3,"Display encoding requires a color/depth/ID scene target");
    if (destination) {
        Poll();
        auto target = std::dynamic_pointer_cast<MetalFramebuffer>(destination);
        Check(target && target.get() != this && target->m_Device == m_Device && target->m_Specification.Attachments.Attachments.size() == 1 && target->m_Specification.Attachments.Attachments[0].TextureFormat==FramebufferTextureFormat::RGBA8,
            "Editor output requires a separate encoded color target on the same device");
        Check(m_Frame && !Backend().IsFramebufferBound(m_Frame), "Unbind editor scene before display encoding");
        Check(target->m_Specification.Width == m_Frame->Width && target->m_Specification.Height == m_Frame->Height, "Editor output extent mismatch");
        target->Bind(); target->Unbind();
        if (!m_Device->m_OutputShader) m_Device->m_OutputShader = std::make_shared<MetalShader>(m_Device, "DisplayOutput", 32, true);
        target->m_Frame->OutputShader = m_Device->m_OutputShader;
        target->m_Frame->DisplaySource = std::move(m_Frame);
        target->m_Frame->Readback = false;
        auto& source = target->m_Frame->DisplaySource;
        source->Result = std::make_shared<FramebufferReadback>();
        source->Result->Width = source->Width; source->Result->Height = source->Height;
        source->EditorPicking = true;
        source->CompletedEntity.resize(size_t(source->Width) * source->Height);
        m_EditorPendingReadbacks.push_back(source->Result);
        m_Device->m_EditorFrames.push_back(target->m_Frame);
        target->m_Frame.reset(); target->m_SubmittedGeneration = true;
        m_SubmittedGeneration = true;
        return;
    }
    Check(m_Frame && !Backend().IsFramebufferBound(m_Frame), "Unbind the completed scene before encoding output");
    Check(!m_Device->m_PresentFrame, "Only one window output is allowed per frame");
    if (!m_Device->m_OutputShader)
        m_Device->m_OutputShader = std::make_shared<MetalShader>(m_Device, "DisplayOutput", 32, true);
    m_Frame->OutputShader = m_Device->m_OutputShader;
    m_Frame->Readback = false;
    m_Device->m_PresentFrame = std::move(m_Frame);
    m_SubmittedGeneration = true;
}
void MetalFramebuffer::DrawEntityOutlineTo(const Ref<Framebuffer>& destination, int entity, int radius, glm::vec4 color) {
    auto target = std::dynamic_pointer_cast<MetalFramebuffer>(destination);
    Check(target && target->m_Device == m_Device && radius >= 1 && radius <= 8 && !m_Device->m_EditorFrames.empty(), "Invalid editor outline destination/radius");
    auto frame = m_Device->m_EditorFrames.back();
    Check(frame->Attachments == target->m_Attachments && frame->DisplaySource && frame->DisplaySource->Attachments == m_Attachments, "Outline requires current display source");
    if (!m_Device->m_OutlineShader) m_Device->m_OutlineShader = std::make_shared<MetalShader>(m_Device, "EntityOutline", 32, true);
    frame->OutlineShader = m_Device->m_OutlineShader;
    frame->OutlineEntity = entity; frame->OutlineRadius = radius; frame->OutlineColor = color;
}
EditorTexture MetalFramebuffer::GetEditorTexture() const {
    Check(m_Specification.Attachments.Attachments.size() == 1 && m_Specification.Attachments.Attachments[0].TextureFormat==FramebufferTextureFormat::RGBA8, "Only encoded editor output is an ImGui image");
    return {reinterpret_cast<uintptr_t>(m_Attachments->Targets[0].get()), m_Attachments};
}
int MetalFramebuffer::ReadEditorEntity(int x, int y) {
    Poll();
    const auto& readback = m_EditorReadback;
    if (!readback || !readback->Completed || !readback->Error.empty() || readback->Width != m_Specification.Width || readback->Height != m_Specification.Height || x < 0 || y < 0 || uint32_t(x) >= readback->Width || uint32_t(y) >= readback->Height) return -1;
    return readback->Entity[size_t(y) * readback->Width + uint32_t(x)];
}
void MetalFramebuffer::DispatchColorTo(const Ref<Framebuffer>& destination, const Ref<ComputePipeline>& pipeline, const std::vector<uint8_t>& parameters) {
    auto target = std::dynamic_pointer_cast<MetalFramebuffer>(destination);
    auto compute = std::dynamic_pointer_cast<MetalComputePipeline>(pipeline);
    Check(target && target.get() != this && target->m_Device == m_Device && compute && compute->Device() == m_Device && parameters.size() == compute->ParameterBytes(),
        "Color compute requires distinct same-device targets/pipeline and 1..256 immutable parameter bytes");
    Check(m_Specification.Attachments.Attachments.size()==3 && m_Frame && !m_Frame->ComputeSource && !m_Frame->DisplaySource && !Backend().IsFramebufferBound(m_Frame) && !target->m_Frame && target->m_Specification.Attachments.Attachments.size() == 3 &&
        m_Frame->Width == target->m_Specification.Width && m_Frame->Height == target->m_Specification.Height,
        "Color compute requires an unbound producer and empty equal-size FP32 destination");
    target->Bind(); target->Unbind();
    target->m_Frame->ComputeSource = std::move(m_Frame);
    target->m_Frame->ComputeSource->Readback = false;
    target->m_Frame->Compute = std::move(compute);
    target->m_Frame->ComputeParameters = parameters;
    m_SubmittedGeneration = true;
}
void MetalFramebuffer::SetWorldProducer()
{
    Check(!m_Frame && !m_TemporalProducer && m_Attachments->Targets[1],
        "Private world scratch requires an empty HDR framebuffer");
    if(!m_WorldProducer)Check(!m_SubmittedGeneration && m_Available.empty(),"World scratch must be configured before first use");
    m_WorldProducer=true;
    if (m_Specification.Attachments.Attachments.size() == 3)
        m_Attachments->Role = "world_radiance";
}
void MetalFramebuffer::ResolveReflectionsTo(const Ref<Framebuffer>& destination,
                                          const ScreenReflectionResolve& parameters)
{
    auto target=std::dynamic_pointer_cast<MetalFramebuffer>(destination);
    Check(target && target.get()!=this && target->m_Device==m_Device && m_WorldProducer &&
        m_Attachments->Targets[3] && target->m_Specification.Attachments.Attachments.size()==3 &&
        m_Frame && !m_Frame->World && !Backend().IsFramebufferBound(m_Frame) && target->m_Frame &&
        Backend().IsFramebufferBound(target->m_Frame) && target->m_Frame->Draws.empty() &&
        !target->m_Frame->World && m_Frame->Width==target->m_Frame->Width &&
        m_Frame->Height==target->m_Frame->Height,
        "SSR requires a private unbound surface producer and a bound empty HDR destination");
    target->m_Frame->PresentationShader=std::static_pointer_cast<MetalShader>(Backend().LoadShader("ScreenReflections"));
    target->m_Frame->Reflections=parameters;
    m_Frame->Readback=false;
    target->m_Frame->World=std::move(m_Frame);
    m_SubmittedGeneration=true;
}
void MetalFramebuffer::CompositeWorldTo(const Ref<Framebuffer>& destination, float exposure,
                                        const BloomPyramid* bloom, const ExposureMeterPass* meter,
                                        const DepthOfFieldPass* defocus, const CameraMotionPass* shutter)
{
    auto target = std::dynamic_pointer_cast<MetalFramebuffer>(destination);
    Check(target && target.get() != this && target->m_Device == m_Device && std::isfinite(exposure) && exposure >= -16 && exposure <= 16, "Invalid Metal world destination/exposure");
    Check(m_Specification.Attachments.Attachments.size()==3 && target->m_Specification.Attachments.Attachments.size()==3 && m_Frame && !Backend().IsFramebufferBound(m_Frame) && target->m_Frame &&
        Backend().IsFramebufferBound(target->m_Frame) && target->m_Frame->Draws.empty() && !target->m_Frame->World &&
        m_Frame->Width == target->m_Frame->Width && m_Frame->Height == target->m_Frame->Height,
        "World conversion requires an unbound world and a bound empty destination of equal extent");
    target->m_Frame->PresentationShader = std::static_pointer_cast<MetalShader>(Backend().LoadShader("WorldPresentation"));
    if (shutter) {
        target->m_Frame->ShutterSubmitted = shutter->Submitted;
        if (shutter->Valid) {
            if (!m_Frame->MotionSource) {
                auto& vectors = static_cast<MetalFramebuffer&>(*shutter->Vectors);
                Check(vectors.m_Frame && !Backend().IsFramebufferBound(vectors.m_Frame),
                    "Shutter requires an unbound vector producer");
                m_Frame->MotionSource = std::move(vectors.m_Frame);
                m_Frame->MotionSource->Readback = false;
                vectors.m_SubmittedGeneration = true;
            }
            MetalShutter pass;
            pass.TileMax = static_cast<const MetalFramebuffer&>(*shutter->TileMax).m_Attachments;
            pass.NeighborMax = static_cast<const MetalFramebuffer&>(*shutter->NeighborMax).m_Attachments;
            pass.Filtered = static_cast<const MetalFramebuffer&>(*shutter->Filtered).m_Attachments;
            pass.Shader = std::static_pointer_cast<MetalShader>(Backend().LoadShader("ShutterFilter"));
            pass.Parameters = shutter->Parameters;
            pass.Timing = shutter->Timing;
            target->m_Frame->Shutter = std::move(pass);
        }
    }
    if (defocus) {
        MetalDefocus pass;
        pass.Tiles = static_cast<const MetalFramebuffer&>(*defocus->Tiles).m_Attachments;
        pass.DilatedTiles = static_cast<const MetalFramebuffer&>(*defocus->DilatedTiles).m_Attachments;
        pass.Prefiltered = static_cast<const MetalFramebuffer&>(*defocus->Prefiltered).m_Attachments;
        pass.Layers = static_cast<const MetalFramebuffer&>(*defocus->Gathered).m_Attachments;
        pass.Parameters = defocus->Parameters;
        pass.Timing = defocus->Timing;
        pass.TileShader = std::static_pointer_cast<MetalShader>(Backend().LoadShader("DefocusTile"));
        pass.PrefilterShader = std::static_pointer_cast<MetalShader>(Backend().LoadShader("DefocusPrefilter"));
        pass.GatherShader = std::static_pointer_cast<MetalShader>(Backend().LoadShader("DefocusGather"));
        target->m_Frame->Defocus = std::move(pass);
    }
    if (meter) {
        auto pass = std::make_shared<MetalExposureMeter>();
        pass->Histogram = static_cast<const MetalFramebuffer&>(*meter->Histogram).m_Attachments;
        pass->Previous = static_cast<const MetalFramebuffer&>(*meter->Previous).m_Attachments;
        pass->Current = static_cast<const MetalFramebuffer&>(*meter->Current).m_Attachments;
        pass->Parameters = meter->Parameters;
        pass->Submitted = meter->Submitted;
        pass->Timing = meter->Timing;
        target->m_Frame->ExposureMeter = std::move(pass);
    }
    if (bloom) {
        auto& packet = *target->m_Frame;
        packet.BloomShader = std::static_pointer_cast<MetalShader>(Backend().LoadShader("BloomFilter"));
        packet.BloomIntensity = bloom->Intensity;
        packet.BloomMetrics = bloom->Timing;
        auto source = m_Frame->Attachments;
        auto append = [&](const Ref<Framebuffer>& output,
                          const std::shared_ptr<const MetalFramebufferAttachments>& fine, float mode) {
            const auto& targetAttachments = static_cast<const MetalFramebuffer&>(*output).m_Attachments;
            packet.BloomPasses.push_back({source, fine, targetAttachments,
                bloom->FilterParameters(mode, source->Width, source->Height), {}});
            source = targetAttachments;
        };
        for (const auto& level : bloom->Levels)
            append(level.Down, source, source == m_Frame->Attachments ? 0.f : 1.f);
        for (size_t i = bloom->Levels.size() - 1; i > 0; --i) {
            const auto& fine = bloom->Levels[i - 1];
            append(fine.Up, static_cast<const MetalFramebuffer&>(*fine.Down).m_Attachments, 2.f);
        }
    }
    m_Frame->Readback = false;
    target->m_Frame->Exposure = exposure;
    target->m_Frame->World = std::move(m_Frame);
    m_SubmittedGeneration = true;
}
uint32_t MetalFramebuffer::GetColorAttachmentRendererID(uint32_t) const { Unsupported(); }
}
