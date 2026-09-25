#include "Hazel/Events/Event.h"
#include "Hazel/Log.h"
#include <Hazel.h>

#include <imgui.h>

class ExampleLayer : public Hazel::Layer
{
public:
    ExampleLayer() : Layer("Example")
    {
    }

    void OnUpdate() override
    {
    }

    void OnEvent(Hazel::Event&) override;

    void OnImGuiRender() override
    {
        static bool showDemo = true;
        ImGui::ShowDemoWindow(&showDemo);
        ImGui::Begin("Test");
        ImGui::Text("Hello world!");
        ImGui::End();
    }
};

class Sandbox : public Hazel::Application
{
public:
    Sandbox()
    {
        PushLayer(new ExampleLayer());
    }

    ~Sandbox()
    {

    }
};

Hazel::Application* Hazel::CreateApplication()
{
    return new Sandbox();
}