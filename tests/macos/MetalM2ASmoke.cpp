#include "Hazel/Renderer/Drawing/Renderer2DQuad.h"
#include "MetalM2A.h"
#include "Hazel/Core/Log.h"
#include "Platform/macOS/Metal/MetalDevice.h"
#include "Platform/macOS/Metal/MetalShader.h"
#include "Hazel/Core/Resources.h"
#include <spirv_cross/spirv_cross.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <map>

namespace {
using MetalM2A::Check;
std::string Error(NS::Error* error) { return error ? error->localizedDescription()->utf8String() : "no compiler diagnostic"; }
struct Pipeline
{
    std::shared_ptr<Hazel::MetalShader> Shader;
    size_t Stride = 0;
};
Pipeline Compile(const std::shared_ptr<Hazel::MetalDevice>& device, const std::filesystem::path& directory, const std::string& variant)
{
    const auto name = variant.substr(0, variant.find('-'));
    const uint32_t slots = variant == "Renderer2D_Quad-16" ? 16 : 32;
    auto shader = std::make_shared<Hazel::MetalShader>(device, name, slots);
    Hazel::WriteMetalShaderFile(directory / (variant + ".native-reflection.txt"), shader->NativeReflection());
    std::cout << "PASS native_pipeline " << variant << " stride=" << shader->VertexStride() << '\n';
    return {shader, shader->VertexStride()};
}
void DrawProof(Hazel::MetalDevice& owner, const Pipeline& pipeline, const std::filesystem::path& directory, bool perspective)
{
    constexpr size_t side = 32, colorRow = 512, scalarRow = 256;
    constexpr size_t idBase = colorRow * side, depthBase = idBase + scalarRow * side;
    auto* device = owner.NativeDevice();
    auto* queue = owner.Queue();
    NS::Error* error = nullptr;
    auto textureDescriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    textureDescriptor->setTextureType(MTL::TextureType2D);
    textureDescriptor->setWidth(side); textureDescriptor->setHeight(side);
    textureDescriptor->setStorageMode(MTL::StorageModePrivate);
    textureDescriptor->setUsage(MTL::TextureUsageRenderTarget);
    std::array<NS::SharedPtr<MTL::Texture>, 3> targets;
    const MTL::PixelFormat formats[] = {MTL::PixelFormatRGBA32Float, MTL::PixelFormatR32Sint, MTL::PixelFormatDepth32Float};
    for (size_t i = 0; i < targets.size(); ++i)
    {
        textureDescriptor->setPixelFormat(formats[i]);
        targets[i] = NS::TransferPtr(device->newTexture(textureDescriptor.get()));
        Check(bool(targets[i]), "Offscreen attachment unavailable");
    }
    const glm::mat4 camera = perspective ? glm::perspective(glm::radians(90.f), 1.f, 1.f, 3.f) : glm::mat4(1.f);
    std::vector<Hazel::QuadVertex> vertices;
    const auto triangle = [&](float distance, glm::vec4 color, int32_t entity)
    {
        const float scale = perspective ? -distance : 1.f;
        for (const auto& p : {glm::vec2(-.75f, -.75f), glm::vec2(.75f, -.75f), glm::vec2(-.75f, .75f)})
            vertices.push_back({{p.x * scale, p.y * scale, distance}, color, {.5f,.5f}, 31, 1, entity, {0,0,-1,-1}});
    };
    triangle(perspective ? -1.5f : -.5f, {2.f, .25f, .5f, 1.f}, -123456789);
    triangle(perspective ? -2.5f : .5f, {0, 1, 0, 1}, 987654321); // Must fail depth after the first.
    triangle(perspective ? -.5f : -2.f, {0, 0, 1, 1}, 42);       // Must be clipped before the near plane.
    auto stream = NS::TransferPtr(device->newBuffer(vertices.data(),
        vertices.size() * sizeof(Hazel::QuadVertex), MTL::ResourceStorageModeShared));
    auto uniform = NS::TransferPtr(device->newBuffer(&camera, sizeof(camera), MTL::ResourceStorageModeShared));
    auto readback = NS::TransferPtr(device->newBuffer(depthBase + scalarRow * side, MTL::ResourceStorageModeShared));
    Check(bool(stream) && bool(uniform) && bool(readback), "Proof buffers unavailable");
    Check(pipeline.Stride == sizeof(Hazel::QuadVertex), "Reflected quad stream stride differs from CPU ABI");
    // MSL tier-2 uses opaque 64-bit resource handles. Native reflection checks
    // offsets 0/256 and 32 entries; execution through slot 31 validates the ABI.
    std::array<NS::SharedPtr<MTL::Texture>, 32> sampled;
    NS::SharedPtr<MTL::SamplerState> sampler;
    NS::SharedPtr<MTL::Buffer> descriptors;
    {
        std::array<MTL::ResourceID, 64> handles{};
        auto samplerDescriptor = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
        samplerDescriptor->setSupportArgumentBuffers(true);
        sampler = NS::TransferPtr(device->newSamplerState(samplerDescriptor.get()));
        Check(bool(sampler), "Tier2 sampler unavailable");
        auto image = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
        image->setTextureType(MTL::TextureType2D); image->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
        image->setWidth(1); image->setHeight(1); image->setStorageMode(MTL::StorageModeShared);
        image->setUsage(MTL::TextureUsageShaderRead);
        for (size_t i = 0; i < sampled.size(); ++i)
        {
            sampled[i] = NS::TransferPtr(device->newTexture(image.get()));
            Check(bool(sampled[i]), "Tier2 texture unavailable");
            const uint32_t pixel = i == 31 ? 0xffffffffu : 0xff000000u;
            sampled[i]->replaceRegion(MTL::Region(0, 0, 1, 1), 0, &pixel, 4);
            handles[i] = sampled[i]->gpuResourceID();
            handles[32 + i] = sampler->gpuResourceID();
        }
        descriptors = NS::TransferPtr(device->newBuffer(handles.data(), sizeof(handles), MTL::ResourceStorageModeShared));
        Check(bool(descriptors), "Tier2 descriptor buffer unavailable");
    }
    auto residencyDescriptor = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
    residencyDescriptor->setInitialCapacity(6);
    auto residency = NS::TransferPtr(device->newResidencySet(residencyDescriptor.get(), &error));
    Check(bool(residency), "Proof residency unavailable: " + Error(error));
    for (auto& target : targets) residency->addAllocation(target.get());
    for (auto* buffer : {stream.get(), uniform.get(), readback.get()}) residency->addAllocation(buffer);
    {
        for (auto& texture : sampled) residency->addAllocation(texture.get());
        residency->addAllocation(descriptors.get());
    }
    residency->commit();
    // Command-local residency, not a persistent addition to M1's presentation queue.
    auto argumentsDescriptor = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    argumentsDescriptor->setMaxBufferBindCount(17);
    argumentsDescriptor->setInitializeBindings(true);
    auto arguments = NS::TransferPtr(device->newArgumentTable(argumentsDescriptor.get(), &error));
    auto fragmentArguments = NS::TransferPtr(device->newArgumentTable(argumentsDescriptor.get(), &error));
    auto allocator = NS::TransferPtr(device->newCommandAllocator());
    auto commands = NS::TransferPtr(device->newCommandBuffer());
    auto completed = NS::TransferPtr(device->newSharedEvent());
    Check(bool(arguments) && bool(fragmentArguments) && bool(allocator) && bool(commands) && bool(completed), "Proof command infrastructure unavailable");
    arguments->setAddress(uniform->gpuAddress(), 0);
    arguments->setAddress(stream->gpuAddress(), 16);
    fragmentArguments->setAddress(descriptors->gpuAddress(), 0);
    auto depthDescriptor = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthDescriptor->setDepthWriteEnabled(true);
    auto depthState = NS::TransferPtr(device->newDepthStencilState(depthDescriptor.get()));
    Check(bool(depthState), "Proof depth state unavailable");
    commands->beginCommandBuffer(allocator.get());
    const MTL::ResidencySet* sets[] = {residency.get()};
    commands->useResidencySets(sets, 1);
    auto pass = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
    for (size_t i = 0; i < 2; ++i)
    {
        auto* attachment = pass->colorAttachments()->object(i);
        attachment->setTexture(targets[i].get()); attachment->setLoadAction(MTL::LoadActionClear);
        attachment->setStoreAction(MTL::StoreActionStore);
        attachment->setClearColor(i == 0 ? MTL::ClearColor{0, 0, 0, 0} : MTL::ClearColor{-1, 0, 0, 0});
    }
    pass->depthAttachment()->setTexture(targets[2].get());
    pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
    pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
    pass->depthAttachment()->setClearDepth(1);
    auto* render = commands->renderCommandEncoder(pass.get());
    Check(render != nullptr, "Proof render encoder unavailable");
    render->setRenderPipelineState(pipeline.Shader->Pipeline());
    render->setArgumentTable(arguments.get(), MTL::RenderStageVertex);
    render->setArgumentTable(fragmentArguments.get(), MTL::RenderStageFragment);
    render->setViewport({0, 0, side, side, 0, 1});
    render->setDepthStencilState(depthState.get());
    render->setCullMode(MTL::CullModeBack);
    render->setFrontFacingWinding(MTL::WindingClockwise);
    render->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, vertices.size());
    render->endEncoding();
    auto* copy = commands->computeCommandEncoder();
    Check(copy != nullptr, "Proof copy encoder unavailable");
    copy->barrierAfterQueueStages(MTL::StageAll, MTL::StageBlit, MTL4::VisibilityOptionDevice);
    copy->copyFromTexture(targets[0].get(), 0, 0, {0, 0, 0}, {side, side, 1}, readback.get(), 0, colorRow, 0);
    copy->copyFromTexture(targets[1].get(), 0, 0, {0, 0, 0}, {side, side, 1}, readback.get(), idBase, scalarRow, 0);
    copy->copyFromTexture(targets[2].get(), 0, 0, {0, 0, 0}, {side, side, 1}, readback.get(), depthBase, scalarRow, 0);
    copy->endEncoding(); commands->endCommandBuffer();
    struct Feedback { std::atomic<bool> Done{false}; std::string Error; };
    auto feedback = std::make_shared<Feedback>();
    auto options = NS::TransferPtr(MTL4::CommitOptions::alloc()->init());
    options->addFeedbackHandler(MTL4::CommitFeedbackHandlerFunction([feedback](MTL4::CommitFeedback* value) {
        if (value->error()) feedback->Error = Error(value->error());
        feedback->Done.store(true);
    }));
    const MTL4::CommandBuffer* buffers[] = {commands.get()};
    queue->commit(buffers, 1, options.get());
    queue->signalEvent(completed.get(), 1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (completed->signaledValue() < 1 || !feedback->Done.load())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            std::cerr << "Metal proof timeout; refusing to free in-flight resources\n";
            std::abort();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(feedback->Error.empty(), "GPU commit feedback: " + feedback->Error);
    // No read, reset, table mutation or resource release until the queue event completes.
    const auto* bytes = static_cast<const unsigned char*>(readback->contents());
    std::ofstream raw(directory / (perspective ? "quad32-perspective.readback.bin" : "quad32.readback.bin"), std::ios::binary);
    raw.write(reinterpret_cast<const char*>(bytes), depthBase + scalarRow * side);
    Check(bool(raw), "Cannot retain deterministic readback");
    size_t interiors = 0, clear = 0;
    const float expectedDepth = perspective ? .5f : .25f;
    for (size_t y = 0; y < side; ++y) for (size_t x = 0; x < side; ++x)
    {
        const auto* color = reinterpret_cast<const float*>(bytes + y * colorRow + x * 16);
        const auto id = *reinterpret_cast<const int32_t*>(bytes + idBase + y * scalarRow + x * 4);
        const auto depth = *reinterpret_cast<const float*>(bytes + depthBase + y * scalarRow + x * 4);
        // Stay one pixel inside/outside the analytically projected AA-off triangle.
        if (x >= 5 && y >= 5 && x + y <= 29)
        {
            ++interiors;
            Check(id == -123456789, "Signed integer ID/depth/orientation/winding mismatch at " + std::to_string(x) + "," + std::to_string(y) + " got=" + std::to_string(id));
            Check(color[0] == 2 && color[1] == .25f && color[2] == .5f && color[3] == 1, "FP32 HDR color differs");
            Check(std::abs(depth - expectedDepth) < 1e-6f, "OpenGL-to-Metal clip depth differs");
        }
        else if (x < 3 || y < 3 || x + y > 33)
        {
            ++clear;
            Check(id == -1 && depth == 1 && color[0] == 0 && color[3] == 0, "Clear/near clipping/orientation mismatch");
        }
    }
    std::cout << "PASS native_draw shader=" << "Quad-32-slot31" << " projection=" << (perspective ? "perspective" : "identity-ortho")
        << " interior=" << interiors << " clear=" << clear << " id=-123456789 clear_id=-1 depth=" << expectedDepth
        << " HDR=2,0.25,0.5,1 completion=1 feedback_errors=0\n";
}
}
int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "Usage: MetalM2ASmoke --translate|--native artifact-directory");
        if (std::string(argv[1]) == "--translate") return MetalM2A::Translate(argv[2]);
        Check(std::string(argv[1]) == "--native", "Unknown mode");
        auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
        Hazel::Log::Init();
        auto owner = std::make_shared<Hazel::MetalDevice>();
        const auto artifacts = std::filesystem::absolute(argv[2]);
        Hazel::Resources::Configure({artifacts / "Resources", artifacts, artifacts / "user-data"});
        for (const auto& name : {"DisplayOutput", "EntityOutline", "EnvironmentSky", "ForwardPBR", "DepthPBR", "Renderer2D_Circle", "Renderer2D_Line", "Renderer2D_Quad-16", "Renderer2D_Quad-32", "Renderer2D_Text", "WorldPresentation"})
            Compile(owner, argv[2], name);
        // The former line shader was a generic triangle passthrough proof.
        // Preserve its depth/clip assertions with the authoritative quad stream;
        // analytical lines are qualified through production Renderer2D in M3.
        const auto quad = Compile(owner, argv[2], "Renderer2D_Quad-32");
        DrawProof(*owner, quad, argv[2], false);
        DrawProof(*owner, quad, argv[2], true);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL M2-A: " << error.what() << '\n';
        return 1;
    }
}
