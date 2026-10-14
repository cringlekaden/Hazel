#include "Hazel.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/UI/RuntimeUI.h"
#include "Hazel/Core/Resources.h"
#include <fstream>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glad/glad.h>
#include <iostream>
#include <random>
using namespace Hazel;
using namespace Hazel::GameUI;
static void Check(bool value, const char *why)
{
    if (!value)
        throw std::runtime_error(why);
}
int main()
{
    try
    {
        Log::Init();
        ApplicationSpecification spec;
        spec.Name = "Migration Game UI";
        spec.EnableImGui = false;
        Application app(spec);
        glfwHideWindow(static_cast<GLFWwindow *>(app.GetWindow().GetNativeWindow()));
        const auto root = std::filesystem::temp_directory_path() /
                          ("hazel-ui-" + std::to_string(std::random_device{}()));
        std::filesystem::create_directories(root);
        struct Cleanup
        {
            std::filesystem::path Root;
            ~Cleanup()
            {
                std::error_code e;
                std::filesystem::remove_all(Root, e);
            }
        } cleanup{root};
        std::filesystem::copy_file(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"),
                                   root / "font.ttf");
        std::filesystem::copy_file("assets/textures/Checkerboard.png", root / "image.png");
        auto scene = CreateRef<Scene>();
        scene->SetAssets(CreateRef<ProjectAssets>(root));
        scene->OnViewportResize(320, 240);
        auto canvas = scene->CreateEntity("Canvas");
        auto &c = canvas.AddComponent<UICanvasComponent>();
        c.ReferenceSize = {320, 240};
        c.SafeInsets = {0, 0, 0, 0};
        auto panel = scene->CreateEntity("Panel");
        scene->Reparent(panel, canvas, TransformPolicy::KeepLocal);
        auto &r = panel.AddComponent<UIRectComponent>();
        r.Offset = {20, 30};
        r.SizeDelta = {180, 100};
        r.Padding = {10, 10, 10, 10};
        panel.AddComponent<SpriteRendererComponent>(glm::vec4(0, 0, 1, 1));
        auto button = scene->CreateEntity("Button");
        scene->Reparent(button, panel, TransformPolicy::KeepLocal);
        auto &b = button.AddComponent<UIRectComponent>();
        b.Offset = {-30, 10};
        b.SizeDelta = {80, 40};
        button.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, 1));
        button.AddComponent<UIButtonComponent>().Action = "play";
        auto text = scene->CreateEntity("Text");
        scene->Reparent(text, panel, TransformPolicy::KeepLocal);
        text.AddComponent<UIRectComponent>().Offset = {0, 55};
        text.AddComponent<TextComponent>().TextString = "Light\nkeeper";
        auto &style = text.AddComponent<UITextStyleComponent>();
        style.Size = 18;
        style.FontPath = "font.ttf";
        button.GetComponent<UIButtonComponent>().Images[1] =
            TextureSpriteSource{"image.png", 1, {}};
        scene->ValidateUI();
        scene->PrepareSprites(true);
        SceneSerializer(scene, root).Serialize((root / "ui.hazel").generic_u8string());
        auto dependencies = AuditSpriteAssets(root, {"ui.hazel"});
        Check(std::find(dependencies.begin(), dependencies.end(), "font.ttf") !=
                      dependencies.end() &&
                  std::find(dependencies.begin(), dependencies.end(), "image.png") !=
                      dependencies.end(),
              "UI font/state image escaped asset closure");
        FramebufferSpecification f;
        f.Width = 320;
        f.Height = 240;
        f.Attachments = {FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
                         FramebufferTextureFormat::Depth};
        auto target = Framebuffer::Create(f);
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        auto raster = RenderCommand::GetRasterState();
        RuntimeUI ui(*scene);
        ui.Resolve({320, 240});
        ui.Draw();
        Check(target->ReadPixel(1, 20, 240 - 65) == int(uint32_t(button)),
              "Draw/hit clip inside disagreement");
        Check(target->ReadPixel(1, 10, 240 - 65) == -1, "Parent clipping leaked");
        auto after = RenderCommand::GetRasterState();
        Check(after.DepthTest == raster.DepthTest && after.ClipEnabled == raster.ClipEnabled &&
                  after.ClipRect == raster.ClipRect,
              "Overlay leaked raster state");
        InputFrame input;
        input.Enabled = true;
        input.Mouse = {25, 65};
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        input.LeftDown = true;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        input.LeftDown = false;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        Action action;
        Check(ui.Consume(button.GetUUID(), action) && action.Name == "play" &&
                  !ui.Consume(0, action),
              "Pointer release did not activate exactly once");
        input.FocusNavigation = true;
        input.NextDown = true;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        input.FocusNavigation = input.NextDown = false;
        input.AcceptDown = true;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        button.GetComponent<UIRectComponent>().Visible = false;
        Check(!ui.Consume(0, action), "Hidden control kept action");
        button.GetComponent<UIRectComponent>().Visible = true;
        auto modal = scene->CreateEntity("Modal");
        auto &modalCanvas = modal.AddComponent<UICanvasComponent>();
        modalCanvas.ReferenceSize = {320, 240};
        modalCanvas.SafeInsets = {0, 0, 0, 0};
        modalCanvas.SortOrder = 10;
        modalCanvas.Modal = true;
        ui.Resolve({320, 240});
        ui.UpdateInput({});
        Check(ui.BlocksGameplay(), "Modal does not block gameplay");
        const auto authored = SceneSerializer(scene, root).CaptureAuthoredState();
        auto copy = Scene::Copy(scene);
        Check(SceneSerializer(copy, root).CaptureAuthoredState() == authored,
              "Scene copy changed UI authored state");
        auto loaded = CreateRef<Scene>();
        SceneSerializer loader(loaded, root);
        Check(loader.DeserializeText(SceneSerializer(scene, root).SerializeText()),
              "UI roundtrip failed");
        Check(loader.CaptureAuthoredState() == authored, "UI roundtrip changed fields");
        Prefab::Save(root, "menu.hprefab", scene, canvas);
        auto instance = Prefab::Instantiate(root, "menu.hprefab", *loaded);
        Check(instance.GetUUID() != canvas.GetUUID() && instance.HasComponent<UICanvasComponent>(),
              "UI prefab root was not remapped");
        auto &changed = copy->GetEntityByUUID(button.GetUUID()).GetComponent<UIButtonComponent>();
        changed.Colors[2].x = .25f;
        Check(SceneSerializer(copy, root).CaptureAuthoredState() != authored,
              "UI style dirty tracking missed field");
        ui.Reset();
        Check(!ui.Consume(0, action), "Reset retained actions");
        ui.Resolve({0, 0});
        ui.UpdateInput(input);
        Check(!ui.Consume(0, action), "Minimized UI activated");
        auto font = Font::GetDefault();
        std::vector<Renderer2D::TextLine> lines;
        Renderer2D::LayoutText("A useful lantern", font, 2, true, 0, lines);
        Check(lines.size() > 1, "Text did not wrap");
        for (auto &line : lines)
            Check(std::abs(line.Width - Renderer2D::MeasureText(line.Text, font)) < .001f,
                  "Text measurement disagrees with layout");
        Check(glGetError() == GL_NO_ERROR, "UI OpenGL error");
        target->Unbind();
        target.reset();
        std::cout << "PASS: camera-free UI render/hit/clip agreement, raster "
                     "restoration, actions, modal gating, retirement, resize, "
                     "authored copy/roundtrip/prefab and text metrics\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
