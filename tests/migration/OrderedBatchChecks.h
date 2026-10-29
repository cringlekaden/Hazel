// Actual shared renderer, compared against immediate painter-order submission.
static void OrderedBatchChecks()
{
    auto font = Hazel::Font::GetDefault();
    auto other = CreateRef<Hazel::Font>("assets/fonts/opensans/OpenSans-Bold.ttf");
    struct Command
    {
        int Kind, ID;
        glm::ivec4 Clip;
        glm::vec4 Tint;
        Ref<Hazel::Font> Face;
    };
    std::vector<Command> commands{{0, 201, {0, 0, 64, 128}, {1, 0, 0, .6f}, {}},
                                  {0, 202, {64, 0, 64, 128}, {0, 0, 1, .7f}, {}},
                                  {1, 203, {16, 8, 80, 100}, {1, 1, 1, .8f}, font},
                                  {0, 204, {28, 20, 44, 80}, {0, 1, 0, .4f}, {}},
                                  {1, 205, {10, 20, 110, 70}, {1, .2f, 1, .5f}, other},
                                  {1, 206, {10, 20, 110, 70}, {.2f, 1, 1, .8f}, font},
                                  {0, 207, {0, 0, 0, 128}, {1, 1, 0, 1}, {}},
                                  {2, 208, {0, 60, 96, 8}, {1, 1, 1, .8f}, {}},
                                  {3, 209, {72, 4, 48, 44}, {.8f, .6f, .1f, .5f}, {}}};
    auto paint = [&](bool ordered) {
        Begin();
        Renderer2D::EndScene();
        auto state = RenderCommand::GetRasterState();
        state.DepthTest = false;
        state.ClipEnabled = !ordered;
        RenderCommand::SetRasterState(state);
        if (ordered)
            Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)), glm::mat4(1));
        for (const auto &c : commands)
        {
            if (ordered)
                Renderer2D::SetPrimitiveClip(c.Clip);
            else
            {
                state.ClipRect = c.Clip;
                RenderCommand::SetRasterState(state);
                Renderer2D::BeginScene(Camera(glm::mat4(1)), glm::mat4(1));
            }
            if (c.Kind == 0)
                Renderer2D::DrawQuad(Transform(0, 0, 2), c.Tint, c.ID);
            if (c.Kind == 1)
                Renderer2D::DrawString("Aé", c.Face, Transform(-.6f, -.3f, 1.2f), {c.Tint}, c.ID);
            if (c.Kind == 2)
                Renderer2D::DrawLine({-1, 0, 0}, {1, 0, 0}, c.Tint, c.ID);
            if (c.Kind == 3)
                Renderer2D::DrawCircle(Transform(.4f, -.5f, .6f), c.Tint, 1, .005f, c.ID);
            if (!ordered)
                Renderer2D::EndScene();
        }
        if (ordered)
            Renderer2D::EndScene();
        std::vector<unsigned char> color(128 * 128 * 4);
        std::vector<int> ids(128 * 128);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(0, 0, 128, 128, GL_RED_INTEGER, GL_INT, ids.data());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        state.ClipEnabled = false;
        RenderCommand::SetRasterState(state);
        return std::make_pair(color, ids);
    };
    auto reference = paint(false), batched = paint(true);
    Check(
        reference.second == batched.second,
        "Ordered image/text/font/line/circle/empty clip picking differs from immediate reference");
    for (size_t i = 0; i < reference.first.size(); ++i)
        Check(std::abs(int(reference.first[i]) - int(batched.first[i])) <= 1,
              "Ordered translucent compositing differs from immediate reference");
    Check(ID(4, 110) == 201 && ID(124, 110) == 202, "Later clip affected earlier queued geometry");
    Begin();
    Renderer2D::DrawString("A", font, Transform(-.3f, -.3f, 1), {}, 211);
    End();
    Check(Pixels(211) > 100, "Ordered pass leaked its font sampler into subsequent world text");
    Begin();
    Renderer2D::EndScene();
    Renderer2D::ResetStats();
    Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)), glm::mat4(1));
    for (int i = 0; i < 64; ++i)
    {
        Renderer2D::SetPrimitiveClip({i, 0, 64, 128});
        Renderer2D::DrawQuad(Transform(), {.2f, .3f, .4f, .7f}, 210);
    }
    End();
    Check(Renderer2D::GetStats().DrawCalls == 1,
          "Compatible adjacent different-clip quads did not batch");
    for (int kind = 0; kind < 4; ++kind)
    {
        Begin();
        Renderer2D::EndScene();
        Renderer2D::ResetStats();
        Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)), glm::mat4(1));
        unsigned capacity = kind == 2 ? 40000 : 20000;
        for (unsigned i = 0; i <= capacity; ++i)
        {
            bool last = i == capacity;
            auto t = Transform(last ? 0 : 10, last ? 0 : 10);
            if (kind == 0)
                Renderer2D::DrawQuad(t, {1, 0, 0, 1}, 220);
            if (kind == 1)
                Renderer2D::DrawCircle(t, {0, 1, 0, 1}, 1, .005f, 221);
            if (kind == 2)
                Renderer2D::DrawLine({last ? -.5f : 10, 0, 0}, {last ? .5f : 11, 0, 0},
                                     {1, 1, 1, 1}, 222);
            if (kind == 3)
                Renderer2D::DrawString("A", font, t, {}, 223);
        }
        End();
        Check(Renderer2D::GetStats().DrawCalls == 2 && Pixels(220 + kind) > 30,
              "Ordered geometry rollover lost final output");
    }
    const unsigned limit = RenderCommand::GetMaxTextureSlots();
    std::vector<Ref<Texture2D>> textures;
    Begin();
    Renderer2D::EndScene();
    Renderer2D::ResetStats();
    Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)), glm::mat4(1));
    for (unsigned i = 0; i < limit + 2; ++i)
    {
        auto t = Texture2D::Create(TextureSpecification());
        uint32_t pixel = 0xff00ff00;
        t->SetData(&pixel, 4);
        textures.push_back(t);
        Renderer2D::DrawQuad(Transform(), t, 1, glm::vec4(1), 230 + i);
    }
    End();
    Check(ID() == int(231 + limit) &&
              Renderer2D::GetStats().DrawCalls == (limit + 2 + limit - 2) / (limit - 1),
          "Ordered texture rollover lost slot or picking");
    Begin();
    Renderer2D::EndScene();
    Renderer2D::ResetStats();
    Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)), glm::mat4(1));
    for (unsigned i = 0; i < limit - 1; ++i)
        Renderer2D::DrawQuad(Transform(), textures[i], 1, glm::vec4(1), 240);
    Renderer2D::DrawString("A", other, Transform(-.7f, -.3f, .8f), {}, 241);
    Renderer2D::DrawQuad(Transform(.7f, 0, .25f), {1, 0, 0, 1}, 242);
    End();
    Check(Pixels(241) > 30 && ID(110, 64) == 242 && Color(110, 64)[0] > 250 &&
              Color(110, 64)[1] < 5,
          "Full texture chunk failed shared font-unit fallback/white restoration");
    Begin();
    Renderer2D::EndScene();
    Renderer2D::ResetStats();
    Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)), glm::mat4(1));
    for (int i = 0; i < 8193; ++i)
    {
        if (i % 2)
            Renderer2D::DrawString("A", font, Transform(10, 10), {}, 250);
        else
            Renderer2D::DrawQuad(Transform(i == 8192 ? 0 : 10, 0), {1, 0, 0, 1}, 251);
    }
    End();
    Check(ID() == 251 &&
              Renderer2D::GetStats().Flushes[size_t(Renderer2D::FlushReason::Capacity)] > 0,
          "Bounded run storage rollover lost painter order");
    Check(glGetError() == GL_NO_ERROR, "Ordered rendering GL error");
    std::cout << "PASS: painter ordered reference RGBA/IDs, adversarial clips, compatible "
                 "batching, fonts/textures and all geometry/run rollover\n";
}
