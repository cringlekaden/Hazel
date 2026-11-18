#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>

#include "Authoring/ConsoleModel.h"
#include "Authoring/RendererLaunch.h"
#include "EditorLayer.h"

namespace Hazel {

    class EarlyConsole {
      protected:
        Scope<ConsoleSession> m_Console;
        explicit EarlyConsole(Scope<ConsoleSession> console) : m_Console(std::move(console)) {}
    };

    class Hazelnut : private EarlyConsole, public Application {
      public:
        Hazelnut(const ApplicationSpecification &spec, Scope<ConsoleSession> console)
            : EarlyConsole(std::move(console)), Application(spec) {
            PushLayer(CreateScope<EditorLayer>(m_Console->Model));
        }
    };

    Scope<Application> CreateApplication(ApplicationCommandLineArgs args) {
        auto console =
            CreateScope<ConsoleSession>(); // Captures CPU prelaunch and renderer startup alike.
        ApplicationSpecification spec;
        spec.Name = "Hazelnut";
        spec.CommandLineArgs = args;

        spec.Resources = Resources::Defaults(spec.Name, spec.Resources);
        Resources::Configure(spec.Resources);
        std::vector<std::string> arguments;
        for (int i = 0; i < args.Count; ++i)
            arguments.emplace_back(args[i]);
        std::optional<RendererBackend> backend;
        for (const auto& argument : arguments) {
            if (argument != "--metal" && argument != "--opengl" && argument != "--automatic") continue;
            if (backend) throw std::invalid_argument("Specify only one backend override: --metal, --opengl or --automatic");
            backend = argument == "--metal" ? RendererBackend::Metal : argument == "--opengl" ? RendererBackend::OpenGL : RendererBackend::Automatic;
        }
        const auto rendering = EditorRendererLaunch::Read(arguments);
        spec.ProjectBackend = rendering.Backend;
        spec.RequestedBackend = backend.value_or(rendering.Backend);
        spec.BackendCLIOverride = backend.has_value();
        spec.GraphicsAPI = RendererPolicy::SelectBackend(rendering.Backend, backend, RendererAPI::GetBackendAvailability()) == RendererBackend::Metal ? RendererAPI::API::Metal : RendererAPI::API::OpenGL;
        spec.Rendering = rendering.Settings;
        spec.WindowVSync = rendering.VSync;
        HZ_CORE_INFO("Editor renderer launch policy: {}",
                     rendering.Project.empty() ? "engine defaults"
                                               : rendering.Project.generic_u8string());
        return CreateScope<Hazelnut>(spec, std::move(console));
    }

} // namespace Hazel
