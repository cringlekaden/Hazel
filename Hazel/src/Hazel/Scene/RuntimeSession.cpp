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
            GameUI::Validate(project->GetUIBindings());
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
        m_Identity = ++s_NextSession; m_Error.clear();m_UIBindingWarning.clear();
        m_UIAdapter.Configure(project?project->GetUIBindings():GameUI::UIBindings::Defaults());
        if(project&&!project->GetConfig().SaveNamespace.empty())try {
            const auto payload=m_Storage.Read("engine-ui-bindings");
            if(!payload.empty()&&payload!="default"){m_UIAdapter.Configure(GameUI::ParseBindings(payload));HZ_CORE_INFO("Runtime UI: accepted player binding override ({})",project->GetConfig().SaveNamespace);}
            else HZ_CORE_INFO("Runtime UI: project binding defaults ({})",project->GetConfig().SaveNamespace);
        }catch(const std::exception& error){m_UIBindingWarning=std::string("UI override rejected; using project defaults: ")+error.what();HZ_CORE_WARN("{}",m_UIBindingWarning);}
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
        m_UIAdapter.Cancel();m_UIEdgeScene=0;m_UINeutralLatch=false;
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
            GameUI::DeviceSample sample;sample.Point=m_Mouse;sample.Pad=Input::GetGamepad();
            for(auto b:m_UIAdapter.Bindings().Actions)for(int key:{b.Key,b.Alternate})if(key>=0)sample.Keys[size_t(key)]=Input::IsKeyPressed(KeyCode(key));
            for(auto key:{Key::LeftShift,Key::RightShift,Key::LeftControl,Key::RightControl,Key::LeftAlt,Key::RightAlt,Key::LeftSuper,Key::RightSuper})sample.Keys[key]=Input::IsKeyPressed(key);
            sample.Mouse[size_t(m_UIAdapter.Bindings().PointerButton)]=Input::IsMouseButtonPressed(MouseCode(m_UIAdapter.Bindings().PointerButton));
            RouteUIInput(sample,timestep.GetSeconds(),m_InputEnabled);
            m_Scene->OnUpdateRuntime(timestep); m_Updating = false;
        }
        catch (...) { m_Updating = false; throw; }
    }
    void RuntimeSession::Resize(uint32_t width, uint32_t height) {
        CheckThread(); m_Width = width; m_Height = height;
        if (m_Scene) m_Scene->OnViewportResize(width, height);
        if(!width||!height)SetInput(m_Mouse,false);
    }
    void RuntimeSession::ResizeFramebuffer(uint32_t width,uint32_t height) {
        CheckThread();m_FramebufferExplicit=true;m_FramebufferSize={width,height};
        if(m_Scene)m_Scene->SetUIFramebufferSize(m_FramebufferSize);
        if(!width||!height)SetInput(m_Mouse,false);
    }
    bool RuntimeSession::IsGameplayInputEnabled() const {return m_InputEnabled&&m_GameplayEnabled&&!m_UINeutralLatch&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0))&&(!m_Scene||!m_Scene->IsGameplayInputBlocked());}
    bool RuntimeSession::IsGameplayMouseEnabled() const {return m_InputEnabled&&m_GameplayEnabled&&!m_UINeutralLatch&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0))&&(!m_Scene||!m_Scene->IsGameplayPointerBlocked());}
    void RuntimeSession::SubmitUIEvent(Event& event,glm::vec2 mouse,bool enabled) {
        if(event.GetEventType()==EventType::WindowLostFocus || (event.GetEventType()==EventType::WindowResize&&(!static_cast<WindowResizeEvent&>(event).GetWidth()||!static_cast<WindowResizeEvent&>(event).GetHeight()))){SetInput(mouse,false);return;}
        if(!enabled||!m_Scene)return;
        if(m_UIEdgeScene!=m_Scene->GetIdentity()){m_UIAdapter.Cancel();m_UIEdgeScene=m_Scene->GetIdentity();}
        if(event.GetEventType()==EventType::MouseButtonPressed)m_UIAdapter.PointerEdge(static_cast<MouseButtonPressedEvent&>(event).GetMouseButton(),true,mouse);
        if(event.GetEventType()==EventType::MouseButtonReleased)m_UIAdapter.PointerEdge(static_cast<MouseButtonReleasedEvent&>(event).GetMouseButton(),false,mouse);
        const bool shift=Input::IsKeyPressed(Key::LeftShift)||Input::IsKeyPressed(Key::RightShift);
        const bool command=Input::IsKeyPressed(Key::LeftControl)||Input::IsKeyPressed(Key::RightControl)||Input::IsKeyPressed(Key::LeftAlt)||Input::IsKeyPressed(Key::RightAlt)||Input::IsKeyPressed(Key::LeftSuper)||Input::IsKeyPressed(Key::RightSuper);
        if(event.GetEventType()==EventType::KeyPressed){auto& e=static_cast<KeyPressedEvent&>(event);m_UIAdapter.KeyEdge(e.GetKeyCode(),true,e.IsRepeat(),shift,command);}
        if(event.GetEventType()==EventType::KeyReleased)m_UIAdapter.KeyEdge(static_cast<KeyReleasedEvent&>(event).GetKeyCode(),false,false,shift,command);
    }
    void RuntimeSession::CancelUIGestures(){CheckThread();if(m_Scene&&m_Scene->IsGameplayInputBlocked())m_UINeutralLatch=true;m_UIAdapter.Cancel();if(m_Scene)m_Scene->CancelUIInput();}
    void RuntimeSession::RouteUIInput(const GameUI::DeviceSample& sample,float seconds,bool enabled){
        CheckThread();if(!m_Scene)return;
        if(m_UIEdgeScene!=m_Scene->GetIdentity()){m_UIAdapter.Cancel();m_UIEdgeScene=m_Scene->GetIdentity();}
        bool neutral=!sample.Mouse[size_t(m_UIAdapter.Bindings().PointerButton)];
        for(auto binding:m_UIAdapter.Bindings().Actions)for(int key:{binding.Key,binding.Alternate})if(key>=0)neutral&=!sample.Keys[size_t(key)];
        for(auto key:{Key::LeftControl,Key::RightControl,Key::LeftAlt,Key::RightAlt,Key::LeftSuper,Key::RightSuper})neutral&=!sample.Keys[key];
        if(sample.Pad.Device>=0){for(bool down:sample.Pad.Buttons)neutral&=!down;for(float axis:sample.Pad.Sticks)neutral&=std::isfinite(axis)&&std::abs(axis)<=m_UIAdapter.Bindings().NeutralZone;}
        if(neutral)m_UINeutralLatch=false;
        const auto drops=m_UIAdapter.Dropped();
        auto frame=m_UIAdapter.Sample(sample,seconds,enabled&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0)),m_Scene->IsUISliderEditing());
        if(drops!=m_UIAdapter.Dropped()){
            static auto last=std::chrono::steady_clock::time_point{};
            const auto now=std::chrono::steady_clock::now();
            if(now-last>=std::chrono::seconds(1)){HZ_CORE_WARN("UI input edge limit (64): cancelling frame; dropped {}",m_UIAdapter.Dropped());last=now;}
        }
        if(m_FramebufferExplicit)m_Scene->SetUIFramebufferSize(m_FramebufferSize);
        m_Scene->UpdateUIInput(frame);
        if((m_Scene->IsGameplayInputBlocked()||(!frame.Enabled&&enabled))&&!neutral)m_UINeutralLatch=true;
    }
    void RuntimeSession::ApplyUIBindings(const GameUI::UIBindings& bindings){
        CheckThread();if(!m_Scene)throw std::logic_error("UI overrides require an active session");
        auto text=GameUI::SerializeBindings(bindings);
        // Write first. A failed persistent write cannot publish a false accepted override.
        if(m_Project&&!m_Project->GetConfig().SaveNamespace.empty())m_Storage.Write("engine-ui-bindings",text);
        else if(m_PersistentStorage)throw std::invalid_argument("Persistent UI overrides require a project SaveNamespace");
        CancelUIGestures();m_UIAdapter.Configure(bindings);m_UIBindingWarning.clear();
    }
    void RuntimeSession::ResetUIBindings(){
        CheckThread();if(!m_Scene)throw std::logic_error("UI overrides require an active session");
        if(m_Project&&!m_Project->GetConfig().SaveNamespace.empty())m_Storage.Write("engine-ui-bindings","default");
        CancelUIGestures();m_UIAdapter.Configure(m_Project?m_Project->GetUIBindings():GameUI::UIBindings::Defaults());m_UIBindingWarning.clear();
    }

    void RuntimeSession::SetInput(glm::vec2 position, bool enabled) {
        CheckThread(); m_Mouse = position;
        m_InputEnabled = enabled&&m_Width&&m_Height&&(!m_FramebufferExplicit||(m_FramebufferSize.x>0&&m_FramebufferSize.y>0));
        m_GameplayEnabled=m_InputEnabled;
        if(!m_InputEnabled){if(m_Scene&&m_Scene->IsGameplayInputBlocked())m_UINeutralLatch=true;m_UIAdapter.Cancel();m_UIEdgeScene=0;if(m_Scene)m_Scene->CancelUIInput();}
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
