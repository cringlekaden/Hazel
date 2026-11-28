#include <Hazel.h>
#include <Hazel/Core/EntryPoint.h>
#include <Hazel/Core/FileSystem.h>
#include <Hazel/Core/InstallationLayout.h>
#include <Hazel/Scene/RuntimeSession.h>
#include <Hazel/Scene/RuntimeCameraTour.h>
#include <Hazel/Events/KeyEvent.h>
#include <Hazel/Scripting/ScriptEngine.h>
#include <iostream>
#include <Hazel/Renderer/Resources/Framebuffer.h>
#include <Hazel/Renderer/Cameras/OrthographicProjection.h>
#include "Platform/macOS/Metal/MetalMemoryAdmissionError.h"

namespace Hazel {
    class RuntimeLayer : public Layer {
    public:
        explicit RuntimeLayer(Ref<Project> project,bool inspect) : Layer("Nutella Runtime"), m_Inspect(inspect),m_Project(std::move(project)) {}
        void OnAttach() override {
            auto scene = m_Project->LoadScene(m_Project->GetConfig().StartScene);
            const auto module=Project::ResolveAssetPath(m_Project->GetAssetRoot(),m_Project->GetConfig().ScriptModulePath);
            // Match native editor readiness: content-only scenes need no assembly.
            // Existing modules still load for later scripted scene transitions;
            // RuntimeSession validates every assigned class before publication.
            if(std::filesystem::is_regular_file(module))ScriptEngine::Init(module);
            Project::SetActive(m_Project);
            auto& window = Application::Get().GetWindow();
            m_Session.Resize(window.GetWidth(), window.GetHeight());
            m_Session.SetPersistentStorage(true);
            m_Session.Start(m_Project, scene);
            const auto& track = m_Project->GetConfig().CameraTour;
            if (!track.empty()) {
                auto camera = m_Session.GetScene()->GetPrimaryCameraEntity();
                if (!camera || uint64_t(m_Session.GetScene()->GetRelationship(camera).Parent) != 0 ||
                    camera.GetComponent<CameraComponent>().Camera.GetProjectionType() != SceneCamera::ProjectionType::Perspective)
                    throw std::runtime_error("Camera tour requires a root primary perspective camera");
                m_Tour.emplace(Project::ResolveOwnedAsset(m_Project->GetAssetRoot(), track));
                m_TourCamera = camera.GetUUID();
                m_TourScene = m_Session.GetScene()->GetIdentity();
                m_Original = camera.GetComponent<CameraComponent>();
                m_Tour->Apply(camera, m_Original, 0);
                HZ_CORE_INFO("Camera tour ready: {:.1f}s loop; Space pause, R restart, I inspect, [/] speed", m_Tour->Duration());
            }
            if(m_Inspect)m_Session.SetCameraInspection(true);
            HZ_CORE_INFO("Nutella ready: {}", m_Project->GetConfig().Name);
        }
        void OnEvent(Event& e) override {
            const bool focused = Application::Get().GetWindow().IsFocused();
            if (focused && TourActive() && e.GetEventType() == EventType::KeyPressed) {
                const auto& key = static_cast<const KeyPressedEvent&>(e);
                if (!key.IsRepeat() && TourKey(key.GetKeyCode())) {
                    e.Handled = true;
                    return;
                }
            }
            m_Session.SubmitUIEvent(e, Input::GetMousePosition(), focused);
        }
        void OnDetach() override { m_Session.Stop(); m_Composite.reset(); Project::SetActive(nullptr); }
        void OnUpdate(Timestep timestep) override try {
            auto& window = Application::Get().GetWindow();
            m_Session.Resize(window.GetWidth(), window.GetHeight());
            m_Session.SetInput(Input::GetMousePosition(), window.IsFocused());
            m_Session.UpdateCameraInspection(timestep,window.IsFocused(),true);
            const auto framebuffer=window.GetFramebufferSize();m_Session.ResizeFramebuffer(framebuffer.first,framebuffer.second);RenderCommand::SetViewport(0,0,framebuffer.first,framebuffer.second);
            if(!framebuffer.first || !framebuffer.second)return;
            if(!m_Composite) {
                FramebufferSpecification spec;spec.Width=framebuffer.first;spec.Height=framebuffer.second;
                const bool metal = Renderer::GetAPI() == RendererAPI::API::Metal;
                spec.Attachments = {metal ? FramebufferTextureFormat::RGBA32F : FramebufferTextureFormat::RGBA16F,
                                    FramebufferTextureFormat::RED_INTEGER,
                                    metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
                m_Composite = Framebuffer::Create(spec);
            } else if(m_Composite->GetSpecification().Width!=framebuffer.first || m_Composite->GetSpecification().Height!=framebuffer.second)m_Composite->Resize(framebuffer.first,framebuffer.second);
            if (TourActive() && !m_Session.IsCameraInspectionEnabled()) {
                auto camera = m_Session.GetScene()->GetEntityByUUID(m_TourCamera);
                if (camera && camera.HasComponent<CameraComponent>()) {
                    m_Tour->Apply(camera, m_Original, m_TourTime);
                    auto& state = camera.GetComponent<CameraComponent>();
                    state.DepthOfField.Enabled = m_Original.DepthOfField.Enabled && m_Defocus;
                    state.MotionBlur.Enabled = m_Original.MotionBlur.Enabled && m_Shutter;
                    if (!m_Flare) state.LensEffects.FlareStrength = 0;
                    if (!m_Dirt) state.LensEffects.DirtStrength = 0;
                    state.ColorGrade = m_Grade ? m_Original.ColorGrade : ColorGradeSettings{};
                    state.ExposureOverride = m_Original.ExposureOverride;
                    if (m_Manual) {
                        CameraExposure exposure;
                        const auto& lens = *state.Camera.GetPhysicalCamera();
                        exposure.CompensationStops = float(std::clamp(lens.EV100() - PhysicalCamera::ReferenceEV100, -16., 16.));
                        state.ExposureOverride = exposure;
                    }
                }
                if (!m_Paused)
                    m_TourTime = std::fmod(m_TourTime + std::clamp(double(timestep.GetSeconds()), 0., .1) * m_TourSpeed,
                                          m_Tour->Duration());
            }
            m_Composite->Bind();
            if (Renderer::GetAPI() == RendererAPI::API::Metal) m_Composite->Clear(m_Session.GetScene()->Presentation().Background);
            else { RenderCommand::SetClearColor(m_Session.GetScene()->Presentation().Background); RenderCommand::Clear(); }
            m_Composite->ClearAttachment(1, -1);
            m_Session.SetRenderTarget(m_Composite);
            m_Session.Update(timestep);
            if(m_Session.IsCameraInspectionEnabled()){
                RenderCommand::StateScope scope;auto state=RenderCommand::GetRasterState();state.DepthTest=state.DepthWrite=false;state.PickingMask=glm::bvec4(false);RenderCommand::SetRasterState(state);
                Camera overlay(OrthographicProjection::Matrix(0,float(window.GetWidth()),0,float(window.GetHeight()),-1,1));
                Renderer2D::BeginScene(overlay,glm::mat4(1));
                Renderer2D::DrawQuad(glm::vec3(window.GetWidth()*.5f,window.GetHeight()-14.f,0),glm::vec2(window.GetWidth(),28),{.025f,.025f,.025f,1});
                Renderer2D::DrawString("INSPECT: Alt/Space drag orbit | Shift drag pan | RMB + WASD Q/E | Shift fast Ctrl slow | Home reset | Esc release",Font::GetDefault(),glm::translate(glm::mat4(1),{8,window.GetHeight()-19.f,0})*glm::scale(glm::mat4(1),glm::vec3(13)),Renderer2D::TextParams{});
                Renderer2D::EndScene();
            }
            m_Composite->Unbind();m_Composite->EncodeOutput();
            m_MemoryError.clear();
            if(m_Session.ConsumeExitRequest())Application::Get().Close();
        }
        catch (const MetalMemoryAdmissionError& error) {
            if (m_Composite) m_Composite->Cancel();
            if (m_MemoryError != error.what()) HZ_CORE_WARN("Nutella frame deferred: {}", error.what());
            m_MemoryError = error.what();
        }
    private:
        bool TourActive() const {
            return m_Tour && m_Session.GetScene() && m_Session.GetScene()->GetIdentity() == m_TourScene;
        }
        bool TourKey(KeyCode key) {
            if (key == Key::I) {
                m_Session.SetCameraInspection(!m_Session.IsCameraInspectionEnabled());
                return true;
            }
            auto& settings = m_Session.GetScene()->Presentation();
            if (key == Key::T) { settings.TemporalAA = !settings.TemporalAA; return true; }
            if (key == Key::Y) { settings.ScreenReflections.Enabled = !settings.ScreenReflections.Enabled; return true; }
            if (m_Session.IsCameraInspectionEnabled()) return false;
            switch (key) {
                case Key::Space: m_Paused = !m_Paused; break;
                case Key::R: m_TourTime = 0; m_Session.ResetCameraHistory(); break;
                case Key::LeftBracket: m_TourSpeed = std::max(.25, m_TourSpeed / 1.25); break;
                case Key::RightBracket: m_TourSpeed = std::min(2., m_TourSpeed * 1.25); break;
                case Key::D: m_Defocus = !m_Defocus; break;
                case Key::F: m_Flare = !m_Flare; break;
                case Key::G: m_Dirt = !m_Dirt; break;
                case Key::M: m_Shutter = !m_Shutter; break;
                case Key::E: m_Manual = !m_Manual; break;
                case Key::C: m_Grade = !m_Grade; break;
                default: return false;
            }
            return true;
        }
        bool m_Inspect=false;
        std::optional<RuntimeCameraTour> m_Tour;
        uint64_t m_TourCamera = 0, m_TourScene = 0;
        CameraComponent m_Original;
        double m_TourTime = 0, m_TourSpeed = 1;
        bool m_Paused = false, m_Defocus = true, m_Shutter = true, m_Flare = true, m_Dirt = true;
        bool m_Grade = true, m_Manual = false;
        Ref<Project> m_Project;
        RuntimeSession m_Session;
        Ref<Framebuffer> m_Composite;
        std::string m_MemoryError;
    };
    class Nutella : public Application {
    public:
        Nutella(const ApplicationSpecification& spec, const Ref<Project>& project,bool inspect) : Application(spec) {
            PushLayer(CreateScope<RuntimeLayer>(project,inspect));
        }
    };
    Scope<Application> CreateApplication(ApplicationCommandLineArgs args) {
        bool help = false, list = false,inspect=false;
        std::optional<RendererBackend> backend;
        std::filesystem::path selected;
        for (int i = 1; i < args.Count; ++i) {
            const std::string argument(args[i]);
            if(argument=="--metal" || argument=="--metal-renderer2d" || argument=="--opengl" || argument=="--automatic") {
                if(backend) throw std::runtime_error("Specify only one backend override: --metal, --opengl or --automatic");
                backend = argument=="--opengl" ? RendererBackend::OpenGL : argument=="--automatic" ? RendererBackend::Automatic : RendererBackend::Metal;
            }
            else if(argument=="--inspect-camera"){if(inspect)throw std::runtime_error("Duplicate --inspect-camera");inspect=true;}
            else if (argument == "--help") { if (help) throw std::runtime_error("Duplicate --help"); help = true; }
            else if (argument == "--list-projects") { if (list) throw std::runtime_error("Duplicate --list-projects"); list = true; }
            else if (argument == "--project") {
                if (!selected.empty()) throw std::runtime_error("Specify --project only once");
                if (++i == args.Count || std::string(args[i]).rfind("--", 0) == 0) throw std::runtime_error("--project requires a .hproj path");
                selected = std::filesystem::absolute(std::filesystem::u8path(args[i]));
            } else throw std::runtime_error("Unknown argument '" + argument + "'. Use Nutella --help.");
        }
        if ((help || list) && ((help && list) || !selected.empty() || inspect || backend)) throw std::runtime_error("Use --help, --list-projects, or --project separately");
        if (help) {
            std::cout << "Nutella — Hazel standalone runtime\n"
                "  Nutella --metal                     Select Metal 4 (--opengl or --automatic also available)\n"
                "  Nutella                        Run the only .hproj in the installed project directory\n"
                "  Nutella --project <file.hproj>  Run a selected project (relative to invocation directory)\n"
                "  Nutella --inspect-camera        Opt in to transient perspective inspection; Home restores the authored view\n"
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
        const auto start=Project::ResolveAssetPath(project->GetAssetRoot(),config.StartScene);
        if(!std::filesystem::is_regular_file(start))throw std::runtime_error("Missing project dependency: "+start.generic_u8string());
        // The descriptor retains the eventual script-module location. Its file
        // is optional for native content; RuntimeSession rejects assigned classes
        // that are unavailable. Do not impose scripting readiness at CLI launch.
        ApplicationSpecification spec; spec.Name = "Nutella"; spec.CommandLineArgs = args; spec.EnableImGui = false;
        spec.ProjectBackend = project->GetRendererRequests().Backend;
        spec.RequestedBackend = backend.value_or(spec.ProjectBackend);
        spec.BackendCLIOverride = backend.has_value();
        spec.GraphicsAPI = RendererPolicy::SelectBackend(spec.ProjectBackend, backend, RendererAPI::GetBackendAvailability()) == RendererBackend::Metal ? RendererAPI::API::Metal : RendererAPI::API::OpenGL;
        spec.Rendering = RendererPolicy::Settings(project->GetRendererRequests());
        spec.WindowVSync = project->GetRendererRequests().VSync;
        return CreateScope<Nutella>(spec, project,inspect);
    }
}
