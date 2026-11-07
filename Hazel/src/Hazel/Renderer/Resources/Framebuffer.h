// Adapted from TheCherno/Hazel 1feb705 for OpenGL 4.1/4.2 and preserved ownership/error handling.
#pragma once

#include "Hazel/Core/Base.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <initializer_list>
#include <vector>

namespace Hazel {

	enum class FramebufferTextureFormat
	{
		None = 0,

		// Color
		RGBA8,
        RGBA16F,
        RGBA32F,
        RED_INTEGER,

		// Depth/stencil
		DEPTH24STENCIL8,

		// Defaults
		Depth = DEPTH24STENCIL8
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: TextureFormat(format) {}

		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
		// TODO: filtering/wrap
	};

	struct FramebufferAttachmentSpecification
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: Attachments(attachments) {}

		std::vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		uint32_t Width = 0, Height = 0;
		FramebufferAttachmentSpecification Attachments;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
        // Display-linear attachment 0 -> encoded RGBA8 output (or window).
        // Does not touch entity IDs. ImGui samples the encoded output unchanged.
        virtual void EncodeOutput(const Ref<Framebuffer>& destination = {}) = 0;
        virtual void CompositeWorldTo(const Ref<Framebuffer> &destination, float exposure) = 0;
        virtual glm::vec4 ReadColorPixel(uint32_t attachment, int x, int y) = 0;
        virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};


}
