#include "Hazel/Assets/UIStyleDocument.h"
#include "Hazel/UI/UIStyleApplication.h"
static void UIStyleSceneChecks(const std::filesystem::path &directory)
{
    auto folder = directory / "style-scene";
    std::filesystem::create_directories(folder);
    auto assets = CreateRef<ProjectAssets>(folder);
    UIStyleDefinition definition;
    auto &values = definition.Variants[0].Values;
    values.Size = 18;
    values.Padding = glm::vec4(4);
    values.Background = {.2f, .3f, .4f, 1};
    values.Focus = {0, 1, 0, 1};
    values.Caret = {1, 0, 1, 1};
    values.Selected = {1, 0, 0, 1};
    UIStyleDocument file(assets);
    file.Create("Shared.hstyle", definition);
    auto s = TextEntryFixture();
    s->SetAssets(assets);
    for (auto id : {500, 501})
    {
        auto e = s->GetEntityByUUID(id);
        auto &c = e.AddComponent<UIAppearanceComponent>();
        c.Styled = true;
        c.Selection = {"Shared.hstyle", "Control"};
    }
    for (auto id : {210, 211})
    {
        auto &c = s->GetEntityByUUID(id).AddComponent<UIAppearanceComponent>();
        c.Styled = true;
        c.Selection = {"Shared.hstyle", "Control"};
        c.Overrides = UIStyleBit(UIStyleGroup::Padding);
    }
    auto local = s->GetEntityByUUID(501);
    local.GetComponent<UIAppearanceComponent>().Overrides = UIStyleBit(UIStyleGroup::Background);
    local.GetComponent<SpriteRendererComponent>().Color = {.7f, .1f, .2f, 1};
    s->ValidateUI();
    s->OnRuntimeStart();
    InputFrame input;
    input.Enabled = true;
    input.Mouse = {399, 299};
    s->UpdateUIInput(input);
    s->UpdateUIInput(input);
    auto target = Framebuffer::Create(
        {400,
         300,
         {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
           FramebufferTextureFormat::Depth}}});
    auto paint = [&] {
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        s->OnUpdateRuntime(0);
    };
    auto pixel = [&](uint64_t id, int dx = 1, int dy = 1) {
        auto n = s->GetUISnapshot().Find(id);
        std::array<uint8_t, 4> rgba{};
        GLint saved;
        glGetIntegerv(GL_READ_BUFFER, &saved);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(int(n->Bounds.Position.x) + dx, 300 - int(n->Bounds.Position.y) - dy, 1, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glReadBuffer(saved);
        return rgba;
    };
    paint();
    auto inherited = pixel(500), overridden = pixel(501);
    Check(std::abs(int(inherited[0]) - 51) <= 2 && std::abs(int(inherited[2]) - 102) <= 2,
          "Inherited style background did not reach actual UI pixels");
    Check(std::abs(int(overridden[0]) - 179) <= 2 && std::abs(int(overridden[2]) - 51) <= 2,
          "Explicit local background did not win over style");
    auto selectedMark = pixel(210, 14, 18), emptyMark = pixel(211, 14, 18);
    Check(selectedMark[0] > 250 && selectedMark[1] < 3 && selectedMark[2] < 3 &&
              emptyMark != selectedMark,
          "Styled selected cue lost the independent filled/unfilled radio shape");
    auto unchangedGeneration = s->GetUISnapshot().Generation;
    local.GetComponent<SpriteRendererComponent>().Color = {.6f, .2f, .3f, 1};
    paint();
    Check(s->GetUISnapshot().Generation == unchangedGeneration &&
              std::abs(int(pixel(501)[0]) - 153) <= 2,
          "Native paint override was hidden by snapshot generation reuse");
    local.GetComponent<SpriteRendererComponent>().Color = {.7f, .1f, .2f, 1};
    paint();
    auto layout = s->GetUITextLayout(500);
    auto builds = Font::Resources().LayoutBuilds;
    file.Draft().Variants[0].Values.Background = {.1f, .5f, .2f, 1};
    file.Changed();
    file.Save();
    paint();
    Check(Font::Resources().LayoutBuilds == builds && s->GetUITextLayout(500) == layout &&
              s->GetUIDiagnostics().PreparedRebuilds == 0,
          "Paint-only style update reshaped/reprepared text");
    inherited = pixel(500);
    overridden = pixel(501);
    Check(std::abs(int(inherited[1]) - 128) <= 2 && std::abs(int(overridden[0]) - 179) <= 2,
          "Shared update lost inheritance/local override isolation");
    auto good = FileDocument::Read(folder / "Shared.hstyle");
    {
        std::ofstream out(folder / "Shared.hstyle");
        out << "malformed replacement";
    }
    paint();
    Check(pixel(500) == inherited && assets->Style("Shared.hstyle").Error.empty(),
          "Ordinary drawing probed a changed style file outside a freshness boundary");
    assets->Reload("Shared.hstyle");
    paint();
    Check(pixel(500) == inherited && pixel(501) == overridden,
          "Failed style replacement partially changed rendered controls");
    {
        std::ofstream out(folder / "Shared.hstyle");
        out << good;
    }
    assets->Reload("Shared.hstyle");
    target->Unbind();
    s->SetUITextEntry(500, "Live value");
    Check(s->FocusUI(500), "Styled input focus failed");
    input.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
    s->UpdateUIInput(input);
    input.Events.clear();
    s->UpdateUIInput(input);
    s->UpdateUIInput(input);
    Check(s->IsUITextEditing(), "Styled text could not enter editing");
    paint();
    auto caret = pixel(500, 4, 5);
    Check(caret[0] > 250 && caret[1] < 3 && caret[2] > 250,
          "Styled text caret color did not reach the actual editing overlay");
    file.Draft().Variants[0].Values.Size = 28;
    file.Draft().Variants[0].Values.Padding = glm::vec4(8);
    file.Changed();
    file.Save();
    paint();
    Check(s->IsUITextEditing() && s->GetUITextEntry(500) == "Live value" &&
              s->GetUIPresentation(500).Size == 28,
          "Metric style update changed text value or invalidated its live edit");
    Check(s->GetUIDiagnostics().Focus == 500, "Metric style update stole focus");
    s->ResetUIStyleOverrides(501, UIStyleBit(UIStyleGroup::Background));
    paint();
    Check(pixel(501)[1] > 120, "Reset-to-style did not remove explicit override presence");
    // Native local field writes remain observed, but only explicitly present groups own
    // presentation.
    auto first = s->GetEntityByUUID(500);
    first.GetComponent<UITextStyleComponent>().Size = 35;
    auto generation = s->GetUISnapshot().Generation;
    s->RefreshUI();
    Check(s->GetUIPresentation(500).Size == 28, "Dormant native local value overrode inheritance");
    first.GetComponent<UIAppearanceComponent>().Overrides |= UIStyleBit(UIStyleGroup::Size);
    s->RefreshUI();
    Check(s->GetUIPresentation(500).Size == 35 && s->GetUISnapshot().Generation > generation,
          "Native override presence was hidden by unchanged generation");
    // A removed named variant retains its prior complete presentation in the running scene.
    auto removed = file.Draft();
    removed.Variants[0].Name = "Replacement";
    {
        std::ofstream out(folder / "Shared.hstyle");
        out << WriteUIStyleText(removed);
    }
    assets->Reload("Shared.hstyle");
    paint();
    Check(s->GetUIPresentation(501).Size == 28 && !s->GetUIDiagnostics().ControlProblem.empty(),
          "Missing variant did not retain accepted presentation with a diagnostic");
    {
        std::ofstream out(folder / "Shared.hstyle");
        out << WriteUIStyleText(file.Draft());
    }
    assets->Reload("Shared.hstyle");
    paint();
    target->Unbind();
    // Every declared variant contributes families, faces/notices and state images.
    std::filesystem::copy_file(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"),
                               folder / "Face.ttf");
    WriteTestFamily(folder, "Face.hfont", "Face.ttf");
    {
        std::ofstream image(folder / "Image.ppm", std::ios::binary);
        image << "P6\n4 4\n255\n";
        for (int i = 0; i < 48; ++i)
            image.put(char(255));
    }
    SpriteSheetDefinition sheet;
    sheet.Texture = "Image.ppm";
    sheet.Sampling.Width = sheet.Sampling.Height = 4;
    sheet.Regions.push_back({7, "Icon", {0, 0, 4, 4}, {.5f, .5f}});
    {
        std::ofstream out(folder / "Images.hsprites");
        out << WriteSpriteSheetText(sheet);
    }
    auto closure = file.Draft();
    UIStyleVariant unused;
    unused.Name = "Unused declared role";
    unused.Values.Font.Family = "Face.hfont";
    unused.Values.Image = SpriteReference{"Images.hsprites", 7};
    unused.Values.States.Images[3] = TextureSpriteSource{"Image.ppm"};
    closure.Variants.push_back(unused);
    {
        std::ofstream out(folder / "Closure.hstyle");
        out << WriteUIStyleText(closure);
    }
    auto closureFiles = AuditSpriteAssets(folder, {"Closure.hstyle"});
    for (auto ref : {"Closure.hstyle", "Face.hfont", "Face.ttf", "font-LICENSE.txt",
                     "Images.hsprites", "Image.ppm"})
        Check(std::find(closureFiles.begin(), closureFiles.end(), ref) != closureFiles.end(),
              "A declared style variant lost a font/notice/state-image dependency");
    auto notice = FileDocument::Read(folder / "font-LICENSE.txt");
    std::filesystem::remove(folder / "font-LICENSE.txt");
    bool rejected = false;
    try
    {
        AuditSpriteAssets(folder, {"Closure.hstyle"});
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Check(rejected, "Style export accepted a missing associated notice");
    {
        std::ofstream out(folder / "font-LICENSE.txt");
        out << notice;
    }
    file.Draft().Variants[0].Values.Image = SpriteReference{"Images.hsprites", 7};
    file.Draft().Variants[0].Values.Slice.Borders = glm::vec4(1);
    file.Changed();
    file.Save();
    paint();
    UIImageVisual visual;
    Check(s->GetUIImageVisual(500, visual) && visual.SourceSize == glm::vec2(4),
          "Style atlas image bypassed the fitted-image/region-size path");
    target->Unbind();
    auto sliced = s->CreateEntityWithUUID(600,"Inherited slice image");
    s->Reparent(sliced,s->GetEntityByUUID(100),TransformPolicy::KeepLocal);
    auto& slicedRect=sliced.AddComponent<UIRectComponent>();slicedRect.Offset={320,220};slicedRect.FixedSize={40,40};
    sliced.AddComponent<SpriteRendererComponent>();sliced.AddComponent<UINineSliceComponent>();
    auto& slicedStyle=sliced.AddComponent<UIAppearanceComponent>();slicedStyle.Styled=true;slicedStyle.Selection={"Shared.hstyle","Control"};
    s->ValidateUI();paint();
    Check(target->ReadPixel(1,340,60)==int(uint32_t(sliced)),"Styled nine-slice did not preserve actual image geometry/picking");
    target->Unbind();
    auto sliceSelection=s->GetUIStyle(600);bool badSlice=false;
    try{s->SetUIStyle(600,{{},"Control"});}catch(const std::exception&){badSlice=true;}
    Check(badSlice&&s->GetUIStyle(600)==sliceSelection,"Style assignment published an incompatible nine-slice composition");
    file.Draft().Variants[0].Values.Slice.Borders=glm::vec4(3);file.Changed();file.Save();paint();
    Check(target->ReadPixel(1,340,60)==int(uint32_t(sliced))&&!s->GetUIDiagnostics().ControlProblem.empty(),"Invalid native/reloaded slice took down unaffected UI instead of diagnosed unsliced fallback");
    file.Draft().Variants[0].Values.Slice.Borders=glm::vec4(1);file.Changed();file.Save();paint();target->Unbind();
    s->OnRuntimeStop();
    SceneSerializer(s).Serialize((folder/"StyledSlice.hazel").generic_u8string());
    (void)AuditSpriteAssets(folder,{"StyledSlice.hazel"});
    auto authored = TextEntryFixture();
    authored->SetAssets(assets);
    authored->Reparent(authored->GetEntityByUUID(900), authored->GetEntityByUUID(100),
                       TransformPolicy::KeepLocal);
    for (auto id : {500, 501})
    {
        auto &a = authored->GetEntityByUUID(id).AddComponent<UIAppearanceComponent>();
        a.Styled = true;
        a.Selection = {"Shared.hstyle", "Control"};
    }
    SceneSerializer(authored).Serialize((folder / "Styled.hazel").generic_u8string());
    auto loaded = CreateRef<Scene>();
    loaded->SetAssets(assets);
    Check(SceneSerializer(loaded).Deserialize((folder / "Styled.hazel").generic_u8string()),
          "Styled scene load failed");
    Check(SceneSerializer(authored).CaptureAuthoredState() ==
              SceneSerializer(loaded).CaptureAuthoredState(),
          "Style assignment did not survive authored roundtrip");
    Prefab::Save(folder, "Styled.hprefab", authored, authored->GetEntityByUUID(100), true);
    auto prefab = Prefab::Load(folder, "Styled.hprefab");
    auto firstInstance = loaded->InstantiateEntity(Prefab::GetEntity(prefab), {});
    auto secondInstance = loaded->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(firstInstance && secondInstance && firstInstance.GetUUID() != secondInstance.GetUUID(),
          "Styled templates did not detach independent identities");
    auto dependencies = AuditSpriteAssets(folder, {"Styled.hazel", "Styled.hprefab"});
    Check(std::find(dependencies.begin(), dependencies.end(),
                    std::filesystem::path("Shared.hstyle")) != dependencies.end(),
          "Style dependency was omitted from package closure");
    auto templateState = SceneSerializer(authored).CaptureAuthoredState();
    auto one = Scene::Copy(authored), two = Scene::Copy(authored);
    one->OnViewportResize(400, 300);
    two->OnViewportResize(400, 300);
    one->OnRuntimeStart();
    two->OnRuntimeStart();
    one->SetUITextEntry(500, "Independent");
    Check(two->GetUITextEntry(500) != one->GetUITextEntry(500),
          "Styled instance shared transient text/value state");
    // Equal path/epoch in another project must never inherit the old project's
    // accepted variant, even when that variant is absent in the new definition.
    auto otherRoot = folder / "other-project";
    std::filesystem::create_directories(otherRoot);
    { std::ofstream out(otherRoot / "Shared.hstyle"); out << WriteUIStyleText(removed); }
    auto otherAssets = CreateRef<ProjectAssets>(otherRoot);
    one->SetAssets(otherAssets);
    Check(one->GetUIPresentation(500).Size == UIStyleValues{}.Size,
          "Project replacement inherited a retired project's accepted variant");
    one->OnRuntimeStop();
    two->OnRuntimeStop();
    Check(SceneSerializer(authored).CaptureAuthoredState() == templateState,
          "Independent styled runtime values leaked into authored template");
    auto native = CreateRef<Scene>();native->OnViewportResize(400,300);
    auto canvas=native->CreateEntity("Native style canvas");canvas.AddComponent<UICanvasComponent>().ReferenceSize={400,300};
    auto primitive=native->CreateEntity("Native image override");native->Reparent(primitive,canvas,TransformPolicy::KeepLocal);
    primitive.AddComponent<UIRectComponent>().FixedSize={40,40};
    auto texture=Texture2D::Create(TextureSpecification());uint32_t red=0xff0000ff;texture->SetData(&red,sizeof(red));
    primitive.AddComponent<SpriteRendererComponent>().Source=TextureSpriteSource{{},1,texture};
    auto& nativeStyle=primitive.AddComponent<UIAppearanceComponent>();nativeStyle.Styled=true;nativeStyle.Overrides=UIStyleBit(UIStyleGroup::Image)|UIStyleBit(UIStyleGroup::Background);
    native->OnRuntimeStart();target->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();target->ClearAttachment(1,-1);native->OnUpdateRuntime(0);
    std::array<uint8_t,4> nativePixel{};GLint read;glGetIntegerv(GL_READ_BUFFER,&read);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(20,280,1,1,GL_RGBA,GL_UNSIGNED_BYTE,nativePixel.data());glReadBuffer(read);
    Check(nativePixel[0]>250&&nativePixel[1]<3&&nativePixel[2]<3&&target->ReadPixel(1,20,280)==int(uint32_t(primitive)),"Styled native texture override lost ownership without project assets");
    target->Unbind();native->OnRuntimeStop();
    std::cout << "PASS: real styled pixels, shared/local/reset ownership, paint/metric "
                 "invalidation, active text reflow, failed/missing-variant reload, native writes, "
                 "scene/prefab closure and independent values\n";
}
