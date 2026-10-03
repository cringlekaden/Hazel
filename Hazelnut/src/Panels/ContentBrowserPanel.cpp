#include "Hazel/Core/Resources.h"
#include "hzpch.h"
#include "ContentBrowserPanel.h"

#include "Hazel/Project/Project.h"

#include <imgui.h>

namespace Hazel {

	ContentBrowserPanel::ContentBrowserPanel()
		: ContentBrowserPanel(Project::GetAssetDirectory()) {}

	ContentBrowserPanel::ContentBrowserPanel(const std::filesystem::path& assetRoot)
		: m_BaseDirectory(assetRoot), m_CurrentDirectory(m_BaseDirectory)
	{
		m_DirectoryIcon = Texture2D::Create(Resources::Resolve("Icons/ContentBrowser/DirectoryIcon.png").generic_u8string());
		m_FileIcon = Texture2D::Create(Resources::Resolve("Icons/ContentBrowser/FileIcon.png").generic_u8string());
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Content Browser");

		if (m_CurrentDirectory != std::filesystem::path(m_BaseDirectory))
		{
			if (ImGui::Button("<-"))
			{
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
		}

		static float padding = 16.0f;
		static float thumbnailSize = 128.0f;
		float cellSize = thumbnailSize + padding;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1)
			columnCount = 1;

		ImGui::Columns(columnCount, 0, false);

		std::error_code error;
		for (auto it = std::filesystem::directory_iterator(m_CurrentDirectory, error);
		     !error && it != std::filesystem::directory_iterator(); it.increment(error))
		{
			const auto& directoryEntry = *it;
			const auto& path = directoryEntry.path();
			std::string filenameString = path.filename().u8string();
			const bool isDirectory = directoryEntry.is_directory(error);
			if (error) break;

			ImGui::PushID(filenameString.c_str());
			Ref<Texture2D> icon = isDirectory ? m_DirectoryIcon : m_FileIcon;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::ImageButton("##file", (ImTextureID)(uintptr_t)icon->GetRendererID(), { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 });

			if (ImGui::BeginDragDropSource())
			{
				std::filesystem::path relativePath(path);
				const std::string itemPath = relativePath.generic_u8string();
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.size() + 1);
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (isDirectory)
					m_CurrentDirectory /= path.filename();

			}
			ImGui::TextWrapped("%s", filenameString.c_str());

			ImGui::NextColumn();

			ImGui::PopID();
		}
		if (error) ImGui::TextWrapped("Cannot read asset directory: %s", error.message().c_str());

		ImGui::Columns(1);

		ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
		ImGui::SliderFloat("Padding", &padding, 0, 32);

		// TODO: status bar
		ImGui::End();
	}

}
