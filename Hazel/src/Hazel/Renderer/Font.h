#pragma once
// Adapted upstream font API: owned MSDF state and explicit context-bound cache lifetime.

#include <filesystem>

#include "Hazel/Core/Base.h"
#include "Hazel/Renderer/Texture.h"

namespace Hazel {

	struct MSDFData;

	class Font
	{
	public:
		Font(const std::filesystem::path& font);
		~Font();

		const MSDFData* GetMSDFData() const { return m_Data.get(); }
		Ref<Texture2D> GetAtlasTexture() const { return m_AtlasTexture; }

		static Ref<Font> GetDefault();
		static void Shutdown();
	private:
		Scope<MSDFData> m_Data;
		Ref<Texture2D> m_AtlasTexture;
		static Ref<Font> s_DefaultFont;
	};

}
