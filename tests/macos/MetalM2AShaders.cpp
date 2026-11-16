// M2-A qualification only. Generated artifacts never enter the OpenGL cache.
#include "MetalM2A.h"
#include "Hazel/Renderer/Drawing/Renderer3DUniforms.h"
#include <cstddef>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.hpp>
#include <spirv_cross/spirv_msl.hpp>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace MetalM2A {
namespace {
void Replace(std::string& source, const std::string& before, const std::string& after)
{
    const auto position = source.find(before);
    Check(position != std::string::npos, "Missing qualified declaration: " + before);
    source.replace(position, before.size(), after);
}
void Write(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
    Check(bool(output), "Cannot write " + path.string());
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
    if (name == "Outline") return {32, {0, 4, 16}}; // Explicit declaration-only legacy adaptation.
    throw std::runtime_error("Unqualified CPU block: " + name);
}
void Reflect(spirv_cross::CompilerMSL& cross, std::ostream& output)
{
    const auto resources = cross.get_shader_resources();
    Check(resources.storage_buffers.empty() && resources.storage_images.empty() &&
        resources.push_constant_buffers.empty(), "Unqualified resource class");
    std::set<uint32_t> buffers, textures, samplers;
    // Separate Metal buffer/texture/sampler namespaces. Preserve GLSL bindings;
    // vertex streams reserve buffer 16, outside the qualified UBO range 0..7.
    for (const auto& resource : resources.uniform_buffers)
    {
        const auto binding = cross.get_decoration(resource.id, spv::DecorationBinding);
        Check(cross.get_decoration(resource.id, spv::DecorationDescriptorSet) == 0 &&
            binding < 8 && buffers.insert(binding).second, "Invalid/colliding UBO binding");
        const auto& type = cross.get_type(resource.base_type_id);
        const auto cpu = CPUContract(cross.get_name(resource.base_type_id));
        Check(cross.get_declared_struct_size(type) == cpu.Size && type.member_types.size() == cpu.Offsets.size(), "CPU/GPU uniform size/member count differs");
        output << "buffer " << resource.name << " binding=" << binding
            << " size=" << cross.get_declared_struct_size(type) << '\n';
        for (uint32_t i = 0; i < type.member_types.size(); ++i)
        {
            const auto& member = cross.get_type(type.member_types[i]);
            Check(cross.type_struct_member_offset(type, i) == cpu.Offsets[i], "CPU/GPU uniform member offset differs");
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
        uint32_t count = 1;
        for (size_t i = 0; i < type.array.size(); ++i)
        {
            Check(type.array_size_literal[i] && type.array[i] > 0, "Unbounded sampled-image array");
            count *= type.array[i];
        }
        Check(cross.get_decoration(resource.id, spv::DecorationDescriptorSet) == 0 &&
            binding + count <= 32, "Texture/sampler table exceeds qualification bounds");
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
int Translate(const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator("Hazel/Resources/shaders"))
        if (entry.path().extension() == ".glsl") paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    Check(paths.size() == 9, "Raster corpus changed: review qualification coverage");
    shaderc::Compiler compiler;
    Check(compiler.IsValid(), "shaderc unavailable");
    int failures = 0;
    for (const auto& path : paths)
    {
        const auto original = Read(path);
        for (const int slots : path.stem() == "Renderer2D_Quad" ? std::vector<int>{16, 32} : std::vector<int>{32})
        {
            const auto name = path.stem().string() + (path.stem() == "Renderer2D_Quad" ? "-" + std::to_string(slots) : "");
            for (const std::string stage : {"vertex", "fragment"})
            {
                const auto identity = name + "." + stage;
                try
                {
                    const auto marker = original.find("#type " + stage);
                    Check(marker != std::string::npos, "Stage missing");
                    const auto start = original.find('\n', marker) + 1;
                    const auto end = original.find("#type", start);
                    auto source = original.substr(start, end == std::string::npos ? end : end - start);
                    if (path.stem() == "Renderer2D_Quad" && stage == "fragment")
                        Replace(source, "#define HZ_MAX_TEXTURE_SLOTS 32", "#define HZ_MAX_TEXTURE_SLOTS " + std::to_string(slots));
                    if (path.stem() == "EntityOutline")
                    {
                        shaderc::CompileOptions legacyOptions;
                        legacyOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
                        const auto legacy = compiler.CompileGlslToSpv(source, stage == "vertex" ? shaderc_vertex_shader : shaderc_fragment_shader,
                            path.string().c_str(), legacyOptions);
                        Check(legacy.GetCompilationStatus() != shaderc_compilation_status_success, "Review legacy adaptation: raw source now compiles");
                        Write(output / (identity + ".legacy-vulkan-diagnostic.txt"), legacy.GetErrorMessage());
                    }
                    // Legacy GLSL loose uniforms cannot form Vulkan SPIR-V. Lift only
                    // declarations to std140; the authoritative function body is untouched.
                    if (path.stem() == "EntityOutline")
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
                            Replace(source, "uniform int u_Selected;\nuniform int u_Radius;\nuniform vec4 u_EncodedColor;",
                                "layout(std140,binding=7) uniform Outline { int u_Selected; int u_Radius; vec4 u_EncodedColor; };");
                        }
                    }
                    shaderc::CompileOptions options;
                    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
                    options.SetGenerateDebugInfo();
                    options.SetOptimizationLevel(shaderc_optimization_level_performance);
                    auto result = compiler.CompileGlslToSpv(source, stage == "vertex" ? shaderc_vertex_shader : shaderc_fragment_shader,
                        path.string().c_str(), options);
                    Check(result.GetCompilationStatus() == shaderc_compilation_status_success, result.GetErrorMessage());
                    Check(result.GetNumWarnings() == 0, result.GetErrorMessage());
                    std::vector<uint32_t> words(result.cbegin(), result.cend());
                    Check(spvtools::SpirvTools(SPV_ENV_VULKAN_1_2).Validate(words), "SPIR-V validation failed");
                    spirv_cross::CompilerMSL cross(words);
                    auto mslOptions = cross.get_msl_options();
                    mslOptions.platform = spirv_cross::CompilerMSL::Options::macOS;
                    mslOptions.set_msl_version(3, 0);
                    mslOptions.enable_decoration_binding = false;
                    const bool argumentBuffer = name == "Renderer2D_Quad-32" && stage == "fragment";
                    mslOptions.argument_buffers = argumentBuffer;
                    mslOptions.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
                    cross.set_msl_options(mslOptions);
                    auto common = cross.get_common_options();
                    common.vertex.fixup_clipspace = true; // OpenGL [-w,w] -> Metal [0,w].
                    common.vertex.flip_vert_y = true;     // OpenGL lower-left geometry -> Metal top-left rows.
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
                    Write(output / (identity + ".glsl"), source);
                    Write(output / (identity + ".spv"), {reinterpret_cast<const char*>(words.data()), words.size() * 4});
                    Write(output / (identity + ".metal"), msl);
                    Write(output / (identity + ".reflection.txt"), reflection.str());
                    std::cout << "PASS translation " << identity << '\n';
                }
                catch (const std::exception& error)
                {
                    ++failures;
                    Write(output / (identity + ".failure.txt"), error.what());
                    std::cerr << "FAIL translation " << identity << ": " << error.what() << '\n';
                }
            }
        }
    }
    return failures ? 1 : 0;
}
}
