#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    constexpr const char* source = R"(
        #version 450
        layout(location = 0) in vec3 a_Position;
        layout(set = 0, binding = 0) uniform Camera { mat4 ViewProjection; } u_Camera;
        void main() { gl_Position = u_Camera.ViewProjection * vec4(a_Position, 1.0); }
    )";

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    options.SetGenerateDebugInfo();
    const auto result = compiler.CompileGlslToSpv(
        source, shaderc_vertex_shader, "hazel-macos-feasibility.vert", options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        throw std::runtime_error(result.GetErrorMessage());

    std::vector<uint32_t> spirv(result.cbegin(), result.cend());
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_2);
    if (!validator.Validate(spirv)) throw std::runtime_error("SPIR-V validation failed");

    spirv_cross::CompilerGLSL cross(spirv);
    const auto resources = cross.get_shader_resources();
    if (resources.uniform_buffers.size() != 1)
        throw std::runtime_error("Expected one reflected uniform buffer");
    const auto& buffer = resources.uniform_buffers.front();
    if (cross.get_decoration(buffer.id, spv::DecorationBinding) != 0)
        throw std::runtime_error("Uniform binding reflection changed");

    auto glslOptions = cross.get_common_options();
    glslOptions.version = 410;
    glslOptions.es = false;
    glslOptions.enable_420pack_extension = false;
    cross.set_common_options(glslOptions);
    const std::string glsl = cross.compile();
    if (glsl.find("#version 410") == std::string::npos || glsl.find("void main") == std::string::npos)
        throw std::runtime_error("SPIRV-Cross did not emit the GLSL 410 path");

    std::cout << "PASS: native shaderc link, Vulkan 1.2 SPIR-V validation/reflection, GLSL 410 cross-compile\n";
    return 0;
}
