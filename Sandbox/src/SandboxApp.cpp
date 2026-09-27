#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include "Sandbox2D.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <memory>
#include <string>

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