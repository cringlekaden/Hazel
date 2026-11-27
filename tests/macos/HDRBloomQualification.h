#pragma once

#include "Renderer3DParity.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#endif
#if defined(HZ_PLATFORM_MACOS) && __has_include(<Metal/Metal.hpp>)
#include "Platform/macOS/Metal/MetalDevice.h"
#endif

inline void HDRBloomQualification(const std::filesystem::path& output,
    const Hazel::Ref<Hazel::Project>& project, const Hazel::Ref<Hazel::ProjectAssets>& assets,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    const auto scene = CreateRef<Scene>();
    SceneSerializer reader(scene, assets->Root(), false, assets);
    if (!reader.DeserializeText(FileDocument::Read(Project::ResolveOwnedAsset(
            assets->Root(), project->GetConfig().StartScene)), false))
        throw std::runtime_error(reader.Report().Error);
    auto emitter = scene->FindEntityByName("Moving HDR tube");
    if (!emitter || !scene->Presentation().Bloom.Enabled || !scene->Presentation().TemporalAA ||
        !scene->Presentation().AmbientOcclusion.Enabled || !scene->Presentation().ScreenReflections.Enabled ||
        !scene->Presentation().Shadows.Enabled ||
        !(SceneDefaults::ReadPresentation(SceneDefaults::Encode(scene->Presentation())) == scene->Presentation()))
        throw std::runtime_error("HDR study settings or serialization failed");
    auto& transform = emitter.GetComponent<TransformComponent>();
    const auto emitterStart = transform.Translation;

    FramebufferSpecification spec;
    spec.Width = 640;
    spec.Height = 360;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    auto draw = [&](const SceneSubmission& frame, SceneRenderView& view, GLuint query = 0,
                    const std::function<void()>& ui = [] {}) {
        target->Bind();
        if (metal)
            target->Clear({0, 0, 0, 1});
        else {
            RenderCommand::SetClearColor({0, 0, 0, 1});
            RenderCommand::Clear();
        }
        target->ClearAttachment(1, -1);
        if (query) glBeginQuery(GL_TIME_ELAPSED, query);
        SceneRenderer::Render(frame, view, target, [] {}, ui);
        if (query) glEndQuery(GL_TIME_ELAPSED);
    };
    auto render = [&](const SceneSubmission& frame, SceneRenderView& view, GLuint query = 0,
                      const std::function<void()>& ui = [] {}) {
        draw(frame, view, query, ui);
        auto result = capture(target);
        if (!result || !result->Completed || !result->Error.empty())
            throw std::runtime_error("Incomplete HDR bloom readback");
        for (const auto& pixel : result->Color)
            if (!std::isfinite(pixel.r) || !std::isfinite(pixel.g) || !std::isfinite(pixel.b))
                throw std::runtime_error("Nonfinite HDR presentation");
        return result;
    };
    auto study = [&](float phase, glm::uvec2 extent) {
        transform.Translation = emitterStart + glm::vec3(1.4f * std::sin(phase * 2), 0, .4f * phase);
        SceneCameraView camera;
        camera.Position = {2.5f * phase, 3.05f, 9.3f};
        camera.View = glm::lookAt(camera.Position, glm::vec3(.8f * phase, 1.05f, -.3f), glm::vec3(0, 1, 0));
        camera.ToCamera = glm::vec3(glm::inverse(camera.View)[2]);
        camera.Projection = glm::perspective(glm::radians(50.f), float(extent.x) / extent.y, .1f, 50.f);
        camera.ViewProjection = camera.Projection * camera.View;
        return SceneRenderer::Collect(*scene, camera, extent);
    };
    auto coverage = [](const auto& a, const auto& b) {
        if (a->Entity != b->Entity || a->Depth != b->Depth)
            throw std::runtime_error("Bloom changed picking IDs or depth");
    };
    auto save = [&](const std::string& name, const auto& result) {
        std::ofstream file(output / (name + ".bin"), std::ios::binary);
        for (size_t i = 0; i < result->Color.size(); ++i) {
            file.write(reinterpret_cast<const char*>(&result->Color[i]), 16);
            file.write(reinterpret_cast<const char*>(&result->Entity[i]), 4);
            file.write(reinterpret_cast<const char*>(&result->Depth[i]), 4);
        }
        if (!file) throw std::runtime_error("Could not save HDR readback");
    };

    if (!FileSystem::GetEnvironmentPath("HAZEL_CAMERA_SHUTTER_BENCHMARK").empty()) {
        constexpr int samples = 31;
        struct FrameQuery {
            GLuint ID = 0;
            ~FrameQuery() { if (ID) glDeleteQueries(1, &ID); }
        } query;
        if (!metal) glGenQueries(1, &query.ID);
        for (const auto extent : {glm::uvec2(1280,720), glm::uvec2(1920,1080), glm::uvec2(3840,2160)}) {
            target->Resize(extent.x, extent.y);
            const auto shot = [&](int i, bool enabled) {
                auto frame = study((i + 4) * .04f, extent);
                frame.FrameSeconds = 1.f / 60;
                frame.Camera.MotionBlur = {enabled, MotionShutterMode::Angle, 270, MotionBlurQuality::Balanced};
                return frame;
            };
            {
                SceneRenderView view;
                view.ShutterTiming = std::make_shared<CameraMotionTiming>();
                for (int i = -4; i < samples; ++i) {
                    auto frame = shot(i, true);
                    frame.Presentation.TemporalAA = false;
                    frame.Presentation.Bloom.Enabled = false;
                    frame.Presentation.AmbientOcclusion.Enabled = false;
                    frame.Presentation.ScreenReflections.Enabled = false;
                    frame.Presentation.Shadows.Enabled = false;
                    frame.LocalLights.clear();
                    frame.ReflectionProbes.clear();
                    *view.ShutterTiming = {};
                    render(frame, view);
                    if (i >= 0) {
                        const auto& t = *view.ShutterTiming;
                        if (t.WorkingBytes == 0 || t.WorkingBytes > 96 * 1024 * 1024 ||
                            *std::min_element(t.Milliseconds.begin(), t.Milliseconds.end()) <= 0)
                            throw std::runtime_error("Shutter timing or scratch outside the measurement contract");
                        std::cout << "SHUTTER_PASS_TIMING " << extent.x << ' ' << extent.y << ' ' << i;
                        for (auto ms : t.Milliseconds) std::cout << ' ' << ms;
                        std::cout << ' ' << t.WorkingBytes << '\n';
                    }
                }
            }
            std::array<SceneRenderView, 2> views;
            for (int i = -4; i < samples; ++i) {
                for (bool enabled : {i % 2 != 0, i % 2 == 0}) {
                    const auto result = render(shot(i, enabled), views[enabled], query.ID);
                    GLuint64 ns = 0;
                    if (query.ID) glGetQueryObjectui64v(query.ID, GL_QUERY_RESULT, &ns);
                    if (i >= 0)
                        std::cout << "SHUTTER_FRAME_TIMING " << extent.x << ' ' << extent.y << ' ' << enabled << ' '
                                  << i << ' ' << result->Serial << ' ' << ns << '\n';
                }
            }
        }
        return;
    }

    if (!FileSystem::GetEnvironmentPath("HAZEL_CAMERA_SHUTTER_TEST").empty()) {
        constexpr glm::uvec2 extent(1280, 720);
        target->Resize(extent.x, extent.y);
        SceneSubmission frame;
        frame.Assets = assets;
        frame.FrameSeconds = 1.f / 60;
        frame.Presentation.SceneReferred = true;
        frame.Camera.Orthographic = true;
        frame.Camera.Position = {0, 0, 2};
        frame.Camera.View = glm::lookAt(frame.Camera.Position, glm::vec3(0), glm::vec3(0, 1, 0));
        frame.Camera.Projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, .1f, 20.f);
        frame.Camera.ViewProjection = frame.Camera.Projection * frame.Camera.View;
        frame.Camera.MotionBlur = {true, MotionShutterMode::Angle, 180, MotionBlurQuality::Cinematic};
        auto cube = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
        if (!cube.Accepted) throw std::runtime_error(cube.Error);
        auto material = CreateRef<MaterialDefinition>();
        material->BaseColor = {0, 0, 0, 1};
        material->Emissive = {.5f, 0, 0};
        material->EmissiveStrength = 1;
        SubmittedMesh stripe;
        stripe.Geometry = cube.Accepted;
        stripe.Identity = stripe.Picking = 7;
        stripe.Materials = {material};
        frame.Meshes.push_back(stripe);
        auto move = [&](float center) {
            frame.Meshes[0].SetWorld(glm::translate(glm::mat4(1), glm::vec3(center * 2 / extent.x, 0, 0)) *
                glm::scale(glm::mat4(1), glm::vec3(10.f / extent.x, 1, .1f)));
        };
        const auto moment = [&](const auto& image) {
            double signal = 0, position = 0, distance = 0;
            const double baseline = image->Color[extent.x * extent.y / 2 + extent.x / 2 - 27].r;
            for (int x = -30; x <= 30; ++x) {
                const auto i = extent.x * extent.y / 2 + extent.x / 2 + x;
                const double w = std::max(0.0, double(image->Color[i].r) - baseline);
                signal += w;
                position += (x + .5) * w;
                distance += (x + .5) * (x + .5) * w;
            }
            if (signal <= 0) throw std::runtime_error("Motion study lost its emitter");
            return std::array{signal, position / signal, distance / signal - std::pow(position / signal, 2)};
        };
        const auto referenceCamera = frame.Camera;
        SceneRenderView view;
        const auto run = [&](const char* name, double support, const std::function<void()>& advance) {
            view.Reset();
            frame.CameraDiscontinuity = false;
            render(frame, view);
            if (view.Shutter.Pass.Valid)
                throw std::runtime_error("Cold motion blur used a previous result");
            advance();
            const auto blur = render(frame, view);
            if (!view.Shutter.Pass.Valid)
                throw std::runtime_error("Motion blur did not use submitted history");
            frame.CameraDiscontinuity = true;
            const auto sharp = render(frame, view);
            coverage(blur, sharp);
            if (std::string_view(name) == "moving-object") {
                save("shutter-motion", blur);
                save("shutter-cut", sharp);
            }
            const auto [energy, center, variance] = moment(blur);
            const auto [sharpEnergy, sharpCenter, sharpVariance] = moment(sharp);
            const double width = 2 * std::sqrt(std::max(0., 3 * (variance - sharpVariance)));
            const double endpointError = std::abs(center - sharpCenter) + .5 * std::abs(width - support);
            const double energyRatio = energy / sharpEnergy;
            std::cout << "SHUTTER_SUPPORT case=" << name << " reference=" << support << " measured=" << width
                      << " centerError=" << center - sharpCenter << " endpointError=" << endpointError
                      << " energyRatio=" << energyRatio << std::endl;
            if (!(endpointError <= 1 && std::abs(energyRatio - 1) <= .12))
                throw std::runtime_error("Native global shutter differs from the sampled constant-motion reference");
        };
        move(-16);
        run("moving-object", 8, [&] { move(0); });

        frame.Camera.MotionBlur.Quality = MotionBlurQuality::Balanced;
        move(-16);
        run("balanced-moving-object", 8, [&] { move(0); });
        frame.Camera.MotionBlur.Quality = MotionBlurQuality::Cinematic;
        frame.FrameSeconds = 1.f / 24;
        move(-32);
        run("fast-object-24hz", 16, [&] { move(0); });
        frame.FrameSeconds = 1.f / 60;

        // An orthographic camera translating by the equivalent world distance
        // has the same image-space exposure endpoints for a stationary object.
        frame.Camera.Position.x = 32.f / extent.x;
        frame.Camera.View = glm::lookAt(frame.Camera.Position, {frame.Camera.Position.x, 0, 0}, {0, 1, 0});
        frame.Camera.ViewProjection = frame.Camera.Projection * frame.Camera.View;
        run("translating-camera", 8, [&] { frame.Camera = referenceCamera; });

        frame.Camera.Orthographic = false;
        frame.Camera.Projection = glm::perspective(glm::radians(50.f), float(extent.x) / extent.y, .1f, 20.f);
        frame.Camera.ViewProjection = frame.Camera.Projection * frame.Camera.View;
        const auto straight = frame.Camera;
        const float projectionX = straight.Projection[0][0];
        const float yaw = std::atan(32.f / (extent.x * projectionX));
        frame.Camera.View = glm::lookAt(frame.Camera.Position, {std::sin(yaw), 0, 2 - std::cos(yaw)}, {0, 1, 0});
        frame.Camera.ViewProjection = frame.Camera.Projection * frame.Camera.View;
        frame.Camera.ToCamera = glm::vec3(glm::inverse(frame.Camera.View)[2]);
        frame.Meshes[0].SetWorld(glm::scale(glm::mat4(1), glm::vec3(19.5f / (projectionX * extent.x), 1, .1f)));
        run("rotating-camera", 8, [&] { frame.Camera = straight; });

        frame.Camera = referenceCamera;
        frame.Camera.Physical = PhysicalCamera{};
        frame.Camera.Physical->ShutterSeconds = 1.f / 48;
        frame.Camera.MotionBlur.Shutter = MotionShutterMode::Duration;
        frame.Presentation.Exposure.CompensationStops = float(frame.Camera.Physical->EV100() - PhysicalCamera::ReferenceEV100);
        for (int fps : {24, 120}) {
            frame.FrameSeconds = 1.f / fps;
            move(-384.f / fps);
            run(fps == 24 ? "duration-24hz" : "duration-120hz", 8, [&] { move(0); });
        }
        frame.Camera = referenceCamera;
        frame.Presentation.Exposure.CompensationStops = 0;
        frame.FrameSeconds = 1.f / 60;
        frame.CameraDiscontinuity = false;
        view.Reset();
        move(-648);
        render(frame, view);
        move(-636);
        const auto edge = render(frame, view);
        frame.CameraDiscontinuity = true;
        const auto edgeSharp = render(frame, view);
        frame.CameraDiscontinuity = false;
        for (unsigned x = extent.x - 20; x < extent.x; ++x)
            if (edge->Color[extent.x * extent.y / 2 + x] != edgeSharp->Color[extent.x * extent.y / 2 + x])
                throw std::runtime_error("Shutter reconstruction wrapped across the viewport edge");
        const auto ui = [] {
            Renderer2D::BeginScene(glm::mat4(1));
            Renderer2D::DrawQuad({0, 0, -.99f}, {.06f, .06f}, glm::vec4(1));
            Renderer2D::EndScene();
        };
        const auto showUI = [&](bool enabled) {
            view.Reset();
            move(-16);
            render(frame, view);
            move(0);
            return enabled ? render(frame, view, 0, ui) : render(frame, view);
        };
        const auto clean = showUI(false);
        const auto composed = showUI(true);
        size_t changed = 0;
        for (unsigned y = 0; y < extent.y; ++y) {
            for (unsigned x = 0; x < extent.x; ++x) {
                const auto i = y * extent.x + x;
                if (clean->Color[i] == composed->Color[i]) continue;
                if (std::abs(int(x) - int(extent.x / 2)) > 20 || std::abs(int(y) - int(extent.y / 2)) > 12)
                    throw std::runtime_error("Screen UI contributed to the shutter reconstruction");
                ++changed;
            }
        }
        if (!changed) throw std::runtime_error("Shutter study did not render the screen overlay");
        std::cout << "SHUTTER_COVERAGE screenEdge=pass screenUI=pass\n";

        frame = {};
        frame.Assets = assets;
        frame.FrameSeconds = 1.f / 60;
        frame.Presentation.SceneReferred = true;
        PhysicalCamera opticsLens;
        opticsLens.Aperture = 1.8f;
        opticsLens.FocusDistance = 2;
        frame.Camera.Physical = opticsLens;
        frame.Camera.MotionBlur = {true, MotionShutterMode::Angle, 180, MotionBlurQuality::Cinematic};
        frame.Camera.DepthOfField = {true, DepthOfFieldQuality::Cinematic, 0, 0, 0};
        frame.Camera.Projection = glm::perspective(float(opticsLens.VerticalFOV(double(extent.x) / extent.y)),
            float(extent.x) / extent.y, .2f, 100.f);
        frame.Camera.ViewProjection = frame.Camera.Projection;
        frame.Presentation.Exposure.CompensationStops = float(opticsLens.EV100() - PhysicalCamera::ReferenceEV100);
        auto dark = CreateRef<MaterialDefinition>(*material);
        dark->Emissive = glm::vec3(0);
        auto plate = stripe;
        plate.Identity = plate.Picking = 8;
        plate.Materials = {dark};
        plate.SetWorld(glm::translate(glm::mat4(1), glm::vec3(0, 0, -6.0025f)) *
            glm::scale(glm::mat4(1), glm::vec3(30, 30, .001f)));
        frame.Meshes = {stripe, plate};
        const float worldPixel = 12 / (frame.Camera.Projection[0][0] * extent.x);
        const auto movePoint = [&](float pixels) {
            frame.Meshes[0].SetWorld(glm::translate(glm::mat4(1), glm::vec3(pixels * worldPixel, 0, -6.0005f)) *
                glm::scale(glm::mat4(1), glm::vec3(3 * worldPixel, 3 * worldPixel, .001f)));
        };
        const auto spreads = [&](const auto& image) {
            double weight = 0, x = 0, y = 0, xx = 0, yy = 0;
            for (int iy = -24; iy <= 24; ++iy) {
                for (int ix = -24; ix <= 24; ++ix) {
                    const double w = image->Color[(int(extent.y / 2) + iy) * extent.x + int(extent.x / 2) + ix].r;
                    weight += w;
                    x += w * (ix + .5);
                    y += w * (iy + .5);
                    xx += w * (ix + .5) * (ix + .5);
                    yy += w * (iy + .5) * (iy + .5);
                }
            }
            if (weight <= .001) throw std::runtime_error("Combined physical optics lost the emitter");
            return glm::dvec2(xx / weight - std::pow(x / weight, 2), yy / weight - std::pow(y / weight, 2));
        };
        view.Reset();
        movePoint(-16);
        render(frame, view);
        movePoint(0);
        const auto integrated = render(frame, view);
        frame.Camera.DepthOfField.Enabled = false;
        frame.Camera.MotionBlur.Enabled = false;
        const auto point = render(frame, view);
        coverage(integrated, point);
        const auto kernel = spreads(integrated) - spreads(point);
        // Independent projected-time and circular-pupil second moments.
        const double lensRadius = (1. / (1. / 50 - 1. / 2000) /
            (1. / (1. / 50 - 1. / 6000)) - 1) * 50 / (2 * 1.8) * 720 / 20.25;
        const double measuredLens = 2 * std::sqrt(std::max(0.0, kernel.y));
        const double measuredShutter = std::sqrt(std::max(0.0, 3 * (kernel.x - kernel.y)));
        std::cout << "SHUTTER_LENS apertureRadius=" << measuredLens << " referenceAperture=" << lensRadius
                  << " shutterHalfWidth=" << measuredShutter << " referenceShutter=4" << std::endl;
        if (!(std::abs(measuredLens - lensRadius) <= 1 && std::abs(measuredShutter - 4) <= 1))
            throw std::runtime_error("Combined shutter and pupil exceeded the independent support bound");

        target->Resize(640, 360);
        SceneRenderView optics, baseline;
        for (unsigned pose = 0; pose < 7; ++pose) {
            auto shot = study(.15f * pose, {640, 360});
            shot.FrameSeconds = 1.f / 60;
            shot.Presentation.Exposure.Mode = ExposureMode::Automatic;
            shot.Camera.Physical = PhysicalCamera{};
            shot.Camera.Physical->FocalLengthMM = 135;
            shot.Camera.Physical->Aperture = 1.2f;
            shot.Camera.Physical->FocusDistance = 5.5f;
            shot.Camera.DepthOfField = {true, DepthOfFieldQuality::Cinematic, 8, .22f, 0};
            shot.CameraDiscontinuity = pose == 4;
            auto sharp = render(shot, baseline);
            shot.Camera.MotionBlur = {true, MotionShutterMode::Angle, 270, MotionBlurQuality::Cinematic};
            auto blurred = render(shot, optics);
            coverage(sharp, blurred);
            if (pose == 2 && (!optics.Shutter.Pass.Valid || sharp->Color == blurred->Color))
                throw std::runtime_error("Combined world shot did not exercise shutter blur");
            if ((pose == 0 || pose == 4) && (optics.Shutter.Pass.Valid || sharp->Color != blurred->Color))
                throw std::runtime_error("World shutter blur survived a cut");
            if (pose == 2) {
                save("shutter-gallery-sharp", sharp);
                save("shutter-gallery-motion", blurred);
            }
        }
        std::cout << "SHUTTER_COMBINED optics=pass cut=pass pickingDepth=exact"
                  << (metal ? " reuseTemporalVectors=pass" : " spatialVectors=pass") << '\n';
#if defined(HZ_PLATFORM_MACOS) && __has_include(<Metal/Metal.hpp>)
        if (metal) {
            auto& backend = static_cast<MetalRendererAPI&>(RenderCommand::Backend());
            target->Resize(257, 149);
            std::array<Ref<const FramebufferReadback>, 3> queued, serial;
            SceneRenderView history;
            const auto shot = [&](unsigned i) {
                auto world = study(i * .13f, {257, 149});
                world.FrameSeconds = 1.f / 60;
                world.Presentation.TemporalAA = false;
                world.Camera.MotionBlur = {true, MotionShutterMode::Angle, 270, MotionBlurQuality::Cinematic};
                return world;
            };
            for (unsigned i = 0; i < serial.size(); ++i) serial[i] = render(shot(i), history);
            history.Reset();
            auto release = backend.GateQueueForTesting();
            try {
                for (unsigned i = 0; i < queued.size(); ++i) {
                    draw(shot(i), history);
                    if (i > 0 && !history.Shutter.Pass.Valid)
                        throw std::runtime_error("Queued shutter frame lost populated history");
                    target->Unbind();
                    queued[i] = target->Submit();
                    if (!queued[i] || queued[i]->Completed)
                        throw std::runtime_error("Shutter queue did not hold the submitted packet");
                }
                history.Reset();
                target->Resize(1, 1);
                if (backend.Observe()->Snapshot().InFlight != queued.size())
                    throw std::runtime_error("Shutter test did not use all three queue slots");
            } catch (...) {
                release();
                throw;
            }
            release();
            for (unsigned i = 0; i < queued.size(); ++i) {
                for (unsigned tries = 0; !queued[i]->Completed && tries < 2500; ++tries) {
                    target->Poll();
                    if (!queued[i]->Completed) std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                if (!queued[i]->Completed || !queued[i]->Error.empty() || queued[i]->Color != serial[i]->Color)
                    throw std::runtime_error("Queued shutter output differed from the serial reference");
                coverage(queued[i], serial[i]);
            }
            std::cout << "SHUTTER_LIFETIME queue=3 reset=pass resize=pass history=exact\n";
        }
#endif
        return;
    }

    if (!FileSystem::GetEnvironmentPath("HAZEL_CAMERA_DEFOCUS_BENCHMARK").empty()) {
        struct FrameQuery {
            GLuint ID = 0;
            ~FrameQuery() { if (ID) glDeleteQueries(1, &ID); }
        } query;
        if (!metal) glGenQueries(1, &query.ID);
        PhysicalCamera lens;
        lens.FocalLengthMM = 85;
        lens.Aperture = 1.4f;
        lens.FocusDistance = 2.75f;
        for (const auto extent : {glm::uvec2(1280,720), glm::uvec2(1920,1080), glm::uvec2(3840,2160)}) {
            target->Resize(extent.x, extent.y);
            auto frame = study(0, extent);
            frame.Camera.Physical = lens;
            frame.FrameSeconds = 1.f / 60;
            {
                auto isolated = frame;
                isolated.Presentation.TemporalAA = false;
                isolated.Presentation.Bloom.Enabled = false;
                isolated.Presentation.AmbientOcclusion.Enabled = false;
                isolated.Presentation.ScreenReflections.Enabled = false;
                isolated.Presentation.Shadows.Enabled = false;
                isolated.LocalLights.clear();
                isolated.ReflectionProbes.clear();
                isolated.Camera.DepthOfField.Enabled = true;
                SceneRenderView view;
                view.DefocusTiming = std::make_shared<DepthOfFieldTiming>();
                for (int i = -4; i < 15; ++i) {
                    *view.DefocusTiming = {};
                    render(isolated, view);
                    if (i >= 0) {
                        const auto& timing = *view.DefocusTiming;
                        if (timing.WorkingBytes == 0 || timing.WorkingBytes > 112 * 1024 * 1024 ||
                            *std::min_element(timing.Milliseconds.begin(), timing.Milliseconds.end()) <= 0)
                            throw std::runtime_error("Defocus timing or scratch outside the measurement contract");
                        std::cout << "DEFOCUS_PASS_TIMING " << extent.x << ' ' << extent.y << ' ' << i;
                        for (auto ms : timing.Milliseconds) std::cout << ' ' << ms;
                        std::cout << ' ' << timing.WorkingBytes << '\n';
                    }
                }
            }
            std::array<SceneRenderView, 2> views;
            for (int i = -4; i < 15; ++i) {
                // Preserve each mode's history and reverse order on alternate
                // pairs to limit queue and thermal drift in the frame delta.
                for (bool enabled : {i % 2 != 0, i % 2 == 0}) {
                    frame.Camera.DepthOfField.Enabled = enabled;
                    const auto result = render(frame, views[enabled], query.ID);
                    GLuint64 ns = 0;
                    if (query.ID) glGetQueryObjectui64v(query.ID, GL_QUERY_RESULT, &ns);
                    if (i >= 0)
                        std::cout << "DEFOCUS_FRAME_TIMING " << extent.x << ' ' << extent.y << ' ' << enabled << ' '
                                  << i << ' ' << result->Serial << ' ' << ns << '\n';
                }
            }
        }
        return;
    }

    if (!FileSystem::GetEnvironmentPath("HAZEL_CAMERA_DEFOCUS_TEST").empty()) {
        constexpr glm::uvec2 extent(1280, 720);
        target->Resize(extent.x, extent.y);
        SceneSubmission frame;
        frame.Assets = assets;
        frame.FrameSeconds = 1.f / 60;
        frame.Presentation.SceneReferred = true;
        PhysicalCamera lens;
        lens.LensProjection = true;
        lens.FocalLengthMM = 50;
        lens.Aperture = 1.8f;
        lens.FocusDistance = 2;
        frame.Camera.Physical = lens;
        frame.Camera.Projection = glm::perspective(float(lens.VerticalFOV(double(extent.x) / extent.y)),
            float(extent.x) / extent.y, .2f, 100.f);
        frame.Camera.ViewProjection = frame.Camera.Projection;
        frame.Presentation.Exposure.CompensationStops = float(lens.EV100() - PhysicalCamera::ReferenceEV100);
        auto cube = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
        if (!cube.Accepted) throw std::runtime_error(cube.Error);
        const auto addCard = [&](float depth, glm::vec2 size, float luminance, int id, glm::vec3 color = glm::vec3(1)) {
            auto material = CreateRef<MaterialDefinition>();
            material->BaseColor = {0, 0, 0, 1};
            material->Emissive = color;
            material->EmissiveStrength = luminance;
            SubmittedMesh card;
            card.Geometry = cube.Accepted;
            card.Materials = {material};
            card.Identity = card.Picking = id;
            card.SetWorld(glm::translate(glm::mat4(1), glm::vec3(0, 0, -depth - .0005f)) *
                glm::scale(glm::mat4(1), glm::vec3(size, .001f)));
            frame.Meshes.push_back(std::move(card));
        };
        const auto measure = [](const auto& image) {
            constexpr double baseline = .06;
            double energy = 0, moment = 0;
            for (int dy = -22; dy <= 22; ++dy) for (int dx = -22; dx <= 22; ++dx) {
                const size_t index = size_t(int(image->Height / 2) + dy) * image->Width +
                                     size_t(int(image->Width / 2) + dx);
                const double v = image->Color[index].r;
                const double radiance = v < .04 ? std::sqrt(std::max(v, 0.0) / 6.25) : v + .04;
                const double weight = std::max(0.0, radiance - baseline - 1e-6);
                energy += weight;
                moment += weight * ((dx + .5) * (dx + .5) + (dy + .5) * (dy + .5));
            }
            if (energy < .001) throw std::runtime_error("Defocus reconstruction lost the emitter");
            return std::array{moment / energy, energy};
        };
        SceneRenderView view;
        double worstRadiusError = 0;
        for (const float depth : {1.2f, 6.f, 3.2f}) {
            const bool balanced = depth == 3.2f;
            frame.Meshes.clear();
            addCard(depth + .002f, {30, 30}, .06f, 1);
            const float pixelWidth = 2 * depth / (frame.Camera.Projection[1][1] * extent.y);
            addCard(depth, glm::vec2(3 * pixelWidth), .25f, 2);
            frame.Camera.DepthOfField.Enabled = false;
            const auto sharp = render(frame, view);
            if (view.Focus.Pass.Gathered)
                throw std::runtime_error("Disabled depth of field allocated scratch");
            frame.Camera.DepthOfField.Enabled = true;
            frame.Camera.DepthOfField.Quality = balanced ? DepthOfFieldQuality::Balanced : DepthOfFieldQuality::Cinematic;
            const auto blurred = render(frame, view);
            coverage(sharp, blurred);
            const bool nearer = depth < lens.FocusDistance;
            save(balanced ? "defocus-balanced-sharp" : nearer ? "defocus-near-sharp" : "defocus-far-sharp", sharp);
            save(balanced ? "defocus-balanced" : nearer ? "defocus-near" : "defocus-far", blurred);
            const auto conjugate = [](double distance) { return 1 / (1. / 50 - 1 / distance); };
            const double expected = std::abs(conjugate(2000) / conjugate(depth * 1000.) - 1) *
                50. / (2 * lens.Aperture) * extent.y / 20.25;
            const auto [sharpMoment, sharpEnergy] = measure(sharp);
            const auto [blurredMoment, blurredEnergy] = measure(blurred);
            const double measured = std::sqrt(std::max(0.0, 2 * (blurredMoment - sharpMoment)));
            worstRadiusError = std::max(worstRadiusError, std::abs(measured - expected));
            std::cout << "DEFOCUS_DISC depth=" << depth << " referenceRadius=" << expected
                      << " measuredRadius=" << measured << " energyRatio=" << blurredEnergy / sharpEnergy << std::endl;
        }
        if (worstRadiusError > 1)
            throw std::runtime_error("Native defocus radius exceeded the one-pixel gate");
        frame.Meshes.clear();
        constexpr float front = 1.2f;
        const float pixelWidth = 2 * front / (frame.Camera.Projection[1][1] * extent.y);
        // A narrow red occluder in front of a green plate. Integrate the
        // visible fraction of a uniform thin-lens pupil analytically.
        addCard(6, {30, 30}, .25f, 1, {0, 1, 0});
        addCard(front, {4 * pixelWidth, 2}, .5f, 2, {1, 0, 0});
        frame.Camera.DepthOfField.Enabled = false;
        const auto stripeSharp = render(frame, view);
        frame.Camera.DepthOfField.Enabled = true;
        frame.Camera.DepthOfField.Quality = DepthOfFieldQuality::Cinematic;
        const auto stripe = render(frame, view);
        coverage(stripeSharp, stripe);
        save("defocus-stripe-sharp", stripeSharp);
        save("defocus-stripe", stripe);
        constexpr double pi = 3.141592653589793;
        const double radius = 50 * 50. * (2000 - 1200) / (2 * 1.8 * 1200 * (2000 - 50)) * extent.y / 20.25;
        const auto section = [&](double x) {
            x = std::clamp(x, -radius, radius);
            return x * std::sqrt(std::max(0.0, radius * radius - x * x)) +
                radius * radius * std::asin(std::clamp(x / radius, -1.0, 1.0));
        };
        double coverageError = 0, colorError = 0, maximumOutside = 0;
        for (int dy = -10; dy < 10; ++dy) {
            for (int dx = -14; dx < 14; ++dx) {
                const double x = dx + .5;
                const auto expected = (section(x + 2) - section(x - 2)) / (pi * radius * radius);
                const size_t i = size_t(int(extent.y / 2) + dy) * extent.x + int(extent.x / 2) + dx;
                const auto& color = stripe->Color[i];
                coverageError += std::abs(color.r * 2 - expected);
                colorError += std::abs(color.g * 4 - (1 - expected));
                if (std::abs(x) >= radius + 3)
                    maximumOutside = std::max(maximumOutside, double(color.r));
            }
        }
        coverageError /= 560;
        colorError /= 560;
        std::cout << "DEFOCUS_OCCLUSION coverageMAE=" << coverageError << " farMAE=" << colorError
                  << " outside=" << maximumOutside << std::endl;
        if (!(coverageError <= .09 && colorError <= .09 && maximumOutside <= .01))
            throw std::runtime_error("Thin foreground coverage exceeded the independent pupil-area gate");

        frame.Camera.DepthOfField.FocusHalfLife = .25f;
        frame.FrameSeconds = .125f;
        frame.Camera.Physical->FocusDistance = 6;
        render(frame, view);
        const auto focus = [&] { return view.Focus.InverseDistance; };
        if (std::abs(focus() - (1. / 6 + (1. / 2 - 1. / 6) * std::sqrt(.5))) > 1e-6)
            throw std::runtime_error("Manual focus pull missed the independent optical-power step");
        frame.CameraDiscontinuity = true;
        render(frame, view);
        if (std::abs(focus() - 1. / 6) > 1e-6)
            throw std::runtime_error("Camera cut did not snap the focus");
        frame.CameraDiscontinuity = false;
        target->Resize(641, 359);
        frame.Camera.Physical->FocusDistance = 4;
        render(frame, view);
        if (focus() != .25 || view.Focus.Pass.Gathered->GetSpecification().Width != 321)
            throw std::runtime_error("Odd resize did not reset focus and scratch");
        frame.Camera.DepthOfField.TrackedSubject = 42;
        frame.Camera.FocusSubjectDepth = .5f;
        render(frame, view);
        if (std::abs(focus() - .5) > 1e-6 || view.Focus.TrackedGoal != 2)
            throw std::runtime_error("Autofocus exceeded its bounded optical-power change");
        frame.Camera.FocusSubjectDepth = .5001f;
        render(frame, view);
        if (view.Focus.TrackedGoal != 2)
            throw std::runtime_error("Autofocus ignored its subject-depth hysteresis");
        frame.Camera.FocusSubjectDepth = 6;
        frame.CameraDiscontinuity = true;
        render(frame, view);
        if (std::abs(focus() - 1. / 6) > 1e-6)
            throw std::runtime_error("Camera cut retained the previous tracked focus");
        frame.CameraDiscontinuity = false;
        frame.Camera.FocusSubjectDepth.reset();
        render(frame, view);
        if (!(focus() > 1. / 6 && focus() < .25))
            throw std::runtime_error("Autofocus did not pull toward manual focus after losing the subject");
        std::cout << "DEFOCUS_FOCUS halfLife=pass tracking=pass hysteresis=pass cameraCut=pass oddResize=pass pickingDepth=exact\n";

        target->Resize(640, 360);
        SceneRenderView baseScene, opticalScene;
        for (unsigned pose = 0; pose < 7; ++pose) {
            auto shot = study(float(pose) * .075f, {640, 360});
            shot.Presentation.Exposure.Mode = ExposureMode::Automatic;
            shot.FrameSeconds = 1.f / 60;
            shot.Camera.Physical = lens;
            shot.Camera.Physical->FocalLengthMM = 135;
            shot.Camera.Physical->Aperture = 1.2f;
            shot.Camera.Physical->FocusDistance = 3.5f + .7f * pose;
            shot.CameraDiscontinuity = pose == 4;
            auto sharp = render(shot, baseScene);
            shot.Camera.DepthOfField = {true, DepthOfFieldQuality::Cinematic, 6, .24f, .08f};
            auto blurred = render(shot, opticalScene);
            coverage(sharp, blurred);
            if (sharp->Color == blurred->Color)
                throw std::runtime_error("Combined shot did not exercise depth of field");
            if (pose == 0 || pose == 4) {
                save("defocus-gallery-sharp-" + std::to_string(pose), sharp);
                save("defocus-gallery-" + std::to_string(pose), blurred);
            }
        }
        std::cout << "DEFOCUS_COMBINED focalPull=pass autoExposure=pass pickingDepth=exact"
                  << (metal ? " TAA_AO_SSR=pass" : " spatialSSR=pass") << '\n';

#if defined(HZ_PLATFORM_MACOS) && __has_include(<Metal/Metal.hpp>)
        if (metal) {
            auto& backend = static_cast<MetalRendererAPI&>(RenderCommand::Backend());
            target->Resize(257, 149);
            std::array<Ref<const FramebufferReadback>, 3> queued, serial;
            auto get = [&](unsigned step) {
                auto shot = study(.03f * step, {257, 149});
                shot.FrameSeconds = 1.f / 60;
                shot.Presentation.TemporalAA = false;
                shot.Presentation.Exposure.Mode = ExposureMode::Automatic;
                shot.Camera.Physical = lens;
                shot.Camera.Physical->FocalLengthMM = 85;
                shot.Camera.Physical->FocusDistance = 1.f + .5f * step;
                shot.Camera.DepthOfField = {true, DepthOfFieldQuality::Cinematic, 5, .2f, .12f};
                return shot;
            };
            SceneRenderView queuedView;
            for (unsigned i = 0; i < serial.size(); ++i)
                serial[i] = render(get(i), queuedView);
            queuedView.Reset();
            auto unblock = backend.GateQueueForTesting();
            try {
                for (unsigned i = 0; i < queued.size(); ++i) {
                    draw(get(i), queuedView);
                    target->Unbind();
                    queued[i] = target->Submit();
                    if (!queued[i] || queued[i]->Completed)
                        throw std::runtime_error("Depth of field lifetime did not hold all three submissions");
                }
                queuedView.Reset();
                target->Resize(1, 1);
                if (backend.Observe()->Snapshot().InFlight != queued.size())
                    throw std::runtime_error("Depth of field lifetime did not exercise the full queue");
            } catch (...) {
                unblock();
                throw;
            }
            unblock();
            for (unsigned i = 0; i < queued.size(); ++i) {
                for (unsigned tries = 0; !queued[i]->Completed && tries < 2500; ++tries) {
                    target->Poll();
                    if (!queued[i]->Completed) std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                if (!queued[i]->Completed || !queued[i]->Error.empty() || queued[i]->Color != serial[i]->Color)
                    throw std::runtime_error("Queued defocus differed from the serial reference");
                coverage(queued[i], serial[i]);
            }
            std::cout << "DEFOCUS_LIFETIME queue=3 reset=pass resize=pass history=exact\n";
        }
#endif
        return;
    }

#if defined(HZ_PLATFORM_MACOS) && __has_include(<Metal/Metal.hpp>)
    const auto admission = FileSystem::GetEnvironmentPath("HAZEL_CAMERA_ADMISSION").string();
    if (!admission.empty()) {
        if (!metal)
            throw std::runtime_error("Camera admission study requires Metal");
        const bool serial = admission == "4k-serial";
        const bool hd = admission == "1080p-queued";
        const bool spatial = admission == "4k-queued-spatial";
        const bool sustained = admission == "4k-sustained";
        if (!serial && !hd && !spatial && !sustained && admission != "4k-queued")
            throw std::runtime_error("Unknown camera admission case");
        const glm::uvec2 extent = hd ? glm::uvec2(1920, 1080) : glm::uvec2(3840, 2160);
        auto& backend = static_cast<MetalRendererAPI&>(RenderCommand::Backend());
        auto device = backend.Device();
        auto layer = NS::RetainPtr(CA::MetalLayer::layer());
        layer->setDevice(device->NativeDevice());
        layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
        layer->setDrawableSize({double(extent.x), double(extent.y)});
        target->Resize(extent.x, extent.y);
        SceneRenderView view;
        auto frame = study(0, extent);
        if (spatial)
            frame.Presentation.TemporalAA = false;
        auto observer = backend.Observe();
        auto waitForCompletion = [&] {
            for (unsigned i = 0; i < 2500 && observer->Snapshot().InFlight; ++i) {
                target->Poll();
                if (observer->Snapshot().InFlight)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        };
        auto unblock = serial ? std::function<void()>{} : backend.GateQueueForTesting();
        const char* step = "world";
        try {
            for (unsigned i = 0; i < 3; ++i) {
                step = "world";
                draw(frame, view);
                target->Unbind();
                step = "output";
                target->EncodeOutput();
                step = "admission";
                device->Present(layer.get(), extent.x, extent.y, extent.x / 2, extent.y / 2, 2., true);
                const auto state = observer->Snapshot();
                std::cout << "CAMERA_ADMISSION case=" << admission << " submitted=" << state.Submitted
                          << " inFlight=" << state.InFlight
                          << " frameBytes=" << state.ResourceBytes << " attachmentBytes=" << state.AttachmentBytes
                          << std::endl;
                if (state.Submitted != i + 1 || state.InFlight != (serial ? 1u : i + 1))
                    throw std::runtime_error("Camera admission did not submit to the drawable");
                if (serial)
                    waitForCompletion();
            }
            if (sustained) {
                const auto oldSize = target->GetSpecification();
                device->SetCriticalMemoryPressureForTesting(true);
                bool rejected = false;
                try { target->Resize(1023, 575); }
                catch (const MetalMemoryAdmissionError&) { rejected = true; }
                device->SetCriticalMemoryPressureForTesting(false);
                if (!rejected || target->GetSpecification().Width != oldSize.Width)
                    throw std::runtime_error("Pressure rejection changed the current surface");
                // Independent view, different extent and settings, while three
                // older packets still own the original framebuffer generations.
                SceneRenderView detached;
                target->Resize(1023, 575);
                auto replacement = study(.5f, {1023, 575});
                replacement.Presentation.Bloom.Enabled = false;
                replacement.Presentation.AmbientOcclusion.Quality = 1;
                draw(replacement, detached);
                target->Cancel();
                detached.Reset();
                target->Resize(extent.x, extent.y);
                if (observer->Snapshot().InFlight != 3)
                    throw std::runtime_error("Resize/cancellation did not cover three queued frames");
                std::cout << "CAMERA_MEMORY pressure=pass pendingResize=pass cancellation=pass\n";
            }
        } catch (...) {
            std::cout << "CAMERA_ADMISSION case=" << admission << " failedStep=" << step << std::endl;
            if (unblock)
                unblock();
            waitForCompletion();
            throw;
        }
        if (unblock)
            unblock();
        waitForCompletion();
        const auto state = observer->Snapshot();
        if (state.Completed != 3 || state.Errors != 0)
            throw std::runtime_error("Camera admission did not complete on the GPU");
        if (sustained) {
            for (unsigned i = 3; i < 30; ++i) {
                // A cut at the start of the second burst exercises history reset.
                if (i == 3) frame.Camera.Identity = 77;
                draw(frame, view);
                if (i == 3 && view.Temporal.Phase != 1)
                    throw std::runtime_error("4K camera cut reused temporal history");
                target->Unbind();
                target->EncodeOutput();
                device->Present(layer.get(), extent.x, extent.y, extent.x / 2, extent.y / 2, 2., true);
                if (observer->Snapshot().Submitted != i + 1)
                    throw std::runtime_error("Sustained camera admission skipped a frame");
                if (i % 3 == 2) waitForCompletion();
            }
            const auto full = observer->Snapshot();
            if (full.Completed != 30 || full.Errors || full.MemoryRejections || full.PeakInFlight != 3 ||
                full.PeakTrackedBytes > full.TrackedBudget || full.PeakNativeBytes > full.NativeBudget)
                throw std::runtime_error("Sustained camera memory qualification failed");
            std::cout << "CAMERA_MEMORY sustained=" << full.Completed << " peakInFlight=" << full.PeakInFlight
                      << " tracked=" << full.PeakTrackedBytes << '/' << full.TrackedBudget
                      << " native=" << full.PeakNativeBytes << '/' << full.NativeBudget << '\n';

            view.Reset();
            target->Resize(1, 1);
            target->Poll();
            if (observer->Snapshot().InFlight || observer->Snapshot().Errors)
                throw std::runtime_error("Cancelled view retained GPU work");
            std::cout << "CAMERA_MEMORY cameraCut=pass retirement=pass\n";
        }
        const auto final = observer->Snapshot();
        std::cout << "CAMERA_ADMISSION case=" << admission << " completed=" << final.Completed
                  << " errors=" << final.Errors << '\n';
        return;
    }
#endif

    if (!FileSystem::GetEnvironmentPath("HAZEL_BLOOM_BENCHMARK").empty()) {
        struct Query {
            GLuint ID = 0;
            ~Query() { if (ID) glDeleteQueries(1, &ID); }
        } query;
        if (!metal) {
            GLint bits = 0;
            glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS, &bits);
            if (!bits) throw std::runtime_error("OpenGL has no elapsed GPU query");
            glGenQueries(1, &query.ID);
        }
        for (const auto extent : {glm::uvec2(1280, 720), glm::uvec2(1920, 1080), glm::uvec2(3840, 2160)}) {
            target->Resize(extent.x, extent.y);
            auto frame = study(0, extent);
            // At native 4K, the full offscreen FP32 readback plus two TAA
            // generations exceed the renderer budget even with bloom off.
            if (extent.x == 3840)
                frame.Presentation.TemporalAA = false;
            for (unsigned enabled : {0u, 1u}) {
                SceneRenderView view;
                if (enabled && metal) view.BloomMetrics = std::make_shared<BloomTiming>();
                frame.Presentation.Bloom.Enabled = enabled != 0;
                for (int sample = -4; sample < 15; ++sample) {
                    const auto result = render(frame, view, query.ID);
                    GLuint64 ns = 0;
                    if (query.ID) glGetQueryObjectui64v(query.ID, GL_QUERY_RESULT, &ns);
                    if (sample < 0) continue;
                    if (view.BloomMetrics && !view.BloomMetrics->Valid)
                        throw std::runtime_error("Bloom GPU phase sample unavailable");
                    std::cout << "BLOOM_TIMING " << extent.x << ' ' << extent.y << ' ' << enabled << ' '
                              << sample << ' ' << result->Serial << ' ' << ns;
                    if (view.BloomMetrics) {
                        const auto& timing = *view.BloomMetrics;
                        for (auto ms : timing.Milliseconds) std::cout << ' ' << ms;
                        std::cout << ' ' << timing.WorkingBytes;
                    }
                    std::cout << '\n';
                }
                if (!metal && enabled) {
                    // Apple GL 4.1 supports elapsed queries. They cannot nest
                    // in the whole-frame elapsed query, so time passes separately.
                    view.BloomMetrics = std::make_shared<BloomTiming>();
                    for (int sample = -4; sample < 15; ++sample) {
                        render(frame, view);
                        if (sample < 0) continue;
                        const auto& timing = *view.BloomMetrics;
                        if (!timing.Valid)
                            throw std::runtime_error("OpenGL bloom elapsed query unavailable");
                        std::cout << "BLOOM_PHASES " << extent.x << ' ' << extent.y << ' ' << sample;
                        for (auto ms : timing.Milliseconds) std::cout << ' ' << ms;
                        std::cout << ' ' << timing.WorkingBytes << '\n';
                    }
                }
            }
        }
        return;
    }

    if (!FileSystem::GetEnvironmentPath("HAZEL_CAMERA_EXPOSURE_BENCHMARK").empty()) {
        struct FrameQuery {
            GLuint ID = 0;
            ~FrameQuery() { if (ID) glDeleteQueries(1, &ID); }
        } frameQuery;
        if (!metal)
            glGenQueries(1, &frameQuery.ID);
        // Measure only the incremental meter; use full-resolution HDR input
        // with representative scene color and a single camera pass.
        for (const auto extent : {glm::uvec2(1280, 720), glm::uvec2(1920, 1080), glm::uvec2(3840, 2160)}) {
            target->Resize(extent.x, extent.y);
            auto frame = study(0, extent);
            frame.Presentation.TemporalAA = false;
            frame.Presentation.AmbientOcclusion.Enabled = false;
            frame.Presentation.ScreenReflections.Enabled = false;
            frame.Presentation.Bloom.Enabled = false;
            frame.Presentation.Shadows.Enabled = false;
            frame.LocalLights.clear();
            frame.Presentation.Exposure.Mode = ExposureMode::Automatic;
            frame.FrameSeconds = 1.f / 60;
            SceneRenderView camera;
            camera.ExposureTiming = std::make_shared<ExposureMeterTiming>();
            for (unsigned i = 0; i < 19; ++i) {
                *camera.ExposureTiming = {};
                render(frame, camera);
                if (i >= 4) {
                    const auto& timing = *camera.ExposureTiming;
                    if (timing.Milliseconds <= 0 || timing.WorkingBytes >= 1024 * 1024)
                        throw std::runtime_error("Invalid exposure benchmark measurement");
                    std::cout << "CAMERA_METER_TIMING " << extent.x << ' ' << extent.y << ' '
                              << i - 4 << ' ' << timing.Milliseconds << ' ' << timing.WorkingBytes << '\n';
                }
            }
        }
        // Time the complete scene separately. Apple GL cannot nest elapsed
        // queries, and the production path must run without diagnostics.
        for (const auto extent : {glm::uvec2(1280, 720), glm::uvec2(1920, 1080), glm::uvec2(3840, 2160)}) {
            target->Resize(extent.x, extent.y);
            auto frame = study(0, extent);
            frame.FrameSeconds = 1.f / 60;
            for (bool automatic : {false, true}) {
                SceneRenderView camera;
                frame.Presentation.Exposure.Mode = automatic ? ExposureMode::Automatic : ExposureMode::ManualPhysical;
                for (int sample = -4; sample < 15; ++sample) {
                    const auto result = render(frame, camera, frameQuery.ID);
                    GLuint64 ns = 0;
                    if (frameQuery.ID) glGetQueryObjectui64v(frameQuery.ID, GL_QUERY_RESULT, &ns);
                    if (sample >= 0)
                        std::cout << "CAMERA_FRAME_TIMING " << extent.x << ' ' << extent.y << ' '
                                  << automatic << ' ' << sample << ' ' << result->Serial << ' ' << ns << '\n';
                }
            }
        }
        return;
    }

    // A uniform grayscale emitter isolates extraction, unit-DC filtering and
    // exposure arithmetic from scene lighting, geometry edges and tone shoulder.
    SceneRenderView reference;
    target->Resize(67, 39);
    SceneSubmission uniform;
    uniform.Assets = assets;
    uniform.Presentation.SceneReferred = true;
    uniform.Presentation.Bloom = {true, .20f, .7f};
    auto cube = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if (!cube.Accepted) throw std::runtime_error(cube.Error);
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {0, 0, 0, 1};
    material->Emissive = glm::vec3(1);
    SubmittedMesh mesh;
    mesh.Geometry = cube.Accepted;
    mesh.Materials = {material};
    mesh.Identity = mesh.Picking = 1;
    mesh.SetWorld(glm::scale(glm::mat4(1), glm::vec3(3, 3, .1f)));
    uniform.Meshes.push_back(mesh);
    uniform.Camera.Position = {0, 0, 2};
    uniform.Camera.Orthographic = true;
    double maxError = 0;
    for (const float luminance : {.25f, .5f, .75f, 1.f, 1.25f, 1.5f, 4.f, 32.f, 128.f}) {
        material->EmissiveStrength = luminance;
        const auto roundtrip = ReadMaterialText(WriteMaterialText(*material));
        if (!(roundtrip == *material))
            throw std::runtime_error("HDR emissive material roundtrip failed");
        const double shoulder = luminance > 32 ? 64. - 1024. / luminance : luminance;
        const double knee = std::clamp(shoulder - .5, 0., 1.);
        const double glow = std::max(shoulder - 1., .5 * knee * knee);
        for (const int ev : {-1, 0, 1}) {
            uniform.Presentation.Exposure.CompensationStops = ev - std::ceil(std::log2(luminance)) - 3;
            const double exposed = (luminance + .2 * glow) * std::exp2(uniform.Presentation.Exposure.CompensationStops);
            const double expected = exposed < .08 ? 6.25 * exposed * exposed : exposed - .04;
            const auto result = render(uniform, reference);
            for (const auto& p : result->Color)
                maxError = std::max(maxError, std::abs(p.r - expected));
        }
    }
    if (maxError > 1e-4)
        throw std::runtime_error("Uniform HDR bloom/exposure differs from the FP64 reference: " + std::to_string(maxError));
    std::cout << "BLOOM_ORACLE uniformCases=27 maxLinearError=" << maxError << '\n';

    uniform.Presentation.Bloom.Enabled = false;
    uniform.Presentation.Exposure.CompensationStops = -1;
    material->EmissiveStrength = .25f;
    double physicalError = 0;
    for (const float iso : {64.f, 100.f, 200.f, 400.f, 800.f}) {
        PhysicalCamera lens;
        lens.Aperture = 8;
        lens.ShutterSeconds = 1.f / 128;
        lens.ISO = iso;
        uniform.Camera.Physical = lens;
        const double sensor = .25 * (1024. * iso / (100. * 128. * 64.)) / 2;
        const double expected = sensor < .08 ? 6.25 * sensor * sensor : sensor - .04;
        const auto result = render(uniform, reference);
        for (const auto& p : result->Color)
            physicalError = std::max(physicalError, std::abs(p.r - expected));
    }
    if (physicalError > 1e-4)
        throw std::runtime_error("Physical camera gain differs from the FP64 reference");
    uniform.Camera.Physical.reset();
    std::cout << "CAMERA_ORACLE physicalCases=5 maxLinearError=" << physicalError << '\n';

    // Independent stop-domain oracle through the production world compositor.
    // Choose patch luminances off bin boundaries to avoid backend rounding ties.
    {
        auto metered = uniform;
        auto& settings = metered.Presentation.Exposure;
        settings.Mode = ExposureMode::Automatic;
        settings.Metering = ExposureMetering::Average;
        settings.CompensationStops = 0;
        constexpr float dim = .071f, bright = .29f;
        const auto expectedTarget = [](double y) {
            double stop = std::log(y) / std::log(2.);
            double midpoint = -24. + (std::floor((stop + 24.) / .375) + .5) * .375;
            return std::log(.18) / std::log(2.) - midpoint;
        };
        const auto measuredGain = [&](SceneRenderView& camera, float luminance) {
            material->EmissiveStrength = luminance;
            const auto result = render(metered, camera);
            const auto color = double(result->Color[(result->Height / 2) * result->Width + result->Width / 2].r);
            const auto exposed = color < .04 ? std::sqrt(color / 6.25) : color + .04;
            return std::log2(exposed / luminance);
        };
        std::ofstream trace(output / "camera-adaptation.csv");
        trace.precision(12);
        trace << "transition,hz,seconds,measuredStops,referenceStops\n";
        double maxTargetError = 0, maxAdaptationError = 0, worstFrameRateDrift = 0;
        for (const auto pair : {std::array{dim, bright}, std::array{bright, dim}}) {
            const auto origin = expectedTarget(pair[0]), destination = expectedTarget(pair[1]);
            const auto rate = destination > origin ? settings.BrighteningRate : settings.DarkeningRate;
            double prior = 0;
            for (const int hz : {30, 60, 120}) {
                SceneRenderView camera;
                metered.FrameSeconds = 1.f / hz;
                maxTargetError = std::max(maxTargetError, std::abs(measuredGain(camera, pair[0]) - origin));
                double gain = 0;
                for (int i = 1; i <= hz; ++i) {
                    gain = measuredGain(camera, pair[1]);
                    const double expected = destination + (origin - destination) * std::exp(-rate * (double(i) / hz));
                    maxAdaptationError = std::max(maxAdaptationError, std::abs(gain - expected));
                    trace << (destination > origin ? "dark" : "bright") << ',' << hz << ','
                          << double(i) / hz << ',' << gain << ',' << expected << '\n';
                }
                if (hz != 30) worstFrameRateDrift = std::max(worstFrameRateDrift, std::abs(gain - prior));
                prior = gain;
            }
        }
        if (!trace) throw std::runtime_error("Could not save adaptation trace");
        SceneRenderView camera;
        metered.FrameSeconds = 1.f / 60;
        const auto unlocked = measuredGain(camera, bright);
        settings.Locked = true;
        const auto locked = measuredGain(camera, dim);
        ++metered.Camera.Identity;
        const auto cut = measuredGain(camera, dim);
        target->Resize(69, 43);
        const auto resize = measuredGain(camera, bright);
        if (maxTargetError > .25 || maxAdaptationError > .02 || worstFrameRateDrift > .02 ||
            std::abs(locked - unlocked) > .001 || std::abs(cut - expectedTarget(dim)) > .001 ||
            std::abs(resize - expectedTarget(bright)) > .001)
            throw std::runtime_error("Camera exposure oracle mismatch: target " + std::to_string(maxTargetError) +
                " adaptation " + std::to_string(maxAdaptationError) + " drift " + std::to_string(worstFrameRateDrift) +
                " lock " + std::to_string(locked - unlocked) + " cut " + std::to_string(cut));
        {
            auto spot = metered;
            spot.Presentation.Background = {dim, dim, dim, 1};
            spot.Presentation.Exposure.Locked = false;
            spot.Presentation.Exposure.Metering = ExposureMetering::Spot;
            spot.Presentation.Exposure.SpotRadius = .12f;
            spot.CameraDiscontinuity = true;
            spot.Meshes[0].SetWorld(glm::translate(glm::mat4(1), glm::vec3(0, .5f, 0)) *
                glm::scale(glm::mat4(1), glm::vec3(3, 1, .1f)));
            material->EmissiveStrength = bright;
            const auto sample = [&](float vertical) {
                spot.Presentation.Exposure.SpotCenter = {.5f, vertical};
                auto result = render(spot, camera);
                const auto picked = std::find(result->Entity.begin(), result->Entity.end(), mesh.Picking);
                if (picked == result->Entity.end()) throw std::runtime_error("Missing spot-meter test patch");
                const auto color = result->Color[size_t(picked - result->Entity.begin())].r;
                return std::log2((double(color) + .04) / bright);
            };
            if (std::abs(sample(.2f) - expectedTarget(bright)) > .001 ||
                std::abs(sample(.8f) - expectedTarget(dim)) > .001)
                throw std::runtime_error("Top-left spot metering orientation or weights differ");
        }
        {
            auto reject = metered;
            reject.CameraDiscontinuity = true;
            auto& trim = reject.Presentation.Exposure;
            trim.Locked = false;
            trim.LowPercentile = .15f;
            trim.HighPercentile = .85f;
            auto hot = CreateRef<MaterialDefinition>(*material);
            auto cold = CreateRef<MaterialDefinition>(*material);
            hot->EmissiveStrength = 256;
            cold->EmissiveStrength = .001f;
            auto addPatch = [&](const auto& patch, float x, int identity) {
                auto drawPatch = mesh;
                drawPatch.Materials = {patch};
                drawPatch.Identity = drawPatch.Picking = identity;
                drawPatch.SetWorld(glm::translate(glm::mat4(1), glm::vec3(x, .45f, -.3f)) *
                    glm::scale(glm::mat4(1), glm::vec3(.5f, .4f, .1f)));
                reject.Meshes.push_back(std::move(drawPatch));
            };
            addPatch(hot, -.6f, 2);
            addPatch(cold, .6f, 3);
            material->EmissiveStrength = bright;
            auto result = render(reject, camera);
            for (int id : {2, 3})
                if (std::find(result->Entity.begin(), result->Entity.end(), id) == result->Entity.end())
                    throw std::runtime_error("Percentile test lost a bright or dark patch");
            const auto center = result->Color[(result->Height / 2) * result->Width + result->Width / 2].r;
            if (std::abs(std::log2((double(center) + .04) / bright) - expectedTarget(bright)) > .001)
                throw std::runtime_error("Trimmed HDR highlights or shadows biased exposure");
        }
        // Empty geometry with an HDR background must produce a valid sky-only
        // estimate when included. An empty exclusion mask keeps neutral startup.
        metered.Meshes.clear();
        metered.Presentation.Background = {.25f, .25f, .25f, 1};
        settings.Locked = false;
        settings.IncludeSky = false;
        SceneRenderView sky;
        const auto noSky = render(metered, sky);
        const auto centerColor = [](const auto& result) {
            return result->Color[(result->Height / 2) * result->Width + result->Width / 2].r;
        };
        settings.IncludeSky = true;
        const auto withSky = render(metered, sky);
        if (std::abs(centerColor(noSky) - .21f) > .001 ||
            std::abs(centerColor(withSky) - float(.25 * std::exp2(expectedTarget(.25)) - .04)) > .001)
            throw std::runtime_error("Empty or sky-only camera metering differs");
        metered.Presentation.Background = {0,0,0,1};
        metered.CameraDiscontinuity = true;
        if (centerColor(render(metered, sky)) != 0)
            throw std::runtime_error("Black scene gained nonzero radiance");
        std::cout << "CAMERA_EXPOSURE startupError=" << maxTargetError
                  << " adaptationError=" << maxAdaptationError << " frameRateDrift=" << worstFrameRateDrift
                  << " lock=pass cut=pass resize=pass\n";
    }

    // Move an HDR pixel through all four half-resolution lattice phases.
    // The background is in the linear segment of Neutral, so the sum of the
    // presentation differences measures the reconstructed bloom energy.
    target->Resize(128, 64);
    uniform.Presentation.Exposure.CompensationStops = -3;
    uniform.Presentation.Background = {.75f, .75f, .75f, 1};
    material->EmissiveStrength = 4;
    double minEnergy = 1e9, maxEnergy = 0;
    for (int y : {0, 1}) for (int x : {0, 1}) {
        auto& source = uniform.Meshes.front();
        source.SetWorld(glm::translate(glm::mat4(1), glm::vec3((2*x+1)/128.f, (2*y+1)/64.f, 0)) *
            glm::scale(glm::mat4(1), glm::vec3(2.f/128, 2.f/64, .1f)));
        uniform.Presentation.Bloom.Enabled = false;
        const auto off = render(uniform, reference);
        uniform.Presentation.Bloom.Enabled = true;
        const auto on = render(uniform, reference);
        double energy = 0;
        for (size_t i = 0; i < on->Color.size(); ++i)
            energy += on->Color[i].r - off->Color[i].r;
        minEnergy = std::min(minEnergy, energy);
        maxEnergy = std::max(maxEnergy, energy);
    }
    if (minEnergy < .05 || maxEnergy / minEnergy > 1.025)
        throw std::runtime_error("Single-pixel HDR source lost bloom energy over the sampling lattice");
    std::cout << "BLOOM_IMPULSE fourPhases=pass minEnergy=" << minEnergy << " maxEnergy=" << maxEnergy << '\n';

    if (metal) {
        SceneRenderView temporal;
        uniform.Presentation.TemporalAA = true;
        for (int i = 0; i < 8; ++i) render(uniform, temporal);
        uniform.Meshes.clear();
        const auto disappeared = render(uniform, temporal);
        const auto settled = render(uniform, reference);
        double residual = 0;
        for (size_t i = 0; i < disappeared->Color.size(); ++i)
            residual = std::max(residual, double(std::abs(disappeared->Color[i].r - settled->Color[i].r)));
        if (residual > .0003)
            throw std::runtime_error("Bloom retained a vanished HDR emitter after temporal resolve");
        std::cout << "BLOOM_DISOCCLUSION maxResidual=" << residual << '\n';
        uniform.Meshes.push_back(mesh);
        uniform.Presentation.TemporalAA = false;
    }

    target->Resize(640, 360);
    std::array<SceneRenderView, 2> views;
    for (unsigned pose = 0; pose < 17; ++pose) {
        auto frame = study(float(pose) / 8 - 1, {640, 360});
        if (frame.ReflectionProbes.size() != 2 || !frame.Light ||
            std::count_if(frame.LocalLights.begin(), frame.LocalLights.end(), [](const auto& light) {
                return light.CastShadows;
            }) < 2)
            throw std::runtime_error("HDR study requires probes, directional, spot and point shadows");
        auto off = frame;
        off.Presentation.Bloom.Enabled = false;
        const auto a = render(off, views[0]);
        const auto b = render(frame, views[1]);
        coverage(a, b);
        if (views[0].Bloom.TexturePayloadBytes() != 0 || views[1].Bloom.TexturePayloadBytes() == 0)
            throw std::runtime_error("Incorrect per-view bloom allocation");
        if (metal && (views[1].Temporal.Phase != pose + 1 || !views[1].TemporalMetrics->Valid ||
            !views[1].AmbientTiming->Valid))
            throw std::runtime_error("Metal AO/TAA did not coexist with bloom");
        save("off-" + std::to_string(pose), a);
        save("on-" + std::to_string(pose), b);
    }

    SceneRenderView automaticView;
    for (unsigned pose = 0; pose < 17; ++pose) {
        auto metered = study(float(pose) / 8 - 1, {640, 360});
        metered.Presentation.Exposure.Mode = ExposureMode::Automatic;
        metered.FrameSeconds = 1.f / 60;
        save("auto-" + std::to_string(pose), render(metered, automaticView));
        if (metal && automaticView.Temporal.Phase != pose + 1)
            throw std::runtime_error("Automatic exposure changed the scene-linear temporal sequence");
    }

    auto frame = study(1, {640, 360});
    if (metal) {
        const auto priorPhase = views[1].Temporal.Phase;
        frame.Presentation.Exposure.CompensationStops += 2;
        frame.Presentation.Bloom.Enabled = false;
        render(frame, views[1]);
        frame.Presentation.Bloom = {true, .12f, .81f};
        render(frame, views[1]);
        if (views[1].Temporal.Phase != priorPhase + 2)
            throw std::runtime_error("Post-history exposure/bloom edits restarted TAA");
    }
    frame.Presentation.Exposure = scene->Presentation().Exposure;
    frame.Presentation.Exposure.Mode = ExposureMode::ManualPhysical;
    frame.Presentation.Bloom = scene->Presentation().Bloom;
    frame.Presentation.TemporalAA = false;
    frame.Presentation.AmbientOcclusion.Enabled = false;
    const auto on = render(frame, reference);
    const auto repeated = render(frame, reference);
    save("parity-on", on);
    if (on->Color != repeated->Color)
        throw std::runtime_error("Repeated AA-off HDR bloom is not deterministic");
    PhysicalCamera physical;
    physical.ISO = 200;
    physical.Aperture = 8;
    auto physicalFrame = frame;
    physicalFrame.Camera.Physical = physical;
    physicalFrame.Camera.Identity = 42;
    auto compensatedFrame = frame;
    compensatedFrame.Presentation.Exposure.CompensationStops -= 1;
    SceneRenderView cameraView;
    const auto photographic = render(physicalFrame, cameraView);
    const auto compensated = render(compensatedFrame, reference);
    coverage(on, photographic);
    if (photographic->Color != compensated->Color || photographic->Color == on->Color ||
        render(frame, cameraView)->Color != on->Color)
        throw std::runtime_error("Physical exposure did not match equivalent gain or leaked across cameras");
    save("camera-reference", on);
    save("camera-physical", photographic);
    save("camera-compensated", compensated);
    std::cout << "CAMERA_STUDY equivalentGain=bit-exact ID/depth=pass cameraSwitch=pass\n";
    frame.Presentation.Bloom.Intensity = 0;
    const auto zero = render(frame, reference);
    frame.Presentation.Bloom.Enabled = false;
    const auto disabled = render(frame, reference);
    save("parity-off", disabled);
    coverage(zero, disabled);
    if (zero->Color != disabled->Color || reference.Bloom.TexturePayloadBytes() != 0)
        throw std::runtime_error("Zero intensity changed the disabled output or retained scratch");
    const auto ui = [] {
        Renderer2D::BeginScene(glm::mat4(1));
        Renderer2D::DrawQuad({.85f, .84f, -.99f}, {.09f, .09f}, glm::vec4(1, 1, 1, 1));
        Renderer2D::EndScene();
    };
    frame.Presentation.Bloom = {true, .12f, .6f};
    const auto noUI = render(frame, reference);
    const auto withUI = render(frame, reference, 0, ui);
    size_t changed = 0;
    for (unsigned y = 0; y < noUI->Height; ++y)
        for (unsigned x = 0; x < noUI->Width; ++x) {
            const size_t i = size_t(y) * noUI->Width + x;
            if (noUI->Color[i] == withUI->Color[i]) continue;
            ++changed;
            if (x < 575 || x > 610 || y < 320 || y > 340)
                throw std::runtime_error("UI contributed to scene bloom");
        }
    if (!changed) throw std::runtime_error("The HDR overlay test did not draw UI");
    const auto& wideGlow = noUI;
    frame.Presentation.Bloom.Radius = .1f;
    const auto narrowGlow = render(frame, reference);
    if (wideGlow->Color == narrowGlow->Color)
        throw std::runtime_error("Bloom radius did not change the filter");

#ifdef HZ_PLATFORM_MACOS
    if (metal) {
        auto& backend = static_cast<MetalRendererAPI&>(RenderCommand::Backend());
        target->Resize(257, 149);
        std::array<Ref<const FramebufferReadback>, 3> queued, serial;
        for (unsigned i = 0; i < serial.size(); ++i) {
            frame = study(.03f * float(i), {257, 149});
            frame.Presentation.TemporalAA = false;
            frame.Presentation.Exposure.Mode = ExposureMode::Automatic;
            frame.FrameSeconds = 1.f / 60;
            serial[i] = render(frame, reference);
        }
        reference.Reset();
        auto unblock = backend.GateQueueForTesting();
        try {
            for (unsigned i = 0; i < queued.size(); ++i) {
                frame = study(.03f * float(i), {257, 149});
                frame.Presentation.TemporalAA = false;
                frame.Presentation.Exposure.Mode = ExposureMode::Automatic;
                frame.FrameSeconds = 1.f / 60;
                draw(frame, reference);
                target->Unbind();
                queued[i] = target->Submit();
                if (!queued[i] || queued[i]->Completed)
                    throw std::runtime_error("Bloom lifetime did not cover a gated submission");
            }
            reference.Reset();
            target->Resize(1, 1);
            if (backend.Observe()->Snapshot().InFlight != queued.size())
                throw std::runtime_error("Bloom lifetime did not use all three queue slots");
        } catch (...) {
            unblock();
            throw;
        }
        unblock();
        for (unsigned i = 0; i < queued.size(); ++i) {
            for (unsigned tries = 0; !queued[i]->Completed && tries < 2500; ++tries) {
                target->Poll();
                if (!queued[i]->Completed) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (!queued[i]->Completed || !queued[i]->Error.empty())
                throw std::runtime_error("Queued bloom did not complete");
            coverage(queued[i], serial[i]);
            if (queued[i]->Color != serial[i]->Color)
                throw std::runtime_error("Queued bloom or exposure sampled a different scratch generation");
        }
        render(uniform, reference);
        std::cout << "BLOOM_LIFETIME queue=3 reset=pass oddResize=pass onePixel=pass exposureHistory=exact\n";
    }
#endif
    std::cout << "BLOOM_STUDY poses=17 ID/depth=pass disabled=pass UI=pass"
              << (metal ? " AO/TAA=pass exposureHistory=pass" : "") << '\n';
}
