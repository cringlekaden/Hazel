#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include <Hazel/Core/FileSystem.h>
#include <Hazel/Core/InstallationLayout.h>
#include <Hazel/Scene/RuntimeSession.h>
#include <Hazel/Scripting/ScriptEngine.h>
#include <iostream>
#include <Hazel/Renderer/Framebuffer.h>

namespace Hazel {
    class RuntimeLayer : public Layer {
    public:
        explicit RuntimeLayer(Ref<Project> project) : Layer("Nutella Runtime"), m_Project(std::move(project)) {}
        void OnAttach() override {
            auto scene = m_Project->LoadScene(m_Project->GetConfig().StartScene);
            ScriptEngine::Init(Project::ResolveAssetPath(m_Project->GetAssetRoot(), m_Project->GetConfig().ScriptModulePath));
            Project::SetActive(m_Project);
            auto& window = Application::Get().GetWindow();
            m_Session.Resize(window.GetWidth(), window.GetHeight());
            m_Session.SetPersistentStorage(true);
            m_Session.Start(m_Project, scene);
            HZ_CORE_INFO("Nutella ready: {}", m_Project->GetConfig().Name);
        }
        void OnEvent(Event& e) override {m_Session.SubmitUIEvent(e,Input::GetMousePosition(),Application::Get().GetWindow().IsFocused());}
        void OnDetach() override { m_Session.Stop(); m_Composite.reset(); Project::SetActive(nullptr); }
        void OnUpdate(Timestep timestep) override {
            auto& window = Application::Get().GetWindow();
            m_Session.Resize(window.GetWidth(), window.GetHeight());
            m_Session.SetInput(Input::GetMousePosition(), window.IsFocused());
            const auto framebuffer=window.GetFramebufferSize();m_Session.ResizeFramebuffer(framebuffer.first,framebuffer.second);RenderCommand::SetViewport(0,0,framebuffer.first,framebuffer.second);
            if(!framebuffer.first || !framebuffer.second)return;
            if(!m_Composite) {
                FramebufferSpecification spec;spec.Width=framebuffer.first;spec.Height=framebuffer.second;
                spec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::Depth};m_Composite=Framebuffer::Create(spec);
            } else if(m_Composite->GetSpecification().Width!=framebuffer.first || m_Composite->GetSpecification().Height!=framebuffer.second)m_Composite->Resize(framebuffer.first,framebuffer.second);
            m_Composite->Bind();
            RenderCommand::SetClearColor(m_Session.GetScene()->Presentation().Background);
            RenderCommand::Clear();
            m_Session.Update(timestep);
            m_Composite->Unbind();m_Composite->EncodeOutput();
            if(m_Session.ConsumeExitRequest())Application::Get().Close();
        }
    private:
        Ref<Project> m_Project;
        RuntimeSession m_Session;
        Ref<Framebuffer> m_Composite;
    };
    class Nutella : public Application {
    public:
        Nutella(const ApplicationSpecification& spec, const Ref<Project>& project) : Application(spec) {
            PushLayer(CreateScope<RuntimeLayer>(project));
        }
    };
    Scope<Application> CreateApplication(ApplicationCommandLineArgs args) {
        bool help = false, list = false;
        std::filesystem::path selected;
        for (int i = 1; i < args.Count; ++i) {
            const std::string argument(args[i]);
            if (argument == "--help") { if (help) throw std::runtime_error("Duplicate --help"); help = true; }
            else if (argument == "--list-projects") { if (list) throw std::runtime_error("Duplicate --list-projects"); list = true; }
            else if (argument == "--project") {
                if (!selected.empty()) throw std::runtime_error("Specify --project only once");
                if (++i == args.Count || std::string(args[i]).rfind("--", 0) == 0) throw std::runtime_error("--project requires a .hproj path");
                selected = std::filesystem::absolute(std::filesystem::u8path(args[i]));
            } else throw std::runtime_error("Unknown argument '" + argument + "'. Use Nutella --help.");
        }
        if ((help || list) && ((help && list) || !selected.empty())) throw std::runtime_error("Use --help, --list-projects, or --project separately");
        if (help) {
            std::cout << "Nutella — Hazel standalone runtime\n"
                "  Nutella                        Run the only .hproj in the installed project directory\n"
                "  Nutella --project <file.hproj>  Run a selected project (relative to invocation directory)\n"
                "  Nutella --list-projects         List sorted root-level candidates, without graphics/Mono\n"
                "  Nutella --help                  Show usage, without graphics/Mono\n"
                "A project descriptor configures StartScene and its asset root. Loose assets cannot start a game.\n"
                "Resources/Mono/project locations follow the installation layout; writable data uses platform user data.\n"
                "HAZEL_RESOURCES, HAZEL_MONO and HAZEL_DATA explicitly override those roots.\n";
            return nullptr;
        }
        if (list || selected.empty()) {
            auto paths = Project::Discover(InstallationLayout::Current().RuntimeProject);
            if (list || paths.size() > 1)
                for (const auto& path : paths) std::cout << path.filename().generic_u8string() << '\n';
            if (list) return nullptr;
            if (paths.empty()) throw std::runtime_error("No root-level .hproj in Nutella's installation project directory. Extract a complete package or use --project <file.hproj>.");
            if (paths.size() != 1) throw std::runtime_error("Multiple projects found. Select one with Nutella --project <file.hproj>.");
            selected = paths.front();
        }
        if (selected.extension() != ".hproj") throw std::runtime_error("--project requires a .hproj file");
        auto project = Project::LoadCandidate(selected);
        if (!project) throw std::runtime_error("Cannot parse/open project: " + selected.generic_u8string());
        const auto& config = project->GetConfig();
        if (config.Name.empty() || config.StartScene.empty() || config.ScriptModulePath.empty() ||
            config.StartScene.extension() != ".hazel" || config.ScriptModulePath.extension() != ".dll")
            throw std::runtime_error("Project requires Name, a .hazel StartScene, and a .dll ScriptModulePath");
        if (!std::filesystem::is_directory(project->GetAssetRoot()))
            throw std::runtime_error("Missing project asset root: " + project->GetAssetRoot().generic_u8string());
        for (const auto& reference : { project->GetConfig().StartScene, project->GetConfig().ScriptModulePath }) {
            const auto path = Project::ResolveAssetPath(project->GetAssetRoot(), reference);
            if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Missing project dependency: " + path.generic_u8string());
        }
        ApplicationSpecification spec; spec.Name = "Nutella"; spec.CommandLineArgs = args; spec.EnableImGui = false;
        spec.Rendering = RendererPolicy::Settings(project->GetRendererRequests());
        spec.WindowVSync = project->GetRendererRequests().VSync;
        return CreateScope<Nutella>(spec, project);
    }
}
