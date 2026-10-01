#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include "Sandbox2D.h"

class Sandbox : public Hazel::Application
{
public:
    Sandbox(const Hazel::ApplicationSpecification& specification) : Application(specification)
    {
        PushLayer(Hazel::CreateScope<Sandbox2D>());
    }
};

Hazel::Scope<Hazel::Application> Hazel::CreateApplication(Hazel::ApplicationCommandLineArgs args)
{
    Hazel::ApplicationSpecification specification;
    specification.Name = "Hazel Engine";
    specification.CommandLineArgs = args;
    return Hazel::CreateScope<Sandbox>(specification);
}
