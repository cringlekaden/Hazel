// Optional bounded UI pass profile; no presentation and no blocking GPU reads.
static void ProfileRendering()
{
    using Clock = std::chrono::steady_clock;
    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    auto summary = [](std::vector<double> v) {
        if (v.empty())
            return std::string("unavailable");
        std::sort(v.begin(), v.end());
        std::ostringstream o;
        o << std::fixed << std::setprecision(4) << v[v.size() / 2] << "/" << v[v.size() * 95 / 100];
        return o.str();
    };
    FramebufferSpecification spec;
    spec.Width = 1280;
    spec.Height = 720;
    spec.Attachments = {FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    target->Bind();
    auto imageA = Texture2D::Create(TextureSpecification()),
         imageB = Texture2D::Create(TextureSpecification());
    uint32_t red = 0xff2030d0, blue = 0xffd03020;
    imageA->SetData(&red, 4);
    imageB->SetData(&blue, 4);
    for (bool mixed : {false, true})
        for (int count : {64, 512, 2048})
        {
            std::ostringstream yaml;
            yaml << "SceneVersion: 2\nScene: Render profile\nEntities:\n- Entity: 1\n  "
                    "Relationship: {Parent: 0, Order: 0}\n  UICanvasComponent: {ReferenceSize: "
                    "[1280, 720], SafeInsets: [0, 0, 0, 0]}\n";
            for (int i = 0; i < count; ++i)
            {
                yaml << "- Entity: " << i + 2 << "\n  Relationship: {Parent: 1, Order: " << i
                     << "}\n  UIRectComponent: {LayoutVersion: 3, Offset: [" << (i % 32) * 40
                     << ", " << (i / 32) * 11
                     << "], FixedSize: [38, 10]}\n  SpriteRendererComponent: {Color: [0.2, 0.4, "
                        "0.6, 0.7]}\n";
                if (mixed && i % 2 == 0)
                    yaml << "  TextComponent: {TextString: 'Item', Color: [1, 1, 1, 0.9], Kerning: "
                            "0, LineSpacing: 0}\n  UITextStyleComponent: {Size: 8, Wrap: false}\n";
            }
            auto scene = CreateRef<Scene>();
            Check(SceneSerializer(scene).DeserializeText(yaml.str()),
                  "Render fixture import failed");
            scene->OnViewportResize(1280, 720);
            scene->PrepareSprites(true);
            RuntimeUI ui(*scene);
            auto first = scene->GetEntityByUUID(2);
            for (int mode = 0; mode < 3; ++mode)
            {
                std::vector<double> source, layout, prepare, submit, total, gpu;
                source.reserve(120);
                layout.reserve(120);
                prepare.reserve(120);
                submit.reserve(120);
                total.reserve(120);
                gpu.reserve(120);
                struct Query
                {
                    GLuint Start = 0, End = 0;
                    bool Pending = false;
                    int Sample = -1;
                };
                std::array<Query, 32> ring{};
                for (auto &q : ring)
                {
                    glGenQueries(1, &q.Start);
                    glGenQueries(1, &q.End);
                }
                size_t next = 0;
                uint64_t allocations = 0;
                uint32_t skipped = 0;
                Renderer2D::Statistics stats;
                for (int frame = -20; frame < 120; ++frame)
                {
                    if (mode == 1)
                        first.GetComponent<UIRectComponent>().Offset.x = float((frame + 20) % 2);
                    if (mode == 2)
                    {
                        first.GetComponent<SpriteRendererComponent>().Source =
                            TextureSpriteSource{{}, 1, (frame + 20) % 2 ? imageA : imageB};
                        if (mixed)
                            first.GetComponent<TextComponent>().TextString =
                                (frame + 20) % 2 ? "Item" : "View";
                    }
                    RenderCommand::SetClearColor({0, 0, 0, 0});
                    RenderCommand::Clear();
                    for (auto &q : ring)
                        if (q.Pending)
                        {
                            GLint ready = 0;
                            glGetQueryObjectiv(q.End, GL_QUERY_RESULT_AVAILABLE, &ready);
                            if (ready)
                            {
                                GLuint64 start = 0, end = 0;
                                glGetQueryObjectui64v(q.Start, GL_QUERY_RESULT, &start);
                                glGetQueryObjectui64v(q.End, GL_QUERY_RESULT, &end);
                                if (q.Sample >= 0)
                                    gpu.push_back(double(end - start) / 1e6);
                                q.Pending = false;
                            }
                        }
                    auto &q = ring[next];
                    bool timing = !q.Pending;
                    if (timing)
                    {
                        q.Pending = true;
                        q.Sample = frame;
                        next = (next + 1) % ring.size();
                    }
                    else if (frame >= 0)
                        ++skipped;
                    s_Allocations = 0;
                    s_CountAllocations = true;
                    auto begin = Clock::now();
                    ui.Resolve({1280, 720});
                    auto resolved = Clock::now();
                    scene->PrepareSprites(true);
                    auto prepared = Clock::now();
                    Renderer2D::ResetStats();
                    if (timing)
                        glQueryCounter(q.Start, GL_TIMESTAMP);
                    ui.Draw();
                    if (timing)
                        glQueryCounter(q.End, GL_TIMESTAMP);
                    auto end = Clock::now();
                    s_CountAllocations = false;
                    if (frame >= 0)
                    {
                        auto d = ui.Diagnostics();
                        source.push_back(d.LastSourceMilliseconds);
                        layout.push_back(d.LastLayoutMilliseconds);
                        prepare.push_back(ms(resolved, prepared) + d.LastPrepareMilliseconds);
                        submit.push_back(d.LastSubmitMilliseconds);
                        total.push_back(ms(begin, end));
                        allocations += s_Allocations;
                        stats = Renderer2D::GetStats();
                    }
                    glFlush(); // Submit without waiting; presentation is deliberately excluded.
                }
                // Collect only ready results. No per-sample wait or forced finish.
                for (auto &q : ring)
                {
                    if (q.Pending)
                    {
                        GLint ready = 0;
                        glGetQueryObjectiv(q.End, GL_QUERY_RESULT_AVAILABLE, &ready);
                        if (ready && q.Sample >= 0)
                        {
                            GLuint64 a = 0, b = 0;
                            glGetQueryObjectui64v(q.Start, GL_QUERY_RESULT, &a);
                            glGetQueryObjectui64v(q.End, GL_QUERY_RESULT, &b);
                            gpu.push_back(double(b - a) / 1e6);
                        }
                    }
                    glDeleteQueries(1, &q.Start);
                    glDeleteQueries(1, &q.End);
                }
                std::cout << "RENDER_PROFILE " << (mixed ? "mixed" : "images") << " count=" << count
                          << " mode=" << mode << " source_ms=" << summary(source)
                          << " layout_ms=" << summary(layout) << " prepare_ms=" << summary(prepare)
                          << " submit_ms=" << summary(submit)
                          << " boundary+paint_ms=" << summary(total)
                          << " gpu_interval_ms=" << summary(gpu) << " gpu_samples=" << gpu.size()
                          << " gpu_skipped=" << skipped << " cpp_new=" << allocations
                          << " draws=" << stats.DrawCalls << " quads=" << stats.QuadCount
                          << " bytes=" << stats.UploadedBytes
                          << " prepared_reuse=" << ui.Diagnostics().PreparedReuses
                          << " prepared_rebuild=" << ui.Diagnostics().PreparedRebuilds
                          << " prepared_bytes=" << ui.Diagnostics().PreparedBytes << " flushes=";
                for (auto n : stats.Flushes)
                    std::cout << n << ",";
                std::cout << std::endl;
            }
        }
    target->Unbind();
    Check(glGetError() == GL_NO_ERROR, "Render profile GL error");
    std::cout << "PASS: 1280x720 UI-only render profile, 20 warm/120 measured frames, median/p95 "
                 "CPU and delayed nonblocking GPU intervals\n";
}
