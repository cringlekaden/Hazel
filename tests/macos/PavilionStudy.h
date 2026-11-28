#pragma once

#include "Hazel/Core/FileDocument.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Scene/ReflectionProbeCapture.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <glad/glad.h>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>
#include <glm/gtc/quaternion.hpp>

// Run the authored cameras through the same scene update as Nutella.
inline void PavilionStudy(const std::filesystem::path& output, const Hazel::Ref<Hazel::Project>& project,
    const Hazel::Ref<Hazel::ProjectAssets>& assets,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    const auto scene = CreateRef<Scene>();
    SceneSerializer serializer(scene, assets->Root(), false, assets);
    const auto path = Project::ResolveOwnedAsset(assets->Root(), project->GetConfig().StartScene);
    if (!serializer.DeserializeText(FileDocument::Read(path), false))
        throw std::runtime_error(serializer.Report().Error);
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    const auto selection = FileSystem::GetEnvironmentPath("HAZEL_PAVILION_STUDY").string();
    if (selection == "bake") {
        const auto authored = scene->Presentation();
        for (const auto* name : {"Hall", "Archive"}) {
            auto probe = scene->FindEntityByName(std::string(name == std::string_view("Hall") ?
                "01 Gallery probe" : "02 Threshold probe"));
            if (!probe) throw std::runtime_error("Pavilion capture point is missing");
            const auto position = glm::vec3(scene->GetWorldTransform(probe)[3]);
            const auto side = probe.GetComponent<ReflectionProbeComponent>().BakeResolution;
            auto record = [&]() {
                ReflectionProbeCapture job(scene, position, side);
                while (!job.Advance()) {
                    if (metal) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return job.Image();
            };
            // Keep geometry, draw ordering and capture coordinates identical while
            // changing settings that must never feed back into linear capture.
            const auto image = record();
            for (auto h : scene->GetAllEntitiesWith<ReflectionProbeComponent>())
                Entity(h, scene.get()).GetComponent<ReflectionProbeComponent>().Enabled = true;
            scene->Presentation().Exposure.CompensationStops = -8;
            scene->Presentation().Bloom.Enabled = !authored.Bloom.Enabled;
            scene->Presentation().AmbientOcclusion.Enabled = !authored.AmbientOcclusion.Enabled;
            scene->Presentation().TemporalAA = !authored.TemporalAA;
            scene->Presentation().ScreenReflections.Enabled = !authored.ScreenReflections.Enabled;
            const auto control = record();
            scene->Presentation() = authored;
            for (auto h : scene->GetAllEntitiesWith<ReflectionProbeComponent>())
                Entity(h, scene.get()).GetComponent<ReflectionProbeComponent>().Enabled = false;
            if (image.Mips.front() != control.Mips.front())
                throw std::runtime_error("Pavilion capture changed with view-only effects");
            FileSystem::WriteNewFile(output / (std::string(name) + ".hlight"), EncodeLightingImage(image));
            std::cout << "PAVILION_BAKE " << name << " face=" << side
                << " peakHDR=" << *std::max_element(image.Mips.front().begin(), image.Mips.front().end())
                << " viewEffectsInvariant=true\n";
        }
        return;
    }
    const auto roundTrip = CreateRef<Scene>();
    SceneSerializer second(roundTrip, assets->Root(), false, assets);
    const auto saved = serializer.SerializeText();
    if (!second.DeserializeText(saved, false) || second.SerializeText() != saved)
        throw std::runtime_error("Pavilion native scene round trip changed authored data");

    FramebufferSpecification spec;
    spec.Width = 960;
    spec.Height = 540;
    if (const auto width = FileSystem::GetEnvironmentPath("HAZEL_PAVILION_WIDTH"); !width.empty()) {
        spec.Width = unsigned(std::stoul(width.string()));
        spec.Height = spec.Width * 9 / 16;
    }
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    const auto target = Framebuffer::Create(spec);
    scene->OnRuntimeStart();
    struct StopScene {
        Scene& Value;
        ~StopScene() { Value.OnRuntimeStop(); }
    } stop{*scene};
    scene->OnViewportResize(spec.Width, spec.Height);

    const auto originalPresentation = scene->Presentation();
    auto draw = [&](SceneRenderView& view, GLuint query = 0) {
        target->Bind();
        if (metal) target->Clear(scene->Presentation().Background);
        else {
            RenderCommand::SetClearColor(scene->Presentation().Background);
            RenderCommand::Clear();
        }
        target->ClearAttachment(1, -1);
        if (query) glBeginQuery(GL_TIME_ELAPSED, query);
        scene->OnUpdateRuntime(1.0f / 30, &view, target);
        if (query) glEndQuery(GL_TIME_ELAPSED);
        if (!scene->RenderError().empty())
            throw std::runtime_error(scene->RenderError());
        const auto result = capture(target);
        if (!result || !result->Completed || !result->Error.empty())
            throw std::runtime_error("Incomplete Pavilion readback");
        return result;
    };
    auto save = [&](const std::string& name, const Ref<const FramebufferReadback>& image) {
        std::ofstream file(output / (name + ".ppm"), std::ios::binary);
        file << "P6\n" << spec.Width << ' ' << spec.Height << "\n255\n";
        for (unsigned y = spec.Height; y-- > 0;)
            for (unsigned x = 0; x < spec.Width; ++x) {
                const auto color = image->Color[size_t(y) * spec.Width + x];
                for (unsigned c = 0; c < 3; ++c) {
                    if (!std::isfinite(color[c])) throw std::runtime_error("Nonfinite Pavilion output");
                    const float linear = glm::clamp(color[c], 0.f, 1.f);
                    const float srgb = linear <= .0031308f ? linear * 12.92f :
                        1.055f * std::pow(linear, 1.f / 2.4f) - .055f;
                    file.put(static_cast<char>(std::lround(srgb * 255)));
                }
            }
        if (!file) throw std::runtime_error("Failed writing Pavilion capture");
    };

    std::vector<uint64_t> cameras;
    for (auto handle : scene->GetAllEntitiesWith<CameraComponent>()) {
        Entity camera(handle, scene.get());
        cameras.push_back(camera.GetUUID());
        camera.GetComponent<CameraComponent>().Primary = false;
    }
    std::sort(cameras.begin(), cameras.end());
    struct Shot {
        unsigned Frames;
        glm::vec3 Travel;
        float Yaw;
    };
    const std::array<Shot, 9> tour{{
        {78, {-1.5f, .06f, -.65f}, -.07f}, // courtyard
        {126, {-.02f, 0, -8.4f}, 0},       // same view identity across the threshold
        {78, {-.98f, -.04f, -1.2f}, -.07f},
        {84, {-3.0f, .06f, -.45f}, -.28f}, // different specular rays over the same floor
        {72, {-.3f, 0, -.28f}, -.05f},
        {80, {.12f, .02f, -2.9f}, -.02f},
        {120, {.045f, -.035f, -.09f}, .025f},
        {64, {-1.88f, .03f, -.19f}, -.48f},
        {84, {.58f, .10f, .33f}, .09f}
    }};
    auto motion = scene->FindEntityByName("Kinetic exhibit");
    auto spotlight = scene->FindEntityByName("Kinetic spotlight");
    auto place = [&](Entity entity, const TransformComponent& original, const Shot& shot, float progress) {
        const float eased = progress * progress * (3 - 2 * progress);
        auto& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = original.Translation + shot.Travel * eased;
        transform.Rotation = original.Rotation + glm::vec3(0, shot.Yaw * eased, 0);
    };
    if (selection == "benchmark") {
        struct Query {
            GLuint ID = 0;
            ~Query() { if (ID) glDeleteQueries(1, &ID); }
        } query;
        if (!metal) glGenQueries(1, &query.ID);
        std::ofstream csv(output / "passes.csv");
        csv << "shot,sample,serial,draws,gl_elapsed_ms,local_cull_ms,forward_ms,csm_ms,spot_ms,point_ms,"
               "ao_ms,taa_resolve_ms,bloom_ms,exposure_ms,dof_ms,shutter_ms,lens_ms\n";
        for (const auto id : {4u, 7u, 8u}) {
            auto entity = scene->GetEntityByUUID(id);
            auto& camera = entity.GetComponent<CameraComponent>();
            camera.Primary = true;
            SceneRenderView view;
            if (metal) {
                view.BloomMetrics = std::make_shared<BloomTiming>();
                view.ExposureTiming = std::make_shared<ExposureMeterTiming>();
                view.DefocusTiming = std::make_shared<DepthOfFieldTiming>();
                view.ShutterTiming = std::make_shared<CameraMotionTiming>();
                view.LensTiming = std::make_shared<LensEffectsTiming>();
            }
            const auto original = entity.GetComponent<TransformComponent>();
            for (unsigned n = 0; n < 24; ++n) {
                place(entity, original, tour[id - 1], float(n) / 95);
                const auto before = RenderCommand::GetDrawSubmissionCount();
                const auto image = draw(view, query.ID);
                const auto draws = RenderCommand::GetDrawSubmissionCount() - before;
                GLuint64 ns = 0;
                if (query.ID) glGetQueryObjectui64v(query.ID, GL_QUERY_RESULT, &ns);
                if (n < 6) continue;
                const auto& local = *view.LocalMetrics;
                auto total = [](const auto& array) { return std::accumulate(array.begin(), array.end(), 0.0); };
                const auto& ao = view.AmbientTiming->Milliseconds;
                csv << id << ',' << n - 6 << ',' << image->Serial << ',' << draws << ',' << double(ns) / 1e6
                    << ',' << local.CullMilliseconds << ',' << local.ForwardMilliseconds << ',' << local.ShadowMilliseconds
                    << ',' << local.SpotShadowMilliseconds << ',' << local.PointShadowMilliseconds
                    << ',' << (view.AmbientTiming->Valid ? ao[0] + ao[1] + ao[2] + ao[3] : 0)
                    << ',' << view.TemporalMetrics->ResolveMilliseconds
                    << ',' << (view.BloomMetrics ? total(view.BloomMetrics->Milliseconds) : 0)
                    << ',' << (view.ExposureTiming ? view.ExposureTiming->Milliseconds : 0)
                    << ',' << (view.DefocusTiming ? total(view.DefocusTiming->Milliseconds) : 0)
                    << ',' << (view.ShutterTiming ? total(view.ShutterTiming->Milliseconds) : 0)
                    << ',' << (view.LensTiming ? view.LensTiming->Milliseconds : 0) << '\n';
            }
            camera.Primary = false;
            entity.GetComponent<TransformComponent>() = original;
        }
        if (!csv) throw std::runtime_error("Could not write Pavilion benchmark");
        return;
    }
    SceneRenderView tourView;
    for (const auto id : cameras) {
        auto entity = scene->GetEntityByUUID(id);
        auto& camera = entity.GetComponent<CameraComponent>();
        const auto name = entity.GetName().substr(0, 2);
        if (selection != "stills" && selection != "pairs" && selection != "tour" && selection != name)
            continue;
        if (selection == "pairs" && name != "04" && name != "06" && name != "07" && name != "08")
            continue;
        camera.Primary = true;
        const auto authored = camera;
        auto shot = [&](const std::string& suffix, unsigned frames) {
            SceneRenderView view;
            for (unsigned n = 1; n < frames; ++n)
                draw(view);
            const auto image = draw(view);
            save(name + suffix, image);
            std::cout << "PAVILION " << name << suffix << " frames=" << frames
                << " localLights=" << view.LocalMetrics->Count << " spots=" << view.LocalMetrics->SpotShadows
                << " points=" << view.LocalMetrics->PointShadows << '\n';
        };
        if (selection == "tour" || (selection == "pairs" && name == "08")) {
            const auto& track = tour.at(size_t(std::stoul(name) - 1));
            const auto original = entity.GetComponent<TransformComponent>();
            const auto motionOriginal = motion.GetComponent<TransformComponent>();
            const auto lightOriginal = spotlight.GetComponent<TransformComponent>();
            auto sequence = [&](bool shutter) {
                camera.MotionBlur.Enabled = shutter && authored.MotionBlur.Enabled;
                SceneRenderView pairView;
                auto& view = selection == "tour" ? tourView : pairView;
                const unsigned count = selection == "tour" ? track.Frames : 30;
                for (unsigned n = 0; n < count; ++n) {
                    const float progress = float(n) / float(track.Frames - 1);
                    place(entity, original, track, progress);
                    if (name == "05") {
                        motion.GetComponent<TransformComponent>().Rotation.y = progress * glm::two_pi<float>();
                        spotlight.GetComponent<TransformComponent>().Rotation.z =
                            lightOriginal.Rotation.z + .16f * std::sin(progress * glm::two_pi<float>());
                    }
                    if (name == "07" && n == 43)
                        camera.DepthOfField.TrackedSubject = scene->FindEntityByName("Archive focus mark").GetUUID();
                    if (name == "07" && n == 84)
                        camera.DepthOfField.TrackedSubject = authored.DepthOfField.TrackedSubject;
                    const auto image = draw(view);
                    if (selection == "tour") {
                        std::ostringstream filename;
                        filename << name << '-' << std::setfill('0') << std::setw(3) << n;
                        save(filename.str(), image);
                    } else if (n + 1 == count) {
                        save(name + (shutter ? "-shutter-on" : "-shutter-off"), image);
                    }
                }
            };
            sequence(true);
            if (selection == "pairs") sequence(false);
            entity.GetComponent<TransformComponent>() = original;
            motion.GetComponent<TransformComponent>() = motionOriginal;
            spotlight.GetComponent<TransformComponent>() = lightOriginal;
            camera = authored;
            camera.Primary = false;
            continue;
        }
        shot("", 12);
        if (selection == "pairs") {
            if (name == "04") {
                scene->Presentation().ScreenReflections.Enabled = false;
                shot("-probe-only", 12);
                std::vector<Entity> probes;
                for (auto handle : scene->GetAllEntitiesWith<ReflectionProbeComponent>()) {
                    Entity entity(handle, scene.get());
                    auto& probe = entity.GetComponent<ReflectionProbeComponent>();
                    if (probe.Enabled) {
                        probes.push_back(entity);
                        probe.Enabled = false;
                    }
                }
                shot("-global-only", 12);
                for (auto probe : probes) probe.GetComponent<ReflectionProbeComponent>().Enabled = true;
            } else if (name == "06") {
                camera.BloomOverride = false;
                shot("-bloom-off", 12);
                camera = authored;
                camera.LensEffects = {};
                shot("-lens-off", 12);
                camera = authored;
                camera.ColorGrade = {};
                shot("-neutral", 12);
                camera = authored;
                auto exposure = camera.ExposureOverride.value_or(scene->Presentation().Exposure);
                exposure.Mode = ExposureMode::ManualPhysical;
                exposure.CompensationStops = 0;
                camera.ExposureOverride = exposure;
                shot("-manual", 12);
            } else {
                camera.DepthOfField.Enabled = false;
                shot("-dof-off", 12);
            }
        }
        camera = authored;
        camera.Primary = false;
        scene->Presentation() = originalPresentation;
    }
}
