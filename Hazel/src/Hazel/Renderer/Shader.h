#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Hazel {

    class Shader
    {
    public:
        Shader(const std::string& vertexSource, const std::string& fragmentSource);
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void Bind() const;
        void Unbind() const;
        void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
    private:
        std::uint32_t m_RendererID = 0;
    };
}