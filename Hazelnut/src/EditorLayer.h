#pragma once

#include "Hazel.h"
#include "Hazel/Events/KeyEvent.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"

#include "Hazel/Renderer/EditorCamera.h"
#include "Hazel/Scene/RuntimeSession.h"

namespace Hazel {

	class AuthoringPanel;
    class ConsoleModel;
    class ConsoleSession;

	class EditorLayer : public Layer
	{
        friend class EditorWorkflowSmoke;
        friend class AuthoringPanel;
	public:
		explicit EditorLayer(std::shared_ptr<ConsoleModel> console = {});
		virtual ~EditorLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;
	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);

		void OnOverlayRender();

		bool OpenProject();
		bool OpenProject(const std::filesystem::path& path,bool repair = false);
		bool SaveProject();

		void NewScene();
		bool OpenScene();
		bool OpenScene(const std::filesystem::path& path,bool repair = false);
		bool SaveScene();
		bool SaveSceneAs();

		bool SerializeScene(Ref<Scene> scene, const std::filesystem::path& path);
		bool ReloadScripts();
		bool ActionFailed(const std::string& message);
		void ClearSceneObservers();

        bool OnScenePlay(bool useSavedAssets = false);
        bool OnSceneSimulate(bool useSavedAssets = false);
		void OnSceneStop();
		void OnScenePause();

		void OnDuplicateEntity();

        std::unique_ptr<ConsoleSession> m_ConsoleSession;
        std::shared_ptr<ConsoleModel> m_Console;

		// UI Panels
		void UI_Toolbar();
	private:
		Hazel::OrthographicCameraController m_CameraController;

		Ref<Framebuffer> m_Framebuffer;

		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;
        RuntimeSession m_RuntimeSession;
		std::filesystem::path m_EditorScenePath;
		std::filesystem::path m_ProjectPath;
		std::string m_ActionError;
        Ref<Font> m_Font;

		Entity m_HoveredEntity;


		EditorCamera m_EditorCamera;


		bool m_ViewportFocused = false, m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2] = {};


		int m_GizmoType = -1;

		bool m_ShowPhysicsColliders = false;
        uint64_t m_EditorSelection = 0;

		enum class SceneState
		{
			Edit = 0, Play = 1, Simulate = 2
		};
		SceneState m_SceneState = SceneState::Edit;

		// Panels
		SceneHierarchyPanel m_SceneHierarchyPanel;
		Scope<ContentBrowserPanel> m_ContentBrowserPanel;
        Scope<AuthoringPanel> m_Authoring;

		// Editor resources
		Ref<Texture2D> m_IconPlay, m_IconPause, m_IconStep, m_IconSimulate, m_IconStop;
	};

}
