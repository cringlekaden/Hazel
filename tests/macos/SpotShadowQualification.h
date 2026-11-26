#pragma once
#include "LocalLightingQualification.h"
#include "Hazel/Renderer/Scene/DirectionalShadows.h"
#include <array>
#include <cmath>

inline void SpotCheck(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

inline void SpotProjectionChecks()
{
    using namespace Hazel;
    SceneSubmission frame;
    SubmittedLocalLight light;
    light.CastShadows = true;
    light.Incident = glm::vec3(1);
    light.Position = {1, -2, 3};
    double maximum = 0;
    unsigned samples = 0;
    for (float range : {.01f, 10.f, 10000.f}) for (double angle : {.0015, .52, 1.553})
        for (unsigned side : {256u, 512u, 1024u}) for (double tilt : {0., 1.43, 1.58}) {
            // The analytic pose is a rotation around X; no lookAt or inverse VP in the oracle.
            const glm::dvec3 forward(0, std::sin(tilt), -std::cos(tilt));
            const glm::dvec3 right(1, 0, 0), up(0, std::cos(tilt), std::sin(tilt));
            light.Range = range;
            light.Direction = glm::vec3(forward);
            light.ShadowUp = glm::vec3(up);
            light.OuterCos = float(std::cos(angle));
            light.ShadowResolution = side;
            frame.LocalLights = {light};
            const auto fit = FitSpotShadows(frame);
            const glm::dmat4 vp(fit.Data.VP[0]);
            const double near = double(range) / 1024;
            const double tangent = std::sqrt(1.0 / (double(light.OuterCos) * light.OuterCos) - 1);
            for (double z : {near, 4 * near, double(range) * .51, double(range)}) {
                const auto axis = vp * glm::dvec4(z * forward, 1);
                const double expected = (1 - near / z) / (1 - near / range);
                maximum = std::max(maximum, std::abs((axis.z / axis.w + 1) * .5 - expected));
                for (const auto& radial : {right, -right, up, -up}) {
                    const auto edge = vp * glm::dvec4(z * (forward + tangent * radial), 1);
                    const double edgeRadius = std::hypot(edge.x, edge.y) / edge.w;
                    SpotCheck(std::abs(edgeRadius - (1 - 4.0 / side)) < .0002, "Spot cone edge projection mismatch");
                    SpotCheck(edgeRadius < 1, "Spot projection clipped the authored cone");
                    ++samples;
                }
            }
            if (range == 10.f && angle == .52 && tilt == 0) {
                auto moved = frame;
                moved.Camera.Position = {17, 32, -5};
                SpotCheck(FitSpotShadows(moved).Data.VP == fit.Data.VP, "Camera changed spot projection");
                auto axisPoint = glm::vec3(forward);
                auto behind = glm::vec3(-forward);
                SpotCheck(ShadowCasterVisible(axisPoint, axisPoint, fit.Data.VP[0]) &&
                    !ShadowCasterVisible(behind, behind, fit.Data.VP[0]), "Spot homogeneous caster rejection failed");
            }
        }
    SpotCheck(maximum < .0002, "Spot axial depth exceeded reference tolerance");
    frame.LocalLights.assign(7, light);
    frame.LocalLights[0].CastShadows = false;
    frame.LocalLights[1].Incident = glm::vec3(0);
    frame.LocalLights[2].ShadowResolution = 256;
    frame.LocalLights[3].ShadowResolution = 512;
    const auto mixed = FitSpotShadows(frame);
    SpotCheck(mixed.Count == 4 && mixed.Requested == 5 && mixed.Extent == glm::uvec2(2816, 1024) &&
        mixed.Data.LightIndices == glm::ivec4(3, 4, 5, 6) && mixed.Data.Tiles[2].x == 768,
        "Spot map admission or mixed tile offset failed");
    std::cout << "PASS spot FP64 projection cone samples=" << samples << " maxDepthError=" << maximum << '\n';
}

inline void SpotShadowQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    SpotProjectionChecks();
    std::filesystem::create_directories(output);
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    auto project = Project::LoadCandidate(FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT"));
    auto assets = CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    auto mask = assets->Material("Reference/Direct_v1/Surfaces/Mask.hmat");
    SpotCheck(mesh.Accepted && mask.Accepted, "Spot fixtures missing cube or texture cutout");
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {.6f, .6f, .6f, 1};
    material->Roughness = .5f;
    SubmittedMesh ground;
    ground.Geometry = mesh.Accepted;
    ground.Materials = {material};
    ground.Picking = 11;
    ground.Identity = 11;
    ground.SetWorld(glm::translate(glm::mat4(1), {0, 0, -.05f}) * glm::scale(glm::mat4(1), {8, 8, .1f}));
    SubmittedMesh caster = ground;
    caster.Picking = 12;
    caster.Identity = 12;
    constexpr glm::vec3 centre(.1f, .1f, .45f), half(.35f, .27f, .45f);
    caster.SetWorld(glm::translate(glm::mat4(1), centre) * glm::scale(glm::mat4(1), 2.f * half));

    FramebufferSpecification spec;
    spec.Width = 321;
    spec.Height = 239;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    SceneRenderView view;
    SceneSubmission frame;
    frame.Assets = assets;
    frame.LocalMetrics = view.LocalMetrics;
    frame.Presentation.SceneReferred = true;
    frame.Meshes = {ground}; // Caster is deliberately outside the visible mesh submission.
    frame.ShadowCasters = {ground, caster};
    frame.Camera.Orthographic = true;
    const auto camera = [&](float offset) {
        frame.Camera.Position = {offset, -2, 5};
        frame.Camera.ToCamera = glm::normalize(frame.Camera.Position - glm::vec3(offset, 0, 0));
        frame.Camera.View = glm::lookAt(frame.Camera.Position, glm::vec3(offset, 0, 0), glm::vec3(0, 1, 0));
        frame.Camera.Projection = glm::ortho(-2.65f, 2.65f, -2.f, 2.f, .1f, 20.f);
        frame.Camera.ViewProjection = frame.Camera.Projection * frame.Camera.View;
    };
    camera(0);
    SubmittedLocalLight red;
    red.Identity = 1;
    red.Position = {-1.25f, .75f, 2.7f};
    red.Direction = glm::normalize(-red.Position);
    red.Incident = {16, 0, 0};
    red.Range = 4;
    red.InnerCos = std::cos(.36f);
    red.OuterCos = std::cos(.65f);
    red.ShadowResolution = 512;
    auto green = red;
    green.Identity = 2;
    green.Position = {1.45f, -.65f, 2.5f};
    green.Direction = glm::normalize(-green.Position);
    green.Incident = {0, 16, 0};
    green.ShadowResolution = 1024;
    frame.LocalLights = {red, green};

    const auto render = [&](SurfaceView debug) {
        frame.Debug = debug;
        frame.LinearLightingInspection = debug != SurfaceView::Shaded;
        target->Bind();
        if (metal) target->Clear({0, 0, 0, 1});
        else { RenderCommand::SetClearColor({0, 0, 0, 1}); RenderCommand::Clear(); }
        target->ClearAttachment(1, -1);
        frame.LocalMetrics->Valid = false;
        SceneRenderer::Render(frame, view, target, [] {}, [] {});
        auto result = capture(target);
        SpotCheck(result && result->Completed && result->Error.empty(), "Spot GPU capture failed");
        return result;
    };
    const auto save = [&](const char* name, const Ref<const FramebufferReadback>& result) {
        std::ofstream outputFile(output / (std::string(name) + ".bin"), std::ios::binary);
        for (size_t p = 0; p < result->Color.size(); ++p) {
            outputFile.write(reinterpret_cast<const char*>(&result->Color[p]), 16);
            outputFile.write(reinterpret_cast<const char*>(&result->Entity[p]), 4);
            outputFile.write(reinterpret_cast<const char*>(&result->Depth[p]), 4);
        }
        SpotCheck(bool(outputFile), "Writing spot evidence failed");
    };

    if (!FileSystem::GetEnvironmentPath("HAZEL_SPOT_BENCHMARK").empty()) {
        SpotCheck(metal, "Spot GPU phase timestamps require Metal");
        target->Resize(1280, 720);
        // Keep tile generation active in all controls. Inert broad-phase candidates
        // isolate the cost of enabling 1, 2 or 4 otherwise identical shadow maps.
        std::ofstream timing(output / "benchmark.csv");
        timing << "spots,side,enabled,sample,cull_ms,spot_ms,forward_ms,atlas_bytes\n";
        frame.Meshes.push_back(caster);
        for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
            auto block = caster;
            block.SetWorld(glm::translate(glm::mat4(1), {.8f * float(x) - 1.2f, .8f * float(y) - 1.2f, .17f}) *
                glm::scale(glm::mat4(1), {.23f, .23f, .34f}));
            frame.Meshes.push_back(block);
            frame.ShadowCasters.push_back(block);
        }
        for (unsigned count : {1u, 2u, 4u}) for (unsigned side : {256u, 512u, 1024u}) for (bool enable : {false, true}) {
            frame.LocalLights.clear();
            for (unsigned i = 0; i < count; ++i) {
                auto light = i % 2 ? green : red;
                light.Identity = i + 1;
                light.CastShadows = enable;
                light.ShadowResolution = side;
                frame.LocalLights.push_back(light);
            }
            frame.LocalLights.resize(12);
            for (unsigned i = count; i < 12; ++i) frame.LocalLights[i].Position = {100, 100, 100};
            for (unsigned sample = 0; sample < 20; ++sample) {
                render(SurfaceView::Direct);
                const auto& metrics = *frame.LocalMetrics;
                SpotCheck(metrics.Valid && metrics.CullMilliseconds > 0 && metrics.ForwardMilliseconds > 0 &&
                    (!enable || metrics.SpotShadowMilliseconds > 0), "Missing spot GPU timing intervals");
                if (sample >= 5) timing << count << ',' << side << ',' << enable << ',' << sample - 5 << ','
                    << metrics.CullMilliseconds << ',' << metrics.SpotShadowMilliseconds << ',' << metrics.ForwardMilliseconds
                    << ',' << metrics.SpotShadowBytes << '\n';
            }
        }
        return;
    }

    const auto positions = render(SurfaceView::WorldPosition);
    const auto clear = render(SurfaceView::Direct);
    for (auto& light : frame.LocalLights) light.CastShadows = true;
    frame.ForceLocalLightCulling = true;
    const auto hard = render(SurfaceView::Direct);
    frame.LocalLightCulling = false;
    const auto fullLoop = render(SurfaceView::Direct);
    frame.LocalLightCulling = true;
    if (metal) SpotCheck(frame.LocalMetrics->Valid && frame.LocalMetrics->Tiles == 0, "Spot full loop did not run");
    SpotCheck(hard->Entity == clear->Entity && hard->Depth == clear->Depth && hard->Entity == fullLoop->Entity,
        "Spot shadow changed camera depth or picking");

    const auto table = ReadRuntimeBRDF();
    const auto average = ReferenceAverageRuntimeBRDF(table, .5);
    const auto viewAB = ReferenceSampleRuntimeBRDF(table, frame.Camera.ToCamera.z, .5);
    const auto intersects = [&](glm::dvec3 p, glm::dvec3 light, double margin) {
        double enter = 0, leave = 1;
        for (unsigned axis = 0; axis < 3; ++axis) {
            const double low = centre[axis] - half[axis] - margin;
            const double high = centre[axis] + half[axis] + margin;
            const double d = light[axis] - p[axis];
            if (std::abs(d) < 1e-12) {
                if (p[axis] < low || p[axis] > high) return false;
                continue;
            }
            const double a = (low - p[axis]) / d, b = (high - p[axis]) / d;
            enter = std::max(enter, std::min(a, b));
            leave = std::min(leave, std::max(a, b));
        }
        return leave > enter;
    };
    std::array<unsigned, 2> blocked{}, blockedMatch{}, lit{}, litMatch{};
    unsigned coneExterior = 0, rangeExterior = 0, contact = 0, overlap = 0;
    double oracleError = 0, loopError = 0;
    for (size_t p = 0; p < positions->Color.size(); ++p) {
        const glm::dvec3 world(positions->Color[p]);
        if (positions->Entity[p] != ground.Picking || std::abs(world.z) > 1e-5) continue;
        for (unsigned i = 0; i < 2; ++i) {
            const auto& light = frame.LocalLights[i];
            const auto delta = glm::dvec3(light.Position) - world;
            const double distance = glm::length(delta);
            const auto L = delta / distance;
            const double cosine = glm::dot(-L, glm::dvec3(light.Direction));
            const double attenuation = LocalReferenceAttenuation(distance, light.Range) *
                LocalReferenceCone(cosine, light.InnerCos, light.OuterCos);
            const auto brdf = ReferenceMultiscatteringBRDF(glm::dvec3(material->BaseColor), 0, .5,
                {0, 0, 1}, glm::dvec3(frame.Camera.ToCamera), L, viewAB,
                ReferenceSampleRuntimeBRDF(table, L.z, .5), average);
            const double expected = attenuation * brdf[i] * light.Incident[i];
            oracleError = std::max(oracleError, std::abs(expected - clear->Color[p][i]));
            loopError = std::max(loopError, double(std::abs(hard->Color[p][i] - fullLoop->Color[p][i])));
            if (distance > light.Range) ++rangeExterior;
            else if (cosine < light.OuterCos) ++coneExterior;
            if (expected < .007) continue;
            const double ratio = hard->Color[p][i] / clear->Color[p][i];
            if (intersects(world, light.Position, -.045)) {
                ++blocked[i];
                blockedMatch[i] += ratio < .05;
            } else if (!intersects(world, light.Position, .045)) {
                ++lit[i];
                litMatch[i] += ratio > .95;
            }
        }
        if (clear->Color[p].r > .05f && clear->Color[p].g > .05f &&
            hard->Color[p].r < .0025f && hard->Color[p].g < .0025f) ++overlap;
        if (std::abs(world.y - centre.y) < half.y - .05 &&
            world.x > centre.x + half.x && world.x < centre.x + half.x + .03 &&
            clear->Color[p].r > .05f && hard->Color[p].r < .0025f) ++contact;
    }
    std::cout << "SPOT referenceError=" << oracleError << " loopError=" << loopError << " redBlocked="
              << blockedMatch[0] << '/' << blocked[0] << " greenBlocked=" << blockedMatch[1] << '/' << blocked[1]
              << " redLit=" << litMatch[0] << '/' << lit[0] << " greenLit=" << litMatch[1] << '/' << lit[1]
              << " overlap=" << overlap << " contact=" << contact << " outsideCone=" << coneExterior
              << " outsideRange=" << rangeExterior << std::endl;
    SpotCheck(oracleError < .003 && loopError < 1e-5 && overlap > 50 && contact > 10 &&
        coneExterior > 100 && rangeExterior > 100, "Spot attenuation, contact, overlap or culling mismatch");
    for (unsigned i = 0; i < 2; ++i)
        SpotCheck(blocked[i] > 100 && lit[i] > 100 && blockedMatch[i] >= .98 * blocked[i] &&
            litMatch[i] >= .98 * lit[i], "Spot ray-box classifications failed");
    save("hard", hard);
    save("unshadowed", clear);

    auto cutout = caster;
    cutout.Materials = {mask.Accepted};
    frame.ShadowCasters = {ground, cutout};
    const auto masked = render(SurfaceView::Direct);
    unsigned revealed = 0, retained = 0;
    for (size_t p = 0; p < masked->Color.size(); ++p) {
        const float before = clear->Color[p].r;
        if (before < .04 || hard->Color[p].r > .05 * before) continue;
        revealed += masked->Color[p].r > .95 * before;
        retained += masked->Color[p].r < .05 * before;
    }
    std::cout << "SPOT mask revealed=" << revealed << " retained=" << retained << std::endl;
    SpotCheck(revealed > 30 && retained > 30, "Texture MASK did not leave both openings and blockers");
    save("masked", masked);

    // A sloped receiver compares against its own depth with the filter active.
    auto tilted = ground;
    tilted.SetWorld(glm::rotate(glm::mat4(1), .45f, glm::vec3(0, 1, 0)) * ground.World);
    frame.Meshes = frame.ShadowCasters = {tilted};
    for (auto& light : frame.LocalLights) light.CastShadows = false;
    const auto slopeClear = render(SurfaceView::Direct);
    for (auto& light : frame.LocalLights) light.CastShadows = true;
    const auto slope = render(SurfaceView::Direct);
    double slopeError = 0;
    for (size_t p = 0; p < slope->Color.size(); ++p)
        if (slope->Entity[p] == ground.Picking)
            slopeError = std::max(slopeError, double(glm::length(slope->Color[p] - slopeClear->Color[p])));
    std::cout << "SPOT slopeError=" << slopeError << std::endl;
    SpotCheck(slopeError < .003, "Tilted geometric receiver self-shadowed");
    save("slope", slope);

    frame.Meshes = {ground};
    frame.ShadowCasters = {ground, caster};
    frame.LocalLights.resize(5, red);
    for (unsigned i = 2; i < 5; ++i) {
        frame.LocalLights[i].Position.x += .12f * float(i);
        frame.LocalLights[i].Identity = i + 1;
        frame.LocalLights[i].Incident = {0, 0, i == 4 ? 8.f : 1.f};
        frame.LocalLights[i].CastShadows = true;
        frame.LocalLights[i].ShadowResolution = 256;
    }
    const auto overflow = render(SurfaceView::Direct);
    SpotCheck(frame.LocalMetrics->RequestedSpotShadows == 5 && frame.LocalMetrics->SpotShadows == 4, "Spot shadow capacity failed");
    frame.LocalLights.back().CastShadows = false;
    SpotCheck(overflow->Color == render(SurfaceView::Direct)->Color, "Overflow spotlight lost its unshadowed direct contribution");
    frame.LocalLights.erase(frame.LocalLights.begin());
    frame.LocalLights.back().CastShadows = true;
    render(SurfaceView::Direct);
    SpotCheck(frame.LocalMetrics->RequestedSpotShadows == 4, "Disabling a light did not free a map");

    // Render an inspectable scene with real visible geometry, jitter and moving lights.
    frame.LocalLights.assign({red, green});
    for (auto& light : frame.LocalLights) light.CastShadows = true;
    frame.Presentation.TemporalAA = metal;
    frame.Presentation.AmbientOcclusion.Enabled = metal;
    for (unsigned phase = 0; phase < 4; ++phase) {
        camera(.06f * float(phase));
        frame.LocalLights[0].Position.x += .16f;
        frame.LocalLights[0].Direction = glm::normalize(-frame.LocalLights[0].Position);
        caster.SetWorld(glm::translate(glm::mat4(1), centre + glm::vec3(.13f * float(phase), 0, 0)) *
            glm::scale(glm::mat4(1), 2.f * half));
        frame.Meshes = frame.ShadowCasters = {ground, caster};
        if (phase == 2) { target->Resize(353, 257); frame.LocalLights[0].ShadowResolution = 256; }
        if (phase == 3) for (auto& light : frame.LocalLights) light.CastShadows = false;
        const auto moved = render(SurfaceView::Shaded);
        SpotCheck(std::find(moved->Entity.begin(), moved->Entity.end(), ground.Picking) != moved->Entity.end() &&
            std::find(moved->Entity.begin(), moved->Entity.end(), caster.Picking) != moved->Entity.end(), "Spot motion lost meshes");
        for (const auto& c : moved->Color)
            SpotCheck(std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b), "Nonfinite spot/AO/TAA composition");
        if (metal) SpotCheck(view.AmbientDepth && view.Jittered && view.Temporal.Submitted &&
            view.Temporal.Submitted->load() && view.Temporal.Phase == (phase == 1 ? 2u : 1u),
            "Spot AO/TAA composition or history reuse/reset failed");
        if (phase == 3) SpotCheck(!view.SpotShadowDepth, "Disabled spot atlas retained by view");
        save(("motion-" + std::to_string(phase)).c_str(), moved);
    }
    // Use the shipped scene and camera for the inspection image.
    auto study = project->LoadScene("Scenes/SpotShadows_v1.hazel");
    SpotCheck(bool(study), "Spotlight study failed to load");
    study->OnViewportResize(960, 540);
    auto cameraEntity = study->GetPrimaryCameraEntity();
    SpotCheck(bool(cameraEntity), "Spotlight study primary camera missing");
    const auto world = study->GetWorldTransform(cameraEntity);
    SceneCameraView authored;
    authored.Position = glm::vec3(world[3]);
    authored.ToCamera = glm::normalize(glm::vec3(world[2]));
    authored.View = glm::inverse(world);
    authored.Projection = cameraEntity.GetComponent<CameraComponent>().Camera.GetProjection();
    authored.ViewProjection = authored.Projection * authored.View;
    target->Resize(960, 540);
    frame = SceneRenderer::Collect(*study, authored, {960, 540});
    frame.LocalMetrics = view.LocalMetrics;
    SpotCheck(frame.LocalLights.size() == 2 && frame.LocalLights[0].CastShadows && frame.LocalLights[1].CastShadows,
        "Spot shadow settings were lost in authored collection");
    const auto studyImage = render(SurfaceView::Shaded);
    save("study", studyImage);
    if (metal) {
        frame.Presentation.AmbientOcclusion.Enabled = true;
        const auto shadedAO = render(SurfaceView::Shaded);
        SpotCheck(view.AmbientDepth && shadedAO->Color != studyImage->Color && shadedAO->Entity == studyImage->Entity,
            "Spot and AO did not modulate the authored scene without changing IDs");
        frame.Presentation.AmbientOcclusion.Enabled = false;
    }
    // Both atlases may be live in one consumer. Existing CSM references live in
    // their own suite. Here require both resource paths without changing IDs/depth.
    frame.Presentation.Shadows.Enabled = true;
    frame.Presentation.Shadows.Distance = 20;
    frame.Presentation.Shadows.Resolution = 256;
    frame.Light = SubmittedDirectional{glm::normalize(glm::vec3(.5f, -.4f, 1)), glm::vec3(.25f)};
    const auto combined = render(SurfaceView::Shaded);
    SpotCheck(view.SpotShadowDepth && view.ShadowDepth, "Spot and directional atlas did not coexist");
    if (metal) SpotCheck(frame.LocalMetrics->Valid && frame.LocalMetrics->SpotShadowMilliseconds > 0 &&
        frame.LocalMetrics->ShadowMilliseconds > 0, "Combined GPU shadow timing failed");
    for (auto& light : frame.LocalLights) light.CastShadows = false;
    const auto directional = render(SurfaceView::Shaded);
    SpotCheck(combined->Entity == directional->Entity && combined->Depth == directional->Depth &&
        combined->Color != directional->Color, "Combined spot and directional shadow contribution failed");
    save("combined", combined);
    std::cout << "PASS spotlight shadows, mixed maps, motion, resize, disable and authored study\n";
}
