#include "FontFamilyFixture.h"
// Scene/runtime boundary checks supplement the renderer's immediate-reference pixels.
static void RuntimeUIPaintChecks(const std::filesystem::path &root)
{
    auto scene = CreateRef<Scene>();
    scene->SetAssets(CreateRef<ProjectAssets>(root));
    scene->OnViewportResize(128, 128);
    auto canvas = scene->CreateEntity("Paint canvas");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {128, 128};
    c.SafeInsets = {};
    auto make = [&](const char *name, Entity parent, glm::vec2 offset, glm::vec2 size) {
        auto e = scene->CreateEntity(name);
        scene->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = offset;
        r.FixedSize = size;
        return e;
    };
    auto panel = make("Panel", canvas, {16, 16}, {96, 96});
    panel.AddComponent<SpriteRendererComponent>(glm::vec4(0, 0, 1, .5f));
    auto first = make("First", panel, {0, 20}, {70, 40});
    first.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, .5f));
    auto second = make("Second", panel, {30, 20}, {70, 40});
    second.AddComponent<SpriteRendererComponent>(glm::vec4(0, 1, 0, .5f));
    second.AddComponent<TextComponent>().TextString = "A";
    second.AddComponent<UITextStyleComponent>().Size = 24;
    WriteTestFamily(root,"font.hfont","font.ttf");second.GetComponent<TextComponent>().Font.Family="font.hfont";
    auto nine = make("Nine slice", panel, {0, 65}, {60, 20});
    nine.AddComponent<SpriteRendererComponent>().Source = TextureSpriteSource{"image.png", 1, {}};
    nine.AddComponent<UINineSliceComponent>().Borders = glm::vec4(2);
    auto text = make("Cached text", panel, {0, 0}, {90, 20});
    text.AddComponent<TextComponent>().TextString = "Cache";
    text.AddComponent<UITextStyleComponent>().Size = 14;
    auto empty = make("Empty clip", panel, {500, 500}, {20, 20});
    empty.AddComponent<SpriteRendererComponent>();
    empty.AddComponent<TextComponent>().TextString = "Hidden";
    // EnTT packed component insertion can move earlier values: acquire after setup.
    auto &label = second.GetComponent<TextComponent>();
    auto &style = second.GetComponent<UITextStyleComponent>();
    scene->PrepareSprites(true);
    RuntimeUI ui(*scene);
    ui.Resolve({128, 128});
    FramebufferSpecification specification;
    specification.Width = specification.Height = 128;
    specification.Attachments = {FramebufferTextureFormat::RGBA8,
                                 FramebufferTextureFormat::RED_INTEGER,
                                 FramebufferTextureFormat::Depth};
    auto framebuffer = Framebuffer::Create(specification);
    framebuffer->Bind();
    auto clear = [&] {
        auto state = RenderCommand::GetRasterState();
        state.ClipEnabled = false;
        RenderCommand::SetRasterState(state);
        RenderCommand::SetClearColor({0, 0, 0, 0});
        RenderCommand::Clear();
        framebuffer->ClearAttachment(1, -1);
    };
    clear();
    auto state = RenderCommand::GetRasterState();
    state.DepthTest = true;
    state.ClipEnabled = true;
    state.ClipRect = {1, 1, 120, 120};
    state.LineWidth = 1;
    RenderCommand::SetRasterState(state);
    // This geometry must drain with the host camera/scissor, before UI replaces them.
    Renderer2D::BeginScene(Camera(glm::mat4(1)), glm::mat4(1));
    Renderer2D::DrawQuad(glm::scale(glm::mat4(1), glm::vec3(2)), {.1f, .1f, .1f, 1}, 30000);
    Renderer2D::ResetStats();
    ui.Draw();
    auto restored = RenderCommand::GetRasterState();
    auto d = ui.Diagnostics();
    Check(restored.DepthTest == state.DepthTest && restored.ClipEnabled == state.ClipEnabled &&
              restored.ClipRect == state.ClipRect && restored.Viewport == state.Viewport &&
              restored.LineWidth == state.LineWidth,
          "Runtime UI leaked host raster state");
    Check(framebuffer->ReadPixel(1, 5, 5) == 30000 && framebuffer->ReadPixel(1, 0, 0) == -1,
          "Pending world geometry used UI camera/clip");
    Check(framebuffer->ReadPixel(1, 30, 128 - 95) == int(uint32_t(nine)),
          "Batched nine-slice lost geometry/picking");
    Check(d.DrawCalls == 4 && d.UploadGroups == 1 && Renderer2D::GetStats().DrawCalls == 5,
          "Scene adjacent images/nine-slice failed batching or UI stats include world geometry");
    Check(d.PreparedRebuilds == 2 && d.PreparedReuses == 0, "Initial text preparation missing");
    auto generation = ui.Snapshot().Generation;
    clear();
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Snapshot().Generation == generation && ui.Diagnostics().PreparedReuses == 2 &&
              ui.Diagnostics().PreparedRebuilds == 0,
          "Unchanged UI failed prepared-data reuse");
    label.Color = {1, 0, 0, .6f};
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Snapshot().Generation == generation && ui.Diagnostics().PreparedReuses == 2,
          "Paint-only text tint escaped live reads or rebuilt preparation");
    label.TextString = "View";
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Snapshot().Generation == generation && ui.Diagnostics().PreparedRebuilds == 1,
          "Fixed-box native text write hid behind an unchanged layout generation");
    generation = ui.Snapshot().Generation;
    label.LineSpacing = .1f;
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedRebuilds == 1,
          "Line spacing mutation reused stale glyph preparation");
    style.Horizontal = Align::End;
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedRebuilds == 1,
          "Paint-only alignment mutation reused stale preparation");
    second.GetComponent<UIRectComponent>().Offset.x += 1;
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedReuses == 2,
          "Position-only layout change rebuilt normalized text preparation");
    second.GetComponent<TextComponent>().Font.Family.clear();
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedRebuilds == 1, "Font mutation reused retired atlas preparation");
    auto acceptedBeforeRefresh=second.GetComponent<TextComponent>().ResolvedFont;
    auto layoutsBeforeRefresh=Font::Resources().LayoutBuilds;
    scene->GetAssets()->Refresh();
    scene->PrepareSprites(true);
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedRebuilds == 0 && ui.Diagnostics().PreparedReuses == 2 &&
              second.GetComponent<TextComponent>().ResolvedFont == acceptedBeforeRefresh &&
              Font::Resources().LayoutBuilds == layoutsBeforeRefresh,
          "Unchanged accepted faces were reshaped/reprepared after asset refresh");
    auto resource = Texture2D::Create(TextureSpecification());
    uint32_t red = 0xff0000ff;
    resource->SetData(&red, 4);
    first.GetComponent<SpriteRendererComponent>().Source = TextureSpriteSource{{}, 1, resource};
    first.GetComponent<SpriteRendererComponent>().Color = glm::vec4(1);
    scene->PrepareSprites(true);
    ui.Resolve({128, 128});
    clear();
    ui.Draw();
    auto pixel = [&] {
        std::array<unsigned char, 4> p{};
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(24, 128 - 55, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
        return p;
    };
    Check(pixel()[0] > 250 && pixel()[1] < 5, "Raw image resource mutation did not repaint");
    generation = ui.Snapshot().Generation;
    uint32_t green = 0xff00ff00;
    resource->SetData(&green, 4);
    scene->PrepareSprites(true);
    ui.Resolve({128, 128});
    clear();
    ui.Draw();
    Check(pixel()[1] > 250 && pixel()[0] < 5 && ui.Snapshot().Generation == generation,
          "Resource pixel upload hidden by snapshot generation/cache");
    text.GetComponent<TextComponent>().TextString = std::string(4097, 'A');
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedRebuilds == 1 &&
              ui.Diagnostics().PreparedBytes <= 16 * 1024 * 1024,
          "Oversized text escaped bounded preparation fallback");
    text.GetComponent<TextComponent>().TextString = "Cache";
    ui.Resolve({128, 128});
    ui.Draw();
    scene->DestroyEntity(second);
    ui.Resolve({128, 128});
    ui.Draw();
    Check(ui.Diagnostics().PreparedReuses == 1, "Retired text/entity retained paint data");
    for (int i = 0; i < 3; ++i)
    {
        ui.Reset();
        ui.Resolve({128, 128});
        ui.Draw();
        Check(ui.Diagnostics().PreparedRebuilds == 1,
              "Repeated scene/UI reset reused retired paint data");
    }
    c.Modal = true;
    first.AddComponent<UIButtonComponent>();
    ui.Resolve({128, 128});
    InputFrame focus;
    focus.Enabled = true;
    ui.UpdateInput(focus);
    auto priorLine = RenderCommand::GetRasterState().LineWidth,
         configuredLine = Renderer2D::GetLineWidth();
    Renderer2D::SetLineWidth(3);
    ui.Draw();
    Check(RenderCommand::GetRasterState().LineWidth == priorLine,
          "Focused UI line width leaked into host rendering");
    Renderer2D::SetLineWidth(configuredLine);
    scene->OnViewportResize(0, 0);
    ui.Resolve({0, 0});
    ui.Draw();
    Check(!ui.Diagnostics().DrawCalls && !ui.Diagnostics().SubmittedElements,
          "Zero surface submitted stale geometry");
    // A real fractional framebuffer must agree with inward-snapped hit coverage.
    auto fractionalScene = CreateRef<Scene>();
    auto fractionalCanvas = fractionalScene->CreateEntity("Fractional canvas");
    auto &fc = fractionalCanvas.AddComponent<UICanvasComponent>();
    fc.ReferenceSize = {128, 128};
    fc.SafeInsets = {};
    auto tiny = fractionalScene->CreateEntity("Fractional control");
    fractionalScene->Reparent(tiny, fractionalCanvas, TransformPolicy::KeepLocal);
    auto &tr = tiny.AddComponent<UIRectComponent>();
    tr.Offset = {.1f, .1f};
    tr.FixedSize = {.4f, .4f};
    tiny.AddComponent<SpriteRendererComponent>();
    tiny.AddComponent<UIButtonComponent>();
    fractionalScene->PrepareSprites(true);
    framebuffer->Resize(160, 160);
    framebuffer->Bind();
    clear();
    RuntimeUI fractional(*fractionalScene);
    fractional.ResolveSurface({{128, 128}, {160, 160}});
    fractional.Draw();
    Check(!fractional.Snapshot().Find(tiny.GetUUID())->FocusEligible &&
              !fractional.Diagnostics().DrawCalls && framebuffer->ReadPixel(1, 1, 158) == -1,
          "Subpixel empty clip rendered or became eligible");
    tr.FixedSize = {2, 2};
    fractional.ResolveSurface({{128, 128}, {160, 160}});
    fractional.Draw();
    Check(fractional.Snapshot().Find(tiny.GetUUID())->FocusEligible &&
              framebuffer->ReadPixel(1, 1, 158) == int(uint32_t(tiny)) &&
              framebuffer->ReadPixel(1, 0, 159) == -1,
          "Fractional GPU coverage differs from snapped hit region");
    framebuffer->Resize(128, 128);
    framebuffer->Bind();
    clear();
    EditorCamera editorCamera(45, 1, .1f, 1000);
    for (int i = 0; i < 3; ++i)
    {
        auto replacement = Scene::Copy(scene);
        replacement->OnViewportResize(128, 128);
        replacement->OnUpdateEditor(0, editorCamera);
        Check(replacement->GetUIError().empty() &&
                  replacement->GetUIDiagnostics().PreparedRebuilds == 1,
              "Real Scene render inherited a replaced scene's preparation");
        auto published = replacement->GetUISnapshot().Generation;
        replacement->OnUpdateEditor(0, editorCamera);
        Check(replacement->GetUIDiagnostics().PreparedReuses == 1 &&
                  replacement->GetUISnapshot().Generation == published,
              "Real Scene render failed unchanged snapshot/preparation reuse");
    }
    framebuffer->Unbind();
    Check(glGetError() == GL_NO_ERROR, "Runtime paint contract OpenGL error");
    std::cout << "PASS: real UI batching/stat isolation, host drain/state, nine-slice picking, "
                 "native text/font/resource invalidation, preparation reuse and reset/retirement\n";
}
