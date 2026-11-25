#include "hzpch.h"
#include "Renderer3D.h"
#include "Renderer3DUniforms.h"
#include <chrono>
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/Drawing/MeshGeometry.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Resources/Shader.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#endif
namespace Hazel
{
namespace
{
using namespace Renderer3DUniforms;
Ref<Shader> shader, skyShader, depthShader, motionShader;
Ref<UniformBuffer> motionBuffer;
Ref<UniformBuffer> environmentBuffer;
Ref<VertexArray> skyTriangle;
Ref<EnvironmentResources> emptyEnvironment;
Ref<UniformBuffer> cameraBuffer, objectBuffer, materialBuffer, localLightBuffer;
Ref<Texture2D> white;
void Retain(const std::shared_ptr<void>& resource)
{
#ifdef HZ_PLATFORM_MACOS
    if (RendererAPI::GetAPI() == RendererAPI::API::Metal)
        static_cast<MetalRendererAPI&>(RenderCommand::Backend()).RetainResource(resource);
#else
    (void)resource;
#endif
}
void EnsureResources()
{
    if (shader)
        return;
    const auto &caps = RenderCommand::GetCapabilities();
    if (caps.MaxTextureSlots < 8 || caps.MaxUniformBindings < 8 ||
        caps.MaxUniformBlockBytes < sizeof(LocalLightData) || caps.MaxVertexAttributes < 5)
        throw std::runtime_error("PBR/IBL needs eight texture inputs, five vertex attributes, eight "
                                 "uniform bindings and a 15376-byte uniform block");
    auto candidate = Shader::Create(
        (Hazel::Resources::Get().Root / "shaders/ForwardPBR.glsl").generic_u8string());
    auto c = UniformBuffer::Create(sizeof(ViewData), 2),
         o = UniformBuffer::Create(sizeof(ObjectData), 3),
         m = UniformBuffer::Create(sizeof(MaterialData), 4),
         locals = UniformBuffer::Create(sizeof(LocalLightData), 7);
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
    // EnvironmentResources supplies RuntimeBRDF_v2 even without an environment.
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
    localLightBuffer = std::move(locals);
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
    localLightBuffer.reset();
    objectBuffer.reset();
    cameraBuffer.reset();
    shader.reset();
    depthShader.reset();
    motionShader.reset();
    motionBuffer.reset();
}
void Renderer3D::Render(const SceneSubmission &frame, bool drawBackground, bool depthOnly, bool motionOnly)
{
    auto raster = RenderCommand::GetRasterState();
    if(depthOnly && !depthShader)
        depthShader=Shader::Create((Resources::Get().Root / "shaders/DepthPBR.glsl").generic_u8string());
    if(motionOnly && !motionShader) {
        motionShader=Shader::Create((Resources::Get().Root / "shaders/MotionPBR.glsl").generic_u8string());
        motionBuffer=UniformBuffer::Create(80,5);
    }
    Ref<EnvironmentResources> lighting;
    if (!frame.Meshes.empty() || frame.Environment) EnsureResources();
    if (!depthOnly && !motionOnly && (!frame.Meshes.empty() || frame.Environment))
    {
        lighting = frame.Environment ? frame.Assets->RealizeEnvironment(frame.Environment) : emptyEnvironment;
        Retain(lighting);
        const auto &instance = frame.Presentation.Environment;
        EnvironmentData env{glm::inverse(frame.Camera.ViewProjection),
            {std::cos(instance.Yaw), std::sin(instance.Yaw), frame.Environment ? instance.Gain : 0,
             frame.Environment ? float(frame.Environment->Specular.Mips.size() - 1) : 0},
            {frame.Environment && instance.Lighting ? 1.f : 0.f,
             !depthOnly && RendererAPI::GetAPI()==RendererAPI::API::Metal && frame.Presentation.AmbientOcclusion.Enabled ? frame.Presentation.AmbientOcclusion.Strength : 0.f,
             !depthOnly && frame.Presentation.AmbientOcclusion.Enabled ? float(frame.AmbientOcclusionView) : 0.f, 0}};
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
                        glm::vec4(frame.Camera.ToCamera, int(frame.Debug) + (frame.LinearLightingInspection ? 16 : 0)),
                        glm::vec4(frame.Light ? frame.Light->ToLight : glm::vec3(0, 0, 1), 0),
                        glm::vec4(frame.Light ? frame.Light->Incident : glm::vec3(0), 0)};
        cameraBuffer->SetData(&camera, sizeof(camera));
        if (!depthOnly && !motionOnly) {
            if (frame.LocalLights.size() > MaxLocalLights)
                throw std::runtime_error("Submission exceeds 320 local lights; no lights were truncated");
            const auto uploadStart=std::chrono::steady_clock::now();
            LocalLightData locals;
            locals.Meta.x = float(frame.LocalLights.size());
            locals.Meta.y = float(frame.LocalLightingView);
            if(frame.LocalLightTileSize!=16 && frame.LocalLightTileSize!=32)throw std::runtime_error("Local light tile size requires 16 or 32");
            locals.Meta.z = !frame.LocalLightCulling?1.f:frame.ForceLocalLightCulling?-1.f:0.f;
            locals.Meta.w = float(frame.LocalLightTileSize);
            for (size_t i=0;i<frame.LocalLights.size();++i) {
                const auto &light=frame.LocalLights[i];
                PointLight validation;validation.Range=light.Range;validation.Validate();
                for (int axis=0;axis<3;++axis)
                    if (!std::isfinite(light.Position[axis]) || !std::isfinite(light.Incident[axis]) ||
                        light.Incident[axis]<0 || light.Incident[axis]>1000)
                        throw std::runtime_error("Invalid submitted local light position/radiance");
                if (!std::isfinite(light.InnerCos) || !std::isfinite(light.OuterCos) ||
                    light.InnerCos>1 || light.OuterCos < -1 ||
                    (light.OuterCos!=-1 && (light.OuterCos<0 || light.InnerCos<=light.OuterCos ||
                    !std::isfinite(glm::length(light.Direction)) || std::abs(glm::length(light.Direction)-1)>1e-4f)))
                    throw std::runtime_error("Invalid submitted spotlight direction/cones");
                locals.Lights[i*3]=glm::vec4(light.Position,light.Range);
                locals.Lights[i*3+1]=glm::vec4(light.Incident,light.InnerCos);
                locals.Lights[i*3+2]=glm::vec4(light.Direction,light.OuterCos);
            }
            localLightBuffer->Bind();localLightBuffer->SetData(&locals,sizeof(locals));
#ifdef HZ_PLATFORM_MACOS
            if(RendererAPI::GetAPI()==RendererAPI::API::Metal)
                static_cast<MetalRendererAPI&>(RenderCommand::Backend()).CaptureLocalLights(frame.LocalMetrics);
#endif
            if(frame.LocalMetrics) {
                frame.LocalMetrics->CollectionMilliseconds=frame.LightCollectionMilliseconds;
                frame.LocalMetrics->UploadMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-uploadStart).count();
                frame.LocalMetrics->Count=unsigned(frame.LocalLights.size());frame.LocalMetrics->LightBytes=sizeof(locals);
            }
        }
        auto activeShader=motionOnly?motionShader:depthOnly?depthShader:shader;
        activeShader->Bind();
        const char *names[]{"u_Base", "u_MetalRough", "u_Normal", "u_Occlusion", "u_Emissive"};
        for (int i = 0; i < 5; ++i)
            if((!depthOnly && !motionOnly) || i==0) activeShader->SetInt(names[i], i);
        if(!depthOnly && !motionOnly) {
            shader->SetInt("u_Irradiance", 5);
            shader->SetInt("u_Specular", 6);
            shader->SetInt("u_BRDF", 7);
        }
        for (auto &draw : frame.Meshes)
        {
            auto geometry = frame.Assets->RealizeMesh(draw.Geometry);
            Retain(geometry);
            if(motionOnly) {
                MotionData motion{frame.PreviousViewProjection*draw.PreviousWorld,glm::vec4(draw.PreviousValid?1.f:0.f)};
                motionBuffer->Bind();motionBuffer->SetData(&motion,sizeof(motion));
            }
            ObjectData object{draw.World, draw.Normal, {draw.Picking, draw.Handedness, 0, 0}};
            objectBuffer->SetData(&object, sizeof(object));
            for (size_t i = 0; i < draw.Geometry->Geometry.Primitives.size(); ++i)
            {
                auto &material = *draw.Materials[i];
                int maps = 0;
                for (unsigned role = 0; role < ((depthOnly || motionOnly) ? 1u : 5u); ++role)
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
                    Retain(texture);
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
