#pragma once

#include "Hazel/Core/Layer.h"
#include "Hazel/Core/FileLease.h"

namespace Hazel {

    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer();
        
        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Event& e) override;

        void Begin();
        void End();
        uint32_t GetActiveWidgetID() const;
        void SetDarkThemeColors();
        void BlockEvents(bool block) { m_BlockEvents = block; }
        bool OwnsWorkspace() const { return bool(m_SettingsLease); }
        const std::string& InstanceToken() const { return m_InstanceToken; }
    private:
        bool m_BlockEvents = true;
        std::string m_IniPath;
        std::string m_IniOriginal, m_InstanceToken;
        bool m_IniExisted = false;
        std::unique_ptr<FileLease> m_SettingsLease;
        bool m_GLFWInitialized = false, m_OpenGLInitialized = false;
    };
}
