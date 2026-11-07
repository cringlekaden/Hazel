#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Renderer/Scene/Lighting.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Scene.h"
#include <glm/gtc/matrix_inverse.hpp>
namespace Hazel
{
namespace
{
bool Visible(const MeshBounds &bounds, const glm::mat4 &clip)
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
                outside &= side * p[axis] > p.w;
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
SceneSubmission SceneRenderer::Collect(Scene &scene, const SceneCameraView &camera)
{
    SceneSubmission frame;
    frame.Camera = camera;
    frame.Presentation = scene.Presentation();
    frame.Presentation.Validate();
    frame.Assets = scene.GetAssets();
    if (!frame.Presentation.Environment.Asset.empty())
    {
        if (!frame.Assets)
            throw std::runtime_error("Image environment requires project assets");
        auto environment = frame.Assets->Environment(frame.Presentation.Environment.Asset);
        if (!environment.Accepted)
            throw std::runtime_error(environment.Error);
        frame.Environment = environment.Accepted;
    }
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
        if (Visible(draw.Geometry->Geometry.Bounds, camera.ViewProjection * draw.World))
            frame.Meshes.push_back(std::move(draw));
    }
    return frame;
}
}
