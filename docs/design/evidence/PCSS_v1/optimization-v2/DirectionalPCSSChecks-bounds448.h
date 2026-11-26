#pragma once
#include "Renderer3DParity.h"
#include "Hazel/Renderer/Scene/Lighting.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

// Caster-only planes deliberately isolate the shadow signal from camera occlusion.
// Authored scenes separately qualify visible geometry/AO/TAA and resource lifetime.
inline void DirectionalPCSSChecks(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;std::filesystem::create_directories(output);
    const bool shadeOnly=!FileSystem::GetEnvironmentPath("HAZEL_PCSS_SHADE_ONLY").empty();
    auto project=Project::LoadCandidate(FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT"));
    if(!project)throw std::runtime_error("PCSS fixtures require the rendering reference project");
    auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.5f,.5f,.5f,1};material->Roughness=.8f;
    FramebufferSpecification spec;spec.Width=1024;spec.Height=256;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
    auto target=Framebuffer::Create(spec);SceneRenderView view;
    SceneSubmission frame;frame.Assets=assets;frame.Presentation.SceneReferred=true;frame.Presentation.Exposure=0;
    auto& settings=frame.Presentation.Shadows;settings.Enabled=true;settings.Distance=80;
    settings.Filter=DirectionalShadowFilter::PCSS;settings.NormalBias=0;settings.DepthBias=.05f;
    frame.Camera.Orthographic=true;frame.Camera.ToCamera={0,1,0};frame.Light=SubmittedDirectional{{0,1,0},{2,2,2}};
    SubmittedMesh ground;ground.Geometry=mesh.Accepted;ground.Materials={material};ground.Picking=11;ground.Identity=11;
    ground.SetWorld(glm::translate(glm::mat4(1),{0,-.1f,-20})*glm::scale(glm::mat4(1),{80,.2f,80}));
    SubmittedMesh wall=ground;wall.Picking=12;wall.Identity=12;
    double totalGPUms=0;
    auto render=[&](unsigned mode,SurfaceView debug=SurfaceView::Direct) {
        frame.ShadowView=mode;frame.Debug=debug;frame.LinearLightingInspection=true;
        target->Bind();target->ClearEditor({0,0,0,1});target->ClearAttachment(1,-1);
        GLuint timer=0;
        if(RendererAPI::GetAPI()==RendererAPI::API::OpenGL && !FileSystem::GetEnvironmentPath("HAZEL_PCSS_BENCHMARK").empty()) {
            glGenQueries(1,&timer);glBeginQuery(GL_TIME_ELAPSED,timer);
        }
        SceneRenderer::Render(frame,view,target,[]{},[]{});
        if(timer)glEndQuery(GL_TIME_ELAPSED);
        if(RendererAPI::GetAPI()==RendererAPI::API::OpenGL)target->Bind();auto image=capture(target);
        if(timer) {
            // Readback submits/completes the queued offscreen work on Apple's
            // GL driver. The elapsed interval ends before that readback.
            GLuint64 nanoseconds=0;glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&nanoseconds);
            glDeleteQueries(1,&timer);totalGPUms=double(nanoseconds)*1e-6;
        }
        if(!image || !image->Completed || !image->Error.empty())throw std::runtime_error("PCSS capture incomplete");
        return image;
    };
    auto save=[&](const std::string& name,const auto& image) {
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for(size_t i=0;i<image->Color.size();++i) {
            if(!std::isfinite(glm::length(image->Color[i])))throw std::runtime_error("Nonfinite PCSS result");
            file.write(reinterpret_cast<const char*>(&image->Color[i]),16);file.write(reinterpret_cast<const char*>(&image->Entity[i]),4);file.write(reinterpret_cast<const char*>(&image->Depth[i]),4);
        }
    };
    auto require=[](bool condition,const char* error){if(!condition)throw std::runtime_error(error);};
    // Independent FP64 grazing-plane intersections at actual native texel
    // centres, including zero physical radius and receiver-only coverage.
    for(unsigned resolution:{256u,1024u,2048u})for(float angle:{0.f,.03490658f}) {
        auto coverage=frame;coverage.Presentation.Shadows.Resolution=resolution;coverage.Presentation.Shadows.MaxPenumbra=.001f;
        coverage.Light->AngularHalfRadius=angle;coverage.Camera.Position={0,15,-20};
        coverage.Camera.ViewProjection=glm::ortho(-2.f,2.f,-.5f,.5f,.1f,100.f)*glm::lookAt(coverage.Camera.Position,glm::vec3(0,0,-20),glm::vec3(0,0,-1));
        const auto fit=FitDirectionalShadows(coverage);const glm::dmat4 matrix(fit.VP[0]);
        const auto inverse=glm::inverse(glm::dmat4(coverage.Camera.ViewProjection));
        const glm::dvec3 right=glm::normalize(glm::dvec3(matrix[0][0],matrix[1][0],matrix[2][0]));
        const glm::dvec3 up=glm::normalize(glm::dvec3(matrix[0][1],matrix[1][1],matrix[2][1]));
        for(int x:{-1,1})for(int y:{-1,1}) {
            const auto h=inverse*glm::dvec4(x,y,-1,1);const glm::dvec3 corner=glm::dvec3(h)/h.w;
            const auto q=(matrix*glm::dvec4(corner,1))*.5+.5;
            const auto cell=glm::floor(glm::dvec2(q)*double(resolution)-.5);
            for(int i:{0,1})for(int j:{0,1})for(int a:{-1,1})for(int b:{-1,1}) {
                const auto delta=(cell+glm::dvec2(i,j)+.5-glm::dvec2(q)*double(resolution))*double(fit.TexelDepth[0].x);
                const auto point=corner+right*delta.x+up*delta.y-glm::dvec3(0,1,0)*(8*(a*delta.x+b*delta.y));
                const double depth=(matrix*glm::dvec4(point,1)).z*.5+.5;
                require(depth>=-1e-6 && depth<=1+1e-6,"PCSS native grazing-plane support exceeds light-depth projection");
            }
        }
    }
    auto authored=CreateRef<Scene>();auto entity=authored->CreateEntityWithUUID(901,"PCSS angular key");
    auto& light=entity.AddComponent<DirectionalLightComponent>();
    SceneSerializer serializer(authored);const auto original=serializer.CaptureAuthoredState(entity);
    light.AngularHalfRadius=.02f;
    require(original!=serializer.CaptureAuthoredState(entity),"Angular radius missing from authored state");
    auto loaded=CreateRef<Scene>();require(SceneSerializer(loaded).DeserializeText(serializer.SerializeText()),"Angular light scene roundtrip failed");
    require(loaded->GetEntityByUUID(901).GetComponent<DirectionalLightComponent>().AngularHalfRadius==.02f,"Angular light radius lost on load");
    if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_BENCHMARK").empty()) {
        frame.LocalMetrics=view.LocalMetrics;wall.SetWorld(glm::translate(glm::mat4(1),{0,8,-20})*glm::scale(glm::mat4(1),{40,.05f,40}));
        if(auto value=FileSystem::GetEnvironmentPath("HAZEL_PCSS_BENCHMARK_HEIGHT");!value.empty()) {
            const float height=std::stof(value.string()),thickness=std::min(height*.1f,.0001f);
            require(std::isfinite(height)&&height>0,"Invalid PCSS benchmark caster height");
            wall.SetWorld(glm::translate(glm::mat4(1),{0,height-thickness*.5f,-20})*glm::scale(glm::mat4(1),{40,thickness,40}));
        }
        if(auto value=FileSystem::GetEnvironmentPath("HAZEL_PCSS_BENCHMARK_ANGLE");!value.empty())frame.Light->AngularHalfRadius=std::stof(value.string());
        frame.Meshes={ground};frame.ShadowCasters={ground,wall};
        if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_LAYERED_BENCHMARK").empty()) {
            // A near-contact sheet below an upper slatted canopy discriminates
            // mixed-depth search and the wider exact receiver-plane fallback.
            for(int i=-40;i<40;++i) {
                auto upper=wall;upper.Identity=unsigned(i+140);upper.Picking=i+140;
                upper.SetWorld(glm::translate(glm::mat4(1),{float(i)*.5f,8.f,-20})*glm::scale(glm::mat4(1),{.25f,.05f,40}));
                frame.ShadowCasters.push_back(upper);
            }
        }
        if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_ANGLED_BENCHMARK").empty())frame.Light->ToLight=glm::normalize(glm::vec3(1,1,0));
        std::ofstream times(output/"overlap.csv");times<<"width,height,resolution,filter,depth,sample,shadowGPUms,forwardGPUms,serializedMs,totalGPUms\n";
        const bool retina=!FileSystem::GetEnvironmentPath("HAZEL_PCSS_RETINA_BENCHMARK").empty();
        const std::vector<glm::uvec2> extents=retina?std::vector<glm::uvec2>{{2560,1440}}:std::vector<glm::uvec2>{{1280,720},{1920,1080}};
        for(auto extent:extents)for(unsigned resolution:{1024u,2048u})
            for(bool soft:{false,true})for(float depth:{18.f,19.07625f,21.f}) {
                if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_FOCUSED_BENCHMARK").empty() &&
                   (extent.x!=1920 || !soft || depth!=19.07625f))continue;
                target->Resize(extent.x,extent.y);settings.Resolution=resolution;settings.Filter=soft?DirectionalShadowFilter::PCSS:DirectionalShadowFilter::PCF;
                frame.Camera.Position={0,depth,-20};frame.Camera.ViewProjection=glm::ortho(-2.f,2.f,-.5f,.5f,.1f,100.f)*
                    glm::lookAt(frame.Camera.Position,glm::vec3(0,0,-20),glm::vec3(0,0,-1));
                for(unsigned sample=0;sample<25;++sample) {
                    frame.LocalMetrics->Valid=false;const auto start=std::chrono::steady_clock::now();render(0);
                    const double whole=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                    if(RendererAPI::GetAPI()==RendererAPI::API::Metal)require(frame.LocalMetrics->Valid && frame.LocalMetrics->ForwardMilliseconds>0,"Missing PCSS GPU timestamp");
                    if(RendererAPI::GetAPI()==RendererAPI::API::OpenGL && sample>=5)require(totalGPUms>0,"Missing measured OpenGL elapsed GPU query");
                    if(sample>=5)times<<extent.x<<','<<extent.y<<','<<resolution<<','<<soft<<','<<depth<<','<<sample-5<<','<<frame.LocalMetrics->ShadowMilliseconds<<','<<frame.LocalMetrics->ForwardMilliseconds<<','<<whole<<','<<totalGPUms<<'\n';
                }
            }
        view.Reset();return;
    }
    if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_AUTHORED").empty()) {
        // WorldPresentation copies packed depth/stencil; match the real editor
        // D24S8 display contract rather than the analytical D32F plane target.
        if(RendererAPI::GetAPI()==RendererAPI::API::OpenGL) {
            spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
            target=Framebuffer::Create(spec);
        }
        auto scene=project->LoadScene("Scenes/DirectionalShadows_v1.hazel");scene->OnViewportResize(1280,720);
        scene->Presentation().Exposure=0;scene->Presentation().Shadows.Filter=DirectionalShadowFilter::PCSS;
        if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_PCF_CONTROL").empty())scene->Presentation().Shadows.Filter=DirectionalShadowFilter::PCF;
        auto cameraEntity=scene->GetPrimaryCameraEntity();require(bool(cameraEntity),"PCSS study camera missing");
        const auto cameraWorld=scene->GetWorldTransform(cameraEntity),projection=cameraEntity.GetComponent<CameraComponent>().Camera.GetProjection();
        const SceneCameraView camera{projection*glm::inverse(cameraWorld),glm::vec3(cameraWorld[3]),glm::normalize(glm::vec3(cameraWorld*glm::vec4(0,0,1,0))),false};
        const auto base=SceneRenderer::Collect(*scene,camera,{1280,720});target->Resize(1280,720);
        for(unsigned state=0;state<12;++state) {
            view.Reset();const unsigned motion=state>=8?state-4:state;
            for(unsigned step=0;step<16;++step) {
                frame=base;frame.Presentation.AmbientOcclusion.Enabled=state==1||(state>=3&&state<8);frame.Presentation.TemporalAA=state>=2&&state<8;
                if(motion==4||motion==5) {
                    auto world=cameraWorld;
                    if(motion==4)world[3].x+=float(step)*.02f;
                    else world=world*glm::rotate(glm::mat4(1),float(step)*.001f,glm::vec3(0,1,0));
                    frame.Camera={projection*glm::inverse(world),glm::vec3(world[3]),glm::normalize(glm::vec3(world*glm::vec4(0,0,1,0))),false};
                }
                if(motion==6&&frame.Light)frame.Light->ToLight=glm::vec3(glm::rotate(glm::mat4(1),float(step)*.003f,glm::vec3(0,1,0))*glm::vec4(base.Light->ToLight,0));
                if(motion==7&&!frame.ShadowCasters.empty()) {
                    auto& caster=frame.ShadowCasters.back();auto moved=caster.World;moved[3].x+=float(step)*.02f;caster.SetWorld(moved);
                    for(auto& draw:frame.Meshes)if(draw.Identity==caster.Identity)draw.SetWorld(moved);
                }
                frame.ShadowView=0;frame.Debug=SurfaceView::Shaded;frame.LinearLightingInspection=false;
                target->Bind();target->ClearEditor(frame.Presentation.Background);target->ClearAttachment(1,-1);
                SceneRenderer::Render(frame,view,target,[]{},[]{});if(RendererAPI::GetAPI()==RendererAPI::API::OpenGL)target->Bind();const auto image=capture(target);
                require(image&&image->Completed&&image->Error.empty(),"PCSS authored capture incomplete");
                for(const auto& color:image->Color)require(std::isfinite(glm::length(color)),"Nonfinite PCSS authored radiance");
                if(step==0||step==7||step==15)save("study-"+std::to_string(state)+"-"+std::to_string(step),image);
            }
        }
        view.Reset();std::cout<<"CAPTURED authored PCSS AO/TAA and moving camera/light/caster; visual gates separate\n";return;
    }
    if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_MOTION").empty()) {
        target->Resize(512,32);
        std::ofstream records(output/"motion.csv");records<<std::setprecision(12)<<"case,resolution,height,motion,step,filter,texel\n";
        unsigned n=0;
        for(unsigned resolution:{1024u,2048u})for(float height:{.05f,8.f})for(unsigned motion:{0u,1u})for(unsigned step=0;step<32;++step) {
            settings.Resolution=resolution;frame.Light=SubmittedDirectional{{0,1,0},glm::vec3(2),.00465f};
            const float shift=(float(step)-15.5f)*.004f;
            auto cameraWorld=glm::inverse(glm::lookAt(glm::vec3(0,15,-20),glm::vec3(0,0,-20),glm::vec3(0,0,-1)));
            if(motion==0)cameraWorld[3].x+=shift;
            else cameraWorld=cameraWorld*glm::rotate(glm::mat4(1),shift*.1f,glm::vec3(0,0,1));
            frame.Camera.Position=glm::vec3(cameraWorld[3]);frame.Camera.ViewProjection=glm::ortho(-.5f,.5f,-.05f,.05f,.1f,100.f)*glm::inverse(cameraWorld);
            wall.SetWorld(glm::translate(glm::mat4(1),{-10,height,-20})*glm::scale(glm::mat4(1),{20,.0001f,80}));
            frame.Meshes={ground};frame.ShadowCasters={ground,wall};
            for(unsigned filter=0;filter<2;++filter) {
                settings.Filter=filter?DirectionalShadowFilter::PCSS:DirectionalShadowFilter::PCF;
                const auto fit=FitDirectionalShadows(frame);records<<n<<','<<resolution<<','<<height<<','<<motion<<','<<step<<','<<filter<<','<<fit.TexelDepth[0].x<<'\n';
                const auto name="motion-"+std::to_string(n++);save(name+"-position",render(0,SurfaceView::WorldPosition));save(name+"-visibility",render(1));
            }
        }
        view.Reset();std::cout<<"CAPTURED TAA-off plane translation/rotation PCF and PCSS\n";return;
    }
    if(!FileSystem::GetEnvironmentPath("HAZEL_PCSS_STRIPS").empty()) {
        std::ofstream records(output/"strips.csv");records<<std::setprecision(12)<<"case,resolution,width,height,angle,texel,span,searchHi,layers,lowerHeight,lightX,lightY";
        for(unsigned i=0;i<16;++i)records<<",m"<<i;
        records<<'\n';unsigned n=0;
        frame.Camera.Position={0,15,-20};frame.Camera.ViewProjection=glm::ortho(-.75f,.75f,-.5f,.5f,.1f,100.f)*
            glm::lookAt(frame.Camera.Position,glm::vec3(0,0,-20),glm::vec3(0,0,-1));
        for(bool grazing:{false,true})for(unsigned resolution:{1024u,2048u})for(float width:{.04f,.2f,.6f})for(float height:{.05f,8.f})for(float angle:{.00465f,.03490658f})for(bool layers:{false,true}) {
            if((layers&&height<1)||(grazing&&(!layers||height<1)))continue;
            const glm::vec3 lightDirection=grazing?glm::normalize(glm::vec3(1,1,0)):glm::vec3(0,1,0);
            const float lowerHeight=grazing?.005f:1.f;
            settings.Resolution=resolution;frame.Light=SubmittedDirectional{lightDirection,glm::vec3(2),angle};
            wall.SetWorld(glm::translate(glm::mat4(1),{height*lightDirection.x/lightDirection.y,height-.00005f,-20})*glm::scale(glm::mat4(1),{width,.0001f,80}));
            frame.Meshes={ground};frame.ShadowCasters={ground,wall};
            if(layers) {
                auto lower=wall;lower.SetWorld(glm::translate(glm::mat4(1),{width*.5f+lowerHeight*lightDirection.x/lightDirection.y,lowerHeight-.00005f,-20})*glm::scale(glm::mat4(1),{width*.75f,.0001f,80}));
                frame.ShadowCasters.push_back(lower);
            }
            settings.Filter=DirectionalShadowFilter::PCSS;const auto fit=FitDirectionalShadows(frame);
            const auto name="strip-"+std::to_string(n);
            records<<n++<<','<<resolution<<','<<width<<','<<height<<','<<angle<<','<<fit.TexelDepth[0].x<<','<<1/fit.TexelDepth[0].y<<','<<fit.Softness.z<<','<<layers<<','<<lowerHeight<<','<<lightDirection.x<<','<<lightDirection.y;
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)records<<','<<fit.VP[0][col][row];
            records<<'\n';
            save(name+"-position",render(0,SurfaceView::WorldPosition));save(name+"-visibility",render(1));save(name+"-physical",render(11));
            settings.Filter=DirectionalShadowFilter::PCF;save(name+"-pcf",render(1));
        }
        view.Reset();std::cout<<"CAPTURED finite thin blockers; independent coverage screen separate\n";return;
    }
    auto roundtrip=SceneDefaults::ReadPresentation(SceneDefaults::Encode(frame.Presentation));
    require(roundtrip==frame.Presentation,"PCSS settings roundtrip failed");
    auto old=SceneDefaults::Encode(frame.Presentation);old["Shadows"].remove("Filter");old["Shadows"].remove("MaxPenumbra");
    require(SceneDefaults::ReadPresentation(old).Shadows.Filter==DirectionalShadowFilter::PCF,"Legacy scene no longer defaults to PCF");
    std::ofstream metadata(output/"planes.csv");metadata<<std::setprecision(12)<<"case,resolution,height,angle,cameraDepth,cascade,texel,span,hi,searchHi,lightX,lightY\n";
    std::ofstream projections(output/"projections.csv");projections<<std::setprecision(12)<<"case,cascade";
    for(unsigned i=0;i<16;++i)projections<<",m"<<i;
    projections<<'\n';
    unsigned number=0;
    for(unsigned resolution:{1024u,2048u})for(float angle:{0.f,.00465f,.03490658f})for(float height:{.025f,1.f,4.f,8.f}) {
        settings.Resolution=resolution;frame.Light->AngularHalfRadius=angle;
        frame.Camera.Position={0,15,-20};frame.Camera.ViewProjection=glm::ortho(-2.f,2.f,-.5f,.5f,.1f,100.f)*
            glm::lookAt(frame.Camera.Position,glm::vec3(0,0,-20),glm::vec3(0,0,-1));
        wall.SetWorld(glm::translate(glm::mat4(1),{-10,height,-20})*glm::scale(glm::mat4(1),{20,.05f,80}));
        frame.Meshes={ground};frame.ShadowCasters={ground,wall};
        const auto fit=FitDirectionalShadows(frame);
        for(unsigned c=0;c<4;++c) {
            metadata<<number<<','<<resolution<<','<<height+.025f<<','<<angle<<",15,"<<c<<','<<fit.TexelDepth[c].x<<','<<1/fit.TexelDepth[c].y<<','<<fit.TexelDepth[c].z<<','<<fit.Softness.z<<",0,1\n";
            projections<<number<<','<<c;
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)projections<<','<<fit.VP[c][col][row];
            projections<<'\n';
        }
        const auto name="plane-"+std::to_string(number++);
        save(name+"-position",render(0,SurfaceView::WorldPosition));save(name+"-visibility",render(1));
        save(name+"-physical",render(11));save(name+"-pair",render(12));
        save(name+"-radiance",render(0));
        if(number==1)save(name+"-repeat",render(1));
        settings.Filter=DirectionalShadowFilter::PCF;
        save(name+"-pcf-visibility",render(1));save(name+"-pcf-radiance",render(0));
        settings.Filter=DirectionalShadowFilter::PCSS;
    }
    // Held-out oblique light includes a blocker below the native slope-bias
    // envelope. It must not silently lose the thin/contact shadow.
    for(unsigned resolution:{1024u,2048u})for(float height:{.005f,.05f,1.f,8.f}) {
        settings.Resolution=resolution;frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(1,1,0)),glm::vec3(2),.03490658f};
        frame.Camera.Position={-height,15,-20};frame.Camera.ViewProjection=glm::ortho(-2.f,2.f,-.5f,.5f,.1f,100.f)*
            glm::lookAt(frame.Camera.Position,glm::vec3(-height,0,-20),glm::vec3(0,0,-1));
        const float thickness=std::min(height*.1f,.0001f);
        wall.SetWorld(glm::translate(glm::mat4(1),{-10,height-thickness*.5f,-20})*glm::scale(glm::mat4(1),{20,thickness,80}));
        frame.Meshes={ground};frame.ShadowCasters={ground,wall};const auto fit=FitDirectionalShadows(frame);
        for(unsigned c=0;c<4;++c) {
            metadata<<number<<','<<resolution<<','<<height<<','<<frame.Light->AngularHalfRadius<<",15,"<<c<<','<<fit.TexelDepth[c].x<<','<<1/fit.TexelDepth[c].y<<','<<fit.TexelDepth[c].z<<','<<fit.Softness.z<<','<<frame.Light->ToLight.x<<','<<frame.Light->ToLight.y<<'\n';
            projections<<number<<','<<c;
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)projections<<','<<fit.VP[c][col][row];
            projections<<'\n';
        }
        const auto name="plane-"+std::to_string(number++);
        save(name+"-position",render(0,SurfaceView::WorldPosition));save(name+"-visibility",render(1));
        save(name+"-physical",render(11));save(name+"-pair",render(12));save(name+"-radiance",render(0));
        settings.Filter=DirectionalShadowFilter::PCF;save(name+"-pcf-visibility",render(1));save(name+"-pcf-radiance",render(0));settings.Filter=DirectionalShadowFilter::PCSS;
    }
    frame.ShadowCasters.clear();
    const auto clear=render(1);if(!shadeOnly)for(const auto& color:clear->Color)require(std::abs(color.x-1)<1e-6f,"Clear atlas was interpreted as a PCSS blocker");
    save("clear-visibility",clear);save("clear-physical",render(11));
    view.Reset();std::cout<<"CAPTURED directional PCSS planes="<<number<<"; independent gates evaluated by runner\n";
}
