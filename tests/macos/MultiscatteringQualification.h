#pragma once
#include "Renderer3DParity.h"
#include <iomanip>
#include <iostream>

// Versioned production-renderer component study. The baseline selector belongs
// only to this test oracle; production has one standard BRDF.
inline void MultiscatteringQualification(const std::filesystem::path &output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)> &capture)
{
    using namespace Hazel;
    const auto singleScattering=IBLReferenceIntegral(1,1);
    if(std::abs(singleScattering.x+singleScattering.y-(1-std::log(2.)))>.00002)
        throw std::runtime_error("Independent historical GGX analytic anchor failed");
    std::filesystem::create_directories(output);
    const bool baseline = !FileSystem::GetEnvironmentPath("HAZEL_GGX_SINGLE_REFERENCE").empty();
    auto selected = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    if (selected.empty()) throw std::runtime_error("GGX study requires explicit frozen project");
    auto project = Project::LoadCandidate(selected);
    auto assets = CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if (!mesh.Accepted) throw std::runtime_error(mesh.Error);
    auto constant = CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(project->GetAssetRoot(), "Reference/IBL_v1/Constant.henv"));
    const auto runtimeBRDF=baseline?constant->BRDF:ReadRuntimeBRDF();
    FramebufferSpecification spec; spec.Width=spec.Height=64;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        RendererAPI::GetAPI()==RendererAPI::API::Metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);
    SceneSubmission frame;frame.Assets=assets;frame.Presentation.SceneReferred=true;
    frame.Environment=constant;frame.Presentation.Environment.Lighting=true;
    frame.Camera.Orthographic=true;frame.Camera.Position={0,0,3};
    frame.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f)*glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0));
    auto material=CreateRef<MaterialDefinition>();
    SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.Materials={material};draw.Picking=-2000000003;frame.Meshes={draw};
    SceneRenderView view;
    const glm::dvec3 colors[]={{.95,.64,.15},{.955,.638,.538},{.91,.92,.92},{.02,.02,.02},{1,1,1},{.2,.4,.7},{1,1,1}};
    const char *names[]={"gold","copper","aluminum","dark-dielectric","white-dielectric","mixed","perfect"};
    const auto light=glm::normalize(glm::vec3(.3f,.4f,1));
    std::ofstream csv(output/"components.csv");csv<<std::setprecision(12)<<"material,rough,NoV,component,r,g,b,expectedR,expectedG,expectedB,continuousR,continuousG,continuousB\n";
    double maximum=0;unsigned count=0;
    for(unsigned color=0;color<7;++color)
      for(float rough:{0.f,.045f,.046f,.25f,.5f,.75f,1.f})
       for(float mu:{.01f,.05f,.25f,.5f,1.f}) {
        material->BaseColor=glm::vec4(glm::vec3(colors[color]),1);material->Roughness=rough;
        material->Metallic=color<3||color==6?1.f:color==5?.5f:0.f;
        frame.Camera.ToCamera={std::sqrt(1-mu*mu),0,mu};
        frame.Light=SubmittedDirectional{light,glm::vec3(2)};
        auto base=glm::dvec3(material->BaseColor),f0=glm::mix(glm::dvec3(.04),base,double(material->Metallic));
        auto ab=MultiscatteringAlbedo(mu,rough);
        auto energy=ReferenceMultiscatteringEnergy(f0,ab,MultiscatteringAverage(rough));
        auto fview=f0+(1.0-f0)*std::pow(1-double(mu),5);
        glm::dvec3 continuous[]={
            2.*(baseline?OracleBRDF(base,material->Metallic,rough,{0,0,1},glm::dvec3(frame.Camera.ToCamera),glm::dvec3(light)):
                OracleMultiscatteringBRDF(base,material->Metallic,rough,{0,0,1},glm::dvec3(frame.Camera.ToCamera),glm::dvec3(light))),
            .25*(1-double(material->Metallic))*base*(baseline?1.0-fview:glm::dvec3(energy.Residual)),
            .25*(baseline?f0*ab.x+ab.y:energy.Reflected)};
        // Exact independently sampled A/B is the implementation oracle; keep
        // the continuous model beside it to expose inherited LUT error.
        auto tableAB=baseline?ReferenceSampleBRDF(runtimeBRDF,mu,std::max(.045,double(rough))):ReferenceSampleRuntimeBRDF(runtimeBRDF,mu,rough);
        auto tableEnergy=baseline?ReferenceTableEnergy(runtimeBRDF,f0,mu,rough):ReferenceRuntimeTableEnergy(runtimeBRDF,f0,mu,rough);
        glm::dvec3 expected[]={baseline?continuous[0]:2.*OracleRuntimeMultiscatteringBRDF(runtimeBRDF,
            base,material->Metallic,rough,{0,0,1},glm::dvec3(frame.Camera.ToCamera),glm::dvec3(light)),
            .25*(1-double(material->Metallic))*base*(baseline?1.0-fview:glm::dvec3(tableEnergy.Residual)),
            .25*(baseline?f0*tableAB.x+tableAB.y:tableEnergy.Reflected)};
        for(unsigned component=0;component<3;++component) {
            frame.Debug=component==0?SurfaceView::Direct:component==1?SurfaceView::DiffuseIBL:SurfaceView::SpecularIBL;
            frame.LinearLightingInspection=true;
            target->Bind();
            if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
            else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
            target->ClearAttachment(1,-1);
            SceneRenderer::Render(frame,view,target,[]{},[]{});auto result=capture(target);
            if(!result||!result->Completed||!result->Error.empty())throw std::runtime_error("GGX capture incomplete");
            const size_t pixel=32*64+32;
            for(unsigned c=0;c<3;++c) {
                const double error=std::abs(result->Color[pixel][c]-expected[component][c]);
                maximum=std::max(maximum,error);
                if(!std::isfinite(result->Color[pixel][c])||error>.003)
                    throw std::runtime_error(std::string("GGX FP64 component oracle: ")+names[color]+" r="+std::to_string(rough)+" NoV="+std::to_string(mu)+" component="+std::to_string(component)+" error="+std::to_string(error));
            }
            if(result->Entity[pixel]!=draw.Picking)throw std::runtime_error("GGX signed picking changed");
            if(!baseline && color==6 && component==2)
                for(unsigned c=0;c<3;++c)if(std::abs(result->Color[pixel][c]/.25-1)>.003)
                    throw std::runtime_error("GPU perfect-reflector furnace failed");
            if(component==0) {
                const auto originalView=frame.Camera.ToCamera;
                frame.Camera.ToCamera=light;frame.Light=SubmittedDirectional{originalView,glm::vec3(2)};
                target->Bind();
                if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
                else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
                target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
                auto reverse=capture(target);
                if(!reverse||!reverse->Completed||!reverse->Error.empty())throw std::runtime_error("Reciprocity capture failed");
                for(unsigned c=0;c<3;++c) {
                    const double a=result->Color[pixel][c]/light.z,b=reverse->Color[pixel][c]/mu;
                    if(std::abs(a-b)>2e-5*std::max({1.,std::abs(a),std::abs(b)}))
                        throw std::runtime_error("Intrinsic GPU BRDF reciprocity failed");
                }
                frame.Camera.ToCamera=originalView;frame.Light=SubmittedDirectional{light,glm::vec3(2)};
            }
            csv<<names[color]<<','<<rough<<','<<mu<<','<<component;
            for(unsigned c=0;c<3;++c)csv<<','<<result->Color[pixel][c];
            for(unsigned c=0;c<3;++c)csv<<','<<expected[component][c];
            for(unsigned c=0;c<3;++c)csv<<','<<continuous[component][c];
            csv<<'\n';
            ++count;
        }
    }
    if(!baseline) {
        // Integrate the actual analytic-light shader, not the split-sum furnace.
        // At V=N the isotropic unit-reflector BRDF is azimuth independent.
        frame.Environment.reset();frame.Camera.ToCamera={0,0,1};
        frame.Debug=SurfaceView::Direct;frame.LinearLightingInspection=true;
        material->BaseColor=glm::vec4(1);material->Metallic=material->Roughness=1;
        glm::dvec3 reflected(0);
        for(auto node:IBLReferenceNodes(64)) {
            frame.Light=SubmittedDirectional{{float(std::sqrt(1-node.x*node.x)),0,float(node.x)},glm::vec3(1)};
            target->Bind();
            if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
            else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
            target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
            auto result=capture(target);
            if(!result||!result->Completed||!result->Error.empty())throw std::runtime_error("Direct furnace capture failed");
            reflected+=glm::dvec3(result->Color[32*64+32])*(2*glm::pi<double>()*node.y);
        }
        std::ofstream furnace(output/"direct-furnace.csv");
        furnace<<std::setprecision(12)<<"rough,NoV,r,g,b\n1,1,"<<reflected.r<<','<<reflected.g<<','<<reflected.b<<'\n';
        furnace.close();
        for(unsigned c=0;c<3;++c)if(std::abs(reflected[c]-1)>.003)
            throw std::runtime_error("Direct GGX furnace failed: reflected="+std::to_string(reflected[c])+"; required 1 +/- .003; legacy LUT albedo differs from analytic direct GGX");
    }
    if(!baseline) {
        // Normal-view importance quadrature also qualifies the sharp FP32 SS
        // lobe; uniform NoL nodes cannot resolve its roughness-floor peak.
        frame.Environment.reset();frame.Camera.ToCamera={0,0,1};
        frame.Debug=SurfaceView::Direct;frame.LinearLightingInspection=true;
        material->BaseColor=glm::vec4(1);material->Metallic=1;material->Roughness=.045f;
        const double alpha2=std::pow(double(material->Roughness),4),pi=glm::pi<double>();
        const double cap=1/(1+alpha2);
        glm::dvec3 reflected(0);
        for(auto node:IBLReferenceNodes(256)) {
            const double u=node.x*cap,hz=std::sqrt((1-u)/(1+(alpha2-1)*u));
            const double hx=std::sqrt(1-hz*hz),den=1-hz*hz+alpha2*hz*hz;
            const double pdf=alpha2/(4*pi*den*den);
            frame.Light=SubmittedDirectional{{float(2*hx*hz),0,float(2*hz*hz-1)},glm::vec3(1)};
            target->Bind();
            if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
            else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
            target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
            const auto result=capture(target);
            if(!result||!result->Completed||!result->Error.empty())throw std::runtime_error("Smooth normal furnace capture failed");
            reflected+=glm::dvec3(result->Color[32*64+32])*(cap*node.y/pdf);
        }
        std::ofstream furnace(output/"smooth-normal-furnace.csv");
        furnace<<std::setprecision(12)<<"rough,NoV,r,g,b\n.045,1,"<<reflected.r<<','<<reflected.g<<','<<reflected.b<<'\n';furnace.close();
        for(unsigned c=0;c<3;++c)if(std::abs(reflected[c]-1)>.003)
            throw std::runtime_error("Smooth normal GPU furnace failed: "+std::to_string(reflected[c]));
    }
    if(!baseline) {
        // Independent two-technique MIS integrates the actual grazing shader.
        // VNDF resolves sharp SS; cosine sampling resolves the broad MS lobe.
        auto grazingTarget=spec;grazingTarget.Width=grazingTarget.Height=1;
        target=Framebuffer::Create(grazingTarget);view.Reset();
        constexpr unsigned samples=16384;
        const double mu=.01,alpha=.045*.045,a2=alpha*alpha,pi=glm::pi<double>();
        const glm::dvec3 v(std::sqrt(1-mu*mu),0,mu);
        const auto stretched=glm::normalize(glm::dvec3(alpha*v.x,0,mu));
        const glm::dvec3 tangent(0,1,0);const auto bitangent=glm::cross(stretched,tangent);
        const double blend=(1+stretched.z)*.5,sv=std::sqrt(mu*mu+a2*(1-mu*mu));
        frame.Camera.ToCamera=glm::vec3(v);material->Roughness=.045f;
        glm::dvec3 reflected(0);
        for(unsigned technique=0;technique<2;++technique)for(unsigned i=0;i<samples;++i) {
            const double u=(i+.5)/samples,phi=2*pi*std::fmod((i+.5)*.6180339887498948482,1.);
            glm::dvec3 l;
            if(technique==0) {
                const double x=std::sqrt(u)*std::cos(phi);
                const double y=(1-blend)*std::sqrt(std::max(0.,1-x*x))+blend*std::sqrt(u)*std::sin(phi);
                const auto disk=tangent*x+bitangent*y+stretched*std::sqrt(std::max(0.,1-x*x-y*y));
                const auto h=glm::normalize(glm::dvec3(alpha*disk.x,alpha*disk.y,std::max(0.,disk.z)));
                l=glm::reflect(-v,h);
            } else l={std::sqrt(u)*std::cos(phi),std::sqrt(u)*std::sin(phi),std::sqrt(1-u)};
            if(l.z<=0)continue;
            const auto h=glm::normalize(v+l);const double den=1-h.z*h.z+a2*h.z*h.z;
            const double pdf=a2/(pi*den*den)/(2*(mu+sv))+l.z/pi;
            frame.Light=SubmittedDirectional{glm::vec3(l),glm::vec3(1)};
            target->Bind();
            if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
            else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
            target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
            const auto result=capture(target);
            if(!result||!result->Completed||!result->Error.empty())throw std::runtime_error("Grazing furnace capture failed");
            reflected+=glm::dvec3(result->Color[0])/(pdf*samples);
        }
        std::ofstream furnace(output/"grazing-furnace.csv");
        furnace<<std::setprecision(12)<<"rough,NoV,r,g,b\n.045,.01,"<<reflected.r<<','<<reflected.g<<','<<reflected.b<<'\n';furnace.close();
        for(unsigned c=0;c<3;++c)if(std::abs(reflected[c]-1)>.003)
            throw std::runtime_error("Grazing direct GPU furnace failed: "+std::to_string(reflected[c]));
    }
    // Actual production sphere images at matched exposure. Geometry/assets are
    // frozen; only private material definitions and transforms are varied.
    auto sphere=assets->Mesh("Reference/Direct_v1/Sphere.hmesh");
    if(!sphere.Accepted)throw std::runtime_error(sphere.Error);
    spec.Width=640;spec.Height=360;target=Framebuffer::Create(spec);view.Reset();
    frame.Camera.ToCamera={0,0,1};frame.Camera.Position={0,0,12};
    frame.Camera.ViewProjection=glm::ortho(-5.6f,5.6f,-3.15f,3.15f,.1f,30.f)*
        glm::lookAt(glm::vec3(0,0,12),glm::vec3(0),glm::vec3(0,1,0));
    frame.Meshes.clear();
    const float rows[]={.045f,.25f,.6f,1.f};
    for(unsigned row=0;row<4;++row)for(unsigned color=0;color<7;++color) {
        auto value=CreateRef<MaterialDefinition>();value->BaseColor=glm::vec4(glm::vec3(colors[color]),1);
        value->Metallic=color<3||color==6?1.f:color==5?.5f:0.f;value->Roughness=rows[row];
        SubmittedMesh instance;instance.Geometry=sphere.Accepted;instance.Materials={value};
        instance.Picking=int(row*7+color+1);
        instance.SetWorld(glm::scale(glm::translate(glm::mat4(1),{(float(color)-3)*1.5f,(1.5f-float(row))*1.5f,0}),glm::vec3(.65f)));
        frame.Meshes.push_back(instance);
    }
    frame.Environment=CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(project->GetAssetRoot(),"Reference/IBL_v1/Photo.henv"));
    auto save=[&](const std::string& name) {
        target->Bind();
        if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
        else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
        target->ClearAttachment(1,-1);
        SceneRenderer::Render(frame,view,target,[]{},[]{});auto result=capture(target);
        if(!result||!result->Completed||!result->Error.empty())throw std::runtime_error("GGX sphere capture failed");
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for(size_t pixel=0;pixel<result->Color.size();++pixel) {
            file.write(reinterpret_cast<const char*>(&result->Color[pixel]),16);
            file.write(reinterpret_cast<const char*>(&result->Entity[pixel]),4);
            file.write(reinterpret_cast<const char*>(&result->Depth[pixel]),4);
        }
        if(!file)throw std::runtime_error("Cannot write GGX component capture");
    };
    frame.Light=SubmittedDirectional{light,glm::vec3(2)};
    for(unsigned component=0;component<3;++component) {
        frame.Debug=component==0?SurfaceView::Direct:component==1?SurfaceView::DiffuseIBL:SurfaceView::SpecularIBL;
        frame.LinearLightingInspection=true;save("linear-"+std::to_string(component));
    }
    frame.Debug=SurfaceView::Shaded;frame.LinearLightingInspection=false;
    for(unsigned phase=0;phase<8;++phase) {
        frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(std::sin(float(phase)*.4f),.5f,1)),glm::vec3(2)};
        save("moving-highlight-"+std::to_string(phase));
    }
    std::cout<<"PASS: Multiscattering_v2 "<<count<<" linear FP32 components; max independent-model error "<<maximum<<"; gold/copper/aluminum, dark/white dielectrics, mixed/perfect, floor/grazing\n";
}
