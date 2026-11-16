#include "MetalM2A.h"
#include "Hazel/Core/Log.h"
#include "Platform/macOS/Metal/MetalDevice.h"
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
NS::String* Label(const std::string& text) { return NS::String::string(text.c_str(), NS::UTF8StringEncoding); }
std::string Error(NS::Error* error) { return error ? error->localizedDescription()->utf8String() : "no compiler diagnostic"; }
struct Pipeline
{
    NS::SharedPtr<MTL::RenderPipelineState> State;
    NS::SharedPtr<MTL::VertexDescriptor> Vertices;
    size_t Stride = 0;
};
void ValidateReflection(MTL::RenderPipelineState* pipeline, const std::filesystem::path& directory, const std::string& name)
{
    auto* reflection = pipeline->reflection();
    Check(reflection != nullptr, "Native pipeline reflection missing");
    std::ofstream output(directory / (name + ".native-reflection.txt"));
    for (const std::string stage : {"vertex", "fragment"})
    {
        const auto* bindings = stage == "vertex" ? reflection->vertexBindings() : reflection->fragmentBindings();
        Check(bindings != nullptr, "Native stage bindings missing");
        spirv_cross::Compiler source(MetalM2A::Words(directory / (name + "." + stage + ".spv")));
        std::map<uint32_t, MTL::BufferBinding*> buffers;
        for (size_t i = 0; i < bindings->count(); ++i)
        {
            auto* binding = bindings->object<MTL::Binding>(i);
            output << stage << " name=" << binding->name()->utf8String() << " type=" << binding->type()
                << " index=" << binding->index() << " used=" << binding->isUsed();
            if (binding->type() == MTL::BindingTypeBuffer)
            {
                auto* buffer = static_cast<MTL::BufferBinding*>(binding);
                buffers.emplace(static_cast<uint32_t>(binding->index()), buffer);
                output << " bytes=" << buffer->bufferDataSize() << " alignment=" << buffer->bufferAlignment();
                if (auto* type = buffer->bufferStructType())
                    for (size_t j = 0; j < type->members()->count(); ++j)
                    {
                        auto* member = type->members()->object<MTL::StructMember>(j);
                        output << "\n  member=" << member->name()->utf8String() << " offset=" << member->offset();
                        if (auto* array = member->arrayType())
                            output << " count=" << array->arrayLength() << " stride=" << array->stride()
                                << " argument_id=" << member->argumentIndex() << " argument_stride=" << array->argumentIndexStride();
                    }
            }
            if (binding->type() == MTL::BindingTypeTexture)
            {
                auto* texture = static_cast<MTL::TextureBinding*>(binding);
                output << " count=" << texture->arrayLength() << " dimension=" << texture->textureType()
                    << " sample_type=" << texture->textureDataType();
            }
            output << '\n';
        }
        for (const auto& resource : source.get_shader_resources().uniform_buffers)
        {
            const auto index = source.get_decoration(resource.id, spv::DecorationBinding);
            Check(buffers.count(index) != 0, "Native uniform buffer binding missing");
            auto* native = buffers.at(index);
            const auto& type = source.get_type(resource.base_type_id);
            Check(native->bufferDataSize() == source.get_declared_struct_size(type), "Native/SPIR-V block size differs");
            auto* members = native->bufferStructType();
            Check(members != nullptr, "Native block member reflection missing");
            for (uint32_t j = 0; j < type.member_types.size(); ++j)
            {
                auto* member = members->memberByName(Label(source.get_member_name(resource.base_type_id, j)));
                Check(member && member->offset() == source.type_struct_member_offset(type, j), "Native/SPIR-V member offset differs");
            }
        }
        for (const auto& resource : source.get_shader_resources().sampled_images)
        {
            if (name == "Renderer2D_Quad-32" && stage == "fragment") continue;
            const auto index = source.get_decoration(resource.id, spv::DecorationBinding);
            const auto& type = source.get_type(resource.type_id);
            bool found = false, samplerFound = false;
            for (size_t i = 0; i < bindings->count(); ++i)
            {
                auto* binding = bindings->object<MTL::Binding>(i);
                if (binding->index() != index) continue;
                if (binding->type() == MTL::BindingTypeSampler) samplerFound = true;
                if (binding->type() != MTL::BindingTypeTexture) continue;
                auto* texture = static_cast<MTL::TextureBinding*>(binding);
                const auto count = type.array.empty() ? 1 : type.array[0];
                Check(texture->arrayLength() == count, "Native texture array extent differs");
                Check(texture->textureType() == (type.image.dim == spv::DimCube ? MTL::TextureTypeCube : MTL::TextureType2D), "Native texture dimension differs");
                Check(texture->textureDataType() == (source.get_type(type.image.type).basetype == spirv_cross::SPIRType::Int ? MTL::DataTypeInt : MTL::DataTypeFloat), "Native texture scalar type differs");
                found = true;
            }
            Check(found, "Native texture binding missing");
            // Integer texelFetch needs no sampler. Float sampled images use a paired sampler.
            if (source.get_type(type.image.type).basetype == spirv_cross::SPIRType::Float)
                Check(samplerFound, "Native sampler binding missing");
        }
        if (name == "Renderer2D_Quad-32" && stage == "fragment")
        {
            Check(buffers.count(0) != 0, "Tier2 argument buffer missing");
            auto* buffer = buffers.at(0);
            Check(buffer->bufferDataSize() == 512 && buffer->bufferAlignment() == 8, "Tier2 CPU descriptor-buffer ABI differs");
            auto* type = buffer->bufferStructType();
            Check(type != nullptr && type->members()->count() == 2, "Tier2 descriptor members differ");
            for (size_t i = 0; i < 2; ++i)
            {
                auto* member = type->members()->object<MTL::StructMember>(i);
                auto* array = member->arrayType();
                Check(array && array->arrayLength() == 32 && array->argumentIndexStride() == 1 &&
                    member->offset() == i * 256 && member->argumentIndex() == i * 32, "Tier2 texture/sampler array layout differs");
            }
        }
    }
    Check(bool(output), "Cannot write native reflection");
}
Pipeline Compile(MTL::Device* device, MTL4::Compiler* compiler, const std::filesystem::path& directory, const std::string& name)
{
    NS::Error* error = nullptr;
    auto vertex = NS::TransferPtr(device->newLibrary(Label((directory / (name + ".vertex.metallib")).string()), &error));
    Check(bool(vertex), "Vertex library " + name + ": " + Error(error));
    auto fragment = NS::TransferPtr(device->newLibrary(Label((directory / (name + ".fragment.metallib")).string()), &error));
    Check(bool(fragment), "Fragment library " + name + ": " + Error(error));
    auto vs = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
    vs->setLibrary(vertex.get()); vs->setName(Label("main0"));
    auto fs = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
    fs->setLibrary(fragment.get()); fs->setName(Label("main0"));
    auto descriptor = NS::TransferPtr(MTL4::RenderPipelineDescriptor::alloc()->init());
    descriptor->setLabel(Label("M2-A " + name));
    descriptor->setVertexFunctionDescriptor(vs.get()); descriptor->setFragmentFunctionDescriptor(fs.get());
    descriptor->setRasterSampleCount(1);
    auto pipelineOptions = NS::TransferPtr(MTL4::PipelineOptions::alloc()->init());
    pipelineOptions->setShaderReflection(MTL4::ShaderReflectionBindingInfo | MTL4::ShaderReflectionBufferTypeInfo);
    descriptor->setOptions(pipelineOptions.get());
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA32Float);
    spirv_cross::Compiler reflection(MetalM2A::Words(directory / (name + ".fragment.spv")));
    for (const auto& resource : reflection.get_shader_resources().stage_outputs)
        if (reflection.get_decoration(resource.id, spv::DecorationLocation) == 1)
        {
            const auto& type = reflection.get_type(resource.type_id);
            Check(type.basetype == spirv_cross::SPIRType::Int && type.vecsize == 1 && type.width == 32, "Picking output must be signed int32");
            descriptor->colorAttachments()->object(1)->setPixelFormat(MTL::PixelFormatR32Sint);
        }
    Pipeline result;
    result.Vertices = NS::TransferPtr(MTL::VertexDescriptor::alloc()->init());
    spirv_cross::Compiler inputs(MetalM2A::Words(directory / (name + ".vertex.spv")));
    std::map<uint32_t, spirv_cross::Resource> attributes;
    for (const auto& resource : inputs.get_shader_resources().stage_inputs)
        attributes.emplace(inputs.get_decoration(resource.id, spv::DecorationLocation), resource);
    for (const auto& [location, resource] : attributes)
    {
        const auto& type = inputs.get_type(resource.type_id);
        Check(location < 16 && type.columns == 1 && type.width == 32 && type.array.empty(), "Unqualified vertex attribute");
        const auto format = type.basetype == spirv_cross::SPIRType::Float ? MTL::VertexFormatFloat : MTL::VertexFormatInt;
        Check(type.basetype == spirv_cross::SPIRType::Float || type.basetype == spirv_cross::SPIRType::Int, "Unqualified vertex scalar type");
        auto* attribute = result.Vertices->attributes()->object(location);
        attribute->setBufferIndex(16); attribute->setOffset(result.Stride);
        attribute->setFormat(static_cast<MTL::VertexFormat>(format + type.vecsize - 1));
        result.Stride += type.vecsize * 4;
    }
    result.Vertices->layouts()->object(16)->setStride(result.Stride);
    const std::map<std::string, size_t> strides = {{"ForwardPBR", 64}, {"Renderer2D_Line", 48},
        {"Renderer2D_Quad-16", 64}, {"Renderer2D_Quad-32", 64}, {"Renderer2D_Circle", 68}, {"Renderer2D_Text", 56}};
    Check(result.Stride == (strides.count(name) ? strides.at(name) : 0), "Reflected CPU vertex stream size differs");
    descriptor->setVertexDescriptor(result.Vertices.get());
    result.State = NS::TransferPtr(compiler->newRenderPipelineState(descriptor.get(), nullptr, &error));
    Check(bool(result.State), "Native MTL4 pipeline " + name + ": " + Error(error));
    ValidateReflection(result.State.get(), directory, name);
    std::cout << "PASS native_pipeline " << name << " stride=" << result.Stride << '\n';
    return result;
}
// Exact CPU stream used by Renderer2D::LineVertex, packed as its BufferLayout.
struct LineVertex
{
    glm::vec3 Position;
    glm::vec4 Color;
    int32_t Entity;
    glm::vec4 Clip;
};
static_assert(sizeof(LineVertex) == 48 && offsetof(LineVertex, Color) == 12 &&
    offsetof(LineVertex, Entity) == 28 && offsetof(LineVertex, Clip) == 32, "Line stream ABI");

void DrawProof(Hazel::MetalDevice& owner, const Pipeline& pipeline, const std::filesystem::path& directory, bool perspective, bool quad = false)
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
    std::vector<LineVertex> vertices;
    const auto triangle = [&](float distance, glm::vec4 color, int32_t entity)
    {
        const float scale = perspective ? -distance : 1.f;
        for (const auto& p : {glm::vec2(-.75f, -.75f), glm::vec2(.75f, -.75f), glm::vec2(-.75f, .75f)})
            vertices.push_back({{p.x * scale, p.y * scale, distance}, color, entity, {0, 0, -1, -1}});
    };
    triangle(perspective ? -1.5f : -.5f, {2.f, .25f, .5f, 1.f}, -123456789);
    triangle(perspective ? -2.5f : .5f, {0, 1, 0, 1}, 987654321); // Must fail depth after the first.
    triangle(perspective ? -.5f : -2.f, {0, 0, 1, 1}, 42);       // Must be clipped before the near plane.
    struct QuadVertex
    {
        glm::vec3 Position; glm::vec4 Color; glm::vec2 UV;
        float Texture, Tiling; int32_t Entity; glm::vec4 Clip;
    };
    static_assert(sizeof(QuadVertex) == 64 && offsetof(QuadVertex, Entity) == 44 && offsetof(QuadVertex, Clip) == 48, "Quad CPU stream ABI");
    std::vector<QuadVertex> quads;
    for (const auto& vertex : vertices)
        quads.push_back({vertex.Position, vertex.Color, {.5f, .5f}, 31, 1, vertex.Entity, vertex.Clip});
    auto stream = NS::TransferPtr(device->newBuffer(quad ? static_cast<const void*>(quads.data()) : vertices.data(),
        vertices.size() * (quad ? sizeof(QuadVertex) : sizeof(LineVertex)), MTL::ResourceStorageModeShared));
    auto uniform = NS::TransferPtr(device->newBuffer(&camera, sizeof(camera), MTL::ResourceStorageModeShared));
    auto readback = NS::TransferPtr(device->newBuffer(depthBase + scalarRow * side, MTL::ResourceStorageModeShared));
    Check(bool(stream) && bool(uniform) && bool(readback), "Proof buffers unavailable");
    Check(pipeline.Stride == (quad ? sizeof(QuadVertex) : sizeof(LineVertex)), "Reflected line stream stride differs from CPU ABI");
    // MSL tier-2 uses opaque 64-bit resource handles. Native reflection checks
    // offsets 0/256 and 32 entries; execution through slot 31 validates the ABI.
    std::array<NS::SharedPtr<MTL::Texture>, 32> sampled;
    NS::SharedPtr<MTL::SamplerState> sampler;
    NS::SharedPtr<MTL::Buffer> descriptors;
    if (quad)
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
    if (quad)
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
    if (quad) fragmentArguments->setAddress(descriptors->gpuAddress(), 0);
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
    render->setRenderPipelineState(pipeline.State.get());
    render->setArgumentTable(arguments.get(), MTL::RenderStageVertex);
    render->setArgumentTable(quad ? fragmentArguments.get() : arguments.get(), MTL::RenderStageFragment);
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
    std::ofstream raw(directory / (quad ? "quad32.readback.bin" : perspective ? "line-perspective.readback.bin" : "line-ortho.readback.bin"), std::ios::binary);
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
    std::cout << "PASS native_draw shader=" << (quad ? "Quad-32-slot31" : "Line") << " projection=" << (perspective ? "perspective" : "identity-ortho")
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
        Hazel::MetalDevice owner;
        auto* device = owner.NativeDevice();
        NS::Error* error = nullptr;
        auto descriptor = NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
        auto compiler = NS::TransferPtr(device->newCompiler(descriptor.get(), &error));
        Check(bool(compiler), "MTL4 compiler unavailable: " + Error(error));
        for (const auto& name : {"DisplayOutput", "EntityOutline", "EnvironmentSky", "ForwardPBR", "Renderer2D_Circle", "Renderer2D_Line", "Renderer2D_Quad-16", "Renderer2D_Quad-32", "Renderer2D_Text", "WorldPresentation"})
            Compile(device, compiler.get(), argv[2], name);
        const auto line = Compile(device, compiler.get(), argv[2], "Renderer2D_Line");
        DrawProof(owner, line, argv[2], false);
        DrawProof(owner, line, argv[2], true);
        const auto quad = Compile(device, compiler.get(), argv[2], "Renderer2D_Quad-32");
        DrawProof(owner, quad, argv[2], false, true);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL M2-A: " << error.what() << '\n';
        return 1;
    }
}
