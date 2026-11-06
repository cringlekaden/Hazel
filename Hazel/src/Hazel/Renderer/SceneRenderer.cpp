#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/MeshGeometry.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/UniformBuffer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Scene.h"
#include <glm/gtc/matrix_inverse.hpp>
namespace Hazel
{
namespace
{
struct alignas(16) ViewData
{
    glm::mat4 VP;
    glm::vec4 Eye, ToCamera, LightDirection, Incident;
};
struct alignas(16) ObjectData
{
    glm::mat4 Model, Normal;
    glm::ivec4 Identity;
};
struct alignas(16) MaterialData
{
    glm::vec4 Base, Emissive, Surface, Options;
};
static_assert(sizeof(ViewData) == 128 && sizeof(ObjectData) == 144 && sizeof(MaterialData) == 64,
              "std140 direct forward layout");
Ref<Shader> shader;
Ref<UniformBuffer> cameraBuffer, objectBuffer, materialBuffer;
Ref<Texture2D> white;
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
void EnsureResources()
{
    if (shader)
        return;
    const auto &caps = RenderCommand::GetCapabilities();
    if (caps.MaxTextureSlots < 5 || caps.MaxUniformBindings < 6 ||
        caps.MaxUniformBlockBytes < 144 || caps.MaxVertexAttributes < 5)
        throw std::runtime_error("Direct PBR needs five texture inputs/vertex attributes, six "
                                 "uniform bindings and a 144-byte uniform block");
    auto candidate = Shader::Create(
        (Hazel::Resources::Get().Root / "shaders/DirectPBR.glsl").generic_u8string());
    auto c = UniformBuffer::Create(sizeof(ViewData), 2),
         o = UniformBuffer::Create(sizeof(ObjectData), 3),
         m = UniformBuffer::Create(sizeof(MaterialData), 4);
    auto spec = TextureSpecification::FileDefaults();
    spec.Width = spec.Height = 1;
    spec.Format = ImageFormat::RGBA8;
    spec.Encoding = TextureEncoding::Linear;
    spec.GenerateMips = false;
    spec.MinFilter = TextureFilter::Linear;
    auto w = Texture2D::Create(spec);
    uint32_t pixel = 0xffffffff;
    w->SetData(&pixel, 4);
    shader = std::move(candidate);
    cameraBuffer = std::move(c);
    objectBuffer = std::move(o);
    materialBuffer = std::move(m);
    white = std::move(w);
}
} // namespace
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
void SceneRenderer::Shutdown()
{
    white.reset();
    materialBuffer.reset();
    objectBuffer.reset();
    cameraBuffer.reset();
    shader.reset();
}
SceneSubmission SceneRenderer::Collect(Scene &scene, const SceneCameraView &camera)
{
    SceneSubmission frame;
    frame.Camera = camera;
    frame.Presentation = scene.Presentation();
    frame.Presentation.Validate();
    frame.Assets = scene.GetAssets();
    for (auto h : scene.GetAllEntitiesWith<DirectionalLightComponent>())
    {
        Entity e(h, &scene);
        auto &light = e.GetComponent<DirectionalLightComponent>();
        light.Validate();
        if (!light.Enabled)
            continue;
        if (frame.Light)
            throw std::runtime_error(
                "This direct-light checkpoint supports one enabled directional light");
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
void SceneRenderer::Render(const SceneSubmission &frame, SceneRenderView &view,
                           const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                           const std::function<void()> &screenUI)
{
    if (unsigned(frame.Debug) > 5)
        throw std::runtime_error("Unsupported surface inspection");
    RenderCommand::StateScope scope;
    auto raster = RenderCommand::GetRasterState();
    raster.ClipEnabled = false;
    raster.SRGB = false;
    raster.DepthTest = true;
    raster.DepthWrite = true;
    raster.Compare = DepthCompare::Less;
    raster.Cull = CullFace::None;
    raster.Clockwise = false;
    raster.Blend = true;
    raster.ColorMask = raster.PickingMask = glm::bvec4(true);
    RenderCommand::SetRasterState(raster);
    const bool hdr = frame.Presentation.SceneReferred && frame.Debug == SurfaceView::Shaded;
    if (hdr)
    {
        if (!display)
            throw std::runtime_error(
                "Scene-referred rendering requires an explicit host view target");
        auto size = display->GetSpecification();
        if (!view.Radiance || view.Radiance->GetSpecification().Width != size.Width ||
            view.Radiance->GetSpecification().Height != size.Height)
        {
            size.Samples = 1;
            size.Attachments = {FramebufferTextureFormat::RGBA32F,
                                FramebufferTextureFormat::RED_INTEGER,
                                FramebufferTextureFormat::Depth};
            view.Radiance = Framebuffer::Create(size);
        }
        view.Radiance->Bind();
        RenderCommand::SetClearColor(frame.Presentation.Background);
        RenderCommand::Clear();
        view.Radiance->ClearAttachment(1, -1);
    }
    else
        view.Radiance.reset();
    if (!frame.Meshes.empty())
    {
        EnsureResources();
        cameraBuffer->Bind();
        objectBuffer->Bind();
        materialBuffer->Bind();
        ViewData camera{frame.Camera.ViewProjection,
                        glm::vec4(frame.Camera.Position, frame.Camera.Orthographic ? 1 : 0),
                        glm::vec4(frame.Camera.ToCamera, int(frame.Debug)),
                        glm::vec4(frame.Light ? frame.Light->ToLight : glm::vec3(0, 0, 1), 0),
                        glm::vec4(frame.Light ? frame.Light->Incident : glm::vec3(0), 0)};
        cameraBuffer->SetData(&camera, sizeof(camera));
        shader->Bind();
        const char *names[]{"u_Base", "u_MetalRough", "u_Normal", "u_Occlusion", "u_Emissive"};
        for (int i = 0; i < 5; ++i)
            shader->SetInt(names[i], i);
        for (auto &draw : frame.Meshes)
        {
            auto geometry = frame.Assets->RealizeMesh(draw.Geometry);
            ObjectData object{draw.World, draw.Normal, {draw.Picking, draw.Handedness, 0, 0}};
            objectBuffer->SetData(&object, sizeof(object));
            for (size_t i = 0; i < draw.Geometry->Geometry.Primitives.size(); ++i)
            {
                auto &material = *draw.Materials[i];
                int maps = 0;
                for (unsigned role = 0; role < 5; ++role)
                {
                    auto &image = material.Textures[role];
                    auto texture = white;
                    if (!image.Image.empty())
                    {
                        texture = frame.Assets->Texture(
                            image.Image,
                            MaterialTextureSpecification(MaterialTexture(role), image));
                        maps |= 1 << role;
                    }
                    texture->Bind(role);
                }
                MaterialData values{material.BaseColor,
                                    glm::vec4(material.Emissive, material.AlphaCutoff),
                                    {material.Metallic, material.Roughness, material.NormalScale,
                                     material.OcclusionStrength},
                                    {float(material.Model == MaterialModel::Unlit),
                                     float(material.Alpha == MaterialAlpha::Mask), float(maps),
                                     float(material.DoubleSided)}};
                materialBuffer->SetData(&values, sizeof(values));
                auto state = RenderCommand::GetRasterState();
                state.DepthTest = true;
                state.DepthWrite = true;
                state.Compare = DepthCompare::Less;
                state.Blend = false;
                state.Cull = material.DoubleSided ? CullFace::None : CullFace::Back;
                state.Clockwise = draw.Handedness < 0;
                RenderCommand::SetRasterState(state);
                auto &range = draw.Geometry->Geometry.Primitives[i];
                RenderCommand::DrawIndexed(geometry->Vertices(), range.IndexCount,
                                           range.FirstIndex);
            }
        }
    }
    raster = RenderCommand::GetRasterState();
    raster.Cull = CullFace::None;
    raster.Clockwise = false;
    raster.Blend = true;
    RenderCommand::SetRasterState(raster);
    world2D();
    if (hdr)
    {
        view.Radiance->Unbind();
        display->Bind();
        view.Radiance->CompositeWorldTo(display, frame.Presentation.Exposure);
    }
    screenUI();
}
} // namespace Hazel
