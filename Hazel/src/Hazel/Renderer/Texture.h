// Adapted from TheCherno/Hazel 1feb705 for OpenGL 4.1/4.2 and preserved ownership/error handling.
#pragma once

#include "Hazel/Core/Base.h"

#include <string>
#include <cstdint>
#include <vector>
#include <filesystem>

namespace Hazel {

	enum class ImageFormat
	{
		None = 0,
		R8,
		RGB8,
		RGBA8,
		RGBA32F
	};

	enum class TextureFilter { Nearest, Linear, NearestMipmapNearest, LinearMipmapNearest, NearestMipmapLinear, LinearMipmapLinear };
	enum class TextureWrap { Repeat, ClampToEdge, MirroredRepeat };
	const char* TextureFilterName(TextureFilter value);
	const char* TextureWrapName(TextureWrap value);
	TextureFilter ParseTextureFilter(const std::string& value);
	TextureWrap ParseTextureWrap(const std::string& value);

	struct TextureSpecification
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		ImageFormat Format = ImageFormat::RGBA8;
		bool GenerateMips = true;
		TextureFilter MinFilter = TextureFilter::LinearMipmapLinear;
		TextureFilter MagFilter = TextureFilter::Nearest;
		TextureWrap WrapS = TextureWrap::Repeat, WrapT = TextureWrap::Repeat;
		void Validate(bool file = false) const;
		static TextureSpecification FileDefaults(); // zero dimensions/None format infer decoded values
	};

	// Shared file decode: top-left rows, linear UNORM bytes. No sRGB conversion.
	struct TextureImage {
		uint32_t Width = 0, Height = 0;
		ImageFormat Format = ImageFormat::None;
		std::vector<uint8_t> Pixels;
	};

	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual const TextureSpecification& GetSpecification() const = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		// File textures report their resolved UTF-8 load location; serializers derive asset-root references.
		virtual const std::string& GetPath() const = 0;

		virtual void SetData(const void* data, uint32_t size) = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual bool IsLoaded() const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(const TextureSpecification& specification);
        static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(const std::string& path);
		static Ref<Texture2D> Create(const std::string& path, const TextureSpecification& specification);
		static TextureImage ReadImage(const std::filesystem::path& path, ImageFormat format = ImageFormat::None);
	};

}
