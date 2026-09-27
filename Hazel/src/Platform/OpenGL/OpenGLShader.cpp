#include "hzpch.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Hazel/Log.h"

#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

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
    }

    OpenGLShader::OpenGLShader(const std::string& filepath)
    {
        const std::string source = ReadFile(filepath);
        const auto shaderSources = PreProcess(source);
        Compile(shaderSources);
        const std::size_t lastSlash = filepath.find_last_of("/\\");
        const std::size_t start = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        const std::size_t lastDot = filepath.find_last_of('.');
        const std::size_t end = lastDot != std::string::npos && lastDot > start ? lastDot : filepath.size();
        m_Name = filepath.substr(start, end - start);
    }

    OpenGLShader::OpenGLShader(
        const std::string& name,
        const std::string& vertexSource,
        const std::string& fragmentSource) : m_Name(name)
    {
        Compile({{ GL_VERTEX_SHADER, vertexSource }, { GL_FRAGMENT_SHADER, fragmentSource }});
    }

    std::string OpenGLShader::ReadFile(const std::string& filepath)
    {
        std::ifstream input(filepath, std::ios::in | std::ios::binary);
        if (!input)
        {
            HZ_CORE_ERROR("Could not open shader file '{}'", filepath);
            throw std::runtime_error("Could not open shader file: " + filepath);
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        if (input.bad())
            throw std::runtime_error("Could not read shader file: " + filepath);
        return contents.str();
    }

    std::unordered_map<std::uint32_t, std::string>
    OpenGLShader::PreProcess(const std::string& source)
    {
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
            else if (type == "fragment")
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

    void OpenGLShader::Compile(const std::unordered_map<std::uint32_t, std::string>& shaderSources)
    {
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

    OpenGLShader::~OpenGLShader()
    {
        if (m_RendererID != 0)
            glDeleteProgram(m_RendererID);
    }

    void OpenGLShader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::Unbind() const
    {
        glUseProgram(0);
    }

    void OpenGLShader::UploadUniformInt(const std::string& name, int value)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform1i(location, value);
    }

    void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform1f(location, value);
    }

    void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform2f(location, value.x, value.y);
    }

    void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform3f(location, value.x, value.y, value.z);
    }

    void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }

    void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
    {
        const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}