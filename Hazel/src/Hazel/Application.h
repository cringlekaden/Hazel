#pragma once

#include "Hazel/Core.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Events/ApplicationEvent.h"
#include "Hazel/Window.h"
#include <memory>

namespace Hazel
{
    class HAZEL_API Application
    {
    public:
        Application();
        virtual ~Application();
        void Run();
        void OnEvent(Event& e);
    private:
        std::unique_ptr<Window> m_Window;
        bool m_Running = true;
        bool OnWindowClose(WindowCloseEvent& e);
    };

    // To be defined in Client App
    Application* CreateApplication();
}