#pragma once

#include "Hazel/Core/Layer.h"

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
    private:
        bool m_BlockEvents = true;
        std::string m_IniPath;
        bool m_GLFWInitialized = false, m_OpenGLInitialized = false;
    };
}
