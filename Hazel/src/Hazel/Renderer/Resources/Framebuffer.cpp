// Adapted from TheCherno/Hazel 1feb705 for OpenGL 4.1/4.2 and preserved ownership/error handling.
#include "hzpch.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"

#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Renderer/RenderCommand.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Hazel/Renderer/RenderCommand.h"
#endif
#include <stdexcept>

namespace Hazel {
    namespace { [[noreturn]] void AsyncUnavailable() { throw std::logic_error("Explicit asynchronous framebuffer operation is unavailable on this backend"); } }
    void Framebuffer::DispatchColorTo(const Ref<Framebuffer>&, const Ref<ComputePipeline>&, const std::vector<uint8_t>&) { AsyncUnavailable(); }
    void Framebuffer::ClearEditor(glm::vec4 color) { RenderCommand::SetClearColor(color); RenderCommand::Clear(); }
    void Framebuffer::Clear(glm::vec4) { AsyncUnavailable(); }
    Ref<const FramebufferReadback> Framebuffer::Submit() { AsyncUnavailable(); }
    void Framebuffer::Poll() { AsyncUnavailable(); }
    void Framebuffer::Cancel() { AsyncUnavailable(); }
    float Framebuffer::ReadDepthPixel(int, int) { AsyncUnavailable(); }


    Ref<Framebuffer> Framebuffer::CreateEditor(const FramebufferSpecification& specification) {
        auto native = specification;
        if (Renderer::GetAPI() == RendererAPI::API::Metal) {
            for (auto& attachment : native.Attachments.Attachments) {
                if (attachment.TextureFormat == FramebufferTextureFormat::RGBA16F) attachment.TextureFormat = FramebufferTextureFormat::RGBA32F;
                if (attachment.TextureFormat == FramebufferTextureFormat::Depth) attachment.TextureFormat = FramebufferTextureFormat::DEPTH32F;
            }
        }
        return Create(native);
    }

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLFramebuffer>(spec);
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal:
                return static_cast<MetalRendererAPI&>(RenderCommand::Backend()).CreateFramebuffer(spec);
#endif
		}

		HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
