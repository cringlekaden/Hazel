#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Renderer/Scene/Lighting.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Scene.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <chrono>
#include <cmath>
#include <type_traits>
namespace Hazel
{
namespace
{
bool Visible(const MeshBounds &bounds, const glm::mat4 &clip, glm::vec2 guard)
{
    glm::vec4 corners[8];
    for (int i = 0; i < 8; ++i)
        corners[i] = clip * glm::vec4((i & 1) ? bounds.Max.x : bounds.Min.x,
                                      (i & 2) ? bounds.Max.y : bounds.Min.y,
                                      (i & 4) ? bounds.Max.z : bounds.Min.z, 1);
    for (int axis = 0; axis < 3; ++axis)
        for (int side : {-1, 1})
        {
            bool outside = true;
            for (auto &p : corners)
                outside &= side * p[axis] > p.w*(axis<2?1+guard[axis]:1);
            if (outside)
                return false;
        }
    return true;
}
}
void SubmittedMesh::SetWorld(const glm::mat4 &world)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(world[c][r]))
                throw std::runtime_error("Mesh world transform is nonfinite");
    auto determinant = glm::determinant(glm::mat3(world));
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f)
        throw std::runtime_error("Mesh world transform is singular");
    World = world;
    Normal = glm::mat4(glm::inverseTranspose(glm::mat3(world)));
    Handedness = determinant < 0 ? -1 : 1;
}
void SceneRenderer::ValidateMaterial(const MaterialDefinition &material,
                                     const ScenePresentation &presentation)
{
    if (material.Model == MaterialModel::MetallicRoughness && !presentation.SceneReferred)
        throw std::runtime_error(
            "Lit materials require Scene-referred presentation in Scene Settings");
}
SceneSubmission SceneRenderer::Collect(Scene &scene, const SceneCameraView &camera, glm::uvec2 physicalExtent,
                                       bool captureRadiance)
{
    SceneSubmission frame;
    frame.Camera = camera;
    frame.Camera.FocusSubjectDepth.reset();
    if (camera.DepthOfField.Enabled && camera.DepthOfField.TrackedSubject && !camera.Orthographic && !captureRadiance) {
        auto subject = scene.GetEntityByUUID(camera.DepthOfField.TrackedSubject);
        if (subject && scene.IsEntityValid(subject.GetUUID())) {
            const auto position = glm::vec3(scene.GetWorldTransform(subject)[3]);
            const float depth = glm::dot(position - camera.Position, -glm::normalize(camera.ToCamera));
            if (std::isfinite(depth) && depth > 0)
                frame.Camera.FocusSubjectDepth = depth;
        }
    }
    frame.SceneIdentity = scene.GetIdentity();
    frame.Presentation = scene.Presentation();
    frame.Presentation.Validate();
    frame.CaptureRadiance = captureRadiance;
    if (captureRadiance)
    {
        if (!frame.Presentation.SceneReferred)
            throw std::runtime_error("Probe capture requires scene-referred presentation");
        frame.Presentation.AmbientOcclusion.Enabled = false;
        frame.Presentation.TemporalAA = false;
        frame.Presentation.ScreenReflections.Enabled = false;
        frame.Presentation.Bloom.Enabled = false;
    }
    frame.Assets = scene.GetAssets();
    if (!frame.Presentation.Environment.Asset.empty())
    {
        if (!frame.Assets)
            throw std::runtime_error("Image environment requires project assets");
        auto environment = frame.Assets->Environment(frame.Presentation.Environment.Asset);
        if (!environment.Accepted)
            throw std::runtime_error(environment.Error);
        // A captured asset may also have been assigned as the global
        // environment. Exclude it from subsequent captures as well.
        if (!captureRadiance || !environment.Accepted->SceneCapture)
            frame.Environment = environment.Accepted;
    }
    if (!captureRadiance)
    for (auto h : scene.GetAllEntitiesWith<ReflectionProbeComponent>())
    {
        Entity entity(h, &scene);
        const auto &probe = entity.GetComponent<ReflectionProbeComponent>();
        probe.Validate();
        if (!probe.Enabled)
            continue;
        if (frame.ReflectionProbes.size() == MaxReflectionProbes)
            throw std::runtime_error("Only two enabled reflection probes are supported");
        if (!frame.Assets || !frame.Presentation.SceneReferred)
            throw std::runtime_error("Enabled reflection probes require scene-referred presentation and project assets");
        const glm::vec3 position(scene.GetWorldTransform(entity)[3]);
        probe.ValidatePosition(position);
        const auto environment = frame.Assets->Environment(probe.Environment);
        if (!environment.Accepted)
            throw std::runtime_error(environment.Error);
        ValidateReflectionProbeEnvironment(*environment.Accepted);
        frame.ReflectionProbes.push_back({uint64_t(entity.GetUUID()), position, probe.InfluenceHalfSize,
            probe.ProjectionOffset, probe.ProjectionHalfSize, probe.BlendDistance, environment.Accepted});
    }
    std::sort(frame.ReflectionProbes.begin(), frame.ReflectionProbes.end(),
              [](const auto &a, const auto &b) { return a.Identity < b.Identity; });
    for (auto h : scene.GetAllEntitiesWith<DirectionalLightComponent>())
    {
        Entity e(h, &scene);
        auto &light = e.GetComponent<DirectionalLightComponent>();
        light.Validate();
        if (!light.Enabled)
            continue;
        if (frame.Light)
            throw std::runtime_error(
                "Only one enabled directional light is supported");
        frame.Light = SubmittedDirectional{-DirectionalTravel(scene.GetWorldTransform(e)),
                                           light.Color * light.Intensity};
    }
    const auto localStart=std::chrono::steady_clock::now();
    auto collectLocal = [&](auto component) {
        using T = decltype(component);
        for (auto h : scene.GetAllEntitiesWith<T>()) {
            Entity e(h, &scene);
            const auto &light = e.GetComponent<T>();
            light.Validate();
            if (!light.Enabled) continue;
            const auto world = scene.GetWorldTransform(e);
            SubmittedLocalLight submitted;
            submitted.Identity = uint64_t(e.GetUUID());
            submitted.Position = LightPosition(world);
            submitted.Incident = light.Color * light.Intensity;
            submitted.Range = light.Range;
            submitted.CastShadows = light.CastShadows;
            submitted.ShadowResolution = light.ShadowResolution;
            if constexpr (std::is_same_v<T, SpotLightComponent>) {
                submitted.Direction = DirectionalTravel(world);
                submitted.InnerCos = std::cos(light.InnerCone);
                submitted.OuterCos = std::cos(light.OuterCone);
                submitted.ShadowUp = glm::vec3(world[1]);
            }
            frame.LocalLights.push_back(submitted);
        }
    };
    collectLocal(PointLightComponent{});
    collectLocal(SpotLightComponent{});
    if (frame.LocalLights.size() > MaxLocalLights)
        throw std::runtime_error("Scene exceeds 320 enabled local lights; no lights were truncated");
    std::sort(frame.LocalLights.begin(), frame.LocalLights.end(), [](const auto &a, const auto &b) {
        return a.Identity!=b.Identity?a.Identity<b.Identity:a.OuterCos<b.OuterCos;
    });
    frame.LightCollectionMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-localStart).count();
    const bool collectCasters = (frame.Presentation.Shadows.Enabled && frame.Light) ||
        std::any_of(frame.LocalLights.begin(), frame.LocalLights.end(), [](const auto& light) {
            return light.CastShadows && glm::any(glm::greaterThan(light.Incident, glm::vec3(0)));
        });
    for (auto h : scene.GetAllEntitiesWith<MeshRendererComponent>())
    {
        Entity e(h, &scene);
        auto &binding = e.GetComponent<MeshRendererComponent>();
        if (binding.Mesh.empty())
            continue;
        if (!frame.Assets)
            throw std::runtime_error("Mesh rendering requires project-owned assets");
        auto resolution = frame.Assets->Mesh(binding.Mesh);
        if (!resolution.Accepted)
            throw std::runtime_error(resolution.Error);
        SubmittedMesh draw;
        draw.Geometry = resolution.Accepted;
        draw.SetWorld(scene.GetWorldTransform(e));
        draw.Picking = int(h);
        draw.Identity = uint64_t(e.GetUUID());
        for (auto &p : draw.Geometry->Geometry.Primitives)
        {
            auto slot = std::find_if(draw.Geometry->Definition.Slots.begin(),
                                     draw.Geometry->Definition.Slots.end(),
                                     [&](auto &s) { return s.ID == p.Slot; });
            auto override = binding.Materials.find(p.Slot);
            auto material = frame.Assets->Material(
                override == binding.Materials.end() ? slot->Material : override->second);
            if (!material.Accepted)
                throw std::runtime_error(material.Error);
            ValidateMaterial(*material.Accepted, frame.Presentation);
            draw.Materials.push_back(material.Accepted);
        }
        // Half-pixel maximum jitter requires one pixel of total NDC guard.
        const auto guard=frame.Presentation.TemporalAA && physicalExtent.x && physicalExtent.y?
            glm::vec2(1)/glm::vec2(physicalExtent):glm::vec2(0);
        if (collectCasters) frame.ShadowCasters.push_back(draw);
        if (Visible(draw.Geometry->Geometry.Bounds, camera.ViewProjection * draw.World,guard))
            frame.Meshes.push_back(std::move(draw));
    }
    return frame;
}
}
