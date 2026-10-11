#pragma once
#include "Hazel/Core/Base.h"
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
        // Host supplies viewport-local coordinates, top-left origin. Inactive
        // viewports suppress input; the engine has no editor/ImGui dependency.
        void SetInput(glm::vec2 mousePosition, bool enabled);
        bool IsInputEnabled() const { return m_InputEnabled; }
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
        glm::vec2 m_Mouse = {};
        bool m_InputEnabled = false, m_Stopping = false, m_Updating = false;
        std::string m_Error;
        RuntimeStorage m_Storage;
        bool m_PersistentStorage=false;
    };
}
