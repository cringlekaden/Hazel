#include "ContentBrowserPayload.h"
#include "Authoring/AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "Hazel/Math/Math.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Renderer/Font.h"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ImGuizmo.h"

namespace Hazel {


	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f)
	{
		m_Font = Font::GetDefault();
	}

	EditorLayer::~EditorLayer() = default;

	void EditorLayer::OnAttach()
	{
		HZ_PROFILE_FUNCTION();


		m_IconPlay = Texture2D::Create(Resources::Resolve("Icons/PlayButton.png").generic_u8string());
		m_IconPause = Texture2D::Create(Resources::Resolve("Icons/PauseButton.png").generic_u8string());
		m_IconSimulate = Texture2D::Create(Resources::Resolve("Icons/SimulateButton.png").generic_u8string());
		m_IconStep = Texture2D::Create(Resources::Resolve("Icons/StepButton.png").generic_u8string());
		m_IconStop = Texture2D::Create(Resources::Resolve("Icons/StopButton.png").generic_u8string());

		FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
        m_Authoring=CreateScope<AuthoringPanel>(*this);
        m_Authoring->MarkSceneSaved();
        Application::Get().SetCloseRequest([this]{m_Authoring->RequestClose();});

		auto commandLineArgs = Application::Get().GetSpecification().CommandLineArgs;
		if (commandLineArgs.Count > 1)
		{
			auto projectFilePath = commandLineArgs[1];
			OpenProject(std::filesystem::u8path(projectFilePath));
		}
		else
		{
			const auto bundled = Project::Discover(FileSystem::GetExecutablePath().parent_path() / "Example");
            if (bundled.size() == 1) OpenProject(bundled.front());
            // No automatic native dialog: File/New Project remains available.

		}

		m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
		Renderer2D::SetLineWidth(4.0f);
	}

	void EditorLayer::OnDetach()
	{
		HZ_PROFILE_FUNCTION();
        Application::Get().SetCloseRequest({});
        m_Authoring.reset();
        if (m_SceneState != SceneState::Edit) OnSceneStop();
        ClearSceneObservers();
        m_ActiveScene.reset(); m_EditorScene.reset(); m_Font.reset();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		HZ_PROFILE_FUNCTION();
        if(m_Authoring) m_Authoring->Tick(ts);

        if (m_SceneState != SceneState::Play)
            m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);

		// Resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f && // zero sized framebuffer is invalid
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
		}

		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		// Clear our entity ID attachment to -1
		m_Framebuffer->ClearAttachment(1, -1);

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				if (m_ViewportFocused && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))
					m_CameraController.OnUpdate(ts);

				if(m_ViewportFocused && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))m_EditorCamera.OnUpdate(ts);

				m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);
				break;
			}
			case SceneState::Simulate:
			{
				if(m_ViewportFocused && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))m_EditorCamera.OnUpdate(ts);

				m_ActiveScene->OnUpdateSimulation(ts, m_EditorCamera);
				break;
			}
			case SceneState::Play:
			{
                m_RuntimeSession.Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
                m_RuntimeSession.SetInput(Input::GetMouseScreenPosition() - m_ViewportBounds[0], m_ViewportHovered && m_ViewportFocused && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId));
                m_RuntimeSession.Update(ts);
                if (m_ActiveScene != m_RuntimeSession.GetScene()) {
                    ClearSceneObservers();
                    m_ActiveScene = m_RuntimeSession.GetScene();
                    m_SceneHierarchyPanel.SetContext(m_ActiveScene);
                    m_ActionError.clear();
                }
                if (!m_RuntimeSession.GetError().empty()) m_ActionError = m_RuntimeSession.GetError();
				break;
			}
		}

		auto[mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my;
        m_HoveredEntity = {};
        if (mx >= 0 && my >= 0 && mx < viewportSize.x && my < viewportSize.y)
        {
            const int pixelData = m_Framebuffer->ReadPixel(1, static_cast<int>(mx), static_cast<int>(my));
            if (pixelData != -1) m_HoveredEntity = Entity(static_cast<entt::entity>(pixelData), m_ActiveScene.get());
        }

		OnOverlayRender();

		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnImGuiRender()
	{

		HZ_PROFILE_FUNCTION();
        ImGuizmo::BeginFrame();

		// Note: Switch this to true to enable dockspace
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();


		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
                if(m_Authoring) { m_Authoring->FileMenu(); }

				ImGui::EndMenu();
			}

            if(m_Authoring) m_Authoring->Menus();

			ImGui::EndMenuBar();
		}

        if(m_Authoring)m_Authoring->Status();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}


		m_SceneHierarchyPanel.OnImGuiRender();
		if (m_ContentBrowserPanel) m_ContentBrowserPanel->OnImGuiRender();

		ImGui::Begin("Stats");


#if 0
		std::string name = "None";
		if (m_HoveredEntity)
			name = m_HoveredEntity.GetComponent<TagComponent>().Tag;
		ImGui::Text("Hovered Entity: %s", name.c_str());
#endif

		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

		ImGui::End();


		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();

		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused || !m_ViewportHovered || io.WantTextInput || ImGui::IsAnyItemActive());

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		uint64_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !io.KeyAlt)m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				auto path=ContentBrowserPath(payload->Data,payload->DataSize);
                if(path.extension()==".hprefab") m_Authoring->InstantiatePrefab(path);
                else if(path.extension()==".hsprites")m_Authoring->SelectAsset(path);
                else if(path.extension()==".hazel")m_Authoring->Guard(OperationIntent::OpenScene,[this,path]{return OpenScene(path);});
			}
			ImGui::EndDragDropTarget();
		}

		// Gizmos
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1 && m_Authoring->Availability(EditorAction::EditScene))
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			// Camera

			// Runtime camera from entity
			// auto cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
			// const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
			// const glm::mat4& cameraProjection = camera.GetProjection();
			// glm::mat4 cameraView = glm::inverse(cameraEntity.GetComponent<TransformComponent>().GetTransform());

			// Editor camera
			const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
			glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

			// Entity transform
			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			// Snapping
			bool snap = Input::IsKeyPressed(Key::LeftControl);
			float snapValue = 0.5f; // Snap to 0.5m for translation/scale
			// Snap to 45 degrees for rotation
			if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
				snapValue = 45.0f;

			float snapValues[3] = { snapValue, snapValue, snapValue };

			ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
				(ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform),
				nullptr, snap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				glm::vec3 translation, rotation, scale;
				if(m_Authoring->Availability(EditorAction::EditScene) && Math::DecomposeTransform(transform, translation, rotation, scale)) {
				glm::vec3 deltaRotation = rotation - tc.Rotation;
				tc.Translation = translation;
				tc.Rotation += deltaRotation;
				tc.Scale = scale;
                }
			}
		}


		ImGui::End();
		ImGui::PopStyleVar();

        if(m_Authoring){m_Authoring->ObserveSceneFocus();m_Authoring->Render();m_Authoring->Shortcuts();}
		UI_Toolbar();
		ImGui::End();
	}

    void EditorLayer::UI_Toolbar() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4, 2});
        ImGui::Begin("##toolbar",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        if(m_Authoring)m_Authoring->Toolbar();
        ImGui::End();
        ImGui::PopStyleVar();
    }

	void EditorLayer::OnEvent(Event& e)
	{
		if(m_ViewportFocused && m_ViewportHovered && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))m_CameraController.OnEvent(e);
		if (m_SceneState == SceneState::Edit && m_ViewportFocused && m_ViewportHovered && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))
		{
			m_EditorCamera.OnEvent(e);
		}

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(HZ_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(HZ_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

    bool EditorLayer::OnKeyPressed(KeyPressedEvent&) {return false;} // Focused commands are sampled once through ImGui.
    bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent&) {return false;} // Image item click below owns picking.

	void EditorLayer::OnOverlayRender()
	{
		if (m_SceneState == SceneState::Play)
		{
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (!camera)
				return;

			Renderer2D::BeginScene(camera.GetComponent<CameraComponent>().Camera, camera.GetComponent<TransformComponent>().GetTransform());
		}
		else
		{
			Renderer2D::BeginScene(m_EditorCamera);
		}

		if (m_ShowPhysicsColliders)
		{
			// Box Colliders
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
				for (auto entity : view)
				{
					auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

					glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.Translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.Offset, 0.001f))
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
				}
			}

			// Circle Colliders
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
				for (auto entity : view)
				{
					auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);

					glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.01f);
				}
			}
		}

		// Draw selected entity outline
		if (Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity())
		{
			const TransformComponent& transform = selectedEntity.GetComponent<TransformComponent>();
            if(auto* sprite=m_ActiveScene->RenderedSprite(selectedEntity)) {
                for(size_t i=0;i<4;++i)Renderer2D::DrawLine(glm::vec3(transform.GetTransform()*glm::vec4(sprite->Corners[i],0,1)),glm::vec3(transform.GetTransform()*glm::vec4(sprite->Corners[(i+1)%4],0,1)),glm::vec4(1,.5f,0,1));
            } else Renderer2D::DrawRect(transform.GetTransform(), glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
		}

		Renderer2D::EndScene();
	}

	bool EditorLayer::ActionFailed(const std::string& message)
	{
		m_ActionError = message;
        if (m_Authoring) m_Authoring->ShowOutput();
		HZ_ERROR("{}", message);
		return false;
	}

	void EditorLayer::ClearSceneObservers()
	{
		m_HoveredEntity = {};
		m_SceneHierarchyPanel.SetContext(nullptr);
	}

	bool EditorLayer::OpenProject(const std::filesystem::path& path,bool repair)
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::ReplaceProject))return false;
		try {
			auto project = Project::LoadCandidate(path);
			if (!project) return ActionFailed("Cannot parse/open project: " + path.generic_u8string());
			const auto assets = project->GetAssetRoot();
			if (!std::filesystem::is_directory(assets)) return ActionFailed("Project asset directory is missing: " + assets.generic_u8string());
            const auto startScene = Project::ResolveAssetPath(assets, project->GetConfig().StartScene);
            auto scene = CreateRef<Scene>();
            if(!SceneSerializer(scene,assets,repair,project->GetAssets()).Deserialize(startScene.generic_u8string()))return ActionFailed("Cannot load startup scene; use File > Open Project for Repair for broken sprite resources");
            scene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
			auto browser = CreateScope<ContentBrowserPanel>(assets);
			// Init stages a complete domain/watcher; failure retains the current scripts.
			ScriptEngine::Init(Project::ResolveAssetPath(assets, project->GetConfig().ScriptModulePath), [this]() {
				if (m_SceneState != SceneState::Edit) OnSceneStop();
			});
			ClearSceneObservers();
			Project::SetActive(project);
			m_ProjectPath = path; m_EditorScenePath = startScene;
			m_EditorScene = scene; m_ActiveScene = scene;
			m_ContentBrowserPanel = std::move(browser);
            if(m_Authoring)m_Authoring->BindProject();
			m_SceneHierarchyPanel.SetContext(scene);
			m_ActionError.clear();
            HZ_CORE_INFO("Hazelnut ready: {}", project->GetConfig().Name);
			return true;
		} catch (const std::runtime_error& error) {
			return ActionFailed("Open project '" + path.generic_u8string() + "': " + error.what());
		}
	}

	bool EditorLayer::OpenProject()
	{
		const auto path = FileDialogs::OpenFile("Hazel Project (*.hproj)\0*.hproj\0");
		return !path.empty() && OpenProject(std::filesystem::u8path(path));
	}

	bool EditorLayer::SaveProject()
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::EditAsset))return false;
		if (m_ProjectPath.empty()) return ActionFailed("Open a project before saving it");
		if (!Project::SaveActive(m_ProjectPath)) return ActionFailed("Cannot save project: " + m_ProjectPath.generic_u8string());
		m_ActionError.clear(); return true;
	}

	void EditorLayer::NewScene()
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::ReplaceScene))return;
		if (m_SceneState != SceneState::Edit) OnSceneStop();
		ClearSceneObservers();
		m_EditorScene = CreateRef<Scene>(); m_ActiveScene = m_EditorScene;
        if(Project::GetActive())m_EditorScene->SetAssets(Project::GetActive()->GetAssets());
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_EditorScenePath.clear(); m_ActionError.clear();
	}

	bool EditorLayer::OpenScene()
	{
		const auto path = FileDialogs::OpenFile("Hazel Scene (*.hazel)\0*.hazel\0");
		return !path.empty() && OpenScene(std::filesystem::u8path(path));
	}

	bool EditorLayer::OpenScene(const std::filesystem::path& path,bool repair)
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::ReplaceScene))return false;
		try {
			if (path.extension() != ".hazel") return ActionFailed("Scene must be a .hazel file: " + path.generic_u8string());
			auto scene = CreateRef<Scene>();
			scene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
			if (!SceneSerializer(scene,Project::GetActive()?Project::GetAssetDirectory():std::filesystem::path{},repair).Deserialize(path.generic_u8string())) return ActionFailed("Cannot load scene/assets; use Open Scene for Repair for broken sprite resources: " + path.generic_u8string());
			if (m_SceneState != SceneState::Edit) OnSceneStop();
			ClearSceneObservers();
			m_EditorScene = scene; m_ActiveScene = scene; m_EditorScenePath = path;
			m_SceneHierarchyPanel.SetContext(scene); m_ActionError.clear();
            if(m_Authoring)m_Authoring->MarkSceneSaved();
			return true;
		} catch (const std::runtime_error& error) {
			return ActionFailed("Open scene '" + path.generic_u8string() + "': " + error.what());
		}
	}

	bool EditorLayer::SaveScene()
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::SaveScene))return false;
		return m_EditorScenePath.empty() ? SaveSceneAs() : SerializeScene(m_EditorScene, m_EditorScenePath);
	}

	bool EditorLayer::SaveSceneAs()
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::SaveScene))return false;
		m_ActionError.clear();
        const auto path = FileDialogs::SaveFile("Hazel Scene (*.hazel)\0*.hazel\0");
		if (path.empty()) return false;
		if (!SerializeScene(m_EditorScene, std::filesystem::u8path(path))) return false;
		m_EditorScenePath = std::filesystem::u8path(path); return true;
	}

	bool EditorLayer::SerializeScene(Ref<Scene> scene, const std::filesystem::path& path)
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::SaveScene))return false;
		try { SceneSerializer(scene).Serialize(path.generic_u8string()); if(m_Authoring)m_Authoring->MarkSceneSaved(); m_ActionError.clear(); return true; }
		catch (const std::runtime_error& error) { return ActionFailed("Save scene '" + path.generic_u8string() + "': " + error.what()); }
	}

	bool EditorLayer::ReloadScripts()
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::ReloadScripts))return false;
		try { ScriptEngine::ReloadAssembly(); m_ActionError.clear(); return true; }
		catch (const std::runtime_error& error) { return ActionFailed(std::string("Reload scripts: ") + error.what()); }
	}

	bool EditorLayer::OnScenePlay(bool useSavedAssets)
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::Play))return false;
        if(m_Authoring && !useSavedAssets){m_Authoring->GuardPlay(false);return m_SceneState==SceneState::Play;}
		if (m_SceneState == SceneState::Play) return true;
		if (!ScriptEngine::IsInitialized() && !m_EditorScene->GetAllEntitiesWith<ScriptComponent>().empty()) {
			return ActionFailed("Open a valid script assembly before playing a scripted scene");
		}
		try {
            if(Project::GetActive())Project::GetActive()->GetAssets()->Refresh();
            if (m_SceneState != SceneState::Edit) OnSceneStop();
            m_RuntimeSession.Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
            m_EditorSelection=uint64_t(m_SceneHierarchyPanel.GetSelectedEntity()?m_SceneHierarchyPanel.GetSelectedEntity().GetUUID():UUID(0));
            m_RuntimeSession.Start(Project::GetActive(), m_EditorScene);
            ClearSceneObservers();
            m_ActiveScene = m_RuntimeSession.GetScene(); m_SceneState = SceneState::Play;
            m_SceneHierarchyPanel.SetContext(m_ActiveScene); m_ActionError.clear();return true;
		} catch (const std::runtime_error& error) { return ActionFailed(std::string("Play scene: ") + error.what()); }
	}

	bool EditorLayer::OnSceneSimulate(bool useSavedAssets)
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::Simulate))return false;
        if(m_Authoring && !useSavedAssets){m_Authoring->GuardPlay(true);return m_SceneState==SceneState::Simulate;}
        try {
		if(Project::GetActive())Project::GetActive()->GetAssets()->Refresh();
		auto scene = Scene::Copy(m_EditorScene);
        RuntimeSession::Validate(scene, false);
        m_EditorSelection=uint64_t(m_SceneHierarchyPanel.GetSelectedEntity()?m_SceneHierarchyPanel.GetSelectedEntity().GetUUID():UUID(0));
		scene->OnSimulationStart();
		if (m_SceneState != SceneState::Edit) OnSceneStop();
		ClearSceneObservers();
		m_ActiveScene = scene; m_SceneState = SceneState::Simulate;
		m_SceneHierarchyPanel.SetContext(scene); m_ActionError.clear();return true;
        }catch(const std::exception& error){return ActionFailed(std::string("Simulate scene: ")+error.what());}
	}

	void EditorLayer::OnSceneStop()
	{
        if(m_SceneState==SceneState::Edit)return;
        if(m_Authoring && !m_Authoring->Require(EditorAction::RuntimeControl))return;
		HZ_CORE_ASSERT(m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate);
		ClearSceneObservers();
		if (m_SceneState == SceneState::Play) m_RuntimeSession.Stop();
		else if (m_SceneState == SceneState::Simulate) m_ActiveScene->OnSimulationStop();
		m_SceneState = SceneState::Edit; m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        if(m_EditorSelection)m_SceneHierarchyPanel.SetSelectedEntity(m_EditorScene->GetEntityByUUID(m_EditorSelection));
	}

	void EditorLayer::OnScenePause()
	{
		if (m_SceneState == SceneState::Edit)
			return;

		m_ActiveScene->SetPaused(true);
	}

	void EditorLayer::OnDuplicateEntity()
	{
        if(m_Authoring && !m_Authoring->Require(EditorAction::EditScene))return;
		if (m_SceneState != SceneState::Edit)
			return;

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity)
		{
			Entity newEntity = m_EditorScene->DuplicateEntity(selectedEntity);
			m_SceneHierarchyPanel.SetSelectedEntity(newEntity);
		}
	}

}
