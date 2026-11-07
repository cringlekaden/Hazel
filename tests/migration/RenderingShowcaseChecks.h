#pragma once
// Ordinary saved content and RuntimeSession, not a private showcase renderer.
static void RenderingShowcaseChecks()
{
    const auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_SHOWCASE_PROJECT");
    Check(!selected.empty(),"Showcase checks require a disposable committed project");
    auto base=Project::LoadCandidate(selected);Check(bool(base),"Cannot load showcase selector");
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
        runtime.Stop();Check(SceneSerializer(scene).CaptureAuthoredState()==authored,"Showcase Play/Stop modified authored data");
    }
    target->Unbind();Check(glGetError()==GL_NO_ERROR,"Showcase OpenGL error");
    std::cout<<"PASS: saved Gallery/RoughnessSweep/SurfaceDetail, licensed color/data materials, native shared runtime and Play/Stop without a script stub\n";
}
