#pragma once
static void SelectionOutlineChecks()
{
    RenderCommand::StateScope scope;
    Fixture fixture;
    NativeStarter::Generate(fixture.Directory);
    auto assets = CreateRef<ProjectAssets>(fixture.Directory);
    auto cube = assets->Mesh("Native/Direct_v1/UnitCube.hmesh").Accepted;
    auto diamond = CreateRef<MeshAsset>(*cube);
    diamond->Definition.ID = 300;
    diamond->Geometry.Mesh = 300;
    auto &geometry = diamond->Geometry;
    geometry.Vertices.clear();
    for (auto point :
         {glm::vec2(-.7f, 0), glm::vec2(0, -.7f), glm::vec2(.7f, 0), glm::vec2(0, .7f)})
    {
        MeshVertex vertex;
        vertex.Position = {point, 0};
        vertex.UV = {point.x / .7f * .5f + .5f, .5f - point.y / .7f * .5f};
        geometry.Vertices.push_back(vertex);
    }
    geometry.Indices = {0, 1, 2, 0, 2, 3};
    geometry.Primitives = {{diamond->Definition.Slots[0].ID, 0, 6, 1, {}}};
    geometry.UpdateBounds();
    auto material = CreateRef<MaterialDefinition>();
    material->Model = MaterialModel::Unlit;
    material->BaseColor = {.1f, .2f, .3f, 1};
    material->DoubleSided = true;
    std::string tga(18, '\0');
    tga[2] = 2;
    tga[12] = tga[14] = 8;
    tga[16] = 32;
    tga[17] = 0x28;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
        {
            tga += std::string(3, char(255));
            tga += char(x >= 2 && x < 6 && y >= 2 && y < 6 ? 0 : 255);
        }
    FileSystem::WriteNewFile(fixture.Directory / "Cutout.tga", tga);
    for (int side : {128, 256})
        for (int kind = 0; kind < 4; ++kind)
        {
            FramebufferSpecification spec;
            spec.Width = spec.Height = side;
            spec.Attachments = {FramebufferTextureFormat::RGBA16F,
                                FramebufferTextureFormat::RED_INTEGER,
                                FramebufferTextureFormat::Depth};
            auto source = Framebuffer::Create(spec);
            spec.Attachments = {FramebufferTextureFormat::RGBA8};
            auto output = Framebuffer::Create(spec);
            SceneSubmission frame;
            SceneRenderView view;
            frame.Assets = assets;
            frame.Presentation.SceneReferred = true;
            frame.Camera.Position = {0, 0, 3};
            frame.Camera.ViewProjection =
                glm::ortho(-1.f, 1.f, -1.f, 1.f, .1f, 10.f) *
                glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0), glm::vec3(0, 1, 0));
            material->Alpha = kind == 3 ? MaterialAlpha::Mask : MaterialAlpha::Opaque;
            material->Textures[0].Image = kind == 3 ? "Cutout.tga" : "";
            SubmittedMesh selected;
            selected.Geometry = diamond;
            selected.Materials = {material};
            selected.Picking = 123;
            selected.SetWorld(glm::rotate(glm::mat4(1), .2f, glm::vec3(0, 0, 1)) *
                              glm::scale(glm::mat4(1), glm::vec3(-1, .8f, 1)));
            frame.Meshes = {selected};
            if (kind == 1 || kind == 2)
            {
                auto wall = selected;
                wall.Geometry = cube;
                wall.Picking = 456;
                wall.SetWorld(
                    glm::translate(glm::mat4(1), glm::vec3(kind == 1 ? -.4f : 0, 0, .5f)) *
                    glm::scale(glm::mat4(1), glm::vec3(kind == 1 ? .8f : 2, 2, .1f)));
                frame.Meshes.push_back(wall);
            }
            source->Bind();
            RenderCommand::SetClearColor({0, 0, 0, 1});
            RenderCommand::Clear();
            source->ClearAttachment(1, -1);
            SceneRenderer::Render(frame, view, source, [] {}, [] {});
            std::vector<int> before(side * side), after(side * side);
            glReadBuffer(GL_COLOR_ATTACHMENT1);
            glReadPixels(0, 0, side, side, GL_RED_INTEGER, GL_INT, before.data());
            source->EncodeOutput(output);
            output->Bind();
            std::vector<unsigned char> pixels(side * side * 4), original(pixels.size());
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, original.data());
            output->Unbind();
            int radius = side / 64;
            auto stateBefore = RenderCommand::GetRasterState();
            GLint bindingBefore;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bindingBefore);
            source->DrawEntityOutlineTo(output, 123, radius, {1, .5f, 0, 1});
            auto stateAfter = RenderCommand::GetRasterState();
            GLint bindingAfter;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bindingAfter);
            Check(bindingBefore == bindingAfter && stateBefore.DepthTest == stateAfter.DepthTest &&
                      stateBefore.Blend == stateAfter.Blend &&
                      stateBefore.PickingMask == stateAfter.PickingMask &&
                      stateBefore.Viewport == stateAfter.Viewport,
                  "Outline failed pass-state restoration");
            output->Bind();
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            output->Unbind();
            int edges = 0, visible = 0;
            for (int y = 0; y < side; ++y)
                for (int x = 0; x < side; ++x)
                {
                    auto at = y * side + x;
                    bool edge = false;
                    if (before[at] == 123)
                    {
                        ++visible;
                        for (int yy = -radius; yy <= radius; ++yy)
                            for (int xx = -radius; xx <= radius; ++xx)
                                if (xx * xx + yy * yy <= radius * radius)
                                {
                                    int sx = x + xx, sy = y + yy;
                                    if (sx < 0 || sy < 0 || sx >= side || sy >= side ||
                                        before[sy * side + sx] != 123)
                                        edge = true;
                                }
                    }
                    if (edge)
                    {
                        ++edges;
                        Check(pixels[at * 4] == 255 &&
                                  std::abs(int(pixels[at * 4 + 1]) - 128) <= 1 &&
                                  pixels[at * 4 + 2] == 0,
                              "Visible mesh silhouette coverage mismatch");
                    }
                    else
                        for (int c = 0; c < 4; ++c)
                            Check(pixels[at * 4 + c] == original[at * 4 + c],
                                  "Outline changed an interior, occluder or background pixel");
                }
            source->Bind();
            glReadBuffer(GL_COLOR_ATTACHMENT1);
            glReadPixels(0, 0, side, side, GL_RED_INTEGER, GL_INT, after.data());
            Check(before == after, "Outline altered picking IDs");
            Check(kind == 2 ? edges == 0 && visible == 0 : edges > 0 && visible > 0,
                  "Partial/full occlusion silhouette failed");
            Check(source->ReadPixel(1, 2, side - 2) == (kind == 2 ? 456 : -1), "Outside silhouette must retain background/occluder picking");
            if (kind == 3)
                Check(source->ReadPixel(1, side / 2, side / 2) == -1,
                      "Alpha cutout wrote picking/outline coverage");
        }
    Check(glGetError() == GL_NO_ERROR, "Outline leaked GL error/state");
    std::cout << "PASS: transformed nonrectangular mesh silhouette, partial/full occlusion, alpha "
                 "cutout, exact unchanged ID/interior coverage and 1x/2x thickness\n";
}
