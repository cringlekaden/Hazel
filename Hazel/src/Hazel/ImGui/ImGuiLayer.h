#pragma once

#include "Hazel/Layer.h"

namespace Hazel {

    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer();
        
        void OnAttach();
        void OnDetach();
        void OnUpdate();
        void OnEvent(Event& e);

        void BlockEvents(bool block) { m_BlockEvents = block; }
    private:
        bool m_BlockEvents;
    };
}
