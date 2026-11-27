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
#include <iostream>
#include <thread>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
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
            uniform.Presentation.Exposure = ev - std::ceil(std::log2(luminance)) - 3;
            const double exposed = (luminance + .2 * glow) * std::exp2(uniform.Presentation.Exposure);
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
    uniform.Presentation.Exposure = -1;
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

    // Move an HDR pixel through all four half-resolution lattice phases.
    // The background is in the linear segment of Neutral, so the sum of the
    // presentation differences measures the reconstructed bloom energy.
    target->Resize(128, 64);
    uniform.Presentation.Exposure = -3;
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

    auto frame = study(1, {640, 360});
    if (metal) {
        const auto priorPhase = views[1].Temporal.Phase;
        frame.Presentation.Exposure += 2;
        frame.Presentation.Bloom.Enabled = false;
        render(frame, views[1]);
        frame.Presentation.Bloom = {true, .12f, .81f};
        render(frame, views[1]);
        if (views[1].Temporal.Phase != priorPhase + 2)
            throw std::runtime_error("Post-history exposure/bloom edits restarted TAA");
    }
    frame.Presentation.Exposure = scene->Presentation().Exposure;
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
    compensatedFrame.Presentation.Exposure -= 1;
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
            frame = study(float(i) - 1, {257, 149});
            frame.Presentation.TemporalAA = false;
            serial[i] = render(frame, reference);
        }
        auto unblock = backend.GateQueueForTesting();
        try {
            for (unsigned i = 0; i < queued.size(); ++i) {
                frame = study(float(i) - 1, {257, 149});
                frame.Presentation.TemporalAA = false;
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
                throw std::runtime_error("Queued bloom sampled a different scratch generation");
        }
        render(uniform, reference);
        std::cout << "BLOOM_LIFETIME queue=3 reset=pass oddResize=pass onePixel=pass\n";
    }
#endif
    std::cout << "BLOOM_STUDY poses=17 ID/depth=pass disabled=pass UI=pass"
              << (metal ? " AO/TAA=pass exposureHistory=pass" : "") << '\n';
}
