#include "hzpch.h"
#include "Hazel/Core/Core.h"
#include "Hazel/Events/Event.h"

#include "Hazel/ImGui/ImGuiLayer.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Log.h"

#include <imgui.h>
#include <climits>
#include <imgui_internal.h>
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/FileSystem.h"
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
        HZ_PROFILE_FUNCTION();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        const float fontSize = 18.0f;
        m_IniPath = (Resources::Get().UserData / "imgui.ini").generic_u8string();
        io.IniFilename = nullptr; // Native filesystem I/O preserves Unicode paths on Windows.
        auto loadFont = [&](const char* name) {
            ScopedBuffer bytes(FileSystem::ReadFileBinary(Resources::Resolve(name)));
            if (!bytes || bytes.Size() > INT_MAX) throw std::runtime_error("Missing ImGui font");
            void* memory = IM_ALLOC(static_cast<size_t>(bytes.Size()));
            std::memcpy(memory, bytes.Data(), static_cast<size_t>(bytes.Size()));
            return io.Fonts->AddFontFromMemoryTTF(memory, static_cast<int>(bytes.Size()), fontSize);
        };
        loadFont("fonts/opensans/OpenSans-Bold.ttf");
        io.FontDefault = loadFont("fonts/opensans/OpenSans-Regular.ttf");
        const auto settings = std::filesystem::u8path(m_IniPath);
        const auto initial = std::filesystem::is_regular_file(settings) ? settings : Resources::Resolve("imgui.ini");
        ScopedBuffer ini(FileSystem::ReadFileBinary(initial));
        if (ini) ImGui::LoadIniSettingsFromMemory(reinterpret_cast<const char*>(ini.Data()), static_cast<size_t>(ini.Size()));
        ImGui::StyleColorsDark();
        SetDarkThemeColors();
        ImGuiStyle& style = ImGui::GetStyle();
        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        m_GLFWInitialized = ImGui_ImplGlfw_InitForOpenGL(window, true);
        if (!m_GLFWInitialized) throw std::runtime_error("Failed to initialize ImGui GLFW backend");
        m_OpenGLInitialized = ImGui_ImplOpenGL3_Init("#version 130");
        if (!m_OpenGLInitialized) throw std::runtime_error("Failed to initialize ImGui OpenGL backend");
    }

    void ImGuiLayer::OnDetach()
    {
        HZ_PROFILE_FUNCTION();
        if (ImGui::GetCurrentContext()) {
            if (m_OpenGLInitialized && m_GLFWInitialized) {
                size_t size = 0;
                const auto* ini = ImGui::SaveIniSettingsToMemory(&size);
                try { FileSystem::WriteFileAtomically(std::filesystem::u8path(m_IniPath), [&](std::ostream& out) { out.write(ini, size); }); }
                catch (const std::runtime_error& error) { HZ_CORE_ERROR("Save editor settings: {}", error.what()); }
            }
            if (m_OpenGLInitialized) ImGui_ImplOpenGL3_Shutdown();
            if (m_GLFWInitialized) ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
        m_OpenGLInitialized = m_GLFWInitialized = false;
    }

    void ImGuiLayer::Begin()
    {
        HZ_PROFILE_FUNCTION();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        HZ_PROFILE_FUNCTION();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupCurrentContext);
        }
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
	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}

	uint32_t ImGuiLayer::GetActiveWidgetID() const
	{
		return GImGui->ActiveId;
	}

}
