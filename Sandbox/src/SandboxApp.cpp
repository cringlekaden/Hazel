#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include "Sandbox2D.h"
#include "ExampleLayer.h"
#include <string_view>

class Sandbox : public Hazel::Application
{
public:
    Sandbox(const Hazel::ApplicationSpecification& specification) : Application(specification)
    {
        if (specification.CommandLineArgs.Count>1 && std::string_view(specification.CommandLineArgs[1])=="--example-layer")
            PushLayer(Hazel::CreateScope<ExampleLayer>());
        else
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
