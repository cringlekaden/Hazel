#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include "Sandbox2D.h"

class Sandbox : public Hazel::Application
{
public:
    Sandbox()
    {
        PushLayer(Hazel::CreateScope<Sandbox2D>());
    }
};

Hazel::Scope<Hazel::Application> Hazel::CreateApplication()
{
    return Hazel::CreateScope<Sandbox>();
}
