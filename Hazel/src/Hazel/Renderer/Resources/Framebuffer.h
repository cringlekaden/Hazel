// Adapted from TheCherno/Hazel 1feb705 for OpenGL 4.1/4.2 and preserved ownership/error handling.
#pragma once

#include "Hazel/Core/Base.h"
#include "EditorTexture.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <initializer_list>
#include <vector>
#include <string>

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
        DEPTH32F,

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

    // Render-thread polling publishes CPU-owned data only after GPU completion.
    // Handles remain valid independently of framebuffer resize/destruction.
    struct FramebufferReadback
    {
        uint64_t Serial = 0;
        uint32_t Width = 0, Height = 0;
        bool Completed = false;
        std::string Error;
        std::vector<glm::vec4> Color;
        std::vector<int32_t> Entity;
        std::vector<float> Depth;
    };

    class ComputePipeline;
	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
        // Explicit asynchronous offscreen pass boundary. Unsupported backends throw.
        // Bind starts a clear/store pass; EndScene then Unbind precede Submit.
        virtual void Clear(glm::vec4 color);
        virtual void ClearEditor(glm::vec4 color);
        // Captures an unbound producer into an empty, equal-size FP32 destination.
        // IDs/depth are preserved; a later render pass or Submit consumes the result.
        virtual void DispatchColorTo(const Ref<Framebuffer>& destination, const Ref<ComputePipeline>& pipeline, const std::vector<uint8_t>& parameters);
        virtual Ref<const FramebufferReadback> Submit(); // null: busy; retry unchanged packet
        virtual void Poll(); // nonblocking; readback fields publish on the render thread
        virtual void Cancel(); // discard unsubmitted work; submitted work is never aborted
        virtual float ReadDepthPixel(int x, int y);
        // Display-linear attachment 0 -> encoded RGBA8 output (or window).
        // Does not touch entity IDs. ImGui samples the encoded output unchanged.
        virtual void EncodeOutput(const Ref<Framebuffer>& destination = {}) = 0;
        // Optional editor presentation operation: reads depth-resolved ID coverage,
        // writes only a separate encoded RGBA8 output. Radius is framebuffer pixels.
        virtual void DrawEntityOutlineTo(const Ref<Framebuffer>& destination,int entity,int radius,glm::vec4 encodedColor) = 0;
        virtual void CompositeWorldTo(const Ref<Framebuffer> &destination, float exposure) = 0;
        virtual glm::vec4 ReadColorPixel(uint32_t attachment, int x, int y) = 0;
        virtual void ResetEditorReadback() {}
        virtual int ReadEditorEntity(int x, int y) { return ReadPixel(1, x, y); }
        virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;

        virtual EditorTexture GetEditorTexture() const { return {uintptr_t(GetColorAttachmentRendererID()), {}, true}; }

		virtual const FramebufferSpecification& GetSpecification() const = 0;

        static Ref<Framebuffer> CreateEditor(const FramebufferSpecification& spec);
		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};


}
