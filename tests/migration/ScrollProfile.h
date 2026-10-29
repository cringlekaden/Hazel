// Modest static document qualification; CPU phases, not whole-frame or GPU duration.
static void ProfileScrolling()
{
    using Clock = std::chrono::steady_clock;
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(640, 480);
    auto canvas = scene->CreateEntity();
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {640, 480};
    c.SafeInsets = {};
    auto view = scene->CreateEntity("Profile viewport");
    scene->Reparent(view, canvas, TransformPolicy::KeepLocal);
    auto &r = view.AddComponent<UIRectComponent>();
    r.FixedSize = {480, 320};
    r.Padding = {8, 8, 24, 8};
    view.AddComponent<SpriteRendererComponent>(glm::vec4(.1f, .1f, .1f, 1));
    auto content = scene->CreateEntity("Static content");
    scene->Reparent(content, view, TransformPolicy::KeepLocal);
    auto &cr = content.AddComponent<UIRectComponent>();
    cr.WidthMode = SizeMode::Fill;
    cr.HeightMode = SizeMode::Content;
    content.AddComponent<UILayoutComponent>().Gap = 2;
    view.AddComponent<UIScrollViewComponent>().Content = content.GetUUID();
    for (int i = 0; i < 64; ++i)
    {
        auto row = scene->CreateEntity("Static row");
        scene->Reparent(row, content, TransformPolicy::KeepLocal);
        auto &rr = row.AddComponent<UIRectComponent>();
        rr.WidthMode = SizeMode::Fill;
        rr.FixedSize.y = 32;
        row.AddComponent<SpriteRendererComponent>(glm::vec4(.15f, .25f, .35f, 1));
        row.AddComponent<TextComponent>().TextString = "Static option " + std::to_string(i + 1);
        row.AddComponent<UITextStyleComponent>().Size = 16;
        row.AddComponent<UIButtonComponent>().TabOrder = i;
    }
    scene->OnRuntimeStart();
    scene->RefreshUI();
    auto target = Framebuffer::Create(
        {640,
         480,
         {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
           FramebufferTextureFormat::Depth}}});
    auto draw = [&] {
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        scene->OnUpdateRuntime(0);
    };
    float range = scene->GetUIScroll(view.GetUUID()).Range.y;
    // Prepare every label/page once, including the final endpoint, then warm both modes.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (float y = 0; y < range; y += 240)
        {
            scene->ScrollUITo(view.GetUUID(), {0, y});
            draw();
        }
        scene->ScrollUITo(view.GetUUID(), {0, range});
        draw();
    }
    for (bool scrolling : {false, true})
    {
        std::vector<double> source, publish, submit;
        source.reserve(100);
        publish.reserve(100);
        submit.reserve(100);
        uint64_t allocations = 0;
        for (int i = 0; i < 20; ++i)
        {
            scene->RefreshUI();
            if (scrolling)
                scene->ScrollUITo(view.GetUUID(), {0, float(i % 2) * range});
            draw();
        }
        auto fonts = Font::Resources();
        auto d = scene->GetUIDiagnostics();
        auto generation = scene->GetUISnapshot().Generation;
        for (int i = 0; i < 100; ++i)
        {
            s_Allocations = 0;
            s_CountAllocations = true;
            auto a = Clock::now();
            scene->RefreshUI();
            auto b = Clock::now();
            if (scrolling)
                scene->ScrollUITo(view.GetUUID(), {0, float(i % 2) * range});
            auto c = Clock::now();
            draw();
            auto e = Clock::now();
            s_CountAllocations = false;
            allocations += s_Allocations;
            source.push_back(std::chrono::duration<double, std::milli>(b - a).count());
            publish.push_back(std::chrono::duration<double, std::milli>(c - b).count());
            submit.push_back(std::chrono::duration<double, std::milli>(e - c).count());
        }
        auto after = Font::Resources();
        auto end = scene->GetUIDiagnostics();
        Check(after.Generations == fonts.Generations && after.LayoutBuilds == fonts.LayoutBuilds &&
                  end.Measurements == d.Measurements && end.Arrangements == d.Arrangements,
              "Warm static scrolling remeasured/reshaped/regenerated pages");
        if (!scrolling)
            Check(scene->GetUISnapshot().Generation == generation,
                  "Idle scroll profile republished geometry");
        auto summary = [](std::vector<double> data) {
            std::sort(data.begin(), data.end());
            std::ostringstream out;
            out << std::fixed << std::setprecision(4) << data[50] << "/" << data[95];
            return out.str();
        };
        std::cout << "SCROLL_PROFILE mode=" << (scrolling ? "scroll" : "idle")
                  << " rows=64 surface=640x480 view=448x304 samples=100 source_cpu_ms="
                  << summary(source) << " scroll_api_cpu_ms=" << summary(publish)
                  << " render_api_cpu_ms=" << summary(submit) << " cpp_new=" << allocations
                  << " layout_build_delta=" << after.LayoutBuilds - fonts.LayoutBuilds
                  << " page_generation_delta=" << after.Generations - fonts.Generations
                  << " measure_delta=" << end.Measurements - d.Measurements << "\n";
    }
    Check(scene->GetUIError().empty(), "Static scroll profile rendering failed");
    target->Unbind();
    scene->OnRuntimeStop();
    std::cout << "PASS: modest static scroll idle/republication CPU and allocation profile; no "
                 "presentation/GPU duration inferred\n";
}
