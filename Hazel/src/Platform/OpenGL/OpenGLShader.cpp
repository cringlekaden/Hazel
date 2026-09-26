#include "hzpch.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Hazel/Log.h"

#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

#include <cstddef>
#include <stdexcept>
#include <string>

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

    OpenGLShader::OpenGLShader(const std::string& vertexSource, const std::string& fragmentSource)
    {
        unsigned int vertexShader = 0;
        unsigned int fragmentShader = 0;
        unsigned int program = 0;
        try
        {
            vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, "Vertex");
            fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment");
            program = glCreateProgram();
            if (program == 0)
            {
                HZ_CORE_ERROR("Failed to create OpenGL shader program...");
                throw std::runtime_error("OpenGL program creation failed...");
            }
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glLinkProgram(program);
            int linked = GL_FALSE;
            glGetProgramiv(
                program, GL_LINK_STATUS, &linked);
            if (linked == GL_FALSE)
            {
                HZ_CORE_ERROR("Shader program link failed:\n{}", GetProgramLog(program));
                throw std::runtime_error(
                    "OpenGL shader program link failed...");
            }
            glDetachShader(program, vertexShader);
            glDetachShader(program, fragmentShader);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            vertexShader = 0;
            fragmentShader = 0;
            m_RendererID = program;
        }
        catch (...)
        {
            if (program != 0)
                glDeleteProgram(program);
            if (fragmentShader != 0)
                glDeleteShader(fragmentShader);
            if (vertexShader != 0)
                glDeleteShader(vertexShader);
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