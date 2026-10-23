#pragma once
#include "Hazel/Core/Base.h"
#include "Hazel/UI/UIActions.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Core/Timestep.h"
#include "Hazel/Project/RuntimeStorage.h"
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <glm/glm.hpp>

namespace Hazel {
    class Scene;
    class Project;

    // Main-thread owner of one Play/player lifetime. Scene Refs may be retained by
    // panels; entity observations must be cleared whenever GetScene changes.
    // ScriptEngine borrows this session between Start and Stop, including OnCreate.
    class RuntimeSession {
    public:
        RuntimeSession();
        ~RuntimeSession();
        RuntimeSession(const RuntimeSession&) = delete;
        RuntimeSession& operator=(const RuntimeSession&) = delete;
        static void Validate(const Ref<Scene>& scene, bool validateScripts = true);
        void Start(const Ref<Project>& project, const Ref<Scene>& authoredScene);
        void Stop();
        // Hosts opt in before Start. Editor default remains isolated memory-only.
        void SetPersistentStorage(bool enabled) { CheckThread(); if(m_Scene)throw std::logic_error("Configure storage before Start");m_PersistentStorage=enabled; }
        RuntimeStorage& Storage() { CheckThread();if(!m_Scene)throw std::logic_error("Storage needs an active session");return m_Storage; }
        void Update(Timestep timestep);
        bool RequestSceneLoad(const std::filesystem::path& assetReference);
        const Ref<Scene>& GetScene() const { return m_Scene; }
        uint64_t GetIdentity() const { return m_Identity; }
        const std::string& GetError() const { return m_Error; }
        void Resize(uint32_t width, uint32_t height);
        void ResizeFramebuffer(uint32_t width, uint32_t height);
        // Host supplies viewport-local coordinates, top-left origin. Inactive
        // viewports suppress input; the engine has no editor/ImGui dependency.
        void SetInput(glm::vec2 mousePosition, bool enabled);
        void SetGameplayInputEnabled(bool enabled) {CheckThread();m_GameplayEnabled=enabled;}
        bool IsInputEnabled() const { return m_InputEnabled; }
        bool IsGameplayInputEnabled() const;
        bool IsGameplayMouseEnabled() const;
        void SubmitUIEvent(Event& event,glm::vec2 viewportMouse,bool enabled);

        const GameUI::UIBindings& GetUIBindings() const {return m_UIAdapter.Bindings();}
        GameUI::UIAction LastUIAction() const {return m_UIAdapter.LastAction();}
        bool IsUIGamepadArmed() const {return m_UIAdapter.GamepadReady();}
        GameUI::InputDevice GetUIDevice() const {return m_UIAdapter.Device();}
        uint64_t DroppedUIEdges() const {return m_UIAdapter.Dropped();}
        const std::string& UIBindingWarning() const {return m_UIBindingWarning;}
        void ApplyUIBindings(const GameUI::UIBindings& bindings); // Per-user full override; disk only for persistent sessions.
        void ResetUIBindings();
        void CancelUIGestures();
        // Same production adapter/router boundary, with caller-supplied samples for deterministic hosts/tests.
        void RouteUIInput(const GameUI::DeviceSample& sample,float seconds,bool enabled);
        bool GetMouseWorldPosition(glm::vec2& position) const;
    private:
        void CheckThread() const;
        void CommitPendingTransition();
        Ref<Project> m_Project;
        Ref<Scene> m_Scene;
        std::optional<std::filesystem::path> m_Pending;
        std::thread::id m_Thread;
        uint64_t m_Identity = 0;
        uint32_t m_Width = 0, m_Height = 0;
        glm::vec2 m_FramebufferSize{};
        bool m_FramebufferExplicit = false;
        glm::vec2 m_Mouse = {};
        GameUI::UIInputAdapter m_UIAdapter;
        std::string m_UIBindingWarning;
        uint64_t m_UIEdgeScene=0;
        bool m_GameplayEnabled=false,m_UINeutralLatch=false;
        bool m_InputEnabled = false, m_Stopping = false, m_Updating = false;
        std::string m_Error;
        RuntimeStorage m_Storage;
        bool m_PersistentStorage=false;
    };
}
