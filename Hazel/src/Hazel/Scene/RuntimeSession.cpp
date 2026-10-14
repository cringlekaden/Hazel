#include "hzpch.h"
#include "RuntimeSession.h"
#include "Scene.h"
#include "Entity.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Events/KeyEvent.h"
#include "Hazel/Events/ApplicationEvent.h"
#include "Hazel/Events/MouseEvent.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>

namespace Hazel {
    static uint64_t s_NextSession = 0; // Main thread only.
    RuntimeSession::RuntimeSession() : m_Thread(std::this_thread::get_id()) {}
    RuntimeSession::~RuntimeSession() { Stop(); }
    void RuntimeSession::CheckThread() const {
        if (std::this_thread::get_id() != m_Thread) throw std::logic_error("RuntimeSession requires its main thread");
    }
    void RuntimeSession::Validate(const Ref<Scene>& scene, bool validateScripts) {
        if (!scene) throw std::runtime_error("No scene to run");
        scene->ValidateSprites();
        scene->ValidateUI();
        scene->ValidateHierarchy(); // Shared roots, planar dimensions, graph/world and physics validation.
        if (validateScripts) for (auto handle : scene->GetAllEntitiesWith<ScriptComponent>()) {
            const auto& script = Entity(handle, scene.get()).GetComponent<ScriptComponent>();
            if (!script.ClassName.empty() && !ScriptEngine::EntityClassExists(script.ClassName))
                throw std::runtime_error("Runtime script class is unavailable: " + script.ClassName);
        }
    }
    void RuntimeSession::Start(const Ref<Project>& project, const Ref<Scene>& authoredScene) {
        CheckThread();
        if (m_Updating || m_Stopping) throw std::logic_error("Cannot start a session inside its update/stop");
        if(project) {
            RendererPolicy::Validate(project->GetRendererRequests());
            if(Application::TryGet()) {
                const auto reason=RendererPolicy::RestartReason(project->GetRendererRequests(),Renderer::GetResolution(),Renderer::GetCapabilities());
                if(!reason.empty())throw std::runtime_error(reason);
            }
        }
        Validate(authoredScene);
        auto scene = Scene::Copy(authoredScene);
        if (auto* current = ScriptEngine::GetRuntimeSession(); current && current != this)
            throw std::logic_error("Only one runtime session may use the script environment");
        Stop();
        m_Project = project; m_Scene = std::move(scene);
        auto root=FileSystem::GetEnvironmentPath("HAZEL_SAVE_ROOT");
        if(root.empty())root=FileSystem::GetUserDataDirectory()/"Hazel/Games";
        m_Storage.Configure(root,project?project->GetConfig().SaveNamespace:"",m_PersistentStorage);
        m_Identity = ++s_NextSession; m_Error.clear();
        m_Scene->OnViewportResize(m_Width, m_Height);
        ScriptEngine::SetRuntimeSession(this);
        try { m_Scene->OnRuntimeStart(); }
        catch (...) { Stop(); throw; }
    }
    void RuntimeSession::Stop() {
        CheckThread();
        if (m_Stopping) return;
        if (m_Updating) throw std::logic_error("Cannot stop a scene inside its callback; request a transition instead");
        m_Stopping = true;
        m_Pending.reset(); m_Identity = 0; m_InputEnabled = false;
        // Unbind before native OnDestroy, so retired callbacks cannot queue work.
        if (ScriptEngine::GetRuntimeSession() == this) ScriptEngine::SetRuntimeSession(nullptr);
        if (m_Scene) m_Scene->OnRuntimeStop();
        m_Storage.Clear();
        m_Scene.reset(); m_Project.reset(); m_Stopping = false;
    }
    bool RuntimeSession::RequestSceneLoad(const std::filesystem::path& reference) {
        CheckThread();
        if (!m_Scene || m_Stopping || !m_Project) return false;
        const auto normalized = Project::NormalizeAssetPath(reference).lexically_normal();
        const auto text = normalized.generic_u8string();
        if (normalized.empty() || normalized.has_root_path() || *normalized.begin() == ".." ||
            (text.size() > 1 && text[1] == ':') || normalized.extension() != ".hazel") {
            HZ_CORE_ERROR("Scene.LoadScene requires an asset-relative .hazel path: {}", text); return false;
        }
        if (m_Pending) {
            if (*m_Pending == normalized) return true;
            HZ_CORE_WARN("Scene transition already requested; ignoring '{}' (first request wins)", text); return false;
        }
        m_Pending = normalized;
        return true;
    }
    void RuntimeSession::CommitPendingTransition() {
        if (!m_Pending) return;
        auto reference = *m_Pending; m_Pending.reset();
        Ref<Scene> candidate;
        try {
            candidate = m_Project->LoadScene(reference);
            Validate(candidate);
            candidate->OnViewportResize(m_Width, m_Height);
        } catch (const std::runtime_error& error) {
            m_Error = "Scene transition '" + reference.generic_u8string() + "': " + error.what();
            HZ_CORE_ERROR("{}", m_Error); return;
        }
        // All callbacks/registry walks have returned. Retire observations before
        // starting the target, with the same domain and assembly watcher alive.
        m_Stopping = true;
        ScriptEngine::SetRuntimeSession(nullptr);
        m_Scene->OnRuntimeStop();
        auto previous = std::move(m_Scene);
        m_Scene = std::move(candidate); m_Stopping = false;
        ScriptEngine::SetRuntimeSession(this);
        try { m_Scene->OnRuntimeStart(); m_Error.clear(); HZ_CORE_INFO("Runtime scene: {}", reference.generic_u8string()); }
        catch (const std::runtime_error& error) {
            m_Pending.reset(); m_Scene->OnRuntimeStop(); m_Scene = std::move(previous);
            m_Scene->OnRuntimeStart();
            m_Error = "Cannot start target scene: " + std::string(error.what()); HZ_CORE_ERROR("{}", m_Error);
        }
    }
    void RuntimeSession::Update(Timestep timestep) {
        CheckThread();
        if (!m_Scene) return;
        if (m_Updating) throw std::logic_error("Reentrant runtime update");
        CommitPendingTransition(); // One transition at most; target OnCreate waits one more frame.
        m_Updating = true;
        try {
            auto input=m_UIEdgeScene==m_Scene->GetIdentity()?m_UIEdges:GameUI::InputFrame{};
            m_UIEdges={};m_UIEdgeScene=0;input.Mouse=m_Mouse;input.Enabled=m_InputEnabled&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0));
            input.FocusNavigation=Input::IsKeyPressed(Key::Tab);
            input.CancelDown=Input::IsKeyPressed(Key::Escape);
            input.LeftDown=Input::IsMouseButtonPressed(Mouse::ButtonLeft);
            input.AcceptDown=Input::IsKeyPressed(Key::Enter)||Input::IsKeyPressed(Key::Space);
            const bool shift=Input::IsKeyPressed(Key::LeftShift)||Input::IsKeyPressed(Key::RightShift);
            input.NextDown=(Input::IsKeyPressed(Key::Tab)&&!shift)||Input::IsKeyPressed(Key::Down)||Input::IsKeyPressed(Key::Right);
            input.PreviousDown=(Input::IsKeyPressed(Key::Tab)&&shift)||Input::IsKeyPressed(Key::Up)||Input::IsKeyPressed(Key::Left);
            if(m_FramebufferExplicit)m_Scene->SetUIFramebufferSize(m_FramebufferSize);
            m_Scene->UpdateUIInput(input);
            m_Scene->OnUpdateRuntime(timestep); m_Updating = false;
        }
        catch (...) { m_Updating = false; throw; }
    }
    void RuntimeSession::Resize(uint32_t width, uint32_t height) {
        CheckThread(); m_Width = width; m_Height = height;
        if (m_Scene) {m_Scene->OnViewportResize(width, height);if(!width||!height)m_Scene->CancelUIInput();}
    }
    void RuntimeSession::ResizeFramebuffer(uint32_t width,uint32_t height) {
        CheckThread();m_FramebufferExplicit=true;m_FramebufferSize={width,height};
        if(m_Scene)m_Scene->SetUIFramebufferSize(m_FramebufferSize);
        if((!width||!height)&&m_Scene)m_Scene->CancelUIInput();
    }
    bool RuntimeSession::IsGameplayInputEnabled() const {return m_InputEnabled&&m_GameplayEnabled&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0))&&(!m_Scene||!m_Scene->IsGameplayInputBlocked());}
    bool RuntimeSession::IsGameplayMouseEnabled() const {return m_InputEnabled&&m_GameplayEnabled&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0))&&(!m_Scene||!m_Scene->IsGameplayPointerBlocked());}
    void RuntimeSession::SubmitUIEvent(Event& event,glm::vec2 mouse,bool enabled) {
        if(event.GetEventType()==EventType::WindowLostFocus || (event.GetEventType()==EventType::WindowResize&&(!static_cast<WindowResizeEvent&>(event).GetWidth()||!static_cast<WindowResizeEvent&>(event).GetHeight()))){m_UIEdges={};m_UIEdgeScene=0;if(m_Scene)m_Scene->CancelUIInput();return;}
        if(!enabled||!m_Scene)return;
        if(m_UIEdgeScene!=m_Scene->GetIdentity()){m_UIEdges={};m_UIEdgeScene=m_Scene->GetIdentity();}
        if(event.GetEventType()==EventType::MouseButtonPressed&&static_cast<MouseButtonPressedEvent&>(event).GetMouseButton()==Mouse::ButtonLeft){m_UIEdges.PointerPressed=true;m_UIEdges.PressPosition=mouse;}
        if(event.GetEventType()==EventType::MouseButtonReleased&&static_cast<MouseButtonReleasedEvent&>(event).GetMouseButton()==Mouse::ButtonLeft){m_UIEdges.PointerReleased=true;m_UIEdges.ReleasePosition=mouse;m_UIEdges.HasReleasePosition=true;}
        if(event.GetEventType()==EventType::KeyPressed){auto& e=static_cast<KeyPressedEvent&>(event);if(!e.IsRepeat()&&(e.GetKeyCode()==Key::Enter||e.GetKeyCode()==Key::Space))m_UIEdges.AcceptPressed=true;}
    }
    void RuntimeSession::SetInput(glm::vec2 position, bool enabled) {
        CheckThread(); m_Mouse = position; m_InputEnabled = enabled;m_GameplayEnabled=enabled; if(!enabled){m_UIEdges={};m_UIEdgeScene=0;if(m_Scene)m_Scene->CancelUIInput();}
    }
    bool RuntimeSession::GetMouseWorldPosition(glm::vec2& position) const {
        if (!m_Scene || !IsGameplayMouseEnabled() || !m_Width || !m_Height || m_Mouse.x < 0 || m_Mouse.y < 0 || m_Mouse.x >= m_Width || m_Mouse.y >= m_Height) return false;
        auto camera = m_Scene->GetPrimaryCameraEntity();
        if (!camera) return false;
        const auto transform = m_Scene->GetWorldTransform(camera);
        const auto viewProjection = camera.GetComponent<CameraComponent>().Camera.GetProjection() * glm::inverse(transform);
        const auto inverse = glm::inverse(viewProjection);
        glm::vec4 rayNear = inverse * glm::vec4(2*m_Mouse.x/m_Width-1, 1-2*m_Mouse.y/m_Height, -1, 1);
        glm::vec4 rayFar = inverse * glm::vec4(2*m_Mouse.x/m_Width-1, 1-2*m_Mouse.y/m_Height, 1, 1);
        if (std::abs(rayNear.w)<1e-7f || std::abs(rayFar.w)<1e-7f) return false;
        rayNear /= rayNear.w; rayFar /= rayFar.w;
        const auto ray = rayFar-rayNear;
        if (std::abs(ray.z)<1e-7f) return false;
        const float distance = -rayNear.z/ray.z; // Example interaction is on world z=0.
        position = glm::vec2(rayNear+distance*ray);
        return std::isfinite(position.x) && std::isfinite(position.y) && distance >= 0 && distance <= 1;
    }
}
