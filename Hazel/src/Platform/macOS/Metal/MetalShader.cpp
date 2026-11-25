#include "hzpch.h"
#include "MetalShader.h"
#include "MetalRendererAPI.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "MetalDevice.h"
#include "Hazel/Core/Resources.h"
#include <spirv_cross/spirv_cross.hpp>
#include <dispatch/dispatch.h>
#include <map>
#include <sstream>

namespace Hazel {
namespace {
void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
NS::String* Label(const std::string& text) { return NS::String::string(text.c_str(), NS::UTF8StringEncoding); }
std::string Error(NS::Error* error) { return error ? error->localizedDescription()->utf8String() : "no compiler diagnostic"; }
std::string ValidateReflection(MTL::RenderPipelineState* pipeline, const MetalShaderArtifact& artifact)
{
    auto* reflection = pipeline->reflection();
    Check(reflection != nullptr, "Native pipeline reflection missing");
    std::ostringstream output;
    const auto name = artifact.Name + (artifact.Name == "Renderer2D_Quad" ? "-" + std::to_string(artifact.TextureSlots) : "");
    for (const std::string stage : {"vertex", "fragment"})
    {
        const auto* bindings = stage == "vertex" ? reflection->vertexBindings() : reflection->fragmentBindings();
        Check(bindings != nullptr, "Native stage bindings missing");
        spirv_cross::Compiler source(artifact.Stages[stage == "vertex" ? 0 : 1].Spirv);
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
    return output.str();
}
}
MetalShader::MetalShader(std::shared_ptr<MetalDevice> deviceOwner, const std::string& shaderName, uint32_t slots, bool drawableOutput, const RasterState* raster)
    : m_Device(std::move(deviceOwner))
{
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    MetalShaderArtifact::ValidateName(shaderName, slots);
    Check(bool(m_Device), "Metal device owner missing");
    const auto name = shaderName + (shaderName == "Renderer2D_Quad" ? "-" + std::to_string(slots) : "");
    try
    {
        const auto packaged = Resources::Resolve("shaders/metal/" + name + ".hzmetal");
        const auto bytes = ReadMetalShaderFile(packaged);
        auto artifact = MetalShaderArtifact::Decode(bytes);
        Check(artifact.Name == shaderName && artifact.TextureSlots == slots, "Packaged shader/variant identity mismatch");
        const auto source = Resources::Resolve("shaders/" + shaderName + ".glsl");
        if (std::filesystem::exists(source))
            Check(artifact.SourceIdentity == MetalShaderArtifact::SourceKey(shaderName, ReadMetalShaderFile(source), slots),
                "Packaged shader is stale; rebuild with scripts/macos/metal_shaders.py");
        m_Identity = MetalShaderArtifact::Digest(bytes);
        const auto cache = Resources::Get().UserData / "cache/shader/metal4" / (m_Identity + ".hzmetal");
        bool cacheValid = false;
        if (std::filesystem::exists(cache))
        {
            try
            {
                const auto cached = ReadMetalShaderFile(cache);
                Check(MetalShaderArtifact::Digest(cached) == m_Identity, "Cache content identity mismatch");
                artifact = MetalShaderArtifact::Decode(cached);
                cacheValid = true;
            }
            catch (const std::exception& error) { HZ_CORE_WARN("Metal shader cache '{}' rejected: {}", cache.u8string(), error.what()); }
        }
        m_Artifact = std::move(artifact);
        auto* device = m_Device->NativeDevice();
        NS::Error* error = nullptr;
        auto compilerDescriptor = NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
        auto compiler = NS::TransferPtr(device->newCompiler(compilerDescriptor.get(), &error));
        Check(bool(compiler), "Native MTL4 compiler unavailable: " + Error(error));
        for (size_t i = 0; i < m_Libraries.size(); ++i)
        {
            const auto& data = m_Artifact.Stages[i].Library;
            auto bytesData = dispatch_data_create(data.data(), data.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            Check(bytesData != nullptr, "Cannot allocate metallib data");
            m_Libraries[i] = NS::TransferPtr(device->newLibrary(bytesData, &error));
            dispatch_release(bytesData);
            Check(bool(m_Libraries[i]), std::string(i == 0 ? "Vertex" : "Fragment") + " metallib load: " + Error(error));
        }
        auto vs = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        vs->setLibrary(m_Libraries[0].get()); vs->setName(Label("main0"));
        auto fs = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        fs->setLibrary(m_Libraries[1].get()); fs->setName(Label("main0"));
        auto descriptor = NS::TransferPtr(MTL4::RenderPipelineDescriptor::alloc()->init());
        descriptor->setLabel(Label("Hazel Metal 4 " + name));
        descriptor->setVertexFunctionDescriptor(vs.get()); descriptor->setFragmentFunctionDescriptor(fs.get());
        descriptor->setRasterSampleCount(1);
        auto pipelineOptions = NS::TransferPtr(MTL4::PipelineOptions::alloc()->init());
        pipelineOptions->setShaderReflection(MTL4::ShaderReflectionBindingInfo | MTL4::ShaderReflectionBufferTypeInfo);
        descriptor->setOptions(pipelineOptions.get());
        Check(!drawableOutput || (shaderName == "DisplayOutput" || shaderName == "EntityOutline"), "Only DisplayOutput supports BGRA8 drawable format");
        if(shaderName!="DepthPBR")descriptor->colorAttachments()->object(0)->setPixelFormat(drawableOutput ? MTL::PixelFormatBGRA8Unorm : MTL::PixelFormatRGBA32Float);
        spirv_cross::Compiler reflection(m_Artifact.Stages[1].Spirv);
        for (const auto& resource : reflection.get_shader_resources().stage_outputs)
            if (reflection.get_decoration(resource.id, spv::DecorationLocation) == 1)
            {
                const auto& type = reflection.get_type(resource.type_id);
                Check(type.basetype == spirv_cross::SPIRType::Int && type.vecsize == 1 && type.width == 32, "Picking output must be signed int32");
                descriptor->colorAttachments()->object(1)->setPixelFormat(MTL::PixelFormatR32Sint);
            }
        if (shaderName == "EnvironmentSky")
        {
            descriptor->colorAttachments()->object(1)->setPixelFormat(MTL::PixelFormatR32Sint);
            descriptor->colorAttachments()->object(1)->setWriteMask(MTL::ColorWriteMaskNone);
        }
        if (raster && shaderName!="DepthPBR")
        {
            auto* color = descriptor->colorAttachments()->object(0);
            color->setBlendingState(raster->Blend ? MTL4::BlendStateEnabled : MTL4::BlendStateDisabled);
            color->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
            color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
            color->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
            color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
            const auto mask = [](glm::bvec4 value) {
                return MTL::ColorWriteMask((value.r ? MTL::ColorWriteMaskRed : 0) |
                    (value.g ? MTL::ColorWriteMaskGreen : 0) | (value.b ? MTL::ColorWriteMaskBlue : 0) |
                    (value.a ? MTL::ColorWriteMaskAlpha : 0));
            };
            color->setWriteMask(mask(raster->ColorMask));
            descriptor->colorAttachments()->object(1)->setWriteMask(mask(raster->PickingMask));
        }
        auto vertices = NS::TransferPtr(MTL::VertexDescriptor::alloc()->init());
        spirv_cross::Compiler inputs(m_Artifact.Stages[0].Spirv);
        const std::map<std::string, std::vector<MTL::VertexFormat>> formats = {
            {"DepthPBR", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat2, MTL::VertexFormatFloat4}},
            {"MotionPBR", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat2, MTL::VertexFormatFloat4}},
            {"ForwardPBR", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat2, MTL::VertexFormatFloat4}},
            {"Renderer2D_Line", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat4, MTL::VertexFormatFloat4, MTL::VertexFormatInt, MTL::VertexFormatFloat4}},
            {"Renderer2D_Circle", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat, MTL::VertexFormatFloat, MTL::VertexFormatInt, MTL::VertexFormatFloat4}},
            {"Renderer2D_Quad", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat2, MTL::VertexFormatFloat, MTL::VertexFormatFloat, MTL::VertexFormatInt, MTL::VertexFormatFloat4}},
            {"Renderer2D_Text", {MTL::VertexFormatFloat3, MTL::VertexFormatFloat4, MTL::VertexFormatFloat2, MTL::VertexFormatInt, MTL::VertexFormatFloat4}}};
        const auto expected = formats.find(m_Artifact.Name);
        const size_t attributeCount = expected == formats.end() ? 0 : expected->second.size();
        std::map<uint32_t, spirv_cross::Resource> attributes;
        for (const auto& resource : inputs.get_shader_resources().stage_inputs)
            attributes.emplace(inputs.get_decoration(resource.id, spv::DecorationLocation), resource);
        Check(attributes.size() == attributeCount, "CPU vertex attribute count differs");
        for (const auto& [location, resource] : attributes)
        {
            const auto& type = inputs.get_type(resource.type_id);
            Check(location < 16 && type.columns == 1 && type.width == 32 && type.vecsize >= 1 && type.vecsize <= 4 && type.array.empty(), "Unqualified vertex attribute");
            const auto format = type.basetype == spirv_cross::SPIRType::Float ? MTL::VertexFormatFloat : MTL::VertexFormatInt;
            Check(type.basetype == spirv_cross::SPIRType::Float || type.basetype == spirv_cross::SPIRType::Int, "Unqualified vertex scalar type");
            auto* attribute = vertices->attributes()->object(location);
            attribute->setBufferIndex(16); attribute->setOffset(m_Stride);
            const auto nativeFormat = static_cast<MTL::VertexFormat>(format + type.vecsize - 1);
            Check(location < attributeCount && nativeFormat == expected->second[location], "CPU vertex attribute format/location differs");
            attribute->setFormat(nativeFormat);
            m_Stride += type.vecsize * 4;
        }
        vertices->layouts()->object(16)->setStride(m_Stride);
        const std::map<std::string, size_t> strides = {{"MotionPBR", 64}, {"DepthPBR", 64}, {"ForwardPBR", 64}, {"Renderer2D_Line", 80},
            {"Renderer2D_Quad-16", 64}, {"Renderer2D_Quad-32", 64}, {"Renderer2D_Circle", 68}, {"Renderer2D_Text", 56}};
        Check(m_Stride == (strides.count(name) ? strides.at(name) : 0), "Reflected CPU vertex stream size differs");
        descriptor->setVertexDescriptor(vertices.get());
        m_Pipeline = NS::TransferPtr(compiler->newRenderPipelineState(descriptor.get(), nullptr, &error));
        Check(bool(m_Pipeline), "Native MTL4 pipeline " + name + ": " + Error(error));
        m_Reflection = ValidateReflection(m_Pipeline.get(), m_Artifact);
        if (!cacheValid)
        {
            try { WriteMetalShaderFile(cache, bytes); }
            catch (const std::exception& error) { HZ_CORE_WARN("Metal shader cache '{}' unavailable: {}; using validated package", cache.u8string(), error.what()); }
        }
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error("Metal shader '" + name + "': " + error.what());
    }
}
namespace { [[noreturn]] void NoDrawing() { throw std::logic_error("Metal shader drawing/uniform commands require a separately qualified drawing backend"); } }
void MetalShader::Bind() const
{
    auto* backend = dynamic_cast<MetalRendererAPI*>(&RenderCommand::Backend());
    if (!backend || backend->Device() != m_Device) throw std::logic_error("Metal shader drawing requires its owning backend");
    backend->BindShader(m_Artifact.Name);
}
void MetalShader::Unbind() const {}
void MetalShader::SetInt(const std::string& name, int value)
{
    static const std::map<std::string, int> bindings = {{"u_Base",0},{"u_MetalRough",1},{"u_Normal",2},{"u_Occlusion",3},
        {"u_Emissive",4},{"u_Irradiance",5},{"u_Specular",6},{"u_BRDF",7},{"u_Sky",5}};
    auto found = bindings.find(name);
    if (found == bindings.end() || found->second != value) throw std::invalid_argument("Metal loose uniform unavailable or sampler differs from reflected GLSL binding");
}
void MetalShader::SetIntArray(const std::string&, const int*, uint32_t) { NoDrawing(); }
void MetalShader::SetFloat(const std::string&, float) { NoDrawing(); }
void MetalShader::SetFloat2(const std::string&, const glm::vec2&) { NoDrawing(); }
void MetalShader::SetFloat3(const std::string&, const glm::vec3&) { NoDrawing(); }
void MetalShader::SetFloat4(const std::string&, const glm::vec4&) { NoDrawing(); }
void MetalShader::SetMat4(const std::string&, const glm::mat4&) { NoDrawing(); }
}
