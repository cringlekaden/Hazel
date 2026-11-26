#pragma once
#include "LocalLightingQualification.h"
#include "CombinedShadowQualification.h"
#include "Hazel/Renderer/Scene/DirectionalShadows.h"
#include <glm/gtx/quaternion.hpp>

inline void PointCheck(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

inline void PointProjectionChecks()
{
    using namespace Hazel;
    const std::array<glm::dvec3, 6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    const std::array<glm::dvec3, 6> horizontal{{{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{-1,0,0},{1,0,0}}};
    SceneSubmission frame;
    SubmittedLocalLight light;
    light.Incident = glm::vec3(1);
    light.CastShadows = true;
    double maxDepth = 0;
    for (float range : {.01f, 6.f, 10000.f}) for (unsigned side : {256u, 512u}) {
        light.Range = range;
        light.ShadowResolution = side;
        frame.LocalLights = {light};
        const auto fit = FitPointShadows(frame);
        const double near = double(range) / 1024;
        auto shifted = frame;
        shifted.LocalLights[0].Position += glm::vec3(900, -24, 2000);
        shifted.Camera.Position = {-400, 800, 800};
        PointCheck(FitPointShadows(shifted).VP == fit.VP, "Point face basis depends on position");
        for (unsigned face = 0; face < directions.size(); ++face) {
            const auto forward = directions[face];
            const auto right = horizontal[face];
            const auto up = glm::cross(right, forward);
            const auto vp = glm::dmat4(fit.VP[face]);
            for (double axial : {near, 4 * near, .5 * range, double(range)}) {
                for (double x : {-1., -.2, 0., .7, 1.}) for (double y : {-1., 0., 1.}) {
                    const auto clip = vp * glm::dvec4(axial * (forward + x * right + y * up), 1);
                    const double expected = (1 - near / axial) / (1 - near / range);
                    maxDepth = std::max(maxDepth, std::abs((clip.z / clip.w + 1) * .5 - expected));
                    PointCheck(std::abs(clip.x / clip.w - x * (1 - 8. / side)) < .0002 &&
                        std::abs(clip.y / clip.w - y * (1 - 8. / side)) < .0002, "Point tile orientation or guard mismatch");
                }
            }
            for (double axial : {near / 2, double(range) * 1.01}) {
                const auto clip = vp * glm::dvec4(axial * forward, 1);
                PointCheck(std::abs(clip.z) > clip.w, "Point face accepted a clipped axial depth");
            }
            // A secondary face must enclose its whole PCF kernel even at
            // the largest axis ratio for which it receives nonzero weight.
            const double overlapRatio = 1 / (1 - 4. / side);
            const auto clip = vp * glm::dvec4(forward + overlapRatio * (right + up), 1);
            PointCheck((1 - clip.x / clip.w) * side * .5 > 1.5 &&
                (1 - clip.y / clip.w) * side * .5 > 1.5, "Point face overlap has no filter guard");
        }
    }
    PointCheck(maxDepth < .0002, "Point perspective depth differs from analytic projection");
    auto spot = light;
    spot.OuterCos = .6f;
    frame.LocalLights = {spot, light, light};
    const auto fit = FitPointShadows(frame);
    PointCheck(fit.Requested == 2 && fit.LightIndex == 1 && FitSpotShadows(frame).Count == 1, "Point/spot admission conflicted");
    std::cout << "POINT maxDepthError=" << maxDepth << '\n';
}

// Clip an analytic light-to-receiver segment against a box in fixture space.
inline bool PointRayBox(glm::dvec3 origin, glm::dvec3 receiver, glm::dvec3 center, glm::dvec3 halfExtent)
{
    double entry = 0, exit = 1;
    const auto direction = receiver - origin;
    for (int axis = 0; axis < 3; ++axis) {
        const double lower = center[axis] - halfExtent[axis], upper = center[axis] + halfExtent[axis];
        if (std::abs(direction[axis]) < 1e-12) {
            if (origin[axis] < lower || origin[axis] > upper) return false;
            continue;
        }
        const double a = (lower - origin[axis]) / direction[axis];
        const double b = (upper - origin[axis]) / direction[axis];
        entry = std::max(entry, std::min(a, b));
        exit = std::min(exit, std::max(a, b));
        if (exit < entry) return false;
    }
    return entry < 1 && exit > 0;
}

inline void PointShadowQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    if (!FileSystem::GetEnvironmentPath("HAZEL_COMBINED_SHADOW_QUALIFICATION").empty()) {
        CombinedShadowQualification(output, capture);
        return;
    }
    PointProjectionChecks();
    std::filesystem::create_directories(output);
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    auto project = Project::LoadCandidate(FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT"));
    auto assets = CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    auto mask = assets->Material("Reference/Direct_v1/Surfaces/Mask.hmat");
    PointCheck(mesh.Accepted && mask.Accepted, "Point fixtures missing cube or MASK");
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {.6f, .6f, .6f, 1};
    material->Roughness = .5f;
    SubmittedMesh receiver, caster;
    receiver.Geometry = caster.Geometry = mesh.Accepted;
    receiver.Materials = caster.Materials = {material};
    receiver.Identity = receiver.Picking = 11;
    caster.Identity = caster.Picking = 12;
    constexpr glm::vec3 lightPosition(0, 0, 3);
    constexpr glm::vec3 center(.03f, -.23f, 1.28f), halfExtent(.34f, .27f, .28f);
    constexpr glm::vec3 pivot(2, -.75f, .125f);
    glm::mat4 pose(1);
    SceneRenderView view;
    SceneRenderView* renderView = &view;
    SceneSubmission frame;
    frame.Assets = assets;
    frame.LocalMetrics = view.LocalMetrics;
    frame.Presentation.SceneReferred = true;
    frame.Camera.Orthographic = true;
    frame.ForceLocalLightCulling = true;
    SubmittedLocalLight light;
    light.Identity = 1;
    light.Incident = {16, 0, 0};
    light.Range = 3.65f;
    light.ShadowResolution = 256;
    const auto place = [&](glm::vec3 direction, float offset = 0) {
        pose = glm::translate(glm::mat4(1), pivot) *
            glm::mat4_cast(glm::rotation(glm::vec3(0, 0, -1), glm::normalize(direction))) *
            glm::translate(glm::mat4(1), -lightPosition);
        const auto transform = [&](glm::vec3 p) { return glm::vec3(pose * glm::vec4(p, 1)); };
        frame.Camera.Position = transform({offset, 0, 5});
        frame.Camera.ToCamera = glm::normalize(frame.Camera.Position - transform({offset, 0, 0}));
        frame.Camera.View = glm::lookAt(frame.Camera.Position, transform({offset, 0, 0}), glm::vec3(pose[1]));
        frame.Camera.Projection = glm::ortho(-2.2f, 2.2f, -1.7f, 1.7f, .1f, 12.f);
        frame.Camera.ViewProjection = frame.Camera.Projection * frame.Camera.View;
        light.Position = pivot;
        receiver.SetWorld(pose * glm::translate(glm::mat4(1), {0, 0, -.05f}) * glm::scale(glm::mat4(1), {4.6f, 3.6f, .1f}));
        caster.SetWorld(pose * glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), 2.f * halfExtent));
        frame.Meshes = {receiver};
        frame.ShadowCasters = {receiver, caster};
    };

    FramebufferSpecification spec;
    spec.Width = 321;
    spec.Height = 239;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    const auto render = [&](SurfaceView debug = SurfaceView::Direct) {
        frame.Debug = debug;
        frame.LinearLightingInspection = debug != SurfaceView::Shaded;
        target->Bind();
        if (metal) target->Clear({0, 0, 0, 1});
        else { RenderCommand::SetClearColor({0, 0, 0, 1}); RenderCommand::Clear(); }
        target->ClearAttachment(1, -1);
        frame.LocalMetrics->Valid = false;
        SceneRenderer::Render(frame, *renderView, target, [] {}, [] {});
        auto result = capture(target);
        PointCheck(result && result->Completed && result->Error.empty(), "Point GPU capture failed");
        return result;
    };
    const auto save = [&](std::string name, const Ref<const FramebufferReadback>& result) {
        std::ofstream file(output / (name + ".bin"), std::ios::binary);
        for (size_t p = 0; p < result->Color.size(); ++p) {
            file.write(reinterpret_cast<const char*>(&result->Color[p]), 16);
            file.write(reinterpret_cast<const char*>(&result->Entity[p]), 4);
            file.write(reinterpret_cast<const char*>(&result->Depth[p]), 4);
        }
        PointCheck(bool(file), "Writing point shadow evidence failed");
    };

    place({-1, 0, -1});
    if (!FileSystem::GetEnvironmentPath("HAZEL_POINT_BENCHMARK").empty()) {
        PointCheck(metal, "Point shadow timestamps require Metal");
        target->Resize(1280, 720);
        frame.Meshes.push_back(caster);
        light.Range = 6;
        for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
            auto block = caster;
            block.SetWorld(pose * glm::translate(glm::mat4(1), {.8f * x - 1.2f, .8f * y - 1.2f, .17f}) *
                glm::scale(glm::mat4(1), {.23f, .23f, .34f}));
            frame.Meshes.push_back(block);
            frame.ShadowCasters.push_back(block);
        }
        frame.LocalLights.resize(12);
        for (size_t i = 1; i < frame.LocalLights.size(); ++i) {
            auto& other = frame.LocalLights[i];
            other.Identity = i + 1;
            other.Position = glm::vec3(pose * glm::vec4(.12f * float(i), 0, 2.5f, 1));
            other.Range = 4.5f;
            other.Incident = glm::vec3(.3f);
        }
        std::ofstream timing(output / "benchmark.csv");
        timing << "side,enabled,sample,cull_ms,point_ms,forward_ms,atlas_bytes\n";
        SceneRenderView control;
        for (unsigned side : {256u, 512u}) {
            frame.LocalLights[0] = light;
            frame.LocalLights[0].ShadowResolution = side;
            for (unsigned sample = 0; sample < 20; ++sample) for (bool enable : {false, true}) {
                frame.LocalLights[0].CastShadows = enable;
                renderView = enable ? &view : &control;
                frame.LocalMetrics = renderView->LocalMetrics;
                render();
                const auto& metrics = *frame.LocalMetrics;
                PointCheck(metrics.Valid && metrics.CullMilliseconds > 0 && metrics.ForwardMilliseconds > 0 &&
                    (!enable || metrics.PointShadowMilliseconds > 0), "Missing point GPU timing intervals");
                if (sample >= 5) timing << side << ',' << enable << ',' << sample - 5 << ','
                    << metrics.CullMilliseconds << ',' << metrics.PointShadowMilliseconds << ','
                    << metrics.ForwardMilliseconds << ',' << metrics.PointShadowBytes << '\n';
            }
        }
        PointCheck(bool(timing), "Writing point benchmark failed");
        return;
    }

    // Six face interiors, every signed axis pair, and all eight three-face corners.
    unsigned samples = 0;
    for (int z : {-1, 0, 1}) for (int y : {-1, 0, 1}) for (int x : {-1, 0, 1}) {
        if (x == 0 && y == 0 && z == 0) continue;
        const std::string name = std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
        place({x, y, z});
        frame.LocalLights = {light};
        const auto clear = render();
        frame.LocalLights[0].CastShadows = true;
        const auto shadow = render();
        PointCheck(shadow->Entity == clear->Entity && shadow->Depth == clear->Depth, "Point shadows altered receiver coverage");
        unsigned lit = 0, litMatch = 0, blocked = 0, blockedMatch = 0, exterior = 0, seamLit = 0, seamBlocked = 0;
        const glm::dmat3 worldBasis(pose);
        for (unsigned row = 0; row < spec.Height; ++row) for (unsigned column = 0; column < spec.Width; ++column) {
            const size_t p = size_t(row) * spec.Width + column;
            const glm::dvec3 point((2 * (column + .5) / spec.Width - 1) * 2.2,
                (2 * (row + .5) / spec.Height - 1) * 1.7, 0);
            if (glm::length(point - glm::dvec3(lightPosition)) >= light.Range + 1e-4) {
                PointCheck(std::abs(shadow->Color[p].r) < 1e-6, "Point shadows extended incident light past range");
                ++exterior;
            }
            if (clear->Color[p].r < .012f) continue;
            const float visibility = shadow->Color[p].r / clear->Color[p].r;
            const auto axes = glm::abs(worldBasis * (point - glm::dvec3(lightPosition)));
            const double dominant = std::max({axes.x, axes.y, axes.z});
            const bool seam = (axes.x > .99 * dominant) + (axes.y > .99 * dominant) + (axes.z > .99 * dominant) >= 2;
            if (PointRayBox(lightPosition, point, center, glm::dvec3(halfExtent) - .025)) {
                ++blocked;
                blockedMatch += visibility <= .05f;
                if (seam) { ++seamBlocked; PointCheck(visibility <= .05f, "Light leaked at a guarded point seam"); }
            } else if (!PointRayBox(lightPosition, point, center, glm::dvec3(halfExtent) + .025)) {
                ++lit;
                litMatch += visibility >= .95f;
                if (seam) { ++seamLit; PointCheck(visibility >= .95f, "Dark line at a guarded point seam"); }
            }
        }
        std::cout << "POINT " << name << " lit=" << litMatch << '/' << lit
                  << " blocked=" << blockedMatch << '/' << blocked << " exterior=" << exterior
                  << " seamLit=" << seamLit << " seamBlocked=" << seamBlocked << std::endl;
        PointCheck(lit > 1000 && blocked > 1000 && exterior > 100 &&
            litMatch >= .98 * lit && blockedMatch >= .98 * blocked, "Point ray-box or seam classification failed");
        save("face-" + name, shadow);
        samples += lit + blocked;
    }
    std::cout << "POINT orientations=26 referenceSamples=" << samples << '\n';

    if (!FileSystem::GetEnvironmentPath("HAZEL_SHADOW_EDGE_PROBES").empty()) {
        place({0, 0, -1});
        frame.LocalLights = {light};
        save("profile-clear", render());
        frame.LocalLights[0].CastShadows = true;
        for (unsigned step = 0; step <= 8; ++step) {
            caster.SetWorld(pose * glm::translate(glm::mat4(1), center + glm::vec3(.004f * step, 0, 0)) *
                glm::scale(glm::mat4(1), 2.f * halfExtent));
            frame.ShadowCasters = {receiver, caster};
            save("profile-" + std::to_string(step), render());
        }
    }

    place({-1, 0, -1});
    frame.LocalLights = {light};
    const auto clear = render();
    frame.LocalLights[0].CastShadows = true;
    const auto hard = render();
    auto cutout = caster;
    cutout.Materials = {mask.Accepted};
    frame.ShadowCasters = {receiver, cutout};
    const auto masked = render();
    unsigned revealed = 0, retained = 0;
    for (size_t p = 0; p < clear->Color.size(); ++p) {
        const float unblocked = clear->Color[p].r;
        if (unblocked < .025 || hard->Color[p].r > .05 * unblocked) continue;
        revealed += masked->Color[p].r > .95 * unblocked;
        retained += masked->Color[p].r < .05 * unblocked;
    }
    std::cout << "POINT mask revealed=" << revealed << " retained=" << retained << std::endl;
    PointCheck(revealed > 30 && retained > 30, "Point cutout did not maintain openings and blockers");
    save("masked", masked);

    auto clipped = caster;
    clipped.SetWorld(glm::translate(glm::mat4(1), pivot) * glm::scale(glm::mat4(1), glm::vec3(light.Range / 2048)));
    frame.ShadowCasters = {receiver, clipped};
    const auto nearClipped = render();
    auto maximumDifference = [](const auto& a, const auto& b) {
        double maximum = 0;
        for (size_t p = 0; p < a->Color.size(); ++p)
            maximum = std::max(maximum, double(glm::length(a->Color[p] - b->Color[p])));
        return maximum;
    };
    PointCheck(maximumDifference(clear, nearClipped) < .003, "Near-clipped caster or receiver acne changed visibility");
    clipped.SetWorld(pose * glm::translate(glm::mat4(1), {0, 0, -6}) * glm::scale(glm::mat4(1), glm::vec3(1)));
    frame.ShadowCasters = {receiver, clipped};
    PointCheck(maximumDifference(clear, render()) < .003, "Far-clipped caster changed visibility");

    // Contact on a hard edge. Scan a strip immediately outside the footprint
    // and inside its analytic shadow with a displaced light.
    const auto contactOrigin = glm::vec3(-1.4f, 0, 2.7f);
    frame.LocalLights[0].Position = glm::vec3(pose * glm::vec4(contactOrigin, 1));
    frame.LocalLights[0].Range = 6;
    auto contact = caster;
    contact.SetWorld(pose * glm::translate(glm::mat4(1), {0, 0, .5f}) * glm::scale(glm::mat4(1), {.7f, .7f, 1.f}));
    frame.ShadowCasters = {receiver, contact};
    const auto attached = render();
    frame.LocalLights[0].CastShadows = false;
    const auto contactClear = render();
    unsigned contactSamples = 0;
    for (unsigned row = 0; row < spec.Height; ++row) for (unsigned column = 0; column < spec.Width; ++column) {
        const double x = (2 * (column + .5) / spec.Width - 1) * 2.2;
        const double y = (2 * (row + .5) / spec.Height - 1) * 1.7;
        if (x < .364 || x > .402 || std::abs(y) > .15) continue;
        const size_t p = size_t(row) * spec.Width + column;
        PointCheck(contactClear->Color[p].r > .01 && attached->Color[p].r < .05 * contactClear->Color[p].r,
            "Point shadow detached at contact");
        ++contactSamples;
    }
    PointCheck(contactSamples > 30, "Point contact strip was empty");
    save("contact", attached);

    // At grazing incidence the bounded plane correction is exercised without
    // another caster. Reject persistent acne above the fixed radiance tolerance.
    frame.ShadowCasters = {receiver};
    frame.LocalLights[0].Position = glm::vec3(pose * glm::vec4(-2.8f, .1f, .15f, 1));
    const auto grazingClear = render();
    frame.LocalLights[0].CastShadows = true;
    const auto grazing = render();
    const double grazingError = maximumDifference(grazingClear, grazing);
    std::cout << "POINT contactSamples=" << contactSamples << " grazingError=" << grazingError << std::endl;
    PointCheck(grazingError < .003, "Point grazing receiver self-shadowed");
    save("grazing", grazing);
    frame.LocalLights = {light};
    frame.LocalLights[0].CastShadows = true;

    auto spot = light;
    spot.Identity = 2;
    spot.Position += glm::vec3(.6f, .1f, .1f);
    spot.Incident = {0, 12, 0};
    spot.Direction = glm::normalize(glm::vec3(pose * glm::vec4(0, 0, 0, 1)) - spot.Position);
    spot.InnerCos = .9f;
    spot.OuterCos = .6f;
    spot.CastShadows = true;
    auto excess = light;
    excess.Identity = 3;
    excess.Incident = {0, 0, 8};
    excess.CastShadows = true;
    frame.LocalLights.resize(3);
    frame.LocalLights[1] = spot;
    frame.LocalLights[2] = excess;
    frame.ShadowCasters = {receiver, caster};
    const auto mixed = render();
    PointCheck(frame.LocalMetrics->PointShadows == 1 && frame.LocalMetrics->RequestedPointShadows == 2 &&
        frame.LocalMetrics->SpotShadows == 1, "Point capacity or mixed atlas routing failed");
    frame.LocalLights[2].CastShadows = false;
    PointCheck(mixed->Color == render()->Color, "Denied point request changed ordinary local lighting");
    frame.LocalLightCulling = false;
    PointCheck(maximumDifference(mixed, render()) < 1e-6, "Point shadow Forward+ and full light loop differ");
    save("mixed", mixed);
    frame.LocalLights[2].CastShadows = true;
    frame.LocalLights.erase(frame.LocalLights.begin());
    PointCheck(mixed->Color != render()->Color && frame.LocalMetrics->RequestedPointShadows == 1,
        "Disabling first point light did not transfer shadow admission");
    frame.LocalLightCulling = true;

    frame.Presentation.TemporalAA = metal;
    frame.Presentation.AmbientOcclusion.Enabled = metal;
    for (unsigned step = 0; step < 4; ++step) {
        place({-1.f, 0, -1.f + .04f * step}, .05f * step);
        light.CastShadows = step != 3;
        light.Position.x += .08f * step;
        light.ShadowResolution = step < 2 ? 256 : 512;
        frame.LocalLights = {light, spot};
        if (step == 2) target->Resize(353, 257);
        frame.Meshes = frame.ShadowCasters = {receiver, caster};
        const auto image = render(SurfaceView::Shaded);
        PointCheck(std::find(image->Entity.begin(), image->Entity.end(), caster.Picking) != image->Entity.end(),
            "Point motion capture lost caster");
        for (const auto& c : image->Color)
            PointCheck(std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b), "Point motion returned nonfinite color");
        if (metal) PointCheck(view.AmbientDepth && view.Jittered && view.Temporal.Submitted &&
            view.Temporal.Submitted->load() && view.Temporal.Phase == (step == 1 ? 2u : 1u),
            "Point motion AO/TAA did not compose or reset");
        if (step == 3) PointCheck(!view.PointShadowDepth, "Disabled point atlas retained by view");
        save("motion-" + std::to_string(step), image);
    }
    auto study = project->LoadScene("Scenes/PointShadows_v1.hazel");
    PointCheck(bool(study), "Point shadow study failed to load");
    study->OnViewportResize(960, 540);
    auto camera = study->GetPrimaryCameraEntity();
    PointCheck(bool(camera), "Point shadow study camera missing");
    const auto world = study->GetWorldTransform(camera);
    SceneCameraView authored;
    authored.Position = glm::vec3(world[3]);
    authored.ToCamera = glm::normalize(glm::vec3(world[2]));
    authored.View = glm::inverse(world);
    authored.Projection = camera.GetComponent<CameraComponent>().Camera.GetProjection();
    authored.ViewProjection = authored.Projection * authored.View;
    target->Resize(960, 540);
    frame = SceneRenderer::Collect(*study, authored, {960, 540});
    frame.LocalMetrics = view.LocalMetrics;
    PointCheck(frame.LocalLights.size() == 2 && frame.LocalLights[0].OuterCos == -1 &&
        frame.LocalLights[0].CastShadows && frame.LocalLights[1].CastShadows, "Authored point and spot did not collect");
    const auto studyImage = render(SurfaceView::Shaded);
    save("study", studyImage);
    frame.LocalLights[0].CastShadows = false;
    const auto studyClear = render(SurfaceView::Shaded);
    PointCheck(studyClear->Entity == studyImage->Entity && studyClear->Depth == studyImage->Depth &&
        maximumDifference(studyClear, studyImage) > .05, "Authored point shadow did not change lighting");
    save("study-clear", studyClear);
    std::cout << "PASS point projection, face interiors and seams, MASK, clipping, light exhaustion, culling and motion\n";
}
