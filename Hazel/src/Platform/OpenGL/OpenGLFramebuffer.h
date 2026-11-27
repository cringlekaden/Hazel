// Adapted from TheCherno/Hazel 1feb705 for OpenGL 4.1/4.2 and preserved ownership/error handling.
#pragma once

#include "Hazel/Renderer/Resources/Framebuffer.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"
#include <array>
#include <utility>

namespace Hazel {

	class Shader;
	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		void Invalidate();

		virtual void Bind() override;
		virtual void Unbind() override;
        void PrepareShadowDepth(const Ref<Framebuffer>& depth, ShadowMapKind kind) override;

		virtual void EncodeOutput(const Ref<Framebuffer>& destination = {}) override;
        virtual void Resize(uint32_t width, uint32_t height) override;
        virtual void DrawEntityOutlineTo(const Ref<Framebuffer>& destination,int entity,int radius,glm::vec4 encodedColor) override;
        void ResolveReflectionsTo(const Ref<Framebuffer>& destination, const ScreenReflectionResolve& parameters) override;
        void CompositeWorldTo(const Ref<Framebuffer> &destination, float exposure, const BloomPyramid* bloom = nullptr, const ExposureMeterPass* meter = nullptr) override;
        std::vector<glm::vec4> ReadColorPixels() override;
        virtual glm::vec4 ReadColorPixel(uint32_t attachment, int x, int y) override;
        virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override;

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }
	private:
        uint32_t MeterExposure(const ExposureMeterPass& meter);
        uint32_t FilterBloom(const BloomPyramid& bloom, bool timePasses);
        void ReadFloatColors(uint32_t attachment, int x, int y, int width, int height, glm::vec4 *pixels);
      Ref<Shader> m_OutputShader, m_BloomShader, m_ToneShader, m_OutlineShader, m_ReflectionShader;
      Ref<Shader> m_ExposureHistogramShader, m_ExposureAdaptShader;
      Ref<UniformBuffer> m_ExposureParameters;
      Ref<UniformBuffer> m_BloomParameters, m_ToneParameters, m_ReflectionParameters;
      std::array<uint32_t, 4> m_BloomQueries{};
      bool m_BloomQueriesPending = false;
      uint32_t m_OutputVAO = 0;
      void Release();
      void DisableIntegerBlending();
      void RestoreBlending();
      std::vector<std::pair<uint32_t, bool>> m_PreviousBlending;
      void Resolve() const;
      uint32_t m_ResolveRendererID = 0;
      std::vector<uint32_t> m_ResolveColorAttachments;
      uint32_t m_RendererID = 0;
      FramebufferSpecification m_Specification;

      std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
      FramebufferTextureSpecification m_DepthAttachmentSpecification =
          FramebufferTextureFormat::None;

      std::vector<uint32_t> m_ColorAttachments;
      uint32_t m_DepthAttachment = 0;
	};

}
