#include "hzpch.h"
#include "Hazel/Core/Core.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Events/KeyEvent.h"
#include "Hazel/Events/MouseEvent.h"
#include "Hazel/Events/ApplicationEvent.h"

#include "Hazel/ImGui/ImGuiLayer.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Log.h"

#include <imgui.h>
#include <climits>
#include <imgui_internal.h>
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/UUID.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include "Hazel/Renderer/Renderer.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Platform/macOS/Metal/MetalDevice.h"
#include "Hazel/Renderer/RenderCommand.h"
#endif

namespace Hazel {
    namespace {
        void (*s_CreatePlatformWindow)(ImGuiViewport*) = nullptr;
        void (*s_DestroyPlatformWindow)(ImGuiViewport*) = nullptr;
        void ForwardPlatformEvents() {
            auto& platform = ImGui::GetPlatformIO();
            s_CreatePlatformWindow = platform.Platform_CreateWindow;
            s_DestroyPlatformWindow = platform.Platform_DestroyWindow;
            platform.Platform_CreateWindow = [](ImGuiViewport* viewport) {
                s_CreatePlatformWindow(viewport);
                auto* window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
                // The official backend owns these GLFW windows. Forward input
                // explicitly; its main-window callback cannot read their user pointer.
                glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scan, int action, int mods) {
                    ImGui_ImplGlfw_KeyCallback(w,key,scan,action,mods);
                    const int modifiers = ((mods&GLFW_MOD_SHIFT)?1:0)|((mods&GLFW_MOD_CONTROL)?2:0)|((mods&GLFW_MOD_ALT)?4:0)|((mods&GLFW_MOD_SUPER)?8:0);
                    if (action == GLFW_RELEASE) { KeyReleasedEvent event(static_cast<KeyCode>(key)); Application::Get().OnEvent(event); }
                    else { KeyPressedEvent event(static_cast<KeyCode>(key),action==GLFW_REPEAT,modifiers); Application::Get().OnEvent(event); }
                });
                glfwSetCharCallback(window, [](GLFWwindow* w,unsigned int code) {
                    ImGui_ImplGlfw_CharCallback(w,code); KeyTypedEvent event(code); Application::Get().OnEvent(event);
                });
                glfwSetMouseButtonCallback(window, [](GLFWwindow* w,int button,int action,int mods) {
                    ImGui_ImplGlfw_MouseButtonCallback(w,button,action,mods);
                    if (action == GLFW_PRESS) { MouseButtonPressedEvent event(static_cast<MouseCode>(button)); Application::Get().OnEvent(event); }
                    else { MouseButtonReleasedEvent event(static_cast<MouseCode>(button)); Application::Get().OnEvent(event); }
                });
                glfwSetScrollCallback(window, [](GLFWwindow* w,double x,double y) {
                    ImGui_ImplGlfw_ScrollCallback(w,x,y); MouseScrolledEvent event{float(x),float(y)}; Application::Get().OnEvent(event);
                });
                glfwSetWindowFocusCallback(window, [](GLFWwindow* w,int focused) {
                    ImGui_ImplGlfw_WindowFocusCallback(w,focused);
                    if (!focused) { WindowLostFocusEvent event; Application::Get().OnEvent(event); }
                });
            };
            platform.Platform_DestroyWindow = [](ImGuiViewport* viewport) {
                if (auto* ui = Application::Get().GetImGuiLayer()) ui->ReleasePlatformWindow(viewport->PlatformHandle);
                s_DestroyPlatformWindow(viewport);
            };
        }
        ImVec2 FramebufferScale(ImGuiViewport* viewport) {
            auto scale=ImGui::GetIO().DisplayFramebufferScale;
            if(auto* window=static_cast<GLFWwindow*>(viewport->PlatformHandle)) {
                int width=0,height=0,pixelsX=0,pixelsY=0;
                glfwGetWindowSize(window,&width,&height);glfwGetFramebufferSize(window,&pixelsX,&pixelsY);
                if(width>0 && height>0 && pixelsX>0 && pixelsY>0)
                    scale={float(pixelsX)/width,float(pixelsY)/height};
            }
            return scale;
        }
    }
    std::pair<float,float> ImGuiLayer::CurrentWindowFramebufferScale() const {
        const auto scale=FramebufferScale(ImGui::GetWindowViewport());
        return {scale.x,scale.y};
    }

    void* ImGuiLayer::InputWindow() const {
        if (m_PointerWindow) return m_PointerWindow;
        for (auto* viewport : ImGui::GetPlatformIO().Viewports)
            if (auto* window = static_cast<GLFWwindow*>(viewport->PlatformHandle))
                if (glfwGetWindowAttrib(window, GLFW_FOCUSED)) return window;
        return Application::Get().GetWindow().GetNativeWindow();
    }
    bool ImGuiLayer::InputFocused() const { return glfwGetWindowAttrib(static_cast<GLFWwindow*>(InputWindow()), GLFW_FOCUSED) != 0; }
    void ImGuiLayer::CapturePointer(bool captured) {
        if (!captured) {
            if (m_PointerWindow) glfwSetInputMode(static_cast<GLFWwindow*>(m_PointerWindow), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_PointerWindow = nullptr;
        } else if (!m_PointerWindow) {
            m_PointerWindow = InputWindow();
            glfwSetInputMode(static_cast<GLFWwindow*>(m_PointerWindow), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    void ImGuiLayer::ReleasePlatformWindow(void* native) { if (m_PointerWindow == native) CapturePointer(false); }
    void ImGuiLayer::Image(const EditorTexture& texture, float width, float height) {
        ImGui::Image(Texture(texture), {width,height}, {0,texture.FlipY ? 1.f : 0.f}, {1,texture.FlipY ? 0.f : 1.f});
    }
    void* ImGuiLayer::Texture(const EditorTexture& texture) {
        auto* layer = Application::Get().GetImGuiLayer();
        if (!layer || !texture.Handle) return nullptr;
        layer->m_Textures.push_back(texture);
        return reinterpret_cast<void*>(texture.Handle);
    }

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
        m_InstanceToken=std::to_string(static_cast<uint64_t>(UUID()));
        try {m_SettingsLease=FileLease::Try(Resources::Get().UserData/"editor-session.lock");}
        catch(const std::exception& error){HZ_CORE_WARN("Editor state ownership: {}. Using an instance snapshot",error.what());}
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
        m_IniExisted=std::filesystem::is_regular_file(settings);
        if(m_IniExisted)m_IniOriginal=FileDocument::Read(settings);
        // Only unreachable detached hosts move. Dock IDs, order and main-window
        // settings remain ImGui-owned; monitor recovery never rebuilds the dock tree.
        const auto displays=Application::Get().GetWindow().GetDisplayAreas();
        auto& context=*ImGui::GetCurrentContext();
        for(auto* entry=context.SettingsWindows.begin();entry;entry=context.SettingsWindows.next_chunk(entry)) {
            if(entry->DockId || !entry->ViewportId || entry->ViewportId==ImGui::GetMainViewport()->ID)continue;
            WindowPlacement p;p.X=entry->ViewportPos.x+entry->Pos.x;p.Y=entry->ViewportPos.y+entry->Pos.y;p.Width=entry->Size.x;p.Height=entry->Size.y;
            bool reachable=false;
            for(const auto& d:displays)if(p.X+48>d.X && p.Y+32>d.Y && p.X<d.X+d.Width-48 && p.Y<d.Y+d.Height-32)reachable=true;
            if(!reachable){p=FitWindow(p,displays);entry->ViewportPos=ImVec2ih(p.X-entry->Pos.x,p.Y-entry->Pos.y);entry->Size=ImVec2ih(p.Width,p.Height);}
        }
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
#ifdef HZ_PLATFORM_MACOS
        if (Renderer::GetAPI() == RendererAPI::API::Metal) {
            m_GLFWInitialized = ImGui_ImplGlfw_InitForOther(window, true);
            if (!m_GLFWInitialized) throw std::runtime_error("Failed to initialize ImGui GLFW backend");
            ForwardPlatformEvents();
            static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device()->InitEditor();
            return;
        }
#endif
        m_GLFWInitialized = ImGui_ImplGlfw_InitForOpenGL(window, true);
        if (!m_GLFWInitialized) throw std::runtime_error("Failed to initialize ImGui GLFW backend");
        ForwardPlatformEvents();
        m_OpenGLInitialized = ImGui_ImplOpenGL3_Init("#version 410 core");
        if (!m_OpenGLInitialized) throw std::runtime_error("Failed to initialize ImGui OpenGL backend");
    }

    void ImGuiLayer::OnDetach()
    {
        HZ_PROFILE_FUNCTION();
        if (ImGui::GetCurrentContext()) {
            CapturePointer(false);
            if (m_GLFWInitialized && ImGui::GetIO().BackendRendererUserData) {
                size_t size = 0;
                const auto* ini = ImGui::SaveIniSettingsToMemory(&size);
                try {
                    auto path=std::filesystem::u8path(m_IniPath);
                    bool unchanged=std::filesystem::exists(path)==m_IniExisted;
                    if(unchanged && m_IniExisted)unchanged=FileDocument::Read(path)==m_IniOriginal;
                    if(!m_SettingsLease || !unchanged) {
                        path=Resources::Get().UserData/"instances"/(m_InstanceToken+".imgui.ini");
                        std::filesystem::create_directories(path.parent_path());
                        if(!unchanged)HZ_CORE_WARN("Layout changed externally; original retained, saving instance snapshot");
                    }
                    FileSystem::WriteFileAtomically(path, [&](std::ostream& out) { out.write(ini, size); });
                }
                catch (const std::runtime_error& error) { HZ_CORE_ERROR("Save editor settings: {}", error.what()); }
            }
#ifdef HZ_PLATFORM_MACOS
            if (Renderer::GetAPI() == RendererAPI::API::Metal)
                static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device()->ShutdownEditor();
#endif
            if (m_OpenGLInitialized) ImGui_ImplOpenGL3_Shutdown();
            if (m_GLFWInitialized) ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
        m_OpenGLInitialized = m_GLFWInitialized = false;
    }

    void ImGuiLayer::Begin()
    {
        HZ_PROFILE_FUNCTION();
        m_Textures.clear();
        if (m_OpenGLInitialized) ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        HZ_PROFILE_FUNCTION();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Render();
        ImGui::GetDrawData()->FramebufferScale=FramebufferScale(ImGui::GetMainViewport());
        if (m_OpenGLInitialized) ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            // The pinned ImGui core inherits the main window's scale for every
            // draw list. Resolve each native host here, at the GLFW integration
            // boundary, so detached windows can move between unlike displays.
            for(auto* viewport:ImGui::GetPlatformIO().Viewports)
                if(viewport->DrawData)viewport->DrawData->FramebufferScale=FramebufferScale(viewport);
            if (m_OpenGLInitialized) {
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backupCurrentContext);
            }
        }
#ifdef HZ_PLATFORM_MACOS
        if (Renderer::GetAPI() == RendererAPI::API::Metal)
            static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device()->QueueEditorTextures(std::move(m_Textures));
#endif
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
