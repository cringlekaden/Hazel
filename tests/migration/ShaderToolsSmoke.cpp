// Verify the pinned toolchain independently before engine pipeline integration.
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv-tools/libspirv.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::vector<uint32_t> Compile(shaderc::Compiler& compiler, shaderc_shader_kind kind,
                                    const std::string& source, bool vulkan)
{
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(vulkan ? shaderc_target_env_vulkan : shaderc_target_env_opengl,
                                 vulkan ? shaderc_env_version_vulkan_1_2 : shaderc_env_version_opengl_4_5);
    if (vulkan) {
        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
    }
    auto module = compiler.CompileGlslToSpv(source, kind, "migration-toolchain", options);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        throw std::runtime_error(module.GetErrorMessage());
    return {module.cbegin(), module.cend()};
}

int main()
{
    try {
        shaderc::Compiler compiler;
        Check(compiler.IsValid(), "shaderc compiler initialization failed");
        const std::string vertex = R"(#version 450 core
layout(location=0) in vec3 position;
layout(location=0) out vec2 uv;
layout(std140,binding=3) uniform Camera { mat4 viewProjection; };
void main() { uv=position.xy; gl_Position=viewProjection*vec4(position,1); }
)";
        const std::string fragment = R"(#version 450 core
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(binding=5) uniform sampler2D image;
void main() { color=texture(image,uv); }
)";
        for (auto kind : {shaderc_glsl_vertex_shader, shaderc_glsl_fragment_shader}) {
            auto spirv = Compile(compiler, kind, kind == shaderc_glsl_vertex_shader ? vertex : fragment, true);
            Check(!spirv.empty() && spirv.front()==0x07230203u, "Invalid Vulkan SPIR-V magic");
            Check(spvtools::SpirvTools(SPV_ENV_VULKAN_1_2).Validate(spirv), "Optimized Vulkan module failed validation");
            spirv_cross::CompilerGLSL cross(spirv);
            auto resources = cross.get_shader_resources();
            if (kind == shaderc_glsl_vertex_shader) {
                Check(resources.uniform_buffers.size()==1, "Uniform block reflection lost");
                auto resource=resources.uniform_buffers.front();
                Check(cross.get_name(resource.base_type_id)=="Camera", "Optimized UBO name lost");
                Check(cross.get_decoration(resource.id,spv::DecorationBinding)==3, "UBO binding lost");
                Check(cross.get_declared_struct_size(cross.get_type(resource.base_type_id))==64,
                      "UBO layout differs");
            } else {
                Check(resources.sampled_images.size()==1, "Sampler reflection lost");
                Check(resources.sampled_images.front().name=="image", "Optimized sampler name lost");
                Check(cross.get_decoration(resources.sampled_images.front().id,spv::DecorationBinding)==5,
                      "Sampler binding lost");
            }
            auto options=cross.get_common_options();
            options.version=410;
            options.es=false;
            options.enable_420pack_extension=false;
            cross.set_common_options(options);
            auto glsl410=cross.compile();
            Check(glsl410.find("#version 410")!=std::string::npos, "GLSL 410 generation failed");
            Check(glsl410.find("binding =")==std::string::npos, "GLSL 410 retained 420 bindings");
            Check(glsl410.find("GL_ARB_shading_language_420pack")==std::string::npos,
                  "GLSL 410 requires 420pack extension");
            // The accelerated SPIR-V loading path uses OpenGL semantics and locations.
            options.version=450;
            options.enable_420pack_extension=true;
            cross.set_common_options(options);
            auto opengl=Compile(compiler,kind,cross.compile(),false);
            Check(!opengl.empty() && opengl.front()==0x07230203u, "OpenGL SPIR-V generation failed");
            Check(spvtools::SpirvTools(SPV_ENV_OPENGL_4_5).Validate(opengl), "OpenGL module failed validation");
        }
        shaderc::CompileOptions hlslOptions;
        hlslOptions.SetSourceLanguage(shaderc_source_language_hlsl);
        hlslOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        hlslOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
        auto hlsl = compiler.CompileGlslToSpv("float4 main(float4 position : POSITION) : SV_Position { return position; }",
            shaderc_vertex_shader, "migration-hlsl", hlslOptions);
        Check(hlsl.GetCompilationStatus() == shaderc_compilation_status_success, hlsl.GetErrorMessage().c_str());
        Check(spvtools::SpirvTools(SPV_ENV_VULKAN_1_2).Validate(hlsl.cbegin(), hlsl.cend() - hlsl.cbegin()),
            "HLSL compiler/optimizer module failed validation");
        auto failure=compiler.CompileGlslToSpv("#version 450\ninvalid",shaderc_glsl_vertex_shader,"invalid-shader");
        Check(failure.GetCompilationStatus()!=shaderc_compilation_status_success && !failure.GetErrorMessage().empty(),
              "Invalid shader lacked failure diagnostics");
        std::cout << "PASS: pinned shaderc/Cross compile GLSL/HLSL, optimize and validate Vulkan/OpenGL SPIR-V, reflect UBO/sampler bindings, generate GLSL 410 without 420pack, and report invalid input\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
