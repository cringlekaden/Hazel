#include "hzpch.h"

#include "Hazel/Renderer/Resources/Shader.h"
#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#endif

namespace Hazel {

    Ref<Shader> Shader::Create(const std::string& filepath)
    {
        switch (Renderer::GetAPI())
        {
            case RendererAPI::API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is invalid...");
                return nullptr;

#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal:
            {
                const auto source = std::filesystem::absolute(std::filesystem::u8path(filepath)).lexically_normal();
                const auto name = source.stem().u8string();
                if (source != Resources::Resolve("shaders/" + name + ".glsl").lexically_normal())
                    throw std::invalid_argument("Metal shaders require canonical engine GLSL resource references");
                return static_cast<MetalRendererAPI&>(RenderCommand::Backend()).LoadShader(name);
            }
#endif
            case RendererAPI::API::OpenGL:
                return CreateRef<OpenGLShader>(filepath);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI...");
        return nullptr;
    }

    Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSource, const std::string& fragmentSource)
    {
        switch (Renderer::GetAPI())
        {
            case RendererAPI::API::None:
                HZ_CORE_ASSERT(
                    false,
                    "RendererAPI::None is invalid...");
                return nullptr;

#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal:
                throw std::runtime_error("Metal shaders require offline .hzmetal resources; inline source compilation is unavailable");
#endif
            case RendererAPI::API::OpenGL:
                return CreateRef<OpenGLShader>(name, vertexSource, fragmentSource);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI...");
        return nullptr;
    }

    void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
    {
        HZ_CORE_ASSERT(!Exists(name), "Shader: {} already exists...", name);
        m_Shaders[name] = shader;
    }

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        Add(shader->GetName(), shader);
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
    {
        Ref<Shader> shader = Shader::Create(filepath);
        Add(shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
    {
        Ref<Shader> shader = Shader::Create(filepath);
        Add(name, shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& name)
    {
        HZ_CORE_ASSERT(Exists(name), "Shader: {} not found...", name);
        return m_Shaders.at(name);
    }

    bool ShaderLibrary::Exists(const std::string& name) const
    {
        return m_Shaders.find(name) != m_Shaders.end();
    }
}