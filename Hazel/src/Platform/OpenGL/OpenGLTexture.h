// Adapted from TheCherno/Hazel 1feb705 for OpenGL 4.1/4.2 and preserved ownership/error handling.
#pragma once

#include "Hazel/Renderer/Texture.h"

#include <glad/glad.h>

namespace Hazel {

	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(const TextureSpecification& specification);
		OpenGLTexture2D(const std::string& path, const TextureSpecification& specification = TextureSpecification::FileDefaults());
		virtual ~OpenGLTexture2D();

		virtual const TextureSpecification& GetSpecification() const override { return m_Specification; }

		virtual uint32_t GetWidth() const override { return m_Width;  }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual uint32_t GetRendererID() const override { return m_RendererID; }

		virtual const std::string& GetPath() const override { return m_Path; }

		virtual void SetData(const void* data, uint32_t size) override;

		virtual void Bind(uint32_t slot = 0) const override;

		virtual bool IsLoaded() const override { return m_IsLoaded; }

		virtual bool operator==(const Texture& other) const override
		{
			const auto* texture = dynamic_cast<const OpenGLTexture2D*>(&other);
            return texture && m_RendererID == texture->m_RendererID;
		}
	private:
		TextureSpecification m_Specification;

		std::string m_Path;
		bool m_IsLoaded = false;
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_RendererID = 0;
		GLenum m_InternalFormat = 0, m_DataFormat = 0;
        GLenum m_DataType = GL_UNSIGNED_BYTE;
        uint32_t m_BytesPerPixel = 0;
        void AllocateStorage(const void* data);
	};

}
