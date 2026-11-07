#pragma once
// Ordinary saved content and RuntimeSession, not a private showcase renderer.
static void RenderingShowcaseChecks()
{
    const auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_SHOWCASE_PROJECT");
    Check(!selected.empty(),"Showcase checks require a disposable committed project");
    auto base=Project::LoadCandidate(selected);Check(bool(base),"Cannot load showcase selector");
    // Imported manifests remain authorable/packageable, including their detached prefabs.
    for(auto mesh:{"Reference/Direct_v1/Sphere.hmesh","Native/Direct_v1/UnitCube.hmesh"})
        AuditImport(base->GetAssetRoot(),ReadMeshAsset(base->GetAssetRoot(),mesh).Definition.Import);
    const auto metadata=YAML::Load(FileDocument::Read(base->GetAssetRoot()/"views.yaml"));
    RenderCommand::StateScope state;
    FramebufferSpecification spec;spec.Width=1280;spec.Height=720;
    spec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);
    for(auto name:{"Gallery","RoughnessSweep","SurfaceDetail"})
    {
        auto scenePath=std::filesystem::u8path(metadata["Views"][name]["Scene"].as<std::string>());
        Check(AssetHash(FileDocument::Read(base->GetAssetRoot()/scenePath))==metadata["Views"][name]["SceneSHA256"].as<std::string>(),"Named showcase camera/scene bytes changed");
        auto scene=base->LoadScene(scenePath);auto authored=SceneSerializer(scene).CaptureAuthoredState();
        RuntimeSession runtime;runtime.Resize(1280,720);runtime.ResizeFramebuffer(1280,720);runtime.SetRenderTarget(target);runtime.Start(base,scene);
        target->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();target->ClearAttachment(1,-1);runtime.SetInput({-100,-100},false);runtime.Update(0);
        Check(runtime.GetError().empty(),"Saved showcase view did not render");
        std::vector<int> ids(1280*720);glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,1280,720,GL_RED_INTEGER,GL_INT,ids.data());
        bool meshVisible=false;for(auto entity:runtime.GetScene()->GetAllEntitiesWith<MeshRendererComponent>())meshVisible|=std::find(ids.begin(),ids.end(),int(entity))!=ids.end();
        Check(meshVisible,"Showcase rendered no visible real mesh/material content");
        const auto capture=FileSystem::GetEnvironmentPath("HAZEL_IBL_QUALITY_CAPTURE");
        if(!capture.empty())
        {
            Check(capture.is_absolute(),"Showcase capture requires an owned absolute path");std::filesystem::create_directories(capture);
            auto outputSpec=spec;outputSpec.Attachments={FramebufferTextureFormat::RGBA8};auto output=Framebuffer::Create(outputSpec);target->EncodeOutput(output);output->Bind();
            std::vector<unsigned char> rgb(size_t(1280)*720*4);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,rgb.data());
            std::ofstream file(capture/(std::string("showcase-")+name+".ppm"),std::ios::binary);file<<"P6\n1280 720\n255\n";
            for(int y=719;y>=0;--y)for(int x=0;x<1280;++x)file.write(reinterpret_cast<const char*>(rgb.data()+(size_t(y)*1280+x)*4),3);Check(bool(file),"Cannot save focused showcase inspection frame");output->Unbind();target->Bind();
        }
        if(std::string(name)=="Gallery"){
            auto primary=runtime.GetScene()->GetPrimaryCameraEntity();auto originalWorld=runtime.GetScene()->GetWorldTransform(primary);
            runtime.SetCameraInspection(true);auto* camera=runtime.GetInspectionCamera();auto seed=*camera;
            auto exact=primary.GetComponent<CameraComponent>().Camera.GetProjection()*glm::inverse(originalWorld);
            for(int c=0;c<4;++c)Check(glm::length(camera->GetViewProjection()[c]-exact[c])<.0001f,"Inspection did not start at exact authored camera");
            for(int step=0;step<5;++step){camera->Orbit({80,0});runtime.Update(0);}
            Check(camera->GetYaw()>seed.GetYaw()+1.9f,"Gallery continuous orbit did not exceed 90 degrees");
            for(uint64_t study:{120,121}){
                auto entity=runtime.GetScene()->GetEntityByUUID(study);auto world=runtime.GetScene()->GetWorldTransform(entity);
                auto geometry=base->GetAssets()->Mesh(entity.GetComponent<MeshRendererComponent>().Mesh).Accepted;
                glm::vec3 lo(1e20f),hi(-1e20f);for(int corner=0;corner<8;++corner){auto p=glm::vec3(world*glm::vec4((corner&1)?geometry->Geometry.Bounds.Max.x:geometry->Geometry.Bounds.Min.x,(corner&2)?geometry->Geometry.Bounds.Max.y:geometry->Geometry.Bounds.Min.y,(corner&4)?geometry->Geometry.Bounds.Max.z:geometry->Geometry.Bounds.Min.z,1));lo=glm::min(lo,p);hi=glm::max(hi,p);}
                runtime.ResetCameraInspection();camera=runtime.GetInspectionCamera();camera->FrameBounds(lo,hi);auto eye=camera->GetPosition();camera->Fly({8,0},{0,0,1},.25f,.2f);runtime.Update(0);
                Check(glm::length(camera->GetPosition()-eye)>.049f,"Close-up inspection fly did not advance");
                target->Bind();glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,1280,720,GL_RED_INTEGER,GL_INT,ids.data());
                Check(std::find(ids.begin(),ids.end(),int(entt::entity(entity)))!=ids.end(),"Approached wood/leather study is not actually visible");
            }
            runtime.ResetCameraInspection();camera=runtime.GetInspectionCamera();for(int c=0;c<4;++c)Check(camera->GetViewProjection()[c]==seed.GetViewProjection()[c],"Inspection reset failed exact authored view");
            Check(runtime.GetScene()->GetWorldTransform(primary)==originalWorld,"Inspection changed runtime camera transform");runtime.SetCameraInspection(false);runtime.Update(0);
            std::cout<<"PASS: opt-in Gallery authored view, >90 degree sphere orbit, visible slow wood/leather approaches and exact reset without camera/document changes\n";
        }
        runtime.Stop();Check(SceneSerializer(scene).CaptureAuthoredState()==authored,"Showcase Play/Stop modified authored data");
    }
    target->Unbind();Check(glGetError()==GL_NO_ERROR,"Showcase OpenGL error");
    std::cout<<"PASS: saved Gallery/RoughnessSweep/SurfaceDetail, licensed color/data materials, native shared runtime and Play/Stop without a script stub\n";
}
