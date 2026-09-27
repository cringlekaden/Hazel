#pragma once

#include "Hazel/Renderer/Shader.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Hazel {

    class OpenGLShader : public Shader
    {
    public:
        explicit OpenGLShader(const std::string& filepath);

        OpenGLShader(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource);

        ~OpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;

        void SetInt(const std::string& name, int value) override;
        void SetFloat3(const std::string& name, const glm::vec3& value) override;
        void SetFloat4(const std::string& name, const glm::vec4& value) override;
        void SetMat4(const std::string& name, const glm::mat4& value) override;

        const std::string& GetName() const override
        {
            return m_Name;
        }

        void UploadUniformInt(
            const std::string& name, int value);
        void UploadUniformFloat(
            const std::string& name, float value);
        void UploadUniformFloat2(
            const std::string& name,
            const glm::vec2& value);
        void UploadUniformFloat3(
            const std::string& name,
            const glm::vec3& value);
        void UploadUniformFloat4(
            const std::string& name,
            const glm::vec4& value);
        void UploadUniformMat3(
            const std::string& name,
            const glm::mat3& matrix);
        void UploadUniformMat4(
            const std::string& name,
            const glm::mat4& matrix);
    private:
        static std::string ReadFile(const std::string& filepath);
        static std::unordered_map<std::uint32_t, std::string>PreProcess(const std::string& source);

        void Compile(const std::unordered_map<std::uint32_t, std::string>& shaderSources);

        std::uint32_t m_RendererID = 0;
        std::string m_Name;
    };
}