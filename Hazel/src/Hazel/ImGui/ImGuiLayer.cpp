#include "Hazel/Core.h"
#include "Hazel/Events/Event.h"
#include "hzpch.h"

#include "Hazel/ImGui/ImGuiLayer.h"
#include "Hazel/Application.h"
#include "Hazel/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

namespace Hazel {

    ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer")
    {
    }

    ImGuiLayer::~ImGuiLayer()
    {
    }

    void ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui::StyleColorsDark();
        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        bool glfwStatus = ImGui_ImplGlfw_InitForOpenGL(window, true);
        HZ_CORE_ASSERT(glfwStatus, "Failed to initialize ImGui GLFW backend...");
        bool openglStatus = ImGui_ImplOpenGL3_Init("#version 130");
        HZ_CORE_ASSERT(openglStatus, "Failed to initialize ImGui OpenGL3 backend...");
    }

    void ImGuiLayer::OnDetach()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::OnUpdate()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        static bool show = true;
        ImGui::ShowDemoWindow(&show);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiLayer::OnEvent(Event& e)
    {
        if(m_BlockEvents)
        {
            ImGuiIO& io = ImGui::GetIO();
            e.Handled |= e.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse;
            e.Handled |= e.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard;
        }
    }
}