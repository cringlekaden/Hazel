#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include <Hazel/Core/FileSystem.h>
#include <Hazel/Scene/RuntimeSession.h>
#include <Hazel/Scripting/ScriptEngine.h>
#include <algorithm>
#include <iostream>

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
            m_Session.Start(m_Project, scene);
            HZ_CORE_INFO("Nutella ready: {}", m_Project->GetConfig().Name);
        }
        void OnDetach() override { m_Session.Stop(); Project::SetActive(nullptr); }
        void OnUpdate(Timestep timestep) override {
            auto& window = Application::Get().GetWindow();
            m_Session.Resize(window.GetWidth(), window.GetHeight());
            m_Session.SetInput(Input::GetMousePosition(), true);
            RenderCommand::SetClearColor({0.08f, 0.09f, 0.12f, 1.0f});
            RenderCommand::Clear();
            m_Session.Update(timestep);
        }
    private:
        Ref<Project> m_Project;
        RuntimeSession m_Session;
    };
    class Nutella : public Application {
    public:
        Nutella(const ApplicationSpecification& spec, const Ref<Project>& project) : Application(spec) {
            PushLayer(CreateScope<RuntimeLayer>(project));
        }
    };
    static std::vector<std::filesystem::path> DiscoverProjects() {
        std::vector<std::filesystem::path> paths;
        const auto directory = FileSystem::GetExecutablePath().parent_path();
        for (const auto& item : std::filesystem::directory_iterator(directory))
            if (item.is_regular_file() && item.path().extension() == ".hproj") paths.push_back(item.path());
        std::sort(paths.begin(), paths.end());
        return paths;
    }
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
                "  Nutella                        Run the only root-level .hproj beside the executable\n"
                "  Nutella --project <file.hproj>  Run a selected project (relative to invocation directory)\n"
                "  Nutella --list-projects         List sorted root-level candidates, without graphics/Mono\n"
                "  Nutella --help                  Show usage, without graphics/Mono\n"
                "A project descriptor configures StartScene and its asset root. Loose assets cannot start a game.\n"
                "Resources default to executable-adjacent Resources and mono; writable data uses platform user data.\n"
                "HAZEL_RESOURCES, HAZEL_MONO and HAZEL_DATA explicitly override those roots.\n";
            return nullptr;
        }
        if (list || selected.empty()) {
            auto paths = DiscoverProjects();
            if (list || paths.size() > 1)
                for (const auto& path : paths) std::cout << path.filename().generic_u8string() << '\n';
            if (list) return nullptr;
            if (paths.empty()) throw std::runtime_error("No root-level .hproj beside Nutella. Extract a complete project package here or use --project <file.hproj>.");
            if (paths.size() != 1) throw std::runtime_error("Multiple projects found. Select one with Nutella --project <file.hproj>.");
            selected = paths.front();
        }
        if (selected.extension() != ".hproj") throw std::runtime_error("--project requires a .hproj file");
        auto project = Project::LoadCandidate(selected);
        if (!project) throw std::runtime_error("Cannot parse/open project: " + selected.generic_u8string());
        for (const auto& reference : { project->GetConfig().StartScene, project->GetConfig().ScriptModulePath }) {
            const auto path = Project::ResolveAssetPath(project->GetAssetRoot(), reference);
            if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Missing project dependency: " + path.generic_u8string());
        }
        ApplicationSpecification spec; spec.Name = "Nutella"; spec.CommandLineArgs = args; spec.EnableImGui = false;
        return CreateScope<Nutella>(spec, project);
    }
}
