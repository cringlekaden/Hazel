#pragma once
#include "GeometricNormalReference.h"
#include "Hazel/Math/Math.h"
#include "Hazel/Scene/SceneSerializer.h"
static void GeometricNormalChecks()
{
    RenderCommand::StateScope scope;
    auto selector = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_SHOWCASE_PROJECT");
    Check(!selector.empty(), "Geometric normal checks need the committed Showcase");
    auto project = Project::LoadCandidate(selector);
    auto scene = project->LoadScene("Scenes/SurfaceDetail.hazel");
    const auto authored = SceneSerializer(scene).CaptureAuthoredState();
    auto native = Application::Get().GetWindow().GetFramebufferSize();
    const unsigned width = native.first, height = native.second;
    Check(width && height && uint64_t(width) * height <= 16777216,
          "Bounded native edge framebuffer required");
    scene->OnViewportResize(width, height);
    auto entity = scene->GetPrimaryCameraEntity();
    auto world = scene->GetWorldTransform(entity);
    const auto &lens = entity.GetComponent<CameraComponent>().Camera;
    NavigationCamera seed(glm::degrees(lens.GetPerspectiveVerticalFOV()), float(width) / height,
                          lens.GetPerspectiveNearClip(), lens.GetPerspectiveFarClip());
    glm::vec3 position, rotation, scale;
    Check(Math::DecomposeTransform(world, position, rotation, scale), "Edge camera pose");
    seed.RestoreWorldPose(position, glm::quat(rotation));
    const int front = int(entt::entity(scene->GetEntityByUUID(114))),
              back = int(entt::entity(scene->GetEntityByUUID(121))),
              occluder = int(entt::entity(scene->GetEntityByUUID(113)));
    FramebufferSpecification spec;
    spec.Width = width;
    spec.Height = height;
    spec.Samples = 1;
    spec.Attachments = {FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto display = Framebuffer::Create(spec);
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto numeric = Framebuffer::Create(spec);
    spec.Attachments = {FramebufferTextureFormat::RGBA8};
    auto encoded = Framebuffer::Create(spec);
    struct Pixels
    {
        std::vector<glm::vec4> Color;
        std::vector<int> IDs;
        std::vector<float> Depth;
    };
    auto read = [&](const Ref<Framebuffer> &target, bool coverage)
    {
        target->Bind();
        Pixels out;
        out.Color.resize(size_t(width) * height);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, out.Color.data());
        if (coverage)
        {
            out.IDs.resize(out.Color.size());
            out.Depth.resize(out.Color.size());
            glReadBuffer(GL_COLOR_ATTACHMENT1);
            glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_INT, out.IDs.data());
            glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, out.Depth.data());
        }
        target->Unbind();
        return out;
    };
    auto render =
        [&](const SceneSubmission &frame, SceneRenderView &view, const Ref<Framebuffer> &target)
    {
        target->Bind();
        RenderCommand::SetClearColor(frame.Presentation.Background);
        RenderCommand::Clear();
        if (target->GetSpecification().Attachments.Attachments.size() > 1)
            target->ClearAttachment(1, -1);
        SceneRenderer::Render(frame, view, target, [] {}, [] {});
    };
    auto make = [&](const NavigationCamera &camera)
    {
        auto frame =
            SceneRenderer::Collect(*scene, {camera.GetViewProjection(), camera.GetPosition(),
                                            -camera.GetForwardDirection(), false});
        frame.Presentation.Environment.Background = false;
        frame.Light.reset();
        frame.Meshes.erase(
            std::remove_if(
                frame.Meshes.begin(), frame.Meshes.end(), [&](auto &m)
                { return m.Picking != front && m.Picking != back && m.Picking != occluder; }),
            frame.Meshes.end());
        Check(frame.Meshes.size() == 3, "Native copper/wood and foreground occluder required");
        return frame;
    };
    // The same overlapping geometry first passes as opaque, flat, unlit color.
    // No AA, filtered coverage, ID change, resolve or post-HDR fringe is allowed.
    auto flat = make(seed);
    flat.Meshes.erase(std::remove_if(flat.Meshes.begin(), flat.Meshes.end(),
                                     [&](auto &m) { return m.Picking == front; }),
                      flat.Meshes.end());
    const glm::vec3 red(.8, .025, .015), blue(.015, .3, .8);
    for (auto &mesh : flat.Meshes)
    {
        auto mat = CreateRef<MaterialDefinition>();
        mat->Model = MaterialModel::Unlit;
        mat->BaseColor = glm::vec4(mesh.Picking == occluder ? red : blue, 1);
        mesh.Materials = {mat};
    }
    SceneRenderView flatView;
    render(flat, flatView, display);
    auto flatHDR = read(flatView.Radiance, true);
    auto flatDisplay = read(display, true);
    display->EncodeOutput(encoded);
    auto flatOutput = read(encoded, false);
    unsigned interfacePixels = 0;
    for (size_t i = 0; i < flatHDR.IDs.size(); ++i)
    {
        int id = flatHDR.IDs[i];
        if (id != occluder && id != back)
            continue;
        auto base = glm::dvec3(id == occluder ? red : blue);
        NearRGB(flatHDR.Color[i], base, 1e-6,
                "Unlit mesh has a coverage/color hole before HDR presentation");
        auto presented = OracleNeutral(base);
        NearRGB(flatDisplay.Color[i], presented, .001,
                "Flat-color HDR presentation mixed neighboring coverage");
        auto transfer = [](double x)
        { return x <= .0031308 ? 12.92 * x : 1.055 * std::pow(x, 1 / 2.4) - .055; };
        NearRGB(flatOutput.Color[i],
                {transfer(presented.r), transfer(presented.g), transfer(presented.b)}, 2. / 255,
                "Final encoding introduced an unlit silhouette fringe");
        Check(flatDisplay.IDs[i] == id && flatDisplay.Depth[i] == flatHDR.Depth[i],
              "Presentation changed unlit picking/depth");
        if (id == occluder && i > width && i + width + 1 < flatHDR.IDs.size())
            for (auto j : {i - 1, i + 1, i - width, i + width})
                if (flatHDR.IDs[j] == back)
                {
                    ++interfacePixels;
                    break;
                }
    }
    Check(interfacePixels > 0, "Flat meshes do not exercise a foreground/rear interface");
    size_t repaired = 0;
    for (int step = -6; step <= 6; ++step)
    {
        auto camera = seed;
        camera.Orbit({step * 2.f, 0});
        auto frame = make(camera);
        SceneRenderView view;
        render(frame, view, display);
        auto hdr = read(view.Radiance, true);
        auto presented = read(display, true);
        frame.Debug = SurfaceView::NormalHorizon;
        frame.LinearLightingInspection = true;
        render(frame, view, numeric);
        auto normals = read(numeric, true);
        Check(hdr.IDs == normals.IDs && hdr.Depth == normals.Depth && presented.IDs == hdr.IDs &&
                  presented.Depth == hdr.Depth,
              "Lit silhouette inspection/presentation changed geometric coverage or picking");
        for (size_t i = 0; i < hdr.IDs.size(); ++i)
        {
            if (hdr.IDs[i] != front && hdr.IDs[i] != back)
                continue;
            const auto &n = normals.Color[i];
            if (n.y > 0 && n.x <= 0)
            {
                ++repaired;
                Check(std::max({hdr.Color[i].r, hdr.Color[i].g, hdr.Color[i].b}) > 1e-4,
                      "Visible geometric mesh sample went black at the shading-normal cutoff");
                Check(n.z > 0, "Visible geometry retained a back-facing shading normal");
            }
        }
    }
    Check(repaired > 0, "Orbit did not reproduce the original shading-normal edge defect");
    Check(SceneSerializer(scene).CaptureAuthoredState() == authored,
          "Edge regression changed authored content");
    Check(glGetError() == GL_NO_ERROR, "Geometric-normal edge GL error");
    std::cout
        << "PASS: native " << width << 'x' << height
        << " single-sample overlapping unlit coverage/presentation and 13-view lit orbit, "
        << repaired
        << " visible shading-horizon samples; exact depth/IDs and unchanged authored content\n";
}
