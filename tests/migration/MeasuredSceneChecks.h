static void MeasuredSceneChecks(const std::filesystem::path &root)
{
    auto scene = CreateRef<Scene>();
    auto assets = CreateRef<ProjectAssets>(root);
    scene->SetAssets(assets);
    scene->SetName("Measured scene fixture");
    scene->OnViewportResize(320, 240);
    auto canvas = scene->CreateEntity("Measured canvas");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {320, 240};
    c.SafeInsets = {};
    auto group = scene->CreateEntity("Measured stack");
    scene->Reparent(group, canvas, TransformPolicy::KeepLocal);
    group.AddComponent<UIRectComponent>();
    group.GetComponent<UIRectComponent>().LayoutVersion = 2;
    group.GetComponent<UIRectComponent>().FixedSize = {120, 80};
    group.GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    group.GetComponent<UIRectComponent>().Padding = glm::vec4(4);
    group.AddComponent<UILayoutComponent>().Gap = 7;
    auto label = scene->CreateEntity("Measured label");
    scene->Reparent(label, group, TransformPolicy::KeepLocal);
    label.AddComponent<UIRectComponent>();
    label.GetComponent<UIRectComponent>().LayoutVersion = 2;
    label.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    label.GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    label.GetComponent<UIRectComponent>().Margin = {2, 3, 2, 3};
    label.AddComponent<SpriteRendererComponent>();
    auto &text = label.AddComponent<TextComponent>();
    text.TextString = "Longer translated labels wrap predictably";
    auto &style = label.AddComponent<UITextStyleComponent>();
    style.FontPath = "font.ttf";
    style.Size = 20;
    style.Wrap = true;
    label.AddComponent<UIButtonComponent>().Action = "measured";
    scene->RefreshUI();
    auto bounds = scene->GetUISnapshot().Find(label.GetUUID())->Bounds;
    std::vector<Renderer2D::TextLine> lines;
    auto font = assets->FontAsset("font.ttf");
    Renderer2D::LayoutText(text.TextString, font, 108.f / 20, true, 0, lines);
    Check(bounds.Size.x == 108 && lines.size() > 1, "Measured label did not receive stack width");
    auto smallHeight = bounds.Size.y;
    auto originalText = text.TextString;
    for (const auto &empty : {std::string{}, std::string("\xef\xbb\xbf")})
    {
        text.TextString = empty;
        scene->RefreshUI();
        Check(scene->GetUISnapshot().Find(label.GetUUID())->Bounds.Size.y == 0,
              "Empty or BOM-only text retained intrinsic height");
    }
    text.TextString = originalText;
    scene->RefreshUI();
    auto generation = scene->GetUISnapshot().Generation;
    for (int i = 0; i < 4; ++i)
        scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation == generation,
          "Measured tree rebuilt unchanged generation");
    auto counts = scene->GetUIDiagnostics();
    group.GetComponent<UIRectComponent>().Offset.x = 5;
    scene->RefreshUI();
    if (scene->GetUIDiagnostics().Measurements != counts.Measurements ||
        scene->GetUIDiagnostics().MeasurementReuses <= counts.MeasurementReuses)
        std::cerr << "Measurement counters " << counts.Measurements << "/"
                  << counts.MeasurementReuses << " -> " << scene->GetUIDiagnostics().Measurements
                  << "/" << scene->GetUIDiagnostics().MeasurementReuses << "; width "
                  << scene->GetUISnapshot().Find(label.GetUUID())->Bounds.Size.x << "\n";
    Check(scene->GetUIDiagnostics().Measurements == counts.Measurements &&
              scene->GetUIDiagnostics().MeasurementReuses > counts.MeasurementReuses,
          "Unchanged wrapped text was remeasured during movement");
    auto height = scene->GetUISnapshot().Find(group.GetUUID())->Bounds.Size.y;
    label.GetComponent<UIRectComponent>().Enabled = false;
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(group.GetUUID())->Bounds.Size.y == height &&
              !scene->GetUISnapshot().Find(label.GetUUID())->FocusEligible,
          "Disabled node changed participation or remained interactive");
    label.GetComponent<UIRectComponent>().Enabled = true;
    label.GetComponent<UIRectComponent>().Visible = false;
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(group.GetUUID())->Bounds.Size.y == 8 &&
              !scene->GetUISnapshot().Find(label.GetUUID())->LayoutResolved,
          "Hidden measured node retained space");
    label.GetComponent<UIRectComponent>().Visible = true;
    scene->RefreshUI();
    InputFrame input;
    input.Enabled = true;
    scene->UpdateUIInput(input);
    auto inputGeneration = scene->GetUIDiagnostics().InputGeneration;
    text.TextString += " with many more words to exercise content height after native mutation";
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation > inputGeneration &&
              scene->GetUIDiagnostics().InputGeneration == inputGeneration &&
              scene->GetUISnapshot().Find(label.GetUUID())->Bounds.Size.y > smallHeight,
          "Native text failed to reflow or replayed input");
    generation = scene->GetUISnapshot().Generation;
    text.Color.x = .2f;
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation == generation,
          "Content color was treated as a measurement dependency");
    style.Size = 28;
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation > generation,
          "Font size mutation missed measurement invalidation");
    std::filesystem::copy_file(root / "font.ttf", root / "other-font.ttf");
    generation = scene->GetUISnapshot().Generation;
    style.FontPath = "other-font.ttf";
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation > generation, "Font asset mutation did not reflow");
    group.GetComponent<UIRectComponent>().Offset = {};
    scene->SetUISizing(group.GetUUID(), Axis::Horizontal, SizeMode::Fill, 0);
    scene->OnViewportResize(640, 240);
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(group.GetUUID())->Bounds.Size.x == 640,
          "Surface resize did not invalidate Fill constraints");
    auto valid = group.GetComponent<UIRectComponent>();
    bool bad = false;
    try
    {
        scene->SetUISizing(group.GetUUID(), Axis::Horizontal, SizeMode::Fixed, 120, 50, 20);
    }
    catch (const std::invalid_argument &)
    {
        bad = true;
    }
    Check(bad && group.GetComponent<UIRectComponent>().WidthMode == valid.WidthMode &&
              group.GetComponent<UIRectComponent>().FixedSize == valid.FixedSize,
          "Invalid sizing setter changed last valid authored state");
    scene->SetUISizing(group.GetUUID(), Axis::Horizontal, SizeMode::Fixed, 120, 0, 200);
    auto image = scene->CreateEntity("Intrinsic image");
    scene->Reparent(image, canvas, TransformPolicy::KeepLocal);
    auto &ir = image.AddComponent<UIRectComponent>();
    ir.LayoutVersion = 2;
    ir.WidthMode = ir.HeightMode = SizeMode::Content;
    ir.Offset = {200, 0};
    image.AddComponent<SpriteRendererComponent>().Source = TextureSpriteSource{"image.png"};
    scene->RefreshUI();
    auto texture = assets->Texture("image.png", TextureSpecification::FileDefaults());
    auto imageSize = scene->GetUISnapshot().Find(image.GetUUID())->Bounds.Size;
    Check(glm::length(imageSize - glm::vec2(texture->GetWidth(), texture->GetHeight())) < .01f,
          "Image intrinsic size differs from source pixels");
    scene->SetUISizing(image.GetUUID(), Axis::Horizontal, SizeMode::Fixed, 17);
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(image.GetUUID())->Bounds.Size ==
              glm::vec2(17, texture->GetHeight()),
          "Image sizing silently introduced aspect locking");
    auto state = SceneSerializer(scene, root).CaptureAuthoredState();
    auto copy = Scene::Copy(scene);
    Check(SceneSerializer(copy, root).CaptureAuthoredState() == state,
          "Measured copy lost authored fields");
    auto serialized = SceneSerializer(scene, root).SerializeText();
    Check(serialized.find("LayoutVersion: 2") != std::string::npos &&
              serialized.find("Measured:") == std::string::npos,
          "Measured schema lost version or persisted derived measurements");
    auto loaded = CreateRef<Scene>();
    Check(SceneSerializer(loaded, root).DeserializeText(serialized) &&
              SceneSerializer(loaded, root).CaptureAuthoredState() == state,
          "Measured serialization roundtrip changed fields");
    Prefab::Save(root, "measured.hprefab", scene, canvas);
    auto instance = Prefab::Instantiate(root, "measured.hprefab", *loaded);
    bool found = false;
    for (auto id : loaded->GetSubtree(instance))
    {
        auto e = loaded->GetEntityByUUID(id);
        if (e.GetName() == "Measured label")
            found = e.GetComponent<UIRectComponent>().LayoutVersion == 2 &&
                    e.GetComponent<UIRectComponent>().HeightMode == SizeMode::Content;
    }
    Check(found, "Measured prefab instantiation lost sizing fields");
    group.GetComponent<UIRectComponent>().WidthMode = SizeMode::Content;
    bool cycle = false;
    try
    {
        scene->RefreshUI();
    }
    catch (const std::invalid_argument &error)
    {
        auto message = std::string(error.what());
        cycle = message.find("Measured scene fixture") != std::string::npos &&
                message.find("Measured label") != std::string::npos &&
                message.find(".width") != std::string::npos;
    }
    Check(cycle && !scene->GetUISnapshot().Valid && !scene->GetUIDiagnostics().Capture,
          "Runtime dependency failure published partial geometry or omitted context");
    label.GetComponent<UIRectComponent>().Visible = false;
    scene->SetUIFramebufferSize({0, 0});
    bool hiddenDependency = false;
    try
    {
        scene->ValidateUILayoutDependencies();
    }
    catch (const std::invalid_argument &)
    {
        hiddenDependency = true;
    }
    Check(hiddenDependency, "Hidden zero-surface dependency escaped authored validation");
    label.GetComponent<UIRectComponent>().Visible = true;
    group.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fixed;
    scene->RefreshUI();
    scene->SetUIFramebufferSize({0, 0});
    scene->RefreshUI();
    Check(scene->GetUISnapshot().PainterOrder.empty(),
          "Zero measured surface retained drawing/hits");
    scene->SetUIFramebufferSize({800, 300});
    scene->RefreshUI();
    auto n = scene->GetUISnapshot().Find(label.GetUUID());
    Check(n->FramebufferClip.Size.x >= 0 && scene->GetUISnapshot().Valid,
          "Fractional restored surface failed measured layout");
    FramebufferSpecification targetSpec;
    targetSpec.Width = 800;
    targetSpec.Height = 300;
    targetSpec.Attachments = {FramebufferTextureFormat::RGBA8,
                              FramebufferTextureFormat::RED_INTEGER,
                              FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(targetSpec);
    target->Bind();
    RenderCommand::Clear();
    target->ClearAttachment(1, -1);
    scene->PrepareSprites(true);
    RuntimeUI rendered(*scene);
    rendered.ResolveSurface({{640, 240}, {800, 300}});
    rendered.Draw();
    Check(target->ReadPixel(1, 20, 300 - 25) == int(uint32_t(label)),
          "Measured arranged geometry did not reach actual GPU picking output");
    InputFrame point;
    point.Enabled = true;
    point.Mouse = {16, 20};
    scene->UpdateUIInput(point);
    point.PointerPressed = point.PointerReleased = point.HasReleasePosition = true;
    point.PressPosition = point.ReleasePosition = point.Mouse;
    scene->UpdateUIInput(point);
    Action activated;
    Check(scene->ConsumeUIAction(label.GetUUID(), activated) && activated.Name == "measured",
          "Measured hit/routing disagreed with drawn geometry");
    target->Unbind();
    target.reset();
    std::cout << "PASS: scene measured text/font/image/native reflow, cache/generation reuse, "
                 "disabled/hidden/zero surfaces, runtime sizing, copy/prefab/serialization and "
                 "contextual cycles\n";
}
