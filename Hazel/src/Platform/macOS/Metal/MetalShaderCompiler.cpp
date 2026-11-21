#include "hzpch.h"
#include "MetalShaderArtifact.h"
#include "Hazel/Renderer/Drawing/Renderer3DUniforms.h"
#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.hpp>
#include <spirv_cross/spirv_msl.hpp>
#include <cstddef>
#include <set>
#include <sstream>

namespace Hazel {
namespace {
void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
void Replace(std::string& source, const std::string& before, const std::string& after)
{
    const auto position = source.find(before);
    Check(position != std::string::npos, "Missing qualified declaration: " + before);
    source.replace(position, before.size(), after);
}
struct Layout { size_t Size; std::vector<size_t> Offsets; };
Layout CPUContract(const std::string& name)
{
    using namespace Hazel::Renderer3DUniforms;
    if (name == "View") return {sizeof(ViewData), {offsetof(ViewData, VP), offsetof(ViewData, Eye), offsetof(ViewData, ToCamera), offsetof(ViewData, LightDirection), offsetof(ViewData, Incident)}};
    if (name == "Object") return {sizeof(ObjectData), {offsetof(ObjectData, Model), offsetof(ObjectData, Normal), offsetof(ObjectData, Identity)}};
    if (name == "Material") return {sizeof(MaterialData), {offsetof(MaterialData, Base), offsetof(MaterialData, Emissive), offsetof(MaterialData, Surface), offsetof(MaterialData, Options)}};
    if (name == "Environment") return {sizeof(EnvironmentData), {offsetof(EnvironmentData, InverseVP), offsetof(EnvironmentData, RotationGain), offsetof(EnvironmentData, Options)}};
    if (name == "Camera") return {sizeof(glm::mat4), {0}};
    if (name == "Presentation") return {sizeof(glm::vec4), {0}};
    if (name == "Outline") return {32, {0, 4, 8, 16}}; // Explicit declaration-only legacy adaptation.
    throw std::runtime_error("Unqualified CPU block: " + name);
}
void Reflect(spirv_cross::CompilerMSL& cross, std::ostream& output)
{
    const auto resources = cross.get_shader_resources();
    Check(resources.storage_buffers.empty() && resources.storage_images.empty() &&
        resources.push_constant_buffers.empty() && resources.separate_images.empty() &&
        resources.separate_samplers.empty() && resources.subpass_inputs.empty() &&
        resources.atomic_counters.empty() && resources.acceleration_structures.empty() &&
        resources.shader_record_buffers.empty(), "Unqualified resource class");
    std::set<uint32_t> buffers, textures, samplers;
    // Separate Metal buffer/texture/sampler namespaces. Preserve GLSL bindings;
    // vertex streams reserve buffer 16, outside the qualified UBO range 0..7.
    for (const auto& resource : resources.uniform_buffers)
    {
        const auto binding = cross.get_decoration(resource.id, spv::DecorationBinding);
        Check(cross.get_decoration(resource.id, spv::DecorationDescriptorSet) == 0 &&
            binding < 8 && buffers.insert(binding).second, "Invalid/colliding UBO binding");
        Check(cross.get_type(resource.type_id).array.empty(), "Uniform buffer arrays are outside the binding contract");
        const auto& type = cross.get_type(resource.base_type_id);
        const auto cpu = CPUContract(cross.get_name(resource.base_type_id));
        Check(cross.get_declared_struct_size(type) == cpu.Size && type.member_types.size() == cpu.Offsets.size(), "CPU/GPU uniform size/member count differs");
        output << "buffer " << resource.name << " binding=" << binding
            << " size=" << cross.get_declared_struct_size(type) << '\n';
        for (uint32_t i = 0; i < type.member_types.size(); ++i)
        {
            const auto& member = cross.get_type(type.member_types[i]);
            Check(cross.type_struct_member_offset(type, i) == cpu.Offsets[i], "CPU/GPU uniform member offset differs: " + cross.get_member_name(resource.base_type_id, i));
            if (member.columns > 1)
                Check(cross.type_struct_member_matrix_stride(type, i) == 16 && !cross.has_member_decoration(resource.base_type_id, i, spv::DecorationRowMajor), "Matrix stride/major order differs");
            output << "  " << cross.get_member_name(resource.base_type_id, i)
                << " offset=" << cross.type_struct_member_offset(type, i)
                << " size=" << cross.get_declared_struct_member_size(type, i);
            if (member.columns > 1)
                output << " matrix_stride=" << cross.type_struct_member_matrix_stride(type, i);
            if (!member.array.empty())
                output << " array_stride=" << cross.type_struct_member_array_stride(type, i);
            output << '\n';
        }
    }
    for (const auto& resource : resources.sampled_images)
    {
        const auto binding = cross.get_decoration(resource.id, spv::DecorationBinding);
        const auto& type = cross.get_type(resource.type_id);
        Check(!type.image.arrayed && (type.image.dim == spv::Dim2D || type.image.dim == spv::DimCube), "Unqualified sampled-image dimension");
        const auto scalar = cross.get_type(type.image.type).basetype;
        Check(scalar == spirv_cross::SPIRType::Float || scalar == spirv_cross::SPIRType::Int, "Unqualified image scalar type");
        uint32_t count = 1;
        for (size_t i = 0; i < type.array.size(); ++i)
        {
            Check(type.array_size_literal[i] && type.array[i] > 0 && type.array[i] <= 32 / count, "Unbounded sampled-image array");
            count *= type.array[i];
        }
        Check(cross.get_decoration(resource.id, spv::DecorationDescriptorSet) == 0 &&
            binding < 32 && count <= 32 - binding, "Texture/sampler table exceeds qualification bounds");
        for (uint32_t i = 0; i < count; ++i)
            Check(textures.insert(binding + i).second && samplers.insert(binding + i).second,
                "Colliding texture/sampler binding");
        output << "sampled_image " << resource.name << " texture=" << binding << " sampler=" << binding
            << " count=" << count << " dimension=" << type.image.dim
            << " arrayed=" << type.image.arrayed << " sample_type=" << cross.get_type(type.image.type).basetype << '\n';
    }
    for (const auto& pair : {std::make_pair("input", resources.stage_inputs),
                            std::make_pair("output", resources.stage_outputs)})
        for (const auto& resource : pair.second)
        {
            const auto& type = cross.get_type(resource.type_id);
            output << pair.first << ' ' << resource.name << " location="
                << cross.get_decoration(resource.id, spv::DecorationLocation)
                << " type=" << type.basetype << " width=" << type.width
                << " vectors=" << type.vecsize << " columns=" << type.columns << '\n';
        }
}
}
std::string ReflectMetalShader(const std::vector<uint32_t>& words)
{
    Check(spvtools::SpirvTools(SPV_ENV_VULKAN_1_2).Validate(words), "Invalid Vulkan 1.2 SPIR-V");
    spirv_cross::CompilerMSL cross(words);
    std::ostringstream output;
    Reflect(cross, output);
    return output.str();
}
MetalShaderArtifact CompileMetalShader(const std::string& name, const std::string& input, uint32_t slots)
{
    MetalShaderArtifact::ValidateName(name, slots);
    MetalShaderArtifact artifact;
    artifact.Name = name; artifact.TextureSlots = slots;
    artifact.SourceIdentity = MetalShaderArtifact::SourceKey(name, input, slots);
    shaderc::Compiler compiler;
    Check(compiler.IsValid(), "Metal shader producer: shaderc unavailable");
    for (const std::string stage : {"vertex", "fragment"})
    {
        try
        {
            const auto marker = input.find("#type " + stage);
            Check(marker != std::string::npos, "Stage missing");
            const auto eol = input.find('\n', marker);
            Check(eol != std::string::npos, "Stage marker has no source");
            Check(input.find("#type " + stage, eol) == std::string::npos, "Duplicate shader stage");
            const auto start = eol + 1;
            const auto end = input.find("#type", start);
            auto source = input.substr(start, end == std::string::npos ? end : end - start);
            if (name == "Renderer2D_Quad" && stage == "fragment")
                Replace(source, "#define HZ_MAX_TEXTURE_SLOTS 32", "#define HZ_MAX_TEXTURE_SLOTS " + std::to_string(slots));
            // Legacy GLSL loose uniforms cannot form Vulkan SPIR-V. Lift only
            // declarations to std140; the authoritative function body is untouched.
            if (name == "EntityOutline")
            {
                Replace(source, "#version 410 core", "#version 450 core");
                if (stage == "vertex")
                {
                    // Vulkan names the same zero-based non-indexed builtin VertexIndex.
                    while (source.find("gl_VertexID") != std::string::npos)
                        Replace(source, "gl_VertexID", "gl_VertexIndex");
                }
                if (stage == "fragment")
                {
                    Replace(source, "uniform isampler2D u_Entities;", "layout(binding=0) uniform isampler2D u_Entities;");
                    Replace(source, "uniform int u_Selected;\nuniform int u_Radius;\nuniform int u_FlipY;\nuniform vec4 u_EncodedColor;",
                        "layout(std140,binding=7) uniform Outline { int u_Selected; int u_Radius; int u_FlipY; vec4 u_EncodedColor; };");
                }
            }
            shaderc::CompileOptions options;
            options.AddMacroDefinition("HZ_METAL", "1");
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
            options.SetGenerateDebugInfo();
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
            auto result = compiler.CompileGlslToSpv(source, stage == "vertex" ? shaderc_vertex_shader : shaderc_fragment_shader,
                (name + ".glsl").c_str(), options);
            Check(result.GetCompilationStatus() == shaderc_compilation_status_success, result.GetErrorMessage());
            Check(result.GetNumWarnings() == 0, result.GetErrorMessage());
            std::vector<uint32_t> words(result.cbegin(), result.cend());
            Check(spvtools::SpirvTools(SPV_ENV_VULKAN_1_2).Validate(words), "SPIR-V validation failed");
            spirv_cross::CompilerMSL cross(words);
            auto mslOptions = cross.get_msl_options();
            mslOptions.platform = spirv_cross::CompilerMSL::Options::macOS;
            mslOptions.set_msl_version(3, 0);
            mslOptions.enable_decoration_binding = false;
            const bool argumentBuffer = (name == "Renderer2D_Quad" && slots == 32) && stage == "fragment";
            mslOptions.argument_buffers = argumentBuffer;
            mslOptions.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
            cross.set_msl_options(mslOptions);
            auto common = cross.get_common_options();
            common.vertex.fixup_clipspace = true; // OpenGL [-w,w] -> Metal [0,w].
            // Offscreen attachments retain OpenGL lower-left rows. Final window output
            // samples those rows in reverse exactly once; intermediate passes preserve them.
            common.vertex.flip_vert_y = name != "DisplayOutput";
            cross.set_common_options(common);
            std::ostringstream reflection;
            Reflect(cross, reflection);
            if (argumentBuffer) reflection << "tier2_argument_buffer fragment_buffer=0 textures_id=0 count=32 samplers_id=32 count=32\n";
            const auto reflected = cross.get_shader_resources();
            std::set<uint32_t> mapped;
            for (const auto& list : {reflected.uniform_buffers, reflected.sampled_images})
                for (const auto& resource : list)
                {
                    const auto binding = cross.get_decoration(resource.id, spv::DecorationBinding);
                    if (!mapped.insert(binding).second) continue;
                    spirv_cross::MSLResourceBinding mapping;
                    mapping.stage = stage == "vertex" ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
                    mapping.binding = binding;
                    mapping.msl_buffer = binding; mapping.msl_texture = binding; mapping.msl_sampler = argumentBuffer ? 32 + binding : binding;
                    cross.add_msl_resource_binding(mapping);
                }
            const auto msl = cross.compile();
            // Cross reports actual assigned indices after compile; fail on a mismatch.
            const auto resources = cross.get_shader_resources();
            for (const auto& resource : resources.uniform_buffers)
                Check(cross.get_automatic_msl_resource_binding(resource.id) == cross.get_decoration(resource.id, spv::DecorationBinding), "MSL buffer mapping differs");
            for (const auto& resource : resources.sampled_images)
            {
                Check(cross.get_automatic_msl_resource_binding(resource.id) == cross.get_decoration(resource.id, spv::DecorationBinding), "MSL texture mapping differs");
                Check(cross.get_automatic_msl_resource_binding_secondary(resource.id) == (argumentBuffer ? 32u : 0u) + cross.get_decoration(resource.id, spv::DecorationBinding), "MSL sampler mapping differs");
            }
            auto& data = artifact.Stages[stage == "vertex" ? 0 : 1];
            data.Source = source; data.Spirv = std::move(words);
            data.MSL = msl; data.Reflection = reflection.str();
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("Metal shader " + name + " slots=" + std::to_string(slots) + " " + stage + ": " + error.what());
        }
    }
    artifact.Validate(false);
    return artifact;
}
}
