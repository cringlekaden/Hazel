#pragma once

#include "Hazel/Core.h"
#include "Hazel/Window.h"
#include "Hazel/LayerStack.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Events/ApplicationEvent.h"
#include "Hazel/ImGui/ImGuiLayer.h"

namespace Hazel
{
    class HAZEL_API Application
    {
    public:
        Application();
        virtual ~Application();
        void Run();
        void OnEvent(Event& e);
        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);
        Window& GetWindow() { return *m_Window; }
        static Application& Get() { return *s_Instance; }
    private:
        static Application* s_Instance;
        std::unique_ptr<Window> m_Window;
        LayerStack m_LayerStack;
        ImGuiLayer* m_ImGuiLayer;
        bool m_Running = true;
        bool OnWindowClose(WindowCloseEvent& e);
    };

    // To be defined in Client App
    Application* CreateApplication();
}