#pragma once
#include "MetalProof.h"
#include <glm/glm.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <array>
#include <filesystem>
#include <functional>

namespace Hazel {
struct MetalQuadFrame;
struct FramebufferReadback;
// All native execution and resource ownership stays here. Metal 4 command buffers
// retain neither referenced resources nor their allocator: slots own both explicitly.
class MetalDevice final {
public:
    MetalDevice();
    ~MetalDevice();
    MTL::Device* NativeDevice() const { return m_Device.get(); }
    MTL4::CommandQueue* Queue() const { return m_Queue.get(); }
    std::shared_ptr<MetalProofObserver> Observer() const { return m_Observer; }
    void Present(CA::MetalLayer* layer, uint32_t width, uint32_t height, uint32_t logicalWidth, uint32_t logicalHeight, double scale, bool visible);
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
    void Poll();
    std::shared_ptr<const FramebufferReadback> SubmitQuadFrame(const std::shared_ptr<MetalQuadFrame>& frame);
    struct Receipt { bool GPU=false, Presented=false; };
    struct Frame {
        NS::SharedPtr<MTL4::CommandAllocator> Allocator;
        NS::SharedPtr<MTL4::ArgumentTable> Arguments;
        NS::SharedPtr<MTL::Buffer> Upload, Private, Readback;
        NS::SharedPtr<MTL::Texture> Texture;
        std::shared_ptr<Receipt> Completion;
        uint64_t Serial=0;
        bool Pending=false;
        std::shared_ptr<MetalQuadFrame> Quad;
    };
    void Verify(Frame& frame);
    void EncodeTransfers(Frame& frame, uint64_t serial);
    void Account();
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
    bool m_Transfers=false, m_Stopped=false, m_Capturing=false, m_SkipDrawable=false, m_Override=false;
    uint32_t m_Width=0,m_Height=0;
};
}
