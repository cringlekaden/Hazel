#pragma once
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include <glad/glad.h>
static void IBLQualityRenderingChecks()
{
    const auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    Check(!selected.empty(),"IBL quality requires maintained Reference content");
    auto project=Project::LoadCandidate(selected);const auto root=project->GetAssetRoot();
    const auto metadata=YAML::Load(FileDocument::Read(root/"Reference/IBLQuality_v2/conditions.yaml"));
    Check(metadata["Version"].as<unsigned>()==2 && metadata["PreparationRecipe"].as<std::string>()==EnvironmentPreparationRecipe,"IBL quality conditions changed");
    for(auto entry:metadata["Files"])
        Check(AssetHash(ReadAssetBytes(root/std::filesystem::u8path(entry.first.as<std::string>()),LightingArtifactByteLimit))==entry.second.as<std::string>(),"Frozen IBLQuality_v2 bytes changed");
    auto requireGL=[](const std::string &where){auto error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error(where+": GL error "+std::to_string(error));};
    requireGL("Before IBL quality checks");
    RenderCommand::StateScope stateScope;
    auto captures=FileSystem::GetEnvironmentPath("HAZEL_IBL_QUALITY_CAPTURE");
    if(!captures.empty()){Check(captures.is_absolute(),"Quality capture requires an owned absolute output path");std::filesystem::create_directories(captures);}
    FramebufferSpecification spec;spec.Width=1280;spec.Height=720;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);SceneRenderView view;
    auto compositeSpec=spec;compositeSpec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto composite=Framebuffer::Create(compositeSpec);
    auto read=[&](){std::vector<glm::vec4> pixels(size_t(spec.Width)*spec.Height);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,spec.Width,spec.Height,GL_RGBA,GL_FLOAT,pixels.data());return pixels;};
    auto capture=[&](const std::string &name){
        if(captures.empty())return;
        auto displaySpec=spec;displaySpec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
        auto display=Framebuffer::Create(displaySpec);display->Bind();target->CompositeWorldTo(display,0);
        auto outputSpec=spec;outputSpec.Attachments={FramebufferTextureFormat::RGBA8};auto output=Framebuffer::Create(outputSpec);display->EncodeOutput(output);output->Bind();
        std::vector<unsigned char> pixels(size_t(spec.Width)*spec.Height*4);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,spec.Width,spec.Height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        std::ofstream file(captures/(name+".ppm"),std::ios::binary);file<<"P6\n"<<spec.Width<<' '<<spec.Height<<"\n255\n";
        for(int y=int(spec.Height)-1;y>=0;--y)for(unsigned x=0;x<spec.Width;++x)file.write(reinterpret_cast<const char*>(pixels.data()+(size_t(y)*spec.Width+x)*4),3);
        Check(bool(file),"Cannot write focused IBL capture");output->Unbind();target->Bind();
    };
    for(auto name:{"Photo","TinyEmitter","Constant"})
    {
        const auto entry=metadata["Cases"][name];
        auto authored=project->LoadScene(std::filesystem::u8path(entry["Scene"].as<std::string>()));
        authored->OnViewportResize(spec.Width,spec.Height);
        auto authoredState=SceneSerializer(authored).CaptureAuthoredState();
        auto cameraEntity=authored->GetEntityByUUID(1);const auto &camera=cameraEntity.GetComponent<CameraComponent>().Camera;
        auto world=authored->GetWorldTransform(cameraEntity);
        SceneCameraView cameraView{camera.GetProjection()*glm::inverse(world),glm::vec3(world[3]),glm::normalize(glm::vec3(world[2])),camera.GetProjectionType()==SceneCamera::ProjectionType::Orthographic};
        auto frame=SceneRenderer::Collect(*authored,cameraView);frame.Light.reset();
        frame.Presentation.Background={0,0,0,1}; // Same black surround for term/combined captures.
        // Only the controlled 18 spheres: labels/other world content cannot
        // contaminate a lighting-term or energy comparison.
        std::vector<SubmittedMesh> controlled;std::vector<int> metallicIDs;
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<6;++col)
        {
            int id=int(uint32_t(authored->GetEntityByUUID(10+row*6+col)));
            auto draw=std::find_if(frame.Meshes.begin(),frame.Meshes.end(),[&](auto &d){return d.Picking==id;});
            Check(draw!=frame.Meshes.end(),"Controlled IBL sphere was culled/missing");controlled.push_back(*draw);if(row==2)metallicIDs.push_back(id);
        }
        frame.Meshes=controlled;
        auto current=frame.Environment;
        // CPU decoded artifact -> all actual uploaded GPU faces and mip levels.
        auto resources=frame.Assets->RealizeEnvironment(current);
        auto verifyGPUData=[&] {
        RenderCommand::StateScope uploadState;
        for(auto pair:{std::make_pair(&current->Irradiance,resources->Irradiance),std::make_pair(&current->Specular,resources->Specular)})
        {
            pair.second->Bind(6);unsigned side=pair.first->Side;
            for(unsigned level=0;level<pair.first->Mips.size();++level,side=std::max(1u,side/2))for(unsigned face=0;face<6;++face)
            {
                std::vector<float> pixels(size_t(side)*side*3);glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,level,GL_RGB,GL_FLOAT,pixels.data());
                Check(std::equal(pixels.begin(),pixels.end(),pair.first->Mips[level].begin()+size_t(face)*side*side*3),"Prepared IBL data changed during actual GPU face/mip upload");
            }
        }
        };
        verifyGPUData();
        requireGL(std::string(name)+" explicit cube readback");
        for(bool baseline:{true,false})
        {
            if(baseline && std::string(name)=="TinyEmitter")continue; // CPU baseline is separately inventoried research data.
            frame.Environment=baseline?CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(root,std::string("Reference/IBL_v1/")+(std::string(name)=="Photo"?"Photo":"Constant")+".henv")):current;
            for(unsigned step=0;step<4;++step)
            {
                frame.Presentation.Environment.Yaw=step*.08f;
                frame.Presentation.Environment.Background=false;
                std::vector<glm::vec4> diffuse,specular;
                for(auto mode:{SurfaceView::DiffuseIBL,SurfaceView::SpecularIBL,SurfaceView::Shaded})
                {
                    auto raster=RenderCommand::GetRasterState();raster.ClipEnabled=false;raster.DepthTest=raster.DepthWrite=true;raster.ColorMask=raster.PickingMask=glm::bvec4(true);RenderCommand::SetRasterState(raster);
                    auto destination=mode==SurfaceView::Shaded?composite:target;
                    destination->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();destination->ClearAttachment(1,-1);
                    frame.Debug=mode;frame.LinearLightingInspection=mode!=SurfaceView::Shaded;
                    SceneRenderer::Render(frame,view,destination,[]{},[]{});
                    requireGL(std::string(name)+" lighting draw");
                    auto output=mode==SurfaceView::Shaded?view.Radiance:target;output->Bind();
                    auto pixels=read();
                    for(auto &p:pixels)Check(std::isfinite(p.r)&&std::isfinite(p.g)&&std::isfinite(p.b)&&std::min({p.r,p.g,p.b})>=0,"Nonfinite/negative IBL radiance");
                    const char *term=mode==SurfaceView::DiffuseIBL?"diffuse":mode==SurfaceView::SpecularIBL?"specular":"combined";
                    // Capture exactly the same camera/materials/EV for old/new.
                    const bool saveFrame=(std::string(name)=="Photo" && (step==0 || (!baseline && mode==SurfaceView::Shaded)))
                        || (!baseline && std::string(name)=="TinyEmitter" && (step==0 || (step==3 && mode==SurfaceView::Shaded)))
                        || (!baseline && std::string(name)=="Constant" && step==0 && mode==SurfaceView::Shaded);
                    if(!captures.empty() && saveFrame) {
                        if(mode==SurfaceView::Shaded){auto held=target;target=view.Radiance;capture(std::string(baseline?"v1-":"v2-")+name+"-"+term+"-yaw"+std::to_string(step));target=held;}
                        else capture(std::string(baseline?"v1-":"v2-")+name+"-"+term+"-yaw"+std::to_string(step));
                    }
                    if(mode==SurfaceView::DiffuseIBL){diffuse=std::move(pixels);std::vector<int> ids(size_t(spec.Width)*spec.Height);glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,spec.Width,spec.Height,GL_RED_INTEGER,GL_INT,ids.data());for(size_t i=0;i<ids.size();++i)if(std::find(metallicIDs.begin(),metallicIDs.end(),ids[i])!=metallicIDs.end())Check(glm::length(glm::vec3(diffuse[i]))<1e-6,"Fully metallic spheres have nonzero diffuse IBL");}
                    else if(mode==SurfaceView::SpecularIBL)specular=std::move(pixels);
                    else {for(size_t i=0;i<pixels.size();++i)for(int c=0;c<3;++c)Check(std::abs(pixels[i][c]-diffuse[i][c]-specular[i][c])<1e-5f*std::max(1.f,pixels[i][c]),"Combined IBL differs from independently captured diffuse+specular");}
                    requireGL(std::string(name)+" term capture/readback");
                    target->Bind();
                }
            }
        }
        verifyGPUData(); // No draw/presentation pass may regenerate or overwrite the chain.
        resources.reset();
        Check(SceneSerializer(authored).CaptureAuthoredState()==authoredState,"Quality inspection changed authored content");
    }
    target->Unbind();Check(glGetError()==GL_NO_ERROR,"IBL quality readback OpenGL error");
    std::cout<<"PASS: IBLQuality_v2 controlled metallic/roughness grid, zero metallic diffuse, separated FP32 terms/combined, yaw motion and all actual GPU faces/mips; same pipeline for retained v1 data\n";
}
