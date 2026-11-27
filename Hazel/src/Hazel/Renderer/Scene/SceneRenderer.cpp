#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <map>
#include <cmath>
#include <limits>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalFramebuffer.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#endif
namespace Hazel
{
namespace {
float Halton(unsigned index,unsigned base) {
    float value=0,fraction=1;
    while(index){fraction/=float(base);value+=fraction*float(index%base);index/=base;}return value;
}
bool ValidCamera(const glm::mat4& matrix) {
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)if(!std::isfinite(matrix[c][r]))return false;
    return std::isfinite(glm::determinant(matrix))&&std::abs(glm::determinant(matrix))>1e-12f;
}
bool CameraCut(const SceneSubmission& frame,const SceneCameraView& previous) {
    const auto& current=frame.Camera;
    if(current.Identity!=previous.Identity || current.Orthographic!=previous.Orthographic)return true;
    // Near-plane displacement exaggerates ordinary camera motion, especially
    // with a small near clip or a long orthographic depth range. Compare rays
    // at submitted mesh depth instead; disocclusion remains a per-pixel test.
    const auto forward=-glm::normalize(current.ToCamera);
    float referenceDepth=std::numeric_limits<float>::max(),fallbackDepth=referenceDepth;
    for(const auto& draw:frame.Meshes)if(draw.Geometry) {
        const auto& bounds=draw.Geometry->Geometry.Bounds;
        auto centre=glm::vec3(draw.World*glm::vec4(bounds.Min*.5f+bounds.Max*.5f,1));
        float depth=glm::dot(centre-current.Position,forward);
        if(std::isfinite(depth)&&depth>0)referenceDepth=std::min(referenceDepth,depth);
        else {
            float radius=glm::length(glm::vec3(draw.World*glm::vec4(bounds.Max*.5f-bounds.Min*.5f,0)));
            if(std::isfinite(radius)&&radius>0)fallbackDepth=std::min(fallbackDepth,radius);
        }
    }
    if(referenceDepth==std::numeric_limits<float>::max())referenceDepth=fallbackDepth==referenceDepth?1.f:fallbackDepth;
    const auto inverse=glm::inverse(current.ViewProjection);
    auto nearPoint=inverse*glm::vec4(0,0,-1,1);
    if(!std::isfinite(nearPoint.w)||nearPoint.w==0)return true;
    float nearDepth=glm::dot(glm::vec3(nearPoint)/nearPoint.w-current.Position,forward);
    if(std::isfinite(nearDepth))referenceDepth=std::max(referenceDepth,nearDepth);
    for(float x:{-.8f,.8f})for(float y:{-.8f,.8f}) {
        auto sample=inverse*glm::vec4(x,y,current.Orthographic?-1.f:0.f,1);
        if(!std::isfinite(sample.w)||sample.w==0)return true;
        auto ray=glm::vec3(sample)/sample.w-current.Position;
        float depth=glm::dot(ray,forward);
        if(!std::isfinite(depth)||(!current.Orthographic&&depth<=0))return true;
        auto point=current.Orthographic?current.Position+ray+forward*(referenceDepth-depth):current.Position+ray*(referenceDepth/depth);
        auto p=previous.ViewProjection*glm::vec4(point,1);
        if(!std::isfinite(p.w)||p.w<=0||glm::length(glm::vec2(p)/p.w-glm::vec2(x,y))>.5f)return true;
    }
    return false;
}
}

void SceneRenderer::Render(const SceneSubmission &input, SceneRenderView &view,
                           const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                           const std::function<void()> &screenUI)
{
#ifdef HZ_PLATFORM_MACOS
    if(RendererAPI::GetAPI()==RendererAPI::API::Metal) {
        auto device=static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device();
        if(view.NativeDevice.lock()!=device){view.Reset();view.NativeDevice=device;}
    } else if(!view.NativeDevice.expired()){view.Reset();view.NativeDevice.reset();}
#endif
    if(input.ShadowView>10)throw std::runtime_error("Unsupported directional shadow inspection");
    const bool shadows = input.Presentation.Shadows.Enabled && input.Light.has_value();
    SpotShadowLayout spotLayout;
    PointShadowLayout pointLayout;
    SceneSubmission shadowed;
    try {
        if (input.CaptureRadiance &&
            (!display || !input.Presentation.SceneReferred || input.Presentation.TemporalAA ||
             input.Presentation.AmbientOcclusion.Enabled || input.Presentation.ScreenReflections.Enabled ||
             !input.ReflectionProbes.empty() ||
             (input.Environment && input.Environment->SceneCapture) || input.Debug != SurfaceView::Shaded ||
             display->GetSpecification().Samples != 1 || display->GetSpecification().Attachments.Attachments.empty() ||
             display->GetSpecification().Attachments.Attachments[0].TextureFormat != FramebufferTextureFormat::RGBA32F))
            throw std::runtime_error("Probe capture requires a private single-sample FP32 radiance view");
        if (input.ReflectionProbes.size() > MaxReflectionProbes ||
            (!input.ReflectionProbes.empty() && (!input.Assets || !input.Presentation.SceneReferred)))
            throw std::runtime_error("Reflection probes require project assets, scene-referred presentation and at most two probes");
        view.ReflectionProbes = {};
        ViewReflectionProbes probes;
        if (!input.Meshes.empty())
            for (size_t i = 0; i < input.ReflectionProbes.size(); ++i)
                probes[i] = input.Assets->RealizeReflectionProbe(input.ReflectionProbes[i].Environment);
        view.ReflectionProbes = std::move(probes);
        if (input.LocalLights.size() > MaxLocalLights)
            throw std::runtime_error("Submission exceeds 320 local lights; no lights were truncated");
        spotLayout = FitSpotShadows(input);
        pointLayout = FitPointShadows(input);
        if (shadows || spotLayout.Count || pointLayout.Side) {
            if (!display) throw std::runtime_error("Shadows require an explicit view target");
            shadowed = input;
            if (shadows) {
                shadowed.Shadows = FitDirectionalShadows(input);
                shadowed.Shadows.Range.w = float(input.ShadowView);
            }
            shadowed.SpotShadows = spotLayout.Data;
            shadowed.PointShadows = pointLayout.Data;
        }
    } catch (...) {
        view.Temporal = {};
        if (display && RendererAPI::GetAPI() == RendererAPI::API::Metal) display->Cancel();
        throw;
    }
    const bool spotShadows = spotLayout.Count != 0;
    const bool pointShadows = pointLayout.Side != 0;
    const bool anyShadows = shadows || spotShadows || pointShadows;
    const auto& frame = anyShadows ? shadowed : input;
    if (!shadows) view.ShadowDepth.reset();
    if (!spotShadows) view.SpotShadowDepth.reset();
    if (!pointShadows) view.PointShadowDepth.reset();
    if (frame.LocalMetrics) {
        frame.LocalMetrics->PointShadows = pointShadows ? 1 : 0;
        frame.LocalMetrics->RequestedPointShadows = pointLayout.Requested;
        frame.LocalMetrics->PointShadowBytes = uint64_t(PointShadowFaces) * pointLayout.Side * pointLayout.Side * sizeof(float);
        frame.LocalMetrics->SpotShadows = spotLayout.Count;
        frame.LocalMetrics->RequestedSpotShadows = spotLayout.Requested;
        frame.LocalMetrics->SpotShadowBytes = uint64_t(spotLayout.Extent.x) * spotLayout.Extent.y * sizeof(float);
    }
    auto renderShadowAtlas = [&](Ref<Framebuffer>& atlas, glm::uvec2 extent, unsigned count, auto tile, bool localAtlas = false) {
        FramebufferSpecification size;
        size.Width = extent.x;
        size.Height = extent.y;
        size.Attachments = {FramebufferTextureFormat::DEPTH32F};
        if (!atlas || atlas->GetSpecification().Width != size.Width || atlas->GetSpecification().Height != size.Height)
            atlas = Framebuffer::Create(size);
        atlas->Bind();
        try {
            if (RendererAPI::GetAPI() == RendererAPI::API::Metal) atlas->Clear(glm::vec4(0));
            else {
                // GL clears respect the scissor and depth write mask. A
                // preceding atlas may have left its last tile's scissor active.
                auto clearRaster = RenderCommand::GetRasterState();
                clearRaster.ClipEnabled = false;
                clearRaster.DepthWrite = true;
                RenderCommand::SetRasterState(clearRaster);
                RenderCommand::SetClearColor(glm::vec4(0));
                RenderCommand::Clear();
            }
            SceneSubmission depth;
            depth.Assets = frame.Assets;
            for (unsigned index = 0; index < count; ++index) {
                glm::vec3 origin(0);
                const auto viewport = tile(index, depth.Camera.ViewProjection, origin);
                RenderCommand::SetViewport(viewport.x, viewport.y, viewport.z, viewport.w);
                if (localAtlas) {
                    auto tileRaster = RenderCommand::GetRasterState();
                    tileRaster.ClipEnabled = true;
                    tileRaster.ClipRect = glm::ivec4(viewport);
                    RenderCommand::SetRasterState(tileRaster);
                }
                depth.Meshes.clear();
                for (const auto& draw : frame.ShadowCasters) {
                    const auto& bounds = draw.Geometry->Geometry.Bounds;
                    auto model = draw.World;
                    if (localAtlas) model[3] -= glm::vec4(origin, 0);
                    if (ShadowCasterVisible(bounds.Min, bounds.Max, depth.Camera.ViewProjection * model)) {
                        depth.Meshes.push_back(draw);
                        depth.Meshes.back().World = model;
                    }
                }
                if (!depth.Meshes.empty()) Renderer3D::Render(depth, false, true);
            }
        } catch (...) {
            atlas->Unbind();
            throw;
        }
        atlas->Unbind();
    };
    auto renderShadows = [&] {
        if (!anyShadows) return;
        RenderCommand::StateScope restoreRaster;
        if (shadows) {
            const unsigned side = frame.Presentation.Shadows.Resolution;
            renderShadowAtlas(view.ShadowDepth, glm::uvec2(side * 2), 4, [&](unsigned index, glm::mat4& vp, glm::vec3&) {
                vp = frame.Shadows.VP[index];
                return glm::uvec4((index % 2) * side, (index / 2) * side, side, side);
            });
        }
        if (spotShadows) {
            renderShadowAtlas(view.SpotShadowDepth, spotLayout.Extent, spotLayout.Count, [&](unsigned index, glm::mat4& vp, glm::vec3& origin) {
                vp = frame.SpotShadows.VP[index];
                origin = frame.LocalLights[frame.SpotShadows.LightIndices[index] - 1].Position;
                const auto& tile = frame.SpotShadows.Tiles[index];
                return glm::uvec4(unsigned(tile.x), 0, unsigned(tile.y), unsigned(tile.y));
            }, true);
        }
        if (pointShadows) {
            const unsigned side = pointLayout.Side;
            renderShadowAtlas(view.PointShadowDepth, {3 * side, 2 * side}, PointShadowFaces,
                [&](unsigned face, glm::mat4& vp, glm::vec3& origin) {
                    vp = pointLayout.VP[face];
                    origin = frame.LocalLights[pointLayout.LightIndex].Position;
                    return glm::uvec4((face % 3) * side, (face / 3) * side, side, side);
                }, true);
        }
    };
    auto prepareShadowDepth = [&](const Ref<Framebuffer>& target) {
        if (shadows) target->PrepareShadowDepth(view.ShadowDepth, ShadowMapKind::Directional);
        if (spotShadows) target->PrepareShadowDepth(view.SpotShadowDepth, ShadowMapKind::Spot);
        if (pointShadows) target->PrepareShadowDepth(view.PointShadowDepth, ShadowMapKind::Point);
    };
    const bool temporal=frame.Presentation.TemporalAA && RendererAPI::GetAPI()==RendererAPI::API::Metal &&
        frame.Presentation.SceneReferred && frame.Debug==SurfaceView::Shaded && frame.AmbientOcclusionView==0 && frame.LocalLightingView==0 && frame.ShadowView==0;
    if(!temporal) {if(view.Temporal.Submitted)view.Radiance.reset();view.Temporal={};view.Jittered.reset();view.Motion.reset();view.TemporalMetrics->Valid=false;}
    if(frame.Presentation.TemporalAA && RendererAPI::GetAPI()!=RendererAPI::API::Metal && !view.WarnedUnsupportedTAA) {
        HZ_CORE_WARN("TAA request inactive: full-resolution temporal resolve requires Metal 4");view.WarnedUnsupportedTAA=true;
    }
    SceneSubmission sample,motion;
    glm::vec2 jitter(0);
    bool historyValid=false;
    std::shared_ptr<std::atomic<bool>> submitted;
    if(temporal) {
        sample=frame;motion=frame;
        if(!display || !ValidCamera(frame.Camera.ViewProjection) ||
            !std::isfinite(glm::dot(frame.Camera.Position,frame.Camera.Position)) ||
            !std::isfinite(glm::dot(frame.Camera.ToCamera,frame.Camera.ToCamera)) || glm::length(frame.Camera.ToCamera)<.5f) {view.Temporal={};throw std::runtime_error("TAA requires a finite invertible camera and explicit view target");}
        const auto& extent=display->GetSpecification();auto& state=view.Temporal;
        historyValid=state.Submitted && state.Submitted->load() && state.Width==extent.Width && state.Height==extent.Height &&
            state.Previous.SceneIdentity==frame.SceneIdentity && state.Previous.Assets==frame.Assets &&
            state.Previous.Presentation.SameTemporalInputs(frame.Presentation) &&
            state.Previous.ReflectionProbes==frame.ReflectionProbes && !CameraCut(frame,state.Previous.Camera);
        if (historyValid) {
            // A different admission or map resolution is an abrupt lighting edit.
            for (unsigned slot = 0; slot < MaxSpotShadows; ++slot) {
                int current = frame.SpotShadows.LightIndices[slot];
                int previous = state.Previous.SpotShadows.LightIndices[slot];
                if (bool(current) != bool(previous) || (current &&
                    (frame.LocalLights[current - 1].Identity != state.Previous.LocalLights[previous - 1].Identity ||
                     frame.SpotShadows.Tiles[slot].y != state.Previous.SpotShadows.Tiles[slot].y))) {
                    historyValid = false;
                    break;
                }
            }
        }
        if (historyValid) {
            const auto current = frame.PointShadows.Parameters;
            const auto previous = state.Previous.PointShadows.Parameters;
            historyValid = current.y == previous.y && (!current.x ||
                frame.LocalLights[unsigned(current.x) - 1].Identity ==
                    state.Previous.LocalLights[unsigned(previous.x) - 1].Identity);
        }
        if(!historyValid)state.Phase=0;
        jitter={Halton(state.Phase%64+1,2)-.5f,Halton(state.Phase%64+1,3)-.5f};
        glm::mat4 offset(1);offset[3][0]=2*jitter.x/float(extent.Width);offset[3][1]=2*jitter.y/float(extent.Height);
        sample.Camera.ViewProjection=offset*frame.Camera.ViewProjection;
        sample.TemporalPhase=state.Phase%64+1;
        motion.PreviousViewProjection=historyValid?state.Previous.Camera.ViewProjection:frame.Camera.ViewProjection;
        std::map<std::pair<uint64_t,int>,const SubmittedMesh*> previous;
        if(historyValid)for(const auto& old:state.Previous.Meshes)if(old.Identity)previous.emplace(std::make_pair(old.Identity,old.Picking),&old);
        for(auto& draw:motion.Meshes) {
            draw.PreviousValid=false;
            auto old=previous.find({draw.Identity,draw.Picking});
            if(old!=previous.end() && draw.Geometry==old->second->Geometry && draw.Materials==old->second->Materials) {
                draw.PreviousWorld=old->second->World;draw.PreviousValid=true;
            }
        }
        submitted=std::make_shared<std::atomic<bool>>(false);
    }
    if(frame.LocalLightingView>3)throw std::runtime_error("Unsupported local-light inspection");
    if(frame.LocalLightingView>1 && RendererAPI::GetAPI()!=RendererAPI::API::Metal)
        throw std::runtime_error("Tile inspection requires Metal 4; OpenGL evaluates the complete light loop");
    if(frame.AmbientOcclusionView>4)throw std::runtime_error("Unsupported ambient-occlusion inspection");
    if(!frame.Presentation.AmbientOcclusion.Enabled || frame.Meshes.empty())view.AmbientTiming->Valid=false;
    if (unsigned(frame.Debug) > unsigned(SurfaceView::NormalHorizon))
        throw std::runtime_error("Unsupported surface inspection");
    if (frame.LinearLightingInspection && (frame.Debug < SurfaceView::Direct || frame.Debug > SurfaceView::NormalHorizon))
        throw std::runtime_error("Linear lighting inspection requires a lighting-term view and a float target");
    if (frame.LinearLightingInspection && (!display || display->GetSpecification().Attachments.Attachments.empty() ||
        display->GetSpecification().Attachments.Attachments[0].TextureFormat != FramebufferTextureFormat::RGBA32F))
        throw std::runtime_error("Linear lighting inspection requires an explicit FP32 color target");
    const auto& bloom = frame.Presentation.Bloom;
    const bool useBloom = bloom.Enabled && bloom.Intensity > 0 && frame.Presentation.SceneReferred &&
        !frame.CaptureRadiance && frame.Debug == SurfaceView::Shaded && frame.AmbientOcclusionView == 0 &&
        frame.LocalLightingView == 0 && frame.ShadowView == 0 && view.TemporalDebug == 0;
    if (!useBloom) {
        view.Bloom = {};
        if (view.BloomMetrics)
            *view.BloomMetrics = {};
    }
    auto presentWorld = [&] {
        const auto lens = frame.Camera.Physical.value_or(PhysicalCamera{});
        lens.Validate();
        const float gain = lens.Log2ExposureGain(frame.Presentation.Exposure);
        if (useBloom) {
            bloom.Validate();
            const auto& size = view.Radiance->GetSpecification();
            view.Bloom.Timing = view.BloomMetrics;
            view.Bloom.Prepare(size.Width, size.Height, bloom.Intensity, bloom.Radius);
        }
        view.Radiance->CompositeWorldTo(display, gain, useBloom ? &view.Bloom : nullptr);
    };
    const bool reflections = frame.Presentation.ScreenReflections.Enabled && frame.Presentation.SceneReferred &&
        !frame.CaptureRadiance && frame.Debug==SurfaceView::Shaded && frame.AmbientOcclusionView==0 &&
        frame.ShadowView==0 && frame.LocalLightingView==0 && !frame.Meshes.empty();
    if (!reflections)
        view.ReflectionSurface.reset();
    auto prepareReflectionSurface = [&](FramebufferSpecification size) {
        size.Samples=1;
        size.Attachments.Attachments.insert(size.Attachments.Attachments.end(), {
            FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::RGBA32F});
        if (!view.ReflectionSurface)
            view.ReflectionSurface=Framebuffer::Create(size);
        else if (view.ReflectionSurface->GetSpecification().Width!=size.Width ||
                 view.ReflectionSurface->GetSpecification().Height!=size.Height)
            view.ReflectionSurface->Resize(size.Width,size.Height);
    };
    auto reflectionParameters = [&](const SceneCameraView& camera) {
        const auto& settings=frame.Presentation.ScreenReflections;
        settings.Validate();
        if (!ValidCamera(camera.View) || !ValidCamera(camera.ViewProjection))
            throw std::runtime_error("SSR requires finite invertible view and view-projection matrices");
        // Camera entity transforms can inherit scale. Use a Euclidean
        // frame for reflection directions and keep the exact raster VP.
        const glm::mat4 cameraWorld=glm::inverse(camera.View);
        const glm::vec3 back=glm::normalize(glm::vec3(cameraWorld[2]));
        const glm::vec3 right=glm::normalize(glm::cross(glm::vec3(cameraWorld[1]),back));
        const glm::vec3 up=glm::cross(back,right);
        const glm::mat4 rigidWorld(glm::vec4(right,0),glm::vec4(up,0),glm::vec4(back,0),cameraWorld[3]);
        const glm::mat4 projection=camera.ViewProjection*rigidWorld;
        const glm::mat4 inverse=glm::inverse(projection);
        const glm::vec4 nearPoint=inverse*glm::vec4(0,0,-1,1);
        const float nearPlane=-nearPoint.z/nearPoint.w;
        if (!std::isfinite(nearPlane) || (!camera.Orthographic && nearPlane<=0))
            throw std::runtime_error("SSR requires a finite camera near plane");
        const float stride[]{6,4,2};
        return ScreenReflectionResolve{projection,inverse,glm::inverse(rigidWorld),
            {settings.Distance,settings.Thickness,float(settings.Steps),stride[settings.Quality]},
            {nearPlane,camera.Orthographic?1.f:0.f,0,0}};
    };
    if (RendererAPI::GetAPI() == RendererAPI::API::Metal)
    {
        if (!display)
            throw std::runtime_error("Metal scene requires an explicit view target");
        RenderCommand::StateScope scope;
        auto raster = RenderCommand::GetRasterState();
        raster.DepthTest = raster.DepthWrite = raster.Blend = true;
        raster.Compare = DepthCompare::Less;
        raster.ColorMask = raster.PickingMask = glm::bvec4(true);
        raster.ClipEnabled = false; raster.Cull = CullFace::None; raster.Clockwise = false;
        RenderCommand::SetRasterState(raster);
        try
        {
            const bool ao=frame.Presentation.AmbientOcclusion.Enabled && !frame.Meshes.empty();
            if(anyShadows || ao)display->Unbind();
            renderShadows();
            if(ao) {
                auto size=display->GetSpecification();
                size.Attachments={FramebufferTextureFormat::DEPTH32F};
                if(!view.AmbientDepth)view.AmbientDepth=Framebuffer::Create(size);
                else if(view.AmbientDepth->GetSpecification().Width!=size.Width || view.AmbientDepth->GetSpecification().Height!=size.Height)
                    view.AmbientDepth->Resize(size.Width,size.Height);
                view.AmbientDepth->Bind();view.AmbientDepth->Clear(glm::vec4(0));
                Renderer3D::Render(temporal?sample:frame,false,true);view.AmbientDepth->Unbind();
            } else view.AmbientDepth.reset();
            auto prepareLighting=[&](const Ref<Framebuffer>& target) {
#ifdef HZ_PLATFORM_MACOS
                prepareShadowDepth(target);
                if(ao) std::static_pointer_cast<MetalFramebuffer>(target)->PrepareAmbientOcclusion(view.AmbientDepth,temporal?sample.Camera:frame.Camera,frame.Presentation.AmbientOcclusion,frame.AmbientOcclusionView,view.AmbientTiming,sample.TemporalPhase);
#else
                (void)target;
#endif
            };
            if (!frame.CaptureRadiance && frame.Presentation.SceneReferred && frame.Debug == SurfaceView::Shaded && (!ao || frame.AmbientOcclusionView==0))
            {
                auto size = display->GetSpecification();
                if (!view.Radiance) view.Radiance = Framebuffer::Create(size);
                else if (view.Radiance->GetSpecification().Width != size.Width || view.Radiance->GetSpecification().Height != size.Height)
                    view.Radiance->Resize(size.Width, size.Height);
#ifdef HZ_PLATFORM_MACOS
                std::static_pointer_cast<MetalFramebuffer>(view.Radiance)->SetWorldProducer();
#endif
                display->Cancel();
                auto drawOpaque = [&](const Ref<Framebuffer>& target, const SceneSubmission& submission) {
                    auto surface=target;
                    if (reflections) {
                        prepareReflectionSurface(size);
                        surface=view.ReflectionSurface;
#ifdef HZ_PLATFORM_MACOS
                        std::static_pointer_cast<MetalFramebuffer>(surface)->SetWorldProducer();
#endif
                    }
                    surface->Bind();
                    surface->Clear(frame.Presentation.Background);
                    prepareLighting(surface);
                    Renderer3D::Render(submission,true,false,false,&view.ReflectionProbes,reflections);
                    if (reflections) {
                        surface->Unbind();
                        target->Bind();
                        surface->ResolveReflectionsTo(target,reflectionParameters(submission.Camera));
                    }
                };
                if(temporal) {
                    auto ensure=[&](Ref<Framebuffer>& target) {
                        if(!target)target=Framebuffer::Create(size);
                        else if(target->GetSpecification().Width!=size.Width || target->GetSpecification().Height!=size.Height)target->Resize(size.Width,size.Height);
                    };
                    ensure(view.Jittered);ensure(view.Motion);
#ifdef HZ_PLATFORM_MACOS
                    std::static_pointer_cast<MetalFramebuffer>(view.Jittered)->SetTemporalProducer();
                    std::static_pointer_cast<MetalFramebuffer>(view.Motion)->SetTemporalProducer();
#endif
                    drawOpaque(view.Jittered,sample);view.Jittered->Unbind();
                    view.Motion->Bind();view.Motion->Clear(glm::vec4(0));Renderer3D::Render(motion,false,false,true);view.Motion->Unbind();
                    view.Radiance->Bind();
#ifdef HZ_PLATFORM_MACOS
                    glm::mat4 direction(1);direction[3]=glm::vec4(-frame.Camera.Position,0);
                    auto skyMotion=motion.PreviousViewProjection*direction*glm::inverse(frame.Camera.ViewProjection);
                    const unsigned skyMode=frame.Camera.Orthographic?
                        (historyValid && frame.Camera.ToCamera==view.Temporal.Previous.Camera.ToCamera?2u:0u):1u;
                    std::static_pointer_cast<MetalFramebuffer>(view.Radiance)->ResolveTemporal(view.Jittered,view.Motion,jitter,historyValid,view.TemporalDebug,skyMotion,skyMode,submitted,view.TemporalMetrics);
#endif
                } else {
                    drawOpaque(view.Radiance,frame);
                }
                auto worldState = raster;
                RenderCommand::SetRasterState(worldState);
                world2D();
                view.Radiance->Unbind();
                display->Bind();
                presentWorld();
            }
            else {
                view.Radiance.reset();
                if(anyShadows || ao) {
#ifdef HZ_PLATFORM_MACOS
                    auto target=std::static_pointer_cast<MetalFramebuffer>(display);
                    if(frame.AmbientOcclusionView)target->Resume(glm::vec4(frame.AmbientOcclusionView==3?0.f:1.f));
                    else target->Resume();
#endif
                }
                prepareLighting(display);
                Renderer3D::Render(frame,frame.CaptureRadiance,false,false,&view.ReflectionProbes);
                RenderCommand::SetRasterState(raster);
                world2D();
            }
            screenUI();
            if(temporal) {
                auto& state=view.Temporal;state.Previous=frame;state.Submitted=submitted;
                state.Jitter=jitter;state.JitteredViewProjection=sample.Camera.ViewProjection;
                state.Width=display->GetSpecification().Width;state.Height=display->GetSpecification().Height;++state.Phase;
            }
        }
        catch (...)
        {
            view.Temporal={};
            Renderer2D::CancelScene();
            if(view.Jittered)view.Jittered->Cancel();
            if(view.ReflectionSurface)view.ReflectionSurface->Cancel();
            if(view.Motion)view.Motion->Cancel();
            if (view.AmbientDepth) view.AmbientDepth->Cancel();
            if (view.ShadowDepth) view.ShadowDepth->Cancel();
            if (view.SpotShadowDepth) view.SpotShadowDepth->Cancel();
            if (view.PointShadowDepth) view.PointShadowDepth->Cancel();
            if (view.Radiance) view.Radiance->Cancel();
            display->Cancel();
            throw;
        }
        return;
    }
    if(frame.Presentation.AmbientOcclusion.Enabled && !view.WarnedUnsupportedAO) {
        HZ_CORE_WARN("XeGTAO request inactive: OpenGL has no ambient-occlusion compute implementation");
        view.WarnedUnsupportedAO=true;
    }
    RenderCommand::StateScope scope;
    auto raster = RenderCommand::GetRasterState();
    raster.ClipEnabled = false;
    raster.SRGB = false;
    raster.SeamlessCubes = true;
    raster.DepthTest = true;
    raster.DepthWrite = true;
    raster.Compare = DepthCompare::Less;
    raster.Cull = CullFace::None;
    raster.Clockwise = false;
    raster.Blend = true;
    raster.ColorMask = raster.PickingMask = glm::bvec4(true);
    RenderCommand::SetRasterState(raster);
    struct WorldBinding
    {
        Ref<Framebuffer> Pending;
        ~WorldBinding()
        {
            if (Pending)
                Pending->Unbind();
        }
        void Finish()
        {
            if (Pending)
            {
                Pending->Unbind();
                Pending.reset();
            }
        }
    } worldBinding;
    if (anyShadows) {
        renderShadows();display->Bind();
        RenderCommand::SetRasterState(raster);
    }
    const bool hdr = !frame.CaptureRadiance && frame.Presentation.SceneReferred && frame.Debug == SurfaceView::Shaded;
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
        if (reflections)
            prepareReflectionSurface(view.Radiance->GetSpecification());
        worldBinding.Pending = reflections ? view.ReflectionSurface : view.Radiance;
        worldBinding.Pending->Bind();
        RenderCommand::SetClearColor(frame.Presentation.Background);
        RenderCommand::Clear();
        worldBinding.Pending->ClearAttachment(1, -1);
    }
    else
        view.Radiance.reset();
    if (anyShadows) prepareShadowDepth(hdr ? worldBinding.Pending : display);
    Renderer3D::Render(frame,hdr || frame.CaptureRadiance,false,false,&view.ReflectionProbes,reflections);
    if (reflections) {
        worldBinding.Finish();
        view.Radiance->Bind();
        worldBinding.Pending=view.Radiance;
        view.ReflectionSurface->ResolveReflectionsTo(view.Radiance,reflectionParameters(frame.Camera));
    }
    raster = RenderCommand::GetRasterState();
    raster.Cull = CullFace::None;
    raster.Clockwise = false;
    raster.Blend = true;
    RenderCommand::SetRasterState(raster);
    world2D();
    if (hdr)
    {
        worldBinding.Finish();
        display->Bind();
        presentWorld();
    }
    screenUI();
}
} // namespace Hazel
