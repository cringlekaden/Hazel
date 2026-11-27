#pragma once

#include "Renderer3DParity.h"
#include "ScreenReflectionQualification.h"
#include "ReflectionSphereQualification.h"
#include "HDRBloomQualification.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Renderer/Scene/ReflectionProbeCapture.h"
#include <thread>
#include "Hazel/Scene/SceneSerializer.h"
#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <iomanip>
#include <iostream>

namespace ReflectionProbeReference
{
using namespace Hazel;

// Intersect all six finite plane rectangles independently in FP64.
inline glm::dvec3 IntersectBox(glm::dvec3 point, glm::dvec3 ray, glm::dvec3 low, glm::dvec3 high,
                         glm::dvec3 capture)
{
    double first = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis)
        for (const double plane : {low[axis], high[axis]})
        {
            if (ray[axis] == 0)
                continue;
            const double distance = (plane - point[axis]) / ray[axis];
            if (distance < 0 || distance >= first)
                continue;
            const glm::dvec3 hit = point + distance * ray;
            bool inside = true;
            for (int other = 0; other < 3; ++other)
                if (axis != other)
                    inside &= hit[other] >= low[other] - 1e-10 && hit[other] <= high[other] + 1e-10;
            if (inside)
                first = distance;
        }
    if (!std::isfinite(first))
        throw std::runtime_error("FP64 test point did not intersect the containing box");
    return point + first * ray - capture;
}

inline glm::dvec3 MeasureCube(unsigned probe, unsigned mip, unsigned face, double u, double v)
{
    return {.1 + .19 * probe + .11 * mip + .07 * face, .1 + .6 * u, .2 + .6 * v};
}

inline Ref<PreparedEnvironment> DirectionField(const PreparedEnvironment &source, unsigned probe)
{
    auto environment = CreateRef<PreparedEnvironment>(source);
    environment->Generation = probe + 1;
    environment->Specular.Side = 16;
    environment->Irradiance.Side = 16;
    auto fill = [&](LightingImage &image, unsigned levels, unsigned offset, double scale) {
        image.Mips.clear();
        for (unsigned mip = 0; mip < levels; ++mip)
        {
            const unsigned side = image.Side >> mip;
            auto &pixels = image.Mips.emplace_back();
            for (unsigned face = 0; face < 6; ++face)
                for (unsigned y = 0; y < side; ++y)
                    for (unsigned x = 0; x < side; ++x)
                    {
                        const auto rgb = MeasureCube(probe, offset + mip, face, (x + .5) / side, (y + .5) / side);
                        for (unsigned c = 0; c < 3; ++c)
                            pixels.push_back(float(rgb[c] * scale));
                    }
        }
    };
    environment->Specular.EncodingVersion = 2;
    fill(environment->Specular, 3, 0, 1);
    fill(environment->Irradiance, 1, 2, glm::pi<double>());
    return environment;
}

// Stay inside a face's texel-center domain so hardware seam interpolation
// cannot influence the analytical bilinear result.
inline bool DirectionUV(glm::dvec3 direction, double guard, unsigned &face, double &u, double &v)
{
    const auto a = glm::abs(direction);
    const double major = std::max({a.x, a.y, a.z});
    glm::dvec2 uv;
    if (major == a.x)
    {
        face = direction.x > 0 ? 0 : 1;
        uv = {face == 0 ? -direction.z : direction.z, -direction.y};
    }
    else if (major == a.y)
    {
        face = direction.y > 0 ? 2 : 3;
        uv = {direction.x, face == 2 ? direction.z : -direction.z};
    }
    else
    {
        face = direction.z > 0 ? 4 : 5;
        uv = {face == 4 ? direction.x : -direction.x, -direction.y};
    }
    u = .5 + .5 * uv.x / major;
    v = .5 + .5 * uv.y / major;
    return u >= guard && u <= 1 - guard && v >= guard && v <= 1 - guard;
}
}

inline void ReflectionProbeQualification(const std::filesystem::path &output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)> &capture)
{
    using namespace Hazel;
    using namespace ReflectionProbeReference;
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    std::filesystem::create_directories(output);
    const auto project = Project::LoadCandidate(FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT"));
    auto assets = CreateRef<ProjectAssets>(project->GetAssetRoot());
    if (!FileSystem::GetEnvironmentPath("HAZEL_HDR_BLOOM").empty()) {
        HDRBloomQualification(output, project, assets, capture);
        return;
    }
    if (const auto workspace=FileSystem::GetEnvironmentPath("HAZEL_REFLECTION_WORKSPACE"); !workspace.empty()) {
        ReflectionSphereStudy::RunWorkspace(output,assets,workspace,capture);
        return;
    }
    if (const auto spheres=FileSystem::GetEnvironmentPath("HAZEL_REFLECTION_SPHERES"); !spheres.empty()) {
        ReflectionSphereStudy::Run(output,assets,capture,spheres=="capture");
        return;
    }
    if (!FileSystem::GetEnvironmentPath("HAZEL_SSR_GALLERY").empty()) {
        ScreenReflectionQualification(output,project,assets,capture);
        return;
    }
    const auto captureSide = FileSystem::GetEnvironmentPath("HAZEL_PROBE_CAPTURE_SIDE").generic_u8string();
    if (!captureSide.empty())
    {
        const unsigned side = unsigned(std::stoul(captureSide));
        auto scene = CreateRef<Scene>();
        SceneSerializer reader(scene, assets->Root(), false, assets);
        if (!reader.DeserializeText(FileDocument::Read(Project::ResolveOwnedAsset(
                assets->Root(), project->GetConfig().StartScene)), false))
            throw std::runtime_error(reader.Report().Error);
        std::vector<Entity> probes;
        for (auto handle : scene->GetAllEntitiesWith<ReflectionProbeComponent>())
            probes.emplace_back(handle, scene.get());
        std::sort(probes.begin(), probes.end(), [](Entity a, Entity b) { return a.GetUUID() < b.GetUUID(); });
        if (probes.size() != 2)
            throw std::runtime_error("Capture qualification expects two Gallery probes");

        // The second capture changes presentation and removes all usable probe
        // inputs. HDR source pixels must be independent of those view settings.
        auto snapshot = Scene::CopyForAuthoring(scene);
        snapshot->Presentation().Exposure = -8;
        snapshot->Presentation().AmbientOcclusion.Enabled = true;
        snapshot->Presentation().TemporalAA = true;
        for (auto handle : snapshot->GetAllEntitiesWith<ReflectionProbeComponent>())
            Entity(handle, snapshot.get()).GetComponent<ReflectionProbeComponent>().Environment = "Absent.henv";

        auto record = [&](const Ref<Scene> &input, glm::vec3 position) {
            ReflectionProbeCapture job(input, position, side);
            while (!job.Advance())
                if (metal) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return job.Image();
        };
        if (side == 128)
        {
            auto axes = CreateRef<Scene>();
            axes->SetAssets(CreateRef<ProjectAssets>(FileSystem::GetEnvironmentPath("HAZEL_PROBE_AXIS_ROOT")));
            axes->Presentation().SceneReferred = true;
            axes->Presentation().Environment.Asset = "Reference/IBL_v1/AsymmetricAxes.henv";
            axes->Presentation().Environment.Background = true;
            const auto image = record(axes, glm::vec3(.3f, 1.f, -.6f));
            const auto &rgb = image.Mips[0];
            double maximum = 0;
            for (unsigned face = 0; face < 6; ++face)
                for (unsigned y : {0u, 29u, 64u, 93u, 127u})
                    for (unsigned x : {0u, 23u, 64u, 101u, 127u})
                    {
                        const auto expected = 1. + CubeDirection(face, (x + .5) / side, (y + .5) / side);
                        const size_t pixel = (size_t(face) * side + y) * side + x;
                        for (unsigned channel = 0; channel < 3; ++channel)
                            maximum = std::max(maximum, std::abs(expected[channel] - rgb[pixel * 3 + channel]));
                    }
            const float peak = *std::max_element(rgb.begin(), rgb.end());
            if (maximum > .02 || peak < 1.95f)
                throw std::runtime_error("FP32 probe face axes, edge directions or HDR range failed");
            std::cout << "REFLECTION_CAPTURE_AXES maximum=" << maximum << " peak=" << peak << '\n';

            auto card = axes->CreateEntity("Mask capture control");
            card.GetComponent<TransformComponent>().Translation = {0, 0, -2};
            card.GetComponent<TransformComponent>().Scale = {2, 2, .08f};
            auto &binding = card.AddComponent<MeshRendererComponent>();
            binding.Mesh = "Native/Direct_v1/UnitCube.hmesh";
            binding.Materials[7] = "Reference/Direct_v1/Surfaces/Mask.hmat";
            const auto origin = glm::vec3(0);
            const auto mask = record(axes, origin);
            card.RemoveComponent<MeshRendererComponent>();
            const auto sky = record(axes, origin);
            const auto faceOffset = size_t(5) * side * side * 3;
            unsigned visible = 0, clipped = 0;
            for (unsigned y = 40; y < 88; ++y)
                for (unsigned x = 40; x < 88; ++x)
                {
                    const auto at = faceOffset + (size_t(y) * side + x) * 3;
                    if (mask.Mips[0][at] == sky.Mips[0][at]) ++clipped;
                    else ++visible;
                }
            if (visible < 100 || clipped < 100)
                throw std::runtime_error("Masked capture did not preserve both occlusion and cutouts");
            std::cout << "REFLECTION_CAPTURE_MASK visible=" << visible << " cutout=" << clipped << '\n';

            auto shadows = Scene::CopyForAuthoring(scene);
            auto pointLights = shadows->GetAllEntitiesWith<PointLightComponent>();
            for (auto handle : pointLights)
                Entity(handle, shadows.get()).GetComponent<PointLightComponent>().CastShadows = true;
            const auto position = glm::vec3(scene->GetWorldTransform(probes.front())[3]);
            const auto off = record(scene, position), on = record(shadows, position);
            size_t shadowed = 0;
            for (size_t pixel = 0; pixel < on.Mips[0].size(); ++pixel)
                shadowed += off.Mips[0][pixel] > on.Mips[0][pixel] + .01f;
            if (shadowed < 100)
                throw std::runtime_error("Shadowed geometry did not affect probe capture");
            std::cout << "REFLECTION_CAPTURE_SHADOW samples=" << shadowed << '\n';
        }
        for (size_t i = 0; i < probes.size(); ++i)
        {
            const glm::vec3 position(scene->GetWorldTransform(probes[i])[3]);
            const auto start = std::chrono::steady_clock::now();
            const auto image = record(scene, position);
            const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now() - start).count();
            auto control = record(snapshot, position);
            if (image.Mips.front() != control.Mips.front())
                throw std::runtime_error("Probe capture depends on exposure, AO/TAA or local probe state");
            FileSystem::WriteNewFile(output / (i == 0 ? "Warm.hlight" : "Cool.hlight"), EncodeLightingImage(image));
            std::cout << "REFLECTION_CAPTURE side=" << side << " index=" << i << " milliseconds=" << ms
                      << " bytes=" << image.Bytes() << " presentationAndProbeInvariance=pass\n";
        }
        return;
    }
    const auto global = assets->Environment("Reference/IBL_v1/Constant.henv").Accepted;
    const auto cube = assets->Mesh("Native/Direct_v1/UnitCube.hmesh").Accepted;
    if (!global || !cube)
        throw std::runtime_error("Missing prepared reflection qualification fixtures");

    SceneSubmission frame;
    frame.Assets = assets;
    frame.Presentation.SceneReferred = true;
    frame.Presentation.Environment.Lighting = true;
    frame.Environment = global;
    frame.Camera.Orthographic = true;
    frame.Camera.Position = {0, 0, 3};
    frame.Camera.ViewProjection = glm::ortho(-2.f, 2.f, -1.f, 1.f, .1f, 10.f) *
        glm::lookAt(frame.Camera.Position, glm::vec3(0), glm::vec3(0, 1, 0));
    const std::vector<SubmittedReflectionProbe> probes{
        {1, {-.65f, 0, .5f}, {1.1f, 1.3f, .65f}, {-.15f, .1f, .2f}, {1.3f, 1.5f, .9f}, .4f, DirectionField(*global, 0)},
        {2, { .65f, 0, .4f}, {1.3f, 1.3f, .65f}, { .25f,-.1f, .1f}, {1.6f, 1.5f, .8f}, .45f, DirectionField(*global, 1)}};
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {.9f, .5f, .2f, 1};
    SubmittedMesh surface;
    surface.Geometry = cube;
    surface.Materials = {material};
    surface.Picking = 42;
    surface.SetWorld(glm::scale(glm::mat4(1), glm::vec3(5, 3, .2f)));
    frame.Meshes = {surface};

    FramebufferSpecification spec;
    spec.Width = 97; spec.Height = 49;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    SceneRenderView view;
    view.LocalMetrics = std::make_shared<LocalLightingMetrics>();
    auto render = [&](SceneSubmission &submission, SurfaceView inspection, GLuint query = 0) {
        submission.Debug = inspection;
        submission.LinearLightingInspection = inspection != SurfaceView::Shaded;
        submission.LocalMetrics = view.LocalMetrics;
        target->Bind();
        if (metal)
            target->Clear({0, 0, 0, 1});
        else
        {
            RenderCommand::SetClearColor({0, 0, 0, 1});
            RenderCommand::Clear();
        }
        target->ClearAttachment(1, -1);
        if (query) glBeginQuery(GL_TIME_ELAPSED, query);
        SceneRenderer::Render(submission, view, target, [] {}, [] {});
        if (query) glEndQuery(GL_TIME_ELAPSED);
        const auto result = capture(target);
        if (!result || !result->Completed || !result->Error.empty())
            throw std::runtime_error("Incomplete reflection probe capture");
        return result;
    };

    if (const auto benchmark = FileSystem::GetEnvironmentPath("HAZEL_REFLECTION_BENCHMARK"); !benchmark.empty())
    {
        std::ofstream csv(output / "timing.csv");
        csv << "width,height,probes,sample,forwardGPUms\n";
        auto timingProbes = probes;
        for (unsigned i = 0; i < timingProbes.size(); ++i)
        {
            auto &probe = timingProbes[i];
            probe.Environment = assets->Environment(i == 0 ? "ReflectionStudy/Warm.henv" : "ReflectionStudy/Cool.henv").Accepted;
            if (!probe.Environment)
                throw std::runtime_error("Missing prepared 128-pixel probe for the benchmark");
        }
        const unsigned largestCount = benchmark == "global-only" ? 0 : unsigned(timingProbes.size());
        struct ElapsedQuery
        {
            GLuint ID = 0;
            ~ElapsedQuery() { if (ID) glDeleteQueries(1, &ID); }
        } query;
        if (!metal)
        {
            GLint bits = 0;
            glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS, &bits);
            if (!bits)
                throw std::runtime_error("OpenGL driver has no time-elapsed GPU counter");
            glGenQueries(1, &query.ID);
        }
        // Alternate enabled states to limit drift from GPU frequency/temperature.
        for (const auto extent : {glm::uvec2(1280, 720), glm::uvec2(1920, 1080), glm::uvec2(2560, 1440)})
        {
            target->Resize(extent.x, extent.y);
            for (int sample = -5; sample < 15; ++sample)
                for (unsigned count = 0; count <= largestCount; ++count)
                {
                    frame.ReflectionProbes.assign(timingProbes.begin(), timingProbes.begin() + count);
                    *view.LocalMetrics = {};
                    render(frame, SurfaceView::Shaded, query.ID);
                    double elapsed = view.LocalMetrics->ForwardMilliseconds;
                    bool valid = view.LocalMetrics->Valid;
                    if (query.ID)
                    {
                        GLuint64 nanoseconds = 0;
                        glGetQueryObjectui64v(query.ID, GL_QUERY_RESULT, &nanoseconds);
                        elapsed = double(nanoseconds) / 1e6;
                        valid = glGetError() == GL_NO_ERROR;
                    }
                    if (sample >= 0 && (!valid || elapsed <= 0))
                        throw std::runtime_error("Native forward timing unavailable at " +
                            std::to_string(extent.x) + "x" + std::to_string(extent.y) + " with " +
                            std::to_string(count) + " probes, validity " +
                            std::to_string(valid) + ", GPU ms " + std::to_string(elapsed));
                    if (sample >= 0)
                        csv << extent.x << ',' << extent.y << ',' << count << ',' << sample << ','
                            << elapsed << '\n';
                }
        }
        return;
    }

    const auto brdf = ReadRuntimeBRDF();
    double maxRadiance = 0, maxEnergy = 0, maxDiffuse = 0;
    size_t compared = 0, overlap = 0, residual = 0;
    unsigned reachedFaces = 0;
    std::ofstream reference(output / "reference.csv");
    reference << std::setprecision(9) << "camera,rough,metal,samples,maxRadianceError,maxSpecularError,maxDiffuseChange\n";
    unsigned camera = 0;
    for (const auto eye : {glm::vec3(0,0,1), glm::normalize(glm::vec3(.7f,0,1)),
                           glm::normalize(glm::vec3(-.7f,0,1)), glm::normalize(glm::vec3(0,.85f,1)),
                           glm::normalize(glm::vec3(0,-.85f,1)), glm::vec3(0,0,-1)})
    {
        ++camera;
        frame.Camera.ToCamera = eye;
        frame.Camera.Position.z = eye.z < 0 ? -3.f : 3.f;
        frame.Camera.ViewProjection = glm::ortho(-2.f, 2.f, -1.f, 1.f, .1f, 10.f) *
            glm::lookAt(frame.Camera.Position, glm::vec3(0), glm::vec3(0, 1, 0));
        const auto position = render(frame, SurfaceView::WorldPosition);
        const glm::dvec3 ray(-eye.x, -eye.y, eye.z);
        for (float rough : {.045f, .37f, .73f, 1.f})
            for (float metallic : {0.f, 1.f})
            {
                material->Roughness = rough;
                material->Metallic = metallic;
                frame.ReflectionProbes.clear();
                const auto globalSpecular = render(frame, SurfaceView::SpecularIBL);
                const auto globalDiffuse = render(frame, SurfaceView::DiffuseIBL);
                frame.ReflectionProbes = probes;
                const auto result = render(frame, SurfaceView::PrefilteredIBL);
                const auto specular = render(frame, SurfaceView::SpecularIBL);
                const auto diffuse = render(frame, SurfaceView::DiffuseIBL);
                const auto energy = ReferenceSampleRuntimeBRDF(brdf, std::abs(eye.z), rough);
                const auto f0 = glm::mix(glm::dvec3(.04), glm::dvec3(material->BaseColor), double(metallic));
                const double lod = 2 * double(rough);
                const unsigned mip = unsigned(lod);
                size_t samples = 0;
                for (size_t pixel = 0; pixel < position->Color.size(); ++pixel)
                {
                    if (position->Entity[pixel] != surface.Picking || result->Entity[pixel] != surface.Picking)
                        throw std::runtime_error("Reflection probe changed picking or test coverage");
                    const glm::dvec3 p(position->Color[pixel]);
                    glm::dvec3 radiance(0);
                    double coverage = 0;
                    unsigned hits = 0;
                    bool valid = true;
                    for (unsigned i = 0; i < probes.size(); ++i)
                    {
                        const auto &probe = probes[i];
                        const glm::dvec3 center(probe.Position);
                        const auto margin = glm::dvec3(probe.InfluenceHalfSize) - glm::abs(p - center);
                        const auto t = std::clamp(std::min({margin.x, margin.y, margin.z}) / probe.BlendDistance, 0., 1.);
                        if (t <= 0)
                            continue;
                        const auto box = center + glm::dvec3(probe.ProjectionOffset);
                        const auto size = glm::dvec3(probe.ProjectionHalfSize);
                        const auto direction = IntersectBox(p, ray, box - size, box + size, center);
                        unsigned face;
                        double u, v;
                        if (!DirectionUV(direction, 1. / 16, face, u, v))
                        {
                            valid = false;
                            break;
                        }
                        reachedFaces |= 1u << face;
                        const double weight = (3 - 2 * t) * t * t;
                        radiance += weight * glm::mix(MeasureCube(i, mip, face, u, v),
                            MeasureCube(i, std::min(mip + 1, 2u), face, u, v), lod - mip);
                        coverage += weight;
                        ++hits;
                    }
                    if (!valid)
                        continue;
                    radiance = radiance / std::max(coverage, 1.) + glm::dvec3(.25) * std::max(1 - coverage, 0.);
                    if (coverage > 0)
                        overlap += hits == 2;
                    residual += coverage > 0 && coverage < 1;
                    for (unsigned c = 0; c < 3; ++c)
                    {
                        const double radianceError = coverage > 0 ?
                            std::abs(result->Color[pixel][c] - radiance[c]) :
                            std::abs(result->Color[pixel][c] - .25);
                        const double specularError = std::abs(double(specular->Color[pixel][c]) -
                            globalSpecular->Color[pixel][c] - (radiance[c] - .25) * (f0[c] * energy.x + energy.y));
                        const double diffuseChange = std::abs(double(diffuse->Color[pixel][c]) - globalDiffuse->Color[pixel][c]);
                        maxRadiance = std::max(maxRadiance, radianceError);
                        maxEnergy = std::max(maxEnergy, specularError);
                        maxDiffuse = std::max(maxDiffuse, diffuseChange);
                        if (!std::isfinite(radianceError + specularError + diffuseChange) ||
                            radianceError > .002 || specularError > .002 || diffuseChange > 1e-7)
                            throw std::runtime_error("Reflection FP64 reference mismatch at camera " +
                                std::to_string(camera) + ", roughness " + std::to_string(rough) +
                                ", pixel " + std::to_string(pixel) + ": radiance " +
                                std::to_string(radianceError) + ", specular " + std::to_string(specularError));
                    }
                    ++samples;
                }
                compared += samples;
                reference << camera << ',' << rough << ',' << metallic << ',' << samples << ','
                          << maxRadiance << ',' << maxEnergy << ',' << maxDiffuse << '\n';
            }
    }
    if (compared < 50000 || overlap < 1000 || residual < 1000 || reachedFaces != 63u)
        throw std::runtime_error("Insufficient reflection orientation/overlap/fade coverage: " +
            std::to_string(compared) + "/" + std::to_string(overlap) + "/" + std::to_string(residual) +
            "/" + std::to_string(reachedFaces));

    std::cout << "REFLECTION_REFERENCE samples=" << compared << " overlap=" << overlap
              << " residual=" << residual << " faces=" << reachedFaces << " radiance=" << maxRadiance
              << " energy=" << maxEnergy << " diffuse=" << maxDiffuse << '\n';

    auto scene = CreateRef<Scene>();
    const auto sceneFile = Project::ResolveOwnedAsset(project->GetAssetRoot(), project->GetConfig().StartScene);
    SceneSerializer serializer(scene, project->GetAssetRoot(), false, assets);
    if (!serializer.DeserializeText(FileDocument::Read(sceneFile), false))
        throw std::runtime_error(serializer.Report().Error);
    scene->ValidateRendering();
    auto roundTrip = CreateRef<Scene>();
    SceneSerializer reader(roundTrip, project->GetAssetRoot(), false, assets);
    if (!reader.DeserializeText(serializer.SerializeText(), false) ||
        serializer.CaptureAuthoredState() != reader.CaptureAuthoredState())
        throw std::runtime_error("Reflection study did not survive authored round trip");
    const auto dependencies = AuditSpriteAssets(project->GetAssetRoot(), {project->GetConfig().StartScene});
    for (auto handle : roundTrip->GetAllEntitiesWith<ReflectionProbeComponent>())
    {
        const auto &probe = Entity(handle, roundTrip.get()).GetComponent<ReflectionProbeComponent>();
        for (const auto &dependency : AuditEnvironment(assets->Root(), probe.Environment))
            if (std::find(dependencies.begin(), dependencies.end(), dependency) == dependencies.end())
                throw std::runtime_error("Reflection environment absent from scene dependency closure");
    }

    target->Resize(640, 360);
    auto save = [&](const char *mode, unsigned pose, const Ref<const FramebufferReadback> &result) {
        std::ofstream file(output / (std::string(mode) + "-" + std::to_string(pose) + ".bin"), std::ios::binary);
        for (size_t i = 0; i < result->Color.size(); ++i)
        {
            for (unsigned c = 0; c < 4; ++c)
                if (!std::isfinite(result->Color[i][c]))
                    throw std::runtime_error("Nonfinite study reflection");
            file.write(reinterpret_cast<const char *>(&result->Color[i]), 16);
            file.write(reinterpret_cast<const char *>(&result->Entity[i]), 4);
            file.write(reinterpret_cast<const char *>(&result->Depth[i]), 4);
        }
        if (!file)
            throw std::runtime_error("Could not write reflection study capture");
    };
    if (!FileSystem::GetEnvironmentPath("HAZEL_REFLECTION_DIAGNOSTIC").empty())
    {
        auto control = CreateRef<Scene>();
        SceneSerializer controlReader(control, project->GetAssetRoot(), false, assets);
        if (!controlReader.DeserializeText(FileDocument::Read(Project::ResolveOwnedAsset(
                project->GetAssetRoot(), "Scenes/ReflectionGallery-Global.hazel")), false))
            throw std::runtime_error(controlReader.Report().Error);
        const auto controlProbes = control->GetAllEntitiesWith<ReflectionProbeComponent>();
        if (controlProbes.size() != MaxReflectionProbes)
            throw std::runtime_error("Reflection gallery requires two authored control probes");
        for (const auto handle : controlProbes)
        {
            auto &probe = Entity(handle, control.get()).GetComponent<ReflectionProbeComponent>();
            if (probe.Enabled)
                throw std::runtime_error("Global-only gallery enables a local probe");
            probe.Enabled = true;
        }
        if (reader.CaptureAuthoredState() != controlReader.CaptureAuthoredState())
            throw std::runtime_error("Gallery variants differ beyond probe enablement");
        for (const auto handle : controlProbes)
            Entity(handle, control.get()).GetComponent<ReflectionProbeComponent>().Enabled = false;

        auto camera = roundTrip->GetPrimaryCameraEntity();
        if (!camera)
            throw std::runtime_error("Gallery primary camera missing");
        const auto world = roundTrip->GetWorldTransform(camera);
        const auto &lens = camera.GetComponent<CameraComponent>().Camera;
        const glm::vec3 center(world[3]);
        const glm::vec3 focus = center - 10.f * glm::vec3(world[2]);
        const bool centerOnly = !FileSystem::GetEnvironmentPath("HAZEL_REFLECTION_GALLERY_CENTER").empty();
        const unsigned poses = centerOnly ? 1 : 33;
        for (unsigned pose = 0; pose < poses; ++pose)
        {
            const float x = centerOnly ? 0.f : 9.f * (float(pose) / float(poses - 1) - .5f);
            SceneCameraView viewCamera;
            viewCamera.Position = center + glm::vec3(x, 0, 0);
            viewCamera.View = pose == poses / 2 ? glm::inverse(world) :
                glm::lookAt(viewCamera.Position, focus + glm::vec3(.38f * x, 0, 0), glm::vec3(0, 1, 0));
            viewCamera.ToCamera = glm::vec3(glm::inverse(viewCamera.View)[2]);
            viewCamera.Projection = glm::perspective(lens.GetPerspectiveVerticalFOV(), 640.f / 360.f,
                lens.GetPerspectiveNearClip(), lens.GetPerspectiveFarClip());
            viewCamera.ViewProjection = viewCamera.Projection * viewCamera.View;
            auto globalOnly = SceneRenderer::Collect(*control, viewCamera, {640, 360});
            auto local = SceneRenderer::Collect(*roundTrip, viewCamera, {640, 360});
            if (!globalOnly.ReflectionProbes.empty() || local.ReflectionProbes.size() != MaxReflectionProbes)
                throw std::runtime_error("Unexpected gallery probe admission");
            const auto a = render(globalOnly, SurfaceView::Shaded);
            const auto b = render(local, SurfaceView::Shaded);
            if (a->Entity != b->Entity || a->Depth != b->Depth)
                throw std::runtime_error("Gallery changed coverage between matched variants");
            save("global", pose, a);
            save("local", pose, b);
            if (pose == poses / 2)
            {
                if (metal && std::any_of(local.ReflectionProbes.begin(), local.ReflectionProbes.end(),
                                        [](const auto &probe) { return probe.Environment->SceneCapture; }))
                {
                    auto temporal = local;
                    temporal.Presentation.TemporalAA = true;
                    temporal.Presentation.AmbientOcclusion.Enabled = true;
                    render(temporal, SurfaceView::Shaded);
                    save("temporal-ao", pose, render(temporal, SurfaceView::Shaded));
                    if (view.Temporal.Phase != 2)
                        throw std::runtime_error("Baked probes did not accumulate with AO/TAA enabled");
                    temporal.ReflectionProbes.pop_back();
                    render(temporal, SurfaceView::Shaded);
                    if (view.Temporal.Phase != 1)
                        throw std::runtime_error("Changing baked probes did not invalidate temporal history");
                    view.Reset();
                    std::cout << "REFLECTION_CAPTURED_TEMPORAL AO/TAA=pass invalidation=pass\n";
                }
                // An oversized proxy approaches a distant environment lookup
                // while keeping each capture and its influence weights intact.
                for (auto &probe : local.ReflectionProbes)
                {
                    probe.ProjectionOffset = glm::vec3(0);
                    probe.ProjectionHalfSize = glm::vec3(10000.f);
                }
                save("distant", pose, render(local, SurfaceView::Shaded));
            }
        }
        std::cout << "REFLECTION_GALLERY poses=" << poses
                  << " matchedAuthoring=pass coverage=pass exposure=" << roundTrip->Presentation().Exposure << '\n';
        return;
    }

    std::vector<SceneSubmission> motionSequence;
    std::vector<Ref<const FramebufferReadback>> settled;
    for (unsigned pose = 0; pose < 9; ++pose)
    {
        const float x = -3.2f + .8f * float(pose);
        SceneCameraView cameraView;
        cameraView.Position = {x, 2.6f, 7.5f};
        const glm::vec3 look(.35f * x, 1.f, -.6f);
        cameraView.ToCamera = glm::normalize(cameraView.Position - look);
        cameraView.View = glm::lookAt(cameraView.Position, look, glm::vec3(0, 1, 0));
        cameraView.Projection = glm::perspective(glm::radians(50.f), 640.f / 360.f, .1f, 50.f);
        cameraView.ViewProjection = cameraView.Projection * cameraView.View;
        auto study = SceneRenderer::Collect(*roundTrip, cameraView, {640, 360});
        if (study.ReflectionProbes.size() != 2 || study.ReflectionProbes[0].Identity >= study.ReflectionProbes[1].Identity)
            throw std::runtime_error("Authored study did not collect two sorted probes");
        const auto active = study.ReflectionProbes;
        study.ReflectionProbes.clear();
        save("global", pose, render(study, SurfaceView::Shaded));
        study.ReflectionProbes = active;
        save("local", pose, render(study, SurfaceView::Shaded));
        if (metal)
        {
            study.Presentation.TemporalAA = true;
            study.Presentation.AmbientOcclusion.Enabled = true;
            study.SceneIdentity = 600;
            motionSequence.push_back(study);
            render(study, SurfaceView::Shaded);
            render(study, SurfaceView::Shaded);
            if (view.Temporal.Phase != 2)
                throw std::runtime_error("AO/TAA did not accumulate with static probes");
            settled.push_back(render(study, SurfaceView::Shaded));
            save("temporal-ao", pose, settled.back());
            study.ReflectionProbes.resize(1);
            render(study, SurfaceView::Shaded);
            if (view.Temporal.Phase != 1)
                throw std::runtime_error("Probe disable did not reset temporal history");
            study.ReflectionProbes = active;
            render(study, SurfaceView::Shaded);
            if (view.Temporal.Phase != 1)
                throw std::runtime_error("Probe enable did not reset temporal history");
            ++study.SceneIdentity;
            target->Resize(641, 361);
            render(study, SurfaceView::Shaded);
            if (view.Temporal.Phase != 1 || view.Temporal.Width != 641 || view.Temporal.Height != 361)
                throw std::runtime_error("Probe view did not recover after resize and scene replacement");
            target->Resize(640, 360);
        }
    }
    view.Reset();
    if (metal)
    {
        float worstP99 = 0;
        for (unsigned pose = 0; pose < motionSequence.size(); ++pose)
        {
            const auto moving = render(motionSequence[pose], SurfaceView::Shaded);
            if (view.Temporal.Phase != pose + 1)
                throw std::runtime_error("Continuous reflection camera motion reset TAA history");
            std::vector<float> errors;
            const auto &stationary = *settled[pose];
            for (size_t p = 0; p < moving->Color.size(); ++p)
                if (moving->Entity[p] >= 0 && moving->Entity[p] == stationary.Entity[p])
                {
                    const auto delta = glm::abs(glm::vec3(moving->Color[p] - stationary.Color[p]));
                    errors.push_back(std::max({delta.x, delta.y, delta.z}));
                }
            if (errors.size() < 50000)
                throw std::runtime_error("Insufficient same-surface pixels in moving AO/TAA study");
            const auto percentile = errors.begin() + errors.size() * 99 / 100;
            std::nth_element(errors.begin(), percentile, errors.end());
            worstP99 = std::max(worstP99, *percentile);
            if (*percentile > .1f)
                throw std::runtime_error("Moving AO/TAA study differs excessively from its stationary counterpart");
        }
        std::cout << "REFLECTION_MOTION poses=9 retainedTAA=pass stationaryVsMovingP99=" << worstP99 << '\n';
    }
    view.Reset();
    std::cout << "REFLECTION_STUDY poses=9 serialization=pass dependencies=pass" << (metal ? " AO/TAA=pass" : "") << '\n';
}
