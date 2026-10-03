#pragma once

#include "Hazel/Renderer/Texture.h"

#include <filesystem>
#include <functional>

namespace Hazel {

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();
		explicit ContentBrowserPanel(const std::filesystem::path& assetRoot);

		void OnImGuiRender();
        std::function<void(const std::filesystem::path&)> SelectAsset;
	private:
		std::filesystem::path m_BaseDirectory;
		std::filesystem::path m_CurrentDirectory;

		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;
	};

}
