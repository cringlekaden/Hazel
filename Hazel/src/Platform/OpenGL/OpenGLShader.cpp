// Adapted actual target shader pipeline: source pins, robust caches and GLSL 410 fallback.
#include "hzpch.h"
#include "Hazel/Core/Resources.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include "Hazel/Core/Timer.h"
#include "Hazel/Core/FileSystem.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace Hazel {
namespace {
        std::string GetShaderLog(unsigned int shader)
        {
            int length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return "(no compiler log provided)";
            std::string log(static_cast<std::size_t>(length), '\0');
            int written = 0;
            glGetShaderInfoLog(shader, length, &written, log.data());
            log.resize(static_cast<std::size_t>(written));
            return log;
        }

        std::string GetProgramLog(unsigned int program)
        {
            int length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return "(no linker log provided)";
            std::string log(static_cast<std::size_t>(length), '\0');
            int written = 0;
            glGetProgramInfoLog(program, length, &written, log.data());
            log.resize(static_cast<std::size_t>(written));
            return log;
        }

        unsigned int CompileShader(unsigned int type, const std::string& source, const char* stageName)
        {
            const unsigned int shader = glCreateShader(type);
            if (shader == 0)
            {
                HZ_CORE_ERROR("Failed to create {} shader...", stageName);
                throw std::runtime_error("OpenGL shader creation failed...");
            }
            const char* sourceText = source.c_str();
            glShaderSource(shader, 1, &sourceText, nullptr);
            glCompileShader(shader);
            int compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled == GL_FALSE)
            {
                const std::string log = GetShaderLog(shader);
                glDeleteShader(shader);
                HZ_CORE_ERROR("{} shader compilation failed:\n{}", stageName, log);
                throw std::runtime_error("OpenGL shader compilation failed...");
            }
            return shader;
        }

shaderc_shader_kind GLShaderStageToShaderC(GLenum stage)
{
    if (stage==GL_VERTEX_SHADER) return shaderc_glsl_vertex_shader;
    if (stage==GL_FRAGMENT_SHADER) return shaderc_glsl_fragment_shader;
    throw std::invalid_argument("Unsupported shader stage");
}
const char* GLShaderStageToString(GLenum stage) { return stage==GL_VERTEX_SHADER ? "vertex" : "fragment"; }
std::filesystem::path GetCacheDirectory() { return Resources::CacheDirectory(); }
std::string CacheKey(GLenum stage,const std::string& source)
{
    // Stable across processes and OSes; filenames alone cannot identify cached source.
    const std::string input="hazel-pipeline-v3-validated-cache-debugnames-glsl410-vulkan12-74a79040df6d52c4667ed87c9c70904c7561aef8d61f1fb981c947ba76a5fdec-"+std::to_string(stage)+source;
    uint64_t hash=14695981039346656037ull;
    for (unsigned char byte:input) { hash^=byte; hash*=1099511628211ull; }
    std::ostringstream output; output<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;
    return output.str();
}
uint64_t CacheChecksum(const void* data, std::size_t bytes)
{
    uint64_t hash=14695981039346656037ull;
    const auto* input=static_cast<const unsigned char*>(data);
    for (std::size_t i=0;i<bytes;++i) { hash^=input[i]; hash*=1099511628211ull; }
    return hash;
}
bool ReadCache(const std::filesystem::path& path,std::vector<uint32_t>& words,spv_target_env environment)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if (!input) return false;
    words.clear();
    const auto size=static_cast<std::streamoff>(input.tellg());
    // Private cache container: magic/version/count, payload checksum and cache
    // identity checksum. Validate before reflection or passing data to a driver.
    if (size<52 || size%4 || size>128*1024*1024) return false;
    uint32_t header[8]{};
    input.seekg(0); input.read(reinterpret_cast<char*>(header),sizeof(header));
    const auto name=path.filename().generic_u8string();
    const auto identity=CacheChecksum(name.data(),name.size());
    if (!input || header[0]!=0x485a5343u || header[1]!=1 || header[2]!=(size-32)/4 || header[3]!=0 ||
        (uint64_t(header[6]) | (uint64_t(header[7])<<32))!=identity) return false;
    words.resize(header[2]);
    input.read(reinterpret_cast<char*>(words.data()),size-32);
    const auto checksum=CacheChecksum(words.data(),words.size()*4);
    if (!input || words.front()!=0x07230203u ||
        (uint64_t(header[4]) | (uint64_t(header[5])<<32))!=checksum ||
        !spvtools::SpirvTools(environment).Validate(words)) {
        words.clear();
        HZ_CORE_WARN("Ignoring corrupted shader cache '{}'",path.generic_u8string());
        return false;
    }
    return true;
}
void WriteCache(const std::filesystem::path& path,const std::vector<uint32_t>& words)
{
    const auto checksum=CacheChecksum(words.data(),words.size()*4);
    const auto name=path.filename().generic_u8string();
    const auto identity=CacheChecksum(name.data(),name.size());
    const uint32_t header[8]={0x485a5343u,1,static_cast<uint32_t>(words.size()),0,
        static_cast<uint32_t>(checksum),static_cast<uint32_t>(checksum>>32),
        static_cast<uint32_t>(identity),static_cast<uint32_t>(identity>>32)};
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    if (!output) { HZ_CORE_WARN("Could not write shader cache '{}'",path.generic_u8string()); return; }
    output.write(reinterpret_cast<const char*>(header),sizeof(header));
    output.write(reinterpret_cast<const char*>(words.data()),static_cast<std::streamsize>(words.size()*4));
    if (!output) HZ_CORE_WARN("Incomplete shader cache write '{}'",path.generic_u8string());
}
bool UsesTargetPipeline(const std::unordered_map<GLenum,std::string>& sources)
{
    for (const auto& entry:sources) {
        // Select from an actual directive, never a comment mentioning a version.
        bool comment=false;
        std::string visible;
        for (std::size_t i=0;i<entry.second.size();++i) {
            const char c=entry.second[i];
            const char next=i+1<entry.second.size() ? entry.second[i+1] : '\0';
            if (comment) {
                if (c=='*' && next=='/') { comment=false; ++i; }
                else if (c=='\n') visible+='\n';
            } else if (c=='/' && next=='*') { comment=true; visible+=' '; ++i; }
            else if (c=='/' && next=='/') {
                while (i<entry.second.size() && entry.second[i]!='\n') ++i;
                visible+='\n';
            } else visible+=c;
        }
        std::istringstream lines(visible); std::string line; int number=0;
        while (std::getline(lines,line)) {
            const auto first=line.find_first_not_of(" \t\r");
            if (first==std::string::npos || line[first]!='#') continue;
            std::istringstream directive(line.substr(first+1)); std::string name;
            directive>>name;
            if (name=="version") { directive>>number; break; }
        }
        // Existing GLSL 330/420 shaders retain their default-uniform/direct-compile API.
        if (number<450) return false;
    }
    return true;
}
}

OpenGLShader::OpenGLShader(const std::string& filepath):m_FilePath(filepath)
{
    HZ_PROFILE_FUNCTION();
    m_Name=std::filesystem::u8path(filepath).stem().u8string();
    CompilePipeline(PreProcess(ReadFile(filepath)));
}
OpenGLShader::OpenGLShader(const std::string& name,const std::string& vertexSrc,const std::string& fragmentSrc)
    :m_FilePath(name),m_Name(name)
{
    HZ_PROFILE_FUNCTION();
    CompilePipeline({{GL_VERTEX_SHADER,vertexSrc},{GL_FRAGMENT_SHADER,fragmentSrc}});
}
OpenGLShader::~OpenGLShader()
{
    HZ_PROFILE_FUNCTION();
    if (m_RendererID) glDeleteProgram(m_RendererID);
}
std::string OpenGLShader::ReadFile(const std::string& filepath)
{
    auto contents=FileSystem::ReadFileBinary(std::filesystem::u8path(filepath));
    if (!contents) throw std::runtime_error("Could not read shader file: "+filepath);
    return {reinterpret_cast<const char*>(contents.Data),static_cast<std::size_t>(contents.Size)};
}
void OpenGLShader::CompilePipeline(const std::unordered_map<GLenum,std::string>& sources)
{
    if (!UsesTargetPipeline(sources)) { CompileLegacy(sources); return; }
    auto specialized=sources;
    // The actual target quad shader retains all 32 cases. Specialize its declared
    // capacity here before shaderc/cache hashing, matching the renderer's limit.
    const std::string declaration="#define HZ_MAX_TEXTURE_SLOTS 32";
    for (auto& [stage,source]:specialized) {
        const auto position=source.find(declaration);
        if (position!=std::string::npos) {
            const auto limit=OpenGLCapabilities::GetSettings().TextureSlots;
            source.replace(position,declaration.size(),"#define HZ_MAX_TEXTURE_SLOTS "+std::to_string(limit));
            HZ_CORE_TRACE("Shader {} texture capacity: {}",m_Name,limit);
        }
    }
    std::error_code error;
    std::filesystem::create_directories(GetCacheDirectory(),error);
    if (error) HZ_CORE_WARN("Shader cache unavailable: {}",error.message());
    Timer timer;
    CompileOrGetVulkanBinaries(specialized);
    CompileOrGetOpenGLBinaries();
    try { CreateProgram(); }
    catch (...) { if (m_RendererID) glDeleteProgram(m_RendererID); m_RendererID=0; throw; }
    HZ_CORE_TRACE("Shader creation took {} ms",timer.ElapsedMillis());
}
void OpenGLShader::CompileOrGetVulkanBinaries(const std::unordered_map<GLenum,std::string>& sources)
{
    shaderc::Compiler compiler;
    if (!compiler.IsValid()) throw std::runtime_error("Could not initialize shaderc");
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan,shaderc_env_version_vulkan_1_2);
    // shaderc's performance profile otherwise strips names needed for GLSL resource binding.
    options.SetGenerateDebugInfo();
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    for (const auto& [stage,source]:sources) {
        m_CacheKeys[stage]=CacheKey(stage,source);
        const auto path=GetCacheDirectory()/(m_CacheKeys[stage]+".cached_vulkan."+GLShaderStageToString(stage));
        auto& data=m_VulkanSPIRV[stage];
        if (ReadCache(path,data,SPV_ENV_VULKAN_1_2)) HZ_CORE_TRACE("Shader cache hit: {}",path.generic_u8string());
        else {
            auto module=compiler.CompileGlslToSpv(source,GLShaderStageToShaderC(stage),m_FilePath.c_str(),options);
            if (module.GetCompilationStatus()!=shaderc_compilation_status_success) {
                HZ_CORE_ERROR("Shader compilation failed ({}): {}",m_FilePath,module.GetErrorMessage());
                throw std::runtime_error(module.GetErrorMessage());
            }
            data.assign(module.cbegin(),module.cend());
            WriteCache(path,data);
        }
        Reflect(stage,data);
    }
}
void OpenGLShader::CompileOrGetOpenGLBinaries()
{
    shaderc::Compiler compiler;
    if (!compiler.IsValid()) throw std::runtime_error("Could not initialize shaderc");
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_opengl,shaderc_env_version_opengl_4_5);
    for (const auto& [stage,spirv]:m_VulkanSPIRV) {
        spirv_cross::CompilerGLSL glslCompiler(spirv);
        auto glslOptions=glslCompiler.get_common_options();
        glslOptions.version=410; glslOptions.es=false; glslOptions.enable_420pack_extension=false;
        glslCompiler.set_common_options(glslOptions);
        // Always generate source, including warm cache hits: 4.1/4.2 cannot load SPIR-V.
        m_OpenGLSourceCode[stage]=glslCompiler.compile();
        const auto path=GetCacheDirectory()/(m_CacheKeys.at(stage)+".cached_opengl."+GLShaderStageToString(stage));
        auto& data=m_OpenGLSPIRV[stage];
        if (ReadCache(path,data,SPV_ENV_OPENGL_4_5)) HZ_CORE_TRACE("Shader cache hit: {}",path.generic_u8string());
        else {
            glslOptions.version=450; glslOptions.enable_420pack_extension=true;
            glslCompiler.set_common_options(glslOptions);
            auto source=glslCompiler.compile();
            auto module=compiler.CompileGlslToSpv(source,GLShaderStageToShaderC(stage),m_FilePath.c_str(),options);
            if (module.GetCompilationStatus()!=shaderc_compilation_status_success) {
                HZ_CORE_ERROR("OpenGL SPIR-V compilation failed ({}): {}",m_FilePath,module.GetErrorMessage());
                throw std::runtime_error(module.GetErrorMessage());
            }
            data.assign(module.cbegin(),module.cend());
            WriteCache(path,data);
        }
    }
}
void OpenGLShader::CreateProgram()
{
    // 4.6 core provides the generated loader's specialization entry point.
    // Extension-only or older contexts use equivalent generated GLSL semantics.
    if (!OpenGLCapabilities::UseShaderBinaries()) {
        CompileLegacy(m_OpenGLSourceCode);
        ApplyResourceBindings();
        m_LoadingPath=ProgramLoadingPath::GeneratedGLSL;
        HZ_CORE_TRACE("Shader {} loaded through GLSL 410 fallback",m_Name);
        return;
    }
    const GLuint program=glCreateProgram();
    if (!program) throw std::runtime_error("OpenGL program creation failed");
    std::vector<GLuint> shaders;
    try {
        for (const auto& [stage,spirv]:m_OpenGLSPIRV) {
            GLuint shader=glCreateShader(stage);
            if (!shader) throw std::runtime_error("OpenGL shader creation failed");
            shaders.push_back(shader);
            glShaderBinary(1,&shader,GL_SHADER_BINARY_FORMAT_SPIR_V,spirv.data(),static_cast<GLsizei>(spirv.size()*4));
            glSpecializeShader(shader,"main",0,nullptr,nullptr);
            GLint compiled=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&compiled);
            if (!compiled) throw std::runtime_error(GetShaderLog(shader));
            glAttachShader(program,shader);
        }
        glLinkProgram(program); GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked);
        if (!linked) throw std::runtime_error(GetProgramLog(program));
        for (GLuint shader:shaders) { glDetachShader(program,shader); glDeleteShader(shader); }
        m_RendererID=program;
        m_LoadingPath=ProgramLoadingPath::SPIRV;
        HZ_CORE_TRACE("Shader {} loaded through native OpenGL SPIR-V specialization",m_Name);
    } catch (const std::exception& error) {
        HZ_CORE_ERROR("SPIR-V program creation failed ({}): {}",m_FilePath,error.what());
        glDeleteProgram(program); for (GLuint shader:shaders) glDeleteShader(shader);
        throw;
    }
}
void OpenGLShader::ApplyResourceBindings()
{
    for (const auto& [name,binding]:m_UniformBufferBindings) {
        GLuint index=glGetUniformBlockIndex(m_RendererID,name.c_str());
        if (index!=GL_INVALID_INDEX) glUniformBlockBinding(m_RendererID,index,binding);
    }
    for (const auto& [name,binding]:m_SamplerBindings) {
        GLint location=glGetUniformLocation(m_RendererID,name.c_str());
        if (location<0) continue;
        // Binding and array extent were reflected from the resource's own stage.
        const uint32_t count=m_SamplerCounts.at(name);
        std::vector<GLint> units(count);
        for (uint32_t i=0;i<count;++i) units[i]=static_cast<GLint>(binding+i);
        // Program uniform updates are core in 4.1 and preserve the current program.
        glProgramUniform1iv(m_RendererID,location,static_cast<GLsizei>(count),units.data());
    }
}
void OpenGLShader::Reflect(GLenum stage,const std::vector<uint32_t>& shaderData)
{
    spirv_cross::Compiler compiler(shaderData);
    const auto resources=compiler.get_shader_resources();
    HZ_CORE_TRACE("OpenGLShader::Reflect - {} {}: {} uniform buffers, {} sampled images",
                  GLShaderStageToString(stage),m_FilePath,resources.uniform_buffers.size(),resources.sampled_images.size());
    for (const auto& resource:resources.uniform_buffers) {
        const auto& type=compiler.get_type(resource.base_type_id);
        const uint32_t binding=compiler.get_decoration(resource.id,spv::DecorationBinding);
        auto name=compiler.get_name(resource.base_type_id);
        if (name.empty()) name=resource.name;
        const auto entry=m_UniformBufferBindings.emplace(name,binding);
        if (!entry.second && entry.first->second!=binding) throw std::runtime_error("Inconsistent shader UBO bindings");
        HZ_CORE_TRACE("  {}: size {}, binding {}, members {}",name,compiler.get_declared_struct_size(type),binding,type.member_types.size());
    }
    for (const auto& resource:resources.sampled_images) {
        const uint32_t binding=compiler.get_decoration(resource.id,spv::DecorationBinding);
        uint32_t count=1;
        const auto& type=compiler.get_type(resource.type_id);
        for (std::size_t i=0;i<type.array.size();++i) {
            if (!type.array_size_literal[i] || !type.array[i]) throw std::runtime_error("Unsized/specialized sampler arrays need an explicit binding adaptation");
            count*=type.array[i];
        }
        const auto extent=m_SamplerCounts.emplace(resource.name,count);
        if (!extent.second && extent.first->second!=count) throw std::runtime_error("Inconsistent sampler array extents");
        const auto entry=m_SamplerBindings.emplace(resource.name,binding);
        if (!entry.second && entry.first->second!=binding) throw std::runtime_error("Inconsistent sampler bindings");
    }
}

    std::unordered_map<std::uint32_t, std::string>
    OpenGLShader::PreProcess(const std::string& source)
    {
        HZ_PROFILE_FUNCTION();
        std::unordered_map<std::uint32_t, std::string> shaderSources;
        const std::string token = "#type";
        std::size_t pos = source.find(token);
        while (pos != std::string::npos)
        {
            const std::size_t eol = source.find_first_of("\r\n", pos);
            if (eol == std::string::npos)
                throw std::runtime_error("Shader #type line has no newline");
            std::string type = source.substr(pos + token.size(), eol - pos - token.size());
            const std::size_t begin = type.find_first_not_of(" \t");
            const std::size_t end = type.find_last_not_of(" \t");
            if (begin == std::string::npos)
                throw std::runtime_error("Shader #type has no stage name");
            type = type.substr(begin, end - begin + 1);
            std::uint32_t shaderType = 0;
            if (type == "vertex")
                shaderType = GL_VERTEX_SHADER;
            else if (type == "fragment" || type == "pixel")
                shaderType = GL_FRAGMENT_SHADER;
            else
                throw std::runtime_error("Unknown shader stage: " + type);
            const std::size_t nextLine =
                source.find_first_not_of("\r\n", eol);
            if (nextLine == std::string::npos)
                throw std::runtime_error("Shader stage has no source: " + type);
            const std::size_t nextType = source.find(token, nextLine);
            const std::string stageSource =
                source.substr(nextLine, nextType == std::string::npos ? std::string::npos : nextType - nextLine);
            if (!shaderSources.emplace(shaderType, stageSource).second)
                throw std::runtime_error("Duplicate shader stage: " + type);
            pos = nextType;
        }
        if (shaderSources.size() != 2 || shaderSources.count(GL_VERTEX_SHADER) == 0 || shaderSources.count(GL_FRAGMENT_SHADER) == 0)
        {
            throw std::runtime_error("Shader requires one vertex and one fragment stage");
        }
        return shaderSources;
    }

    void OpenGLShader::CompileLegacy(const std::unordered_map<std::uint32_t, std::string>& shaderSources)
    {
        HZ_PROFILE_FUNCTION();
        if (shaderSources.size() != 2 || shaderSources.count(GL_VERTEX_SHADER) == 0 || shaderSources.count(GL_FRAGMENT_SHADER) == 0)
        {
            throw std::runtime_error("Shader requires one vertex and one fragment stage");
        }
        const unsigned int program = glCreateProgram();
        if (program == 0)
            throw std::runtime_error("OpenGL program creation failed...");
        std::vector<unsigned int> shaders;
        shaders.reserve(2);
        try
        {
            for (const auto& [type, source] : shaderSources)
            {
                const unsigned int shader = CompileShader(type, source, type == GL_VERTEX_SHADER ? "Vertex" : "Fragment");
                shaders.push_back(shader);
                glAttachShader(program, shader);
            }
            glLinkProgram(program);
            int linked = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked == GL_FALSE)
            {
                HZ_CORE_ERROR("Shader program link failed:\n{}", GetProgramLog(program));
                throw std::runtime_error("OpenGL shader program link failed...");
            }
            for (unsigned int shader : shaders)
            {
                glDetachShader(program, shader);
                glDeleteShader(shader);
            }
            m_RendererID = program;
        }
        catch (...)
        {
            glDeleteProgram(program);
            for (unsigned int shader : shaders)
                glDeleteShader(shader);
            throw;
        }
    }


	void OpenGLShader::Bind() const
	{
		HZ_PROFILE_FUNCTION();

		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		HZ_PROFILE_FUNCTION();

		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
		HZ_PROFILE_FUNCTION();

		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, const int* values, uint32_t count)
	{
		UploadUniformIntArray(name, values, count);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value)
	{
		HZ_PROFILE_FUNCTION();

		UploadUniformFloat(name, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
	{
		HZ_PROFILE_FUNCTION();

		UploadUniformFloat2(name, value);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{
		HZ_PROFILE_FUNCTION();

		UploadUniformFloat3(name, value);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{
		HZ_PROFILE_FUNCTION();

		UploadUniformFloat4(name, value);
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
	{
		HZ_PROFILE_FUNCTION();

		UploadUniformMat4(name, value);
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}

	void OpenGLShader::UploadUniformIntArray(const std::string& name, const int* values, uint32_t count)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1iv(location, count, values);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform2f(location, value.x, value.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, value.x, value.y, value.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

}
