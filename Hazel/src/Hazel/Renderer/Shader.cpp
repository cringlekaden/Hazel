#include "hzpch.h"

#include "Hazel/Renderer/Shader.h"
#include "Hazel/Log.h"

#include <cstddef>
#include <glad/glad.h>
#include <stdexcept>
#include <string>

namespace Hazel {

    namespace {

        std::string GetShaderLog(unsigned int shader)
        {
            int length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if(length <= 1)
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
            if(length <= 1)
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
            if(shader == 0)
            {
                HZ_CORE_ERROR("Failed to create {} shader...", stageName);
                throw std::runtime_error("OpenGL shader creation failed...");
            }
            const char* sourceText = source.c_str();
            glShaderSource(shader, 1, &sourceText, nullptr);
            glCompileShader(shader);
            int compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if(compiled == GL_FALSE)
            {
                const std::string log = GetShaderLog(shader);
                glDeleteShader(shader);
                HZ_CORE_ERROR("{} shader compilation failed:\n{}", stageName, log);
                throw std::runtime_error("OpenGL shader compilation failed...");
            }
            return shader;
        }
    }

    Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource)
    {
        unsigned int vertexShader = 0;
        unsigned int fragmentShader = 0;
        unsigned int program = 0;
        try {
            vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, "Vertex");
            fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment");
            program = glCreateProgram();
            if(program == 0)
            {
                HZ_CORE_ERROR("Failed to create OpenGL shader program...");
                throw std::runtime_error("OpenGL program creation failed...");
            }
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glLinkProgram(program);
            int linked = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if(linked == GL_FALSE)
            {
                HZ_CORE_ERROR("Shader program link failed:\n{}", GetProgramLog(program));
                throw std::runtime_error("OpenGL shader program link failed...");
            }
            glDetachShader(program, vertexShader);
            glDetachShader(program, fragmentShader);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            vertexShader = 0;
            fragmentShader = 0;
            m_RendererID = program;
        } catch (...) {
            if(program != 0)
                glDeleteProgram(program);
            if(fragmentShader != 0)
                glDeleteShader(fragmentShader);
            if(vertexShader != 0)
                glDeleteShader(vertexShader);
            throw;
        }
    }

    Shader::~Shader()
    {
        if(m_RendererID != 0)
            glDeleteProgram(m_RendererID);
    }

    void Shader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void Shader::Unbind() const
    {
        glUseProgram(0);
    }
}