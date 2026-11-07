#pragma once
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include <iomanip>
// Controlled, additive inspection of ordinary saved Showcase content. Numeric
// probes are transient scene submissions, not authored material/scene settings.
static void IBLSurfaceChecks()
{
    const auto selected = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_SHOWCASE_PROJECT");
    Check(!selected.empty(), "Surface inspection needs a disposable Showcase selector");
    auto project = Project::LoadCandidate(selected);
    auto scene = project->LoadScene("Scenes/SurfaceDetail.hazel");
    const auto authored = SceneSerializer(scene).CaptureAuthoredState();
    const auto output = FileSystem::GetEnvironmentPath("HAZEL_IBL_SURFACE_CAPTURE");
    Check(!output.empty() && output.is_absolute(),
          "Surface inspection requires an explicit absolute capture directory");
    std::filesystem::create_directories(output);
    constexpr unsigned width = 1280, height = 720;
    scene->OnViewportResize(width, height);
    auto entity = scene->GetPrimaryCameraEntity();
    auto world = scene->GetWorldTransform(entity);
    auto &lens = entity.GetComponent<CameraComponent>().Camera;
    NavigationCamera camera(glm::degrees(lens.GetPerspectiveVerticalFOV()), float(width) / height,
                            lens.GetPerspectiveNearClip(), lens.GetPerspectiveFarClip());
    glm::vec3 position, rotation, scale;
    Check(Math::DecomposeTransform(world, position, rotation, scale), "Diagnostic camera pose");
    camera.RestoreWorldPose(position, glm::quat(rotation));
    auto seed = camera;
    FramebufferSpecification spec;
    spec.Width = width;
    spec.Height = height;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto numeric = Framebuffer::Create(spec);
    spec.Attachments = {FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto display = Framebuffer::Create(spec);
    spec.Attachments = {FramebufferTextureFormat::RGBA8};
    auto encoded = Framebuffer::Create(spec);
    SceneRenderView view;
    auto dump = [&](const std::string &name, const Ref<Framebuffer> &target, bool image)
    {
        target->Bind();
        std::vector<glm::vec4> pixels(size_t(width) * height);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels.data());
        std::ofstream raw(output / (name + ".rgba32f"), std::ios::binary);
        raw.write(reinterpret_cast<const char *>(pixels.data()), pixels.size() * sizeof(glm::vec4));
        Check(bool(raw), "Cannot save linear surface inspection");
        std::vector<int> ids(size_t(width) * height);
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_INT, ids.data());
        std::ofstream mask(output / (name + ".ids32"), std::ios::binary);
        mask.write(reinterpret_cast<const char *>(ids.data()), ids.size() * 4);
        if (image)
        {
            target->CompositeWorldTo(display, 0);
            display->EncodeOutput(encoded);
            encoded->Bind();
            std::vector<unsigned char> rgb(size_t(width) * height * 4);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgb.data());
            std::ofstream ppm(output / (name + ".ppm"), std::ios::binary);
            ppm << "P6\n" << width << ' ' << height << "\n255\n";
            for (int y = height - 1; y >= 0; --y)
                for (unsigned x = 0; x < width; ++x)
                    ppm.write(
                        reinterpret_cast<const char *>(rgb.data() + (size_t(y) * width + x) * 4),
                        3);
        }
    };
    YAML::Node metadata;
    metadata["Width"] = width;
    metadata["Height"] = height;
    metadata["Exposure"] = 0;
    metadata["Yaw"] = scene->Presentation().Environment.Yaw;
    metadata["Gain"] = scene->Presentation().Environment.Gain;
    metadata["PBRShaderSHA256"] =
        AssetHash(FileDocument::Read(Resources::Resolve("shaders/ForwardPBR.glsl")));
    for (auto uuid : {110, 111, 112, 113, 114, 120, 121})
        metadata["Entities"][std::to_string(uuid)] =
            int(entt::entity(scene->GetEntityByUUID(uuid)));
    // Thirteen saved views over a slow 6.9-degree orbit. No lighting/material
    // edits.
    for (int step = -6; step <= 6; ++step)
    {
        camera = seed;
        camera.Orbit({step * 2.f, 0});
        SceneCameraView cv{camera.GetViewProjection(), camera.GetPosition(),
                           -camera.GetForwardDirection(), false};
        auto frame = SceneRenderer::Collect(*scene, cv);
        frame.Light.reset();
        metadata["Views"][std::to_string(step)]["Eye"] =
            std::vector<float>{cv.Position.x, cv.Position.y, cv.Position.z};
        for (bool dark : {false, true})
        {
            frame.Presentation.Environment.Background = !dark;
            frame.Presentation.Background = {0, 0, 0, 1};
            for (auto mode :
                 {SurfaceView::Shaded, SurfaceView::BaseColor, SurfaceView::MetalRoughAO,
                  SurfaceView::DiffuseIBL, SurfaceView::SpecularIBL, SurfaceView::Normal,
                  SurfaceView::PrefilteredIBL, SurfaceView::BRDFLUT, SurfaceView::SurfaceParameters,
                  SurfaceView::WorldPosition, SurfaceView::GeometricNormal,
                  SurfaceView::BaseNormal})
            {
                if (step != 0 && (!dark ? mode != SurfaceView::Shaded
                                        : step % 3 != 0 || mode == SurfaceView::Shaded))
                    continue;
                if (!dark && step == 0 && mode != SurfaceView::Shaded)
                    continue;
                frame.Debug = mode;
                frame.LinearLightingInspection = int(mode) >= 6;
                auto destination = mode == SurfaceView::Shaded ? display : numeric;
                destination->Bind();
                RenderCommand::SetClearColor({0, 0, 0, 1});
                RenderCommand::Clear();
                destination->ClearAttachment(1, -1);
                SceneRenderer::Render(frame, view, destination, [] {}, [] {});
                auto target = mode == SurfaceView::Shaded ? view.Radiance : numeric;
                dump(std::string(dark ? "dark-" : "studio-") + std::to_string(step) + "-" +
                         std::to_string(int(mode)),
                     target, mode == SurfaceView::Shaded);
            }
        }
    }
    // Exercise the real editor scene entry and player session on exactly the
    // authored view, not merely two hand-built submissions.
    auto readDisplay = [&]
    {
        display->Bind();
        std::vector<glm::vec4> pixels(size_t(width) * height);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels.data());
        return pixels;
    };
    camera = seed;
    display->Bind();
    RenderCommand::Clear();
    display->ClearAttachment(1, -1);
    scene->OnUpdateEditor(0, camera, &view, display);
    auto editorPixels = readDisplay();
    dump("host-editor-display", display, false);
    RuntimeSession runtime;
    runtime.Resize(width, height);
    runtime.ResizeFramebuffer(width, height);
    runtime.SetRenderTarget(display);
    runtime.Start(project, scene);
    display->Bind();
    RenderCommand::Clear();
    display->ClearAttachment(1, -1);
    runtime.Update(0);
    auto playerPixels = readDisplay();
    dump("host-player-display", display, false);
    Check(runtime.GetError().empty(), "Surface player session failed");
    double maximumHostDifference = 0;
    for (size_t i = 0; i < editorPixels.size(); ++i)
        for (int c = 0; c < 3; ++c)
            maximumHostDifference = std::max(
                maximumHostDifference, double(std::abs(editorPixels[i][c] - playerPixels[i][c])));
    std::cout << "Surface editor/player whole-image maximum: " << maximumHostDifference << '\n';
    auto readIDs = [&]
    {
        std::vector<int> ids(size_t(width) * height);
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_INT, ids.data());
        return ids;
    };
    // Entity handles differ in the runtime copy. Compare UUID-defined coverage,
    // then colors one pixel inside it: a sub-ULP view-matrix difference at the
    // shading horizon can legitimately change the domain of a boundary sample.
    auto playerIDs = readIDs();
    display->Bind();
    scene->OnUpdateEditor(0, camera, &view, display);
    auto editorIDs = readIDs();
    double interiorDifference = 0;
    size_t interiorSamples = 0;
    for (uint64_t uuid : {110, 111, 112, 113, 114, 120, 121})
    {
        int a = int(entt::entity(scene->GetEntityByUUID(uuid))),
            b = int(entt::entity(runtime.GetScene()->GetEntityByUUID(uuid)));
        for (size_t i = width + 1; i + width + 1 < editorIDs.size(); ++i)
        {
            Check((editorIDs[i] == a) == (playerIDs[i] == b),
                  "Editor/player mesh coverage differs");
            if (editorIDs[i] != a)
                continue;
            bool interior = true;
            for (auto j : {i - 1, i + 1, i - width, i + width})
                interior &= editorIDs[j] == a && playerIDs[j] == b;
            if (!interior)
                continue;
            ++interiorSamples;
            for (int c = 0; c < 3; ++c)
                interiorDifference = std::max(
                    interiorDifference, double(std::abs(editorPixels[i][c] - playerPixels[i][c])));
        }
    }
    Check(interiorSamples > 200000 && interiorDifference < .003,
          "Editor/player surface interior disagrees");
    metadata["MaximumEditorPlayerInteriorDifference"] = interiorDifference;
    runtime.Stop();
    metadata["MaximumEditorPlayerDisplayDifference"] = maximumHostDifference;
    camera = seed;
    SceneCameraView cv{camera.GetViewProjection(), camera.GetPosition(),
                       -camera.GetForwardDirection(), false};
    auto originalFrame = SceneRenderer::Collect(*scene, cv);
    originalFrame.Light.reset();
    originalFrame.Presentation.Environment.Background = false;
    originalFrame.Presentation.Background = {0, 0, 0, 1};
    for (const std::string variant : {"woodNoNormal", "dielectric", "conductor", "constant"})
    {
        auto frame = originalFrame;
        int wood = int(entt::entity(scene->GetEntityByUUID(121)));
        for (auto &mesh : frame.Meshes)
            if (mesh.Picking == wood)
            {
                auto material = CreateRef<MaterialDefinition>(*mesh.Materials[0]);
                if (variant != "constant")
                    material->Textures[2].Image.clear();
                if (variant == "dielectric" || variant == "conductor")
                {
                    material->Textures = {};
                    material->BaseColor = {.18f, .4f, .7f, 1};
                    material->Roughness = .5f;
                    material->Metallic = variant == "conductor" ? 1 : 0;
                    material->OcclusionStrength = 1;
                }
                mesh.Materials = {material};
            }
        if (variant == "constant")
        {
            auto constant = CreateRef<PreparedEnvironment>(*frame.Environment);
            constant->Identity += "/surface-constant";
            constant->Artifacts += "/surface-constant";
            for (auto image : {&constant->Background, &constant->Irradiance, &constant->Specular})
            {
                image->EncodingVersion = 1;
                image->Side = 1;
                image->Mips = {std::vector<float>(18, image->Kind == LightingImageKind::Irradiance
                                                          ? float(std::acos(-1.))
                                                          : 1.f)};
            }
            frame.Environment = constant;
        }
        for (auto mode :
             {SurfaceView::Shaded, SurfaceView::BaseColor, SurfaceView::MetalRoughAO,
              SurfaceView::DiffuseIBL, SurfaceView::SpecularIBL, SurfaceView::Normal,
              SurfaceView::PrefilteredIBL, SurfaceView::BRDFLUT, SurfaceView::SurfaceParameters})
        {
            frame.Debug = mode;
            frame.LinearLightingInspection = int(mode) >= 6;
            auto destination = mode == SurfaceView::Shaded ? display : numeric;
            destination->Bind();
            RenderCommand::SetClearColor({0, 0, 0, 1});
            RenderCommand::Clear();
            destination->ClearAttachment(1, -1);
            SceneRenderer::Render(frame, view, destination, [] {}, [] {});
            dump(variant + "-" + std::to_string(int(mode)),
                 mode == SurfaceView::Shaded ? view.Radiance : numeric,
                 mode == SurfaceView::Shaded);
        }
    }
    // A small raster probe sweeps the BRDF domain without changing any saved
    // material or lighting. Rows contain exact shader inputs and isolated HDR
    // terms; the independent source-cell oracle consumes the paired input file.
    auto probeSpec = spec;
    probeSpec.Width = probeSpec.Height = 16;
    probeSpec.Attachments = {FramebufferTextureFormat::RGBA32F,
                             FramebufferTextureFormat::RED_INTEGER,
                             FramebufferTextureFormat::Depth};
    auto probe = Framebuffer::Create(probeSpec);
    SceneRenderView probeView;
    auto sweep = originalFrame;
    sweep.Meshes.clear();
    SubmittedMesh cube;
    cube.Geometry = project->GetAssets()->Mesh("Native/Direct_v1/UnitCube.hmesh").Accepted;
    cube.Picking = 77;
    auto surface = CreateRef<MaterialDefinition>();
    surface->BaseColor = {.18f, .4f, .7f, 1};
    cube.Materials = {surface};
    std::ofstream inputs(output / "sweep-inputs.txt"), results(output / "sweep-results.txt");
    for (auto n : {glm::dvec3(.3, .8, -.4), glm::dvec3(-.8, -.2, .3), glm::dvec3(1, .13, .9999),
                   glm::dvec3(.9999, .13, 1)})
    {
        n = glm::normalize(n);
        auto t = glm::normalize(glm::cross(n, glm::dvec3(0, 0, 1)));
        auto N = glm::vec3(n), up = std::abs(N.y) > .9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        cube.SetWorld(glm::inverse(glm::lookAt(glm::vec3(0), -N, up)));
        sweep.Meshes = {cube};
        for (double nv : {-.02, .02, .1, .4, .7, 1.})
            for (float rough : {.05f, .2f, .45f, .75f, 6.f / 7, .9f, .95f, 1.f})
                for (float metallic : {0.f, 1.f})
                {
                    auto V = glm::vec3(n * nv + t * std::sqrt(1 - nv * nv));
                    surface->Roughness = rough;
                    surface->Metallic = metallic;
                    sweep.Camera = {glm::ortho(-1.f, 1.f, -1.f, 1.f, .1f, 10.f) *
                                        glm::lookAt(N * 3.f, glm::vec3(0), up),
                                    N * 3.f, V, true};
                    double yaw = sweep.Presentation.Environment.Yaw;
                    auto env = [&](glm::dvec3 v)
                    {
                        return glm::dvec3(std::cos(yaw) * v.x - std::sin(yaw) * v.z, v.y,
                                          std::sin(yaw) * v.x + std::cos(yaw) * v.z);
                    };
                    for (auto v :
                         {env(n), env(glm::dvec3(V)), env(n), glm::dvec3(surface->BaseColor)})
                        inputs << std::setprecision(12) << v.x << ' ' << v.y << ' ' << v.z << ' ';
                    inputs << rough << ' ' << metallic << " 1\n";
                    for (auto mode :
                         {SurfaceView::SpecularIBL, SurfaceView::DiffuseIBL, SurfaceView::BRDFLUT})
                    {
                        sweep.Debug = mode;
                        sweep.LinearLightingInspection = true;
                        probe->Bind();
                        RenderCommand::Clear();
                        probe->ClearAttachment(1, -1);
                        SceneRenderer::Render(sweep, probeView, probe, [] {}, [] {});
                        auto p = probe->ReadColorPixel(0, 8, 8);
                        Check(probe->ReadPixel(1, 8, 8) == 77, "Surface sweep lost coverage");
                        results << std::setprecision(12) << p.x << ' ' << p.y << ' ' << p.z << ' ';
                    }
                    results << '\n';
                }
    }
    Check(bool(inputs) && bool(results), "Cannot save grazing sweep");
    YAML::Emitter yaml;
    yaml.SetFloatPrecision(9);
    yaml << metadata;
    FileSystem::WriteNewFile(output / "capture.yaml", std::string(yaml.c_str()) + "\n");
    Check(SceneSerializer(scene).CaptureAuthoredState() == authored,
          "Surface diagnostic changed authored Showcase");
    Check(glGetError() == GL_NO_ERROR, "Surface diagnostic GL error");
    std::cout << "PASS: native surface diagnostic, fixed studio inputs, linear "
                 "HDR terms and slow orbit; other RuntimeSession checks not "
                 "executed in focused mode\n";
}
