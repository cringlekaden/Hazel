#include "hzpch.h"
#include "Renderer3D.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/Drawing/MeshGeometry.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Resources/Shader.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"
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
struct alignas(16) EnvironmentData
{
    glm::mat4 InverseVP;
    glm::vec4 RotationGain, Options;
};
static_assert(sizeof(EnvironmentData) == 96, "std140 environment layout");
Ref<Shader> shader, skyShader;
Ref<UniformBuffer> environmentBuffer;
Ref<VertexArray> skyTriangle;
Ref<EnvironmentResources> emptyEnvironment;
Ref<UniformBuffer> cameraBuffer, objectBuffer, materialBuffer;
Ref<Texture2D> white;
void EnsureResources()
{
    if (shader)
        return;
    const auto &caps = RenderCommand::GetCapabilities();
    if (caps.MaxTextureSlots < 8 || caps.MaxUniformBindings < 7 ||
        caps.MaxUniformBlockBytes < 144 || caps.MaxVertexAttributes < 5)
        throw std::runtime_error("PBR/IBL needs eight texture inputs, five vertex attributes, seven "
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
    auto sky = Shader::Create((Resources::Get().Root / "shaders/EnvironmentSky.glsl").generic_u8string());
    auto env = UniformBuffer::Create(sizeof(EnvironmentData), 6);
    auto triangle = VertexArray::Create();
    uint32_t indices[] = {0, 1, 2};
    triangle->SetIndexBuffer(IndexBuffer::Create(indices, 3));
    PreparedEnvironment defaults;
    unsigned kind = 0;
    for (auto image : {&defaults.Background, &defaults.Irradiance, &defaults.Specular, &defaults.BRDF})
    {
        image->Kind = LightingImageKind(++kind);
        image->Side = 1;
        image->Mips = {std::vector<float>(image->Channels() * image->Faces(), 0)};
    }
    auto complete = CreateRef<EnvironmentResources>(defaults);
    skyShader = std::move(sky);
    environmentBuffer = std::move(env);
    skyTriangle = std::move(triangle);
    emptyEnvironment = std::move(complete);
    shader = std::move(candidate);
    cameraBuffer = std::move(c);
    objectBuffer = std::move(o);
    materialBuffer = std::move(m);
    white = std::move(w);
}
} // namespace
void Renderer3D::Shutdown()
{
    emptyEnvironment.reset();
    skyTriangle.reset();
    environmentBuffer.reset();
    skyShader.reset();
    white.reset();
    materialBuffer.reset();
    objectBuffer.reset();
    cameraBuffer.reset();
    shader.reset();
}
void Renderer3D::Render(const SceneSubmission &frame, bool drawBackground)
{
    auto raster = RenderCommand::GetRasterState();
    Ref<EnvironmentResources> lighting;
    if (!frame.Meshes.empty() || frame.Environment)
    {
        EnsureResources();
        lighting = frame.Environment ? frame.Assets->RealizeEnvironment(frame.Environment) : emptyEnvironment;
        const auto &instance = frame.Presentation.Environment;
        EnvironmentData env{glm::inverse(frame.Camera.ViewProjection),
            {std::cos(instance.Yaw), std::sin(instance.Yaw), frame.Environment ? instance.Gain : 0,
             frame.Environment ? float(frame.Environment->Specular.Mips.size() - 1) : 0},
            {frame.Environment && instance.Lighting ? 1.f : 0.f, 0, 0, 0}};
        environmentBuffer->Bind();
        environmentBuffer->SetData(&env, sizeof(env));
        if (drawBackground && frame.Environment && instance.Background)
        {
            auto skyState = raster;
            skyState.DepthTest = false;
            skyState.DepthWrite = false;
            skyState.Blend = false;
            skyState.Cull = CullFace::None;
            skyState.PickingMask = glm::bvec4(false);
            RenderCommand::SetRasterState(skyState);
            lighting->Background->Bind(5);
            skyShader->Bind();
            skyShader->SetInt("u_Sky", 5);
            RenderCommand::DrawIndexed(skyTriangle, 3);
            RenderCommand::SetRasterState(raster);
        }
        lighting->Irradiance->Bind(5);
        lighting->Specular->Bind(6);
        lighting->BRDF->Bind(7);
    }
    if (!frame.Meshes.empty())
    {
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
        shader->SetInt("u_Irradiance", 5);
        shader->SetInt("u_Specular", 6);
        shader->SetInt("u_BRDF", 7);
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
}
} // namespace Hazel
