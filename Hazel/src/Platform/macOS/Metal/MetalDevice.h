#pragma once
#include "MetalProof.h"
#include "Hazel/Renderer/Resources/EditorTexture.h"
#include <glm/glm.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <vector>
#include <string>

namespace Hazel {
struct RasterState;
struct MetalRenderer2DFrame;
class MetalShader;
struct FramebufferReadback;
struct MetalFramebufferAttachments;
struct MetalAOResources;
struct EditorTexture;
// All native execution and resource ownership stays here. Metal 4 command buffers
// retain neither referenced resources nor their allocator: slots own both explicitly.
class MetalDevice final {
public:
    static std::string AvailabilityReason();
    MetalDevice();
    ~MetalDevice();
    MTL::Device* NativeDevice() const { return m_Device.get(); }
    MTL4::CommandQueue* Queue() const { return m_Queue.get(); }
    std::shared_ptr<MetalProofObserver> Observer() const { return m_Observer; }
    void Present(CA::MetalLayer* layer, uint32_t width, uint32_t height, uint32_t logicalWidth, uint32_t logicalHeight, double scale, bool visible);
    void EnableEditorReadback();
    std::shared_ptr<MetalEditorCapture> CaptureEditorFrame();
    void InitEditor();
    void ShutdownEditor();
    void QueueEditorTextures(std::vector<EditorTexture> textures);
    void Drain();
    void SetClear(const glm::vec4& color);
    void EnableTransferProof();
    void SkipNextDrawable() { m_SkipDrawable=true; }
    void OverrideNextExtent(uint32_t w,uint32_t h) { m_Override=true; m_Width=w; m_Height=h; }
    std::function<void()> GateQueue();
    void StartCapture(const std::filesystem::path& path);
    void StopCapture();
    static void FailNextStartup(MetalStartupFailure point);
    static void CheckStartupFailure(MetalStartupFailure point);
private:
    friend class MetalRendererAPI;
    friend class MetalFramebuffer;
    void PresentEditor(CA::MetalLayer* layer, bool visible);
    std::shared_ptr<MetalEditorCapture> m_EditorCapture;
    bool m_EditorReadbackEnabled = false;
    bool m_Editor = false, m_EditorQueued = false;
    uint64_t m_EditorTimestampFrequency = 0;
    std::vector<std::shared_ptr<MetalRenderer2DFrame>> m_EditorFrames;
    std::vector<EditorTexture> m_EditorTextures;
    void Poll();
    NS::SharedPtr<MTL::Buffer> AcquireRenderer2DBuffer(size_t bytes, const void* data = nullptr);
    NS::SharedPtr<MTL::DepthStencilState> GetDepthState(const RasterState& raster);
    std::array<NS::SharedPtr<MTL::DepthStencilState>, 6> m_DepthStates;
    void PrepareTemporal(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    void EncodeTemporal(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    void PrepareAmbientOcclusion(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    void EncodeAmbientOcclusion(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    void PrepareRenderer2DFrame(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    void EncodeRenderer2DFrame(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    std::shared_ptr<MetalRenderer2DFrame> m_PresentFrame;
    std::shared_ptr<MetalShader> m_OutputShader, m_OutlineShader;
    std::vector<std::weak_ptr<const MetalFramebufferAttachments>> m_AttachmentGenerations;
    std::shared_ptr<const FramebufferReadback> SubmitRenderer2DFrame(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    struct Receipt { bool GPU=false, Presented=false; };
    struct Frame {
        NS::SharedPtr<MTL4::CommandAllocator> Allocator;
        NS::SharedPtr<MTL4::ArgumentTable> Arguments;
        NS::SharedPtr<MTL::Buffer> Upload, Private, Readback;
        NS::SharedPtr<MTL::Texture> Texture;
        std::shared_ptr<Receipt> Completion;
        uint64_t Serial=0;
        bool Pending=false;
        std::shared_ptr<MetalRenderer2DFrame> Quad;
        std::vector<std::shared_ptr<MetalRenderer2DFrame>> EditorFrames;
        std::vector<EditorTexture> EditorTextures;
        std::shared_ptr<MetalEditorCapture> EditorCapture;
        std::vector<NS::SharedPtr<MTL::Buffer>> EditorCaptureBuffers;
        std::shared_ptr<std::atomic<bool>> EditorFeedback;
        NS::SharedPtr<MTL4::CounterHeap> EditorCounters;
        std::vector<NS::SharedPtr<CA::MetalDrawable>> EditorDrawables;
        std::vector<NS::SharedPtr<CA::MetalLayer>> EditorLayers;
        NS::SharedPtr<MTL::ResidencySet> EditorResidency;
        std::vector<NS::SharedPtr<MTL::Buffer>> Renderer2DBuffers;
        size_t Renderer2DCursor = 0;
    };
    void Verify(Frame& frame);
    void EncodeTransfers(Frame& frame, uint64_t serial);
    std::vector<std::weak_ptr<MetalAOResources>> m_AOGenerations;
    void Account(const MetalRenderer2DFrame* preparing = nullptr);
    NS::SharedPtr<MTL::Device> m_Device;
    NS::SharedPtr<MTL4::CommandQueue> m_Queue;
    NS::SharedPtr<MTL4::CommandBuffer> m_Commands;
    NS::SharedPtr<MTL::SharedEvent> m_Completed, m_Gate;
    NS::SharedPtr<MTL::ResidencySet> m_Residency;
    std::array<Frame,3> m_Frames;
    std::shared_ptr<MetalProofObserver> m_Observer=std::make_shared<MetalProofObserver>();
    uint64_t m_Serial=0;
    size_t m_Next=0;
    MTL::ClearColor m_Clear{0.08,0.18,0.3,1};
    bool m_Trace=false;
    bool m_Transfers=false, m_Stopped=false, m_Capturing=false, m_SkipDrawable=false, m_Override=false;
    uint32_t m_Width=0,m_Height=0;
};
}
