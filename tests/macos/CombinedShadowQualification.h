#pragma once
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Scene/Entity.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

inline void CombinedShadowQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    const auto require = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    const bool timing = !FileSystem::GetEnvironmentPath("HAZEL_COMBINED_SHADOW_TIMING").empty();
    require(!timing || metal, "Combined shadow phase timings require Metal");
    auto project = Project::LoadCandidate(FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT"));
    require(bool(project), "Missing combined shadow project");
    auto scene = project->LoadScene("Scenes/CombinedShadows_v1.hazel");
    require(bool(scene), "Missing combined shadow scene");
    auto camera = scene->GetPrimaryCameraEntity();
    require(bool(camera), "Missing combined shadow camera");
    auto& eye = camera.GetComponent<TransformComponent>().Translation;
    auto& lamp = scene->GetEntityByUUID(2).GetComponent<TransformComponent>().Translation;
    auto& cone = scene->GetEntityByUUID(16).GetComponent<TransformComponent>().Rotation;
    auto& block = scene->GetEntityByUUID(11).GetComponent<TransformComponent>().Translation;
    const auto initialEye = eye, initialLamp = lamp, initialCone = cone, initialBlock = block;

    std::filesystem::create_directories(output);
    FramebufferSpecification spec;
    spec.Width = 960;
    spec.Height = 540;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    enum class Shadows { None, All, Directional, Local };
    const auto render = [&](SceneRenderView& view, glm::uvec2 extent, unsigned side, Shadows shadows,
                            bool effects, unsigned step, bool tiled = true) {
        const float phase = float(step) * .06f;
        eye = initialEye + glm::vec3(phase, 0, 0);
        lamp = initialLamp + glm::vec3(0, phase, 0);
        cone = initialCone + glm::vec3(0, phase * .3f, 0);
        block = initialBlock + glm::vec3(phase, 0, 0);
        const auto world = scene->GetWorldTransform(camera);
        SceneCameraView cameraView;
        cameraView.Position = glm::vec3(world[3]);
        cameraView.ToCamera = glm::normalize(glm::vec3(world[2]));
        cameraView.View = glm::inverse(world);
        cameraView.Projection = camera.GetComponent<CameraComponent>().Camera.GetProjection();
        cameraView.ViewProjection = cameraView.Projection * cameraView.View;
        auto frame = SceneRenderer::Collect(*scene, cameraView, extent);
        require(frame.Light && frame.LocalLights.size() == 13, "Wrong combined shadow light inventory");
        frame.LocalLightCulling = tiled;
        frame.LocalMetrics = view.LocalMetrics;
        frame.LocalMetrics->Valid = false;
        frame.Presentation.AmbientOcclusion.Enabled = effects;
        frame.Presentation.TemporalAA = effects;
        const bool directional = shadows == Shadows::All || shadows == Shadows::Directional;
        const bool local = shadows == Shadows::All || shadows == Shadows::Local;
        frame.Presentation.Shadows.Enabled = directional;
        frame.Presentation.Shadows.Resolution = side;
        for (auto& light : frame.LocalLights) {
            light.CastShadows &= local;
            light.ShadowResolution = std::min(side, light.OuterCos == -1 ? 512u : 1024u);
        }
        target->Bind();
        if (metal) target->Clear({0, 0, 0, 1});
        else { RenderCommand::SetClearColor({0, 0, 0, 1}); RenderCommand::Clear(); }
        target->ClearAttachment(1, -1);
        SceneRenderer::Render(frame, view, target, [] {}, [] {});
        auto result = capture(target);
        require(result && result->Completed && result->Error.empty(), "Combined shadow GPU capture failed");
        const auto& metrics = *frame.LocalMetrics;
        require(metrics.SpotShadows == (local ? 2u : 0u) && metrics.PointShadows == (local ? 1u : 0u),
                "Wrong combined shadow map admission");
        require(bool(view.ShadowDepth) == directional && bool(view.SpotShadowDepth) == local &&
                bool(view.PointShadowDepth) == local, "Wrong combined shadow atlas lifetime");
        return result;
    };
    if (timing) {
        std::ofstream csv(output / "combined.csv");
        csv << "width,height,csm_side,enabled,sample,cull_ms,csm_ms,spot_ms,point_ms,forward_ms,atlas_bytes\n";
        for (glm::uvec2 extent : {glm::uvec2(1280, 720), glm::uvec2(1920, 1080)}) {
            target->Resize(extent.x, extent.y);
            scene->OnViewportResize(extent.x, extent.y);
            for (unsigned side : {256u, 512u, 1024u, 2048u}) {
                std::array<SceneRenderView, 2> views;
                for (unsigned sample = 0; sample < 20; ++sample) for (unsigned order = 0; order < 2; ++order) {
                    const bool enabled = (sample + order) % 2 != 0;
                    auto& view = views[enabled];
                    render(view, extent, side, enabled ? Shadows::All : Shadows::None, false, 0);
                    if (sample < 5) continue;
                    const auto& m = *view.LocalMetrics;
                    require(m.Valid && m.CullMilliseconds > 0 && m.ForwardMilliseconds > 0 &&
                            (!enabled || (m.ShadowMilliseconds > 0 && m.SpotShadowMilliseconds > 0 &&
                                        m.PointShadowMilliseconds > 0)), "Missing combined shadow GPU intervals");
                    const auto bytes = (enabled ? uint64_t(side) * side * 16 : 0) + m.SpotShadowBytes + m.PointShadowBytes;
                    csv << extent.x << ',' << extent.y << ',' << side << ',' << enabled << ','
                        << sample - 5 << ',' << m.CullMilliseconds << ',' << m.ShadowMilliseconds << ','
                        << m.SpotShadowMilliseconds << ',' << m.PointShadowMilliseconds << ','
                        << m.ForwardMilliseconds << ',' << bytes << '\n';
                }
            }
        }
        require(bool(csv), "Writing combined timings failed");
        std::cout << "PASS matched combined atlas and forward GPU intervals\n";
        return;
    }

    const glm::uvec2 extent(spec.Width, spec.Height);
    scene->OnViewportResize(extent.x, extent.y);
    const auto save = [&](const std::string& name, const Ref<const FramebufferReadback>& image) {
        std::ofstream file(output / (name + ".bin"), std::ios::binary);
        for (size_t pixel = 0; pixel < image->Color.size(); ++pixel) {
            const auto& color = image->Color[pixel];
            require(std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b),
                    "Combined shadow capture contains nonfinite pixels");
            file.write(reinterpret_cast<const char*>(&color), 16);
            file.write(reinterpret_cast<const char*>(&image->Entity[pixel]), 4);
            file.write(reinterpret_cast<const char*>(&image->Depth[pixel]), 4);
        }
        require(bool(file), "Writing combined shadow capture failed");
    };
    SceneRenderView view;
    auto enabled = render(view, extent, 1024, Shadows::All, false, 0);
    save("combined", enabled);
    auto fullLoop = render(view, extent, 1024, Shadows::All, false, 0, false);
    require(enabled->Entity == fullLoop->Entity && enabled->Depth == fullLoop->Depth, "Tiling changed combined geometry");
    for (size_t p = 0; p < enabled->Color.size(); ++p)
        require(glm::length(enabled->Color[p] - fullLoop->Color[p]) < 1e-6f, "Tiling changed combined shading");
    auto control = render(view, extent, 1024, Shadows::None, false, 0);
    require(control->Entity == enabled->Entity && control->Depth == enabled->Depth && control->Color != enabled->Color,
            "Combined shadows failed to change only shading");
    save("control", control);
    for (bool effects : {false, true}) {
        if (effects && !metal) continue;
        view.Reset();
        for (unsigned step = 0; step < 4; ++step) {
            auto moved = render(view, extent, 1024, Shadows::All, effects, step);
            if (effects) require(view.AmbientDepth && view.Jittered && view.Temporal.Submitted &&
                view.Temporal.Submitted->load() && view.Temporal.Phase == step + 1, "Combined AO/TAA did not accumulate");
            save(std::string(effects ? "ao-taa-" : "motion-") + std::to_string(step), moved);
            if (!effects && step == 3) {
                SceneRenderView fresh;
                auto unprimed = render(fresh, extent, 1024, Shadows::All, false, step);
                require(moved->Entity == unprimed->Entity && moved->Depth == unprimed->Depth &&
                        moved->Color == unprimed->Color, "Combined shadow atlases retained earlier caster depths");
            }
        }
    }
    save("motion-control-3", render(view, extent, 1024, Shadows::None, false, 3));
    save("motion-directional-3", render(view, extent, 1024, Shadows::Directional, false, 3));
    save("motion-local-3", render(view, extent, 1024, Shadows::Local, false, 3));
    if (metal) {
        const glm::uvec2 largeExtent(3200, 1800);
        target->Resize(largeExtent.x, largeExtent.y);
        scene->OnViewportResize(largeExtent.x, largeExtent.y);
        auto large = render(view, largeExtent, 2048, Shadows::All, true, 4);
        require(large->Color.size() == size_t(largeExtent.x) * largeExtent.y &&
            view.AmbientDepth && view.Jittered && view.Temporal.Phase == 1,
            "Combined large target failed to reset AO/TAA history");
        std::cout << "PASS 3200x1800 offscreen with Ultra CSM and AO/TAA\n";
    }
    std::cout << "PASS combined admission, disable, tiling and moving camera, lights and casters\n";
}
