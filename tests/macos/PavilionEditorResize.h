#pragma once

// Included by the native EditorSmoke host after its platform headers.
namespace Hazel
{
static int PavilionEditorResize(const std::filesystem::path& project,
    const std::filesystem::path& resources, bool metal, const std::filesystem::path& output)
{
    Log::Init();
    ConsoleSession console;
    std::shared_ptr<MetalProofObserver> observer;
    std::shared_ptr<MetalEditorCapture> capture;
    {
        auto name = std::string("Pavilion editor resize");
        auto descriptor = project.generic_u8string();
        char* args[] = {name.data(), descriptor.data()};
        ApplicationSpecification spec;
        spec.Name = name;
        spec.Resources.Root = resources;
        spec.GraphicsAPI = metal ? RendererAPI::API::Metal : RendererAPI::API::OpenGL;
        spec.CommandLineArgs = {2, args};
        Application app(spec);
        MetalRendererAPI* backend = nullptr;
        if (metal) {
            backend = &static_cast<MetalRendererAPI&>(RenderCommand::Backend());
            backend->EnableEditorReadbackForTesting();
            observer = backend->Observe();
        }
        app.PushLayer(CreateScope<EditorLayer>(console.Model));
        auto& host = app.GetWindow();
        auto* window = static_cast<GLFWwindow*>(host.GetNativeWindow());
        unsigned phase = 0, rendered = 0;
        const auto initialSize = host.GetFramebufferSize();
        auto previousDraws = RenderCommand::GetDrawSubmissionCount();
        auto separated = uint64_t(0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
        app.SetBackgroundTick([&] {
            Check(std::chrono::steady_clock::now() < deadline, "Pavilion editor resize timed out");
            const auto active = Project::GetActive();
            if (!active || active->GetConfig().StartScene.filename() != "Pavilion.hazel")
                return;
            const auto draws = RenderCommand::GetDrawSubmissionCount();
            if (draws != previousDraws) ++rendered;
            previousDraws = draws;
            const auto state = observer ? observer->Snapshot() : MetalProofSnapshot{};
            Check(state.Errors == 0, "Pavilion editor reported a GPU submission error");
            if (!phase && rendered >= 12 && (!metal || state.CompletedPresented >= 8)) {
                glfwSetWindowSize(window, 1120, 760);
                separated = state.CompletedPresented;
                phase = 1;
                rendered = 0;
            }
            if (phase && rendered >= 12 && (!metal || state.CompletedPresented >= separated + 8)) {
                const auto size = host.GetFramebufferSize();
                Check(size != initialSize && host.GetWidth() == 1120 && host.GetHeight() == 760,
                      "Pavilion editor did not reach the requested drawable extent");
                if (metal) {
                    if (!capture) capture = backend->CaptureEditorFrameForTesting();
                    if (!capture || !capture->Completed) return;
                } else
                    Check(glGetError() == GL_NO_ERROR, "OpenGL Pavilion editor resize failed");
                std::cout << "PAVILION_EDITOR " << initialSize.first << 'x' << initialSize.second
                    << " -> " << size.first << 'x' << size.second << " draws=" << draws << '\n';
                app.Close();
            }
        });
        app.Run();
        Check(phase == 1 && rendered >= 12, "Pavilion editor closed before resize finished");
        if (capture) {
            const auto image = std::find_if(capture->Images.begin(), capture->Images.end(),
                [](const auto& item) { return item.Viewport == ImGui::GetMainViewport()->ID; });
            Check(image != capture->Images.end(), "Pavilion editor drawable capture is missing");
            std::ofstream file(output, std::ios::binary);
            file << "P6\n" << image->Width << ' ' << image->Height << "\n255\n";
            for (size_t pixel = 0; pixel < image->BGRA.size(); pixel += 4) {
                const char rgb[] = {char(image->BGRA[pixel + 2]), char(image->BGRA[pixel + 1]),
                                    char(image->BGRA[pixel])};
                file.write(rgb, sizeof(rgb));
            }
            Check(bool(file), "Could not save the resized Pavilion editor");
        }
    }
    if (observer) {
        const auto state = observer->Snapshot();
        Check(state.Errors == 0 && state.InFlight == 0 && state.ResourceBytes == 0 &&
              state.AttachmentGenerations == 0 && state.ImGuiResourceBytes == 0,
              "Pavilion editor GPU resources did not retire");
    }
    std::cout << "PASS: Pavilion in native EditorLayer after drawable resize and normal shutdown\n";
    return 0;
}
}
