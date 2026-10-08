#include "hzpch.h"
#include "Font.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/FileSystem.h"
#include <limits>
#include <stdexcept>

#undef INFINITE
#include "msdf-atlas-gen.h"
#include "FontGeometry.h"
#include "GlyphGeometry.h"

#include "MSDFData.h"

namespace Hazel {

	Ref<Font> Font::s_DefaultFont;

	struct FontHandles
	{
		msdfgen::FreetypeHandle* Library = nullptr;
		msdfgen::FontHandle* Face = nullptr;
		~FontHandles()
		{
			if (Face) msdfgen::destroyFont(Face);
			if (Library) msdfgen::deinitializeFreetype(Library);
		}
	};

	template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
	static Ref<Texture2D> CreateAndCacheAtlas(const std::string& fontName, float fontSize, const std::vector<msdf_atlas::GlyphGeometry>& glyphs,
		const msdf_atlas::FontGeometry& fontGeometry, uint32_t width, uint32_t height)
	{
		msdf_atlas::GeneratorAttributes attributes;
		attributes.config.overlapSupport = true;
		attributes.scanlinePass = true;

		msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width, height);
		generator.setAttributes(attributes);
		generator.setThreadCount(2);
		generator.generate(glyphs.data(), (int)glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

		TextureSpecification spec;
		spec.Width = bitmap.width;
		spec.Height = bitmap.height;
		spec.Format = ImageFormat::RGB8;
		spec.GenerateMips = false; spec.MinFilter = TextureFilter::Linear;

		Ref<Texture2D> texture = Texture2D::Create(spec);
		texture->SetData(bitmap.pixels, bitmap.width * bitmap.height * 3);
		return texture;
	}

	Font::Font(const std::filesystem::path& filepath)
		: m_Data(CreateScope<MSDFData>())
	{
		// Read through native filesystem paths, then use the actual pinned memory API.
		// This preserves UTF-8 project paths on Windows without a common OS branch.
		auto encoded = FileSystem::ReadFileBinary(filepath);
		if (!encoded || encoded.Size > static_cast<uint64_t>(std::numeric_limits<int>::max()))
			throw std::runtime_error("Failed to read font: " + filepath.generic_u8string());
		FontHandles handles;
		handles.Library = msdfgen::initializeFreetype();
		if (!handles.Library) throw std::runtime_error("Failed to initialize FreeType");
		handles.Face = msdfgen::loadFontData(handles.Library, encoded.Data, static_cast<int>(encoded.Size));
		auto* font = handles.Face;
		if (!font)
		{
			HZ_CORE_ERROR("Failed to load font: {}", filepath.generic_u8string());
			throw std::runtime_error("Failed to load font: " + filepath.generic_u8string());
		}

		struct CharsetRange
		{
			uint32_t Begin, End;
		};

		// From imgui_draw.cpp
		static const CharsetRange charsetRanges[] =
		{
			{ 0x0020, 0x00FF }
		};

		msdf_atlas::Charset charset;
		for (CharsetRange range : charsetRanges)
		{
			for (uint32_t c = range.Begin; c <= range.End; c++)
				charset.add(c);
		}

		double fontScale = 1.0;
		m_Data->FontGeometry = msdf_atlas::FontGeometry(&m_Data->Glyphs);
		int glyphsLoaded = m_Data->FontGeometry.loadCharset(font, fontScale, charset);
		if (glyphsLoaded <= 0) throw std::runtime_error("Font has no usable glyphs");
		HZ_CORE_INFO("Loaded {} glyphs from font (out of {})", glyphsLoaded, charset.size());


		double emSize = 40.0;

		msdf_atlas::TightAtlasPacker atlasPacker;
		// atlasPacker.setDimensionsConstraint()
		atlasPacker.setPixelRange(2.0);
		atlasPacker.setMiterLimit(1.0);
		atlasPacker.setPadding(0);
		atlasPacker.setScale(emSize);
		int remaining = atlasPacker.pack(m_Data->Glyphs.data(), (int)m_Data->Glyphs.size());
		if (remaining != 0) throw std::runtime_error("Failed to pack font atlas");

		int width = 0, height = 0;
		atlasPacker.getDimensions(width, height);
		if (width <= 0 || height <= 0) throw std::runtime_error("Invalid font atlas dimensions");
		emSize = atlasPacker.getScale();

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 2
		// if MSDF || MTSDF

		uint64_t coloringSeed = 0;
		bool expensiveColoring = false;
		if (expensiveColoring)
		{
			msdf_atlas::Workload([&glyphs = m_Data->Glyphs, &coloringSeed](int i, int threadNo) -> bool {
				unsigned long long glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
				glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
				return true;
				}, m_Data->Glyphs.size()).finish(THREAD_COUNT);
		}
		else {
			unsigned long long glyphSeed = coloringSeed;
			for (msdf_atlas::GlyphGeometry& glyph : m_Data->Glyphs)
			{
				glyphSeed *= LCG_MULTIPLIER;
				glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
			}
		}


		m_AtlasTexture = CreateAndCacheAtlas<uint8_t, float, 3, msdf_atlas::msdfGenerator>("Test", (float)emSize, m_Data->Glyphs, m_Data->FontGeometry, width, height);


#if 0
		msdfgen::Shape shape;
		if (msdfgen::loadGlyph(shape, font, 'C'))
		{
			shape.normalize();
			//                      max. angle
			msdfgen::edgeColoringSimple(shape, 3.0);
			//           image width, height
			msdfgen::Bitmap<float, 3> msdf(32, 32);
			//                     range, scale, translation
			msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, msdfgen::Vector2(4.0, 4.0));
			msdfgen::savePng(msdf, "output.png");
		}
#endif

	}

	Font::~Font()
	{
	}


	Ref<Font> Font::GetDefault()
	{
		if (!s_DefaultFont)
			s_DefaultFont = CreateRef<Font>(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"));

		return s_DefaultFont;
	}

	void Font::Shutdown()
	{
		s_DefaultFont.reset();
	}

}
