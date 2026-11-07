#pragma once
#include "Hazel/Assets/ProjectAssets.h"
// Saved additive IBL cases use the same editor/player scene submission. The
// independent radiance oracle lives in Renderer2DSmoke, not a second renderer.
static void IBLReferenceChecks()
{
    const auto selected = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    if (selected.empty())
        throw std::runtime_error("IBL Reference requires an explicit maintained project selector");
    auto project = Project::LoadCandidate(selected);
    auto root = project->GetAssetRoot();
    auto conditions = YAML::Load(FileDocument::Read(root / "Reference/IBL_v1/conditions.yaml"));
    Check(conditions["Version"].as<unsigned>() == 1 &&
              conditions["Recipe"].as<std::string>() == IBLRecipe,
          "Frozen IBL recipe changed");
    for (auto item : conditions["Files"])
    {
        auto p = std::filesystem::u8path(item.first.as<std::string>());
        if (AssetHash(ReadAssetBytes(Project::ResolveOwnedAsset(root, p),
                                     LightingArtifactByteLimit)) != item.second.as<std::string>())
            throw std::runtime_error("Frozen IBL_v1 condition changed: " + p.generic_u8string());
    }
    RenderCommand::StateScope scope;
    for (auto name : {"DirectOnly", "IBLOnly", "Combined", "Photo", "AxesYaw90"})
    {
        auto entry = conditions["Cases"][name];
        auto path = std::filesystem::u8path(entry["Scene"].as<std::string>());
        Check(AssetHash(FileDocument::Read(root / path)) == entry["SceneHash"].as<std::string>(),
              "Frozen IBL scene bytes changed");
        auto authored = project->LoadScene(path);
        auto state = SceneSerializer(authored).CaptureAuthoredState();
        auto &env = authored->Presentation().Environment;
        Check(env.Yaw == entry["Yaw"].as<float>() && env.Gain == entry["Gain"].as<float>() &&
                  authored->Presentation().Exposure == 0,
              "Frozen environment instance/exposure changed");
        for (unsigned scale : {1u, 2u})
        {
            if (scale == 2 && std::string(name) != "Combined")
                continue;
            FramebufferSpecification spec;
            spec.Width = 1280 * scale;
            spec.Height = 720 * scale;
            spec.Attachments = {FramebufferTextureFormat::RGBA16F,
                                FramebufferTextureFormat::RED_INTEGER,
                                FramebufferTextureFormat::Depth};
            auto target = Framebuffer::Create(spec);
            RuntimeSession runtime;
            runtime.Resize(1280, 720);
            runtime.ResizeFramebuffer(spec.Width, spec.Height);
            runtime.SetRenderTarget(target);
            runtime.Start(project, authored);
            runtime.SetInput({-100, -100}, false);
            auto prepare = [&] {
                target->Bind();
                auto raster = RenderCommand::GetRasterState();
                raster.ClipEnabled = false;
                raster.DepthTest = raster.DepthWrite = true;
                raster.PickingMask = raster.ColorMask = glm::bvec4(true);
                RenderCommand::SetRasterState(raster);
                RenderCommand::SetClearColor(authored->Presentation().Background);
                RenderCommand::Clear();
                target->ClearAttachment(1, -1);
            };
            prepare();
            runtime.Update(0);
            Check(runtime.GetError().empty(), "Saved IBL runtime rendering failed");
            double height = 20 * std::tan(std::acos(-1.) / 8);
            std::vector<glm::vec4> samples;
            auto point = [&](double x, double y) {
                return glm::ivec2((.5 + x / (height * 1280 / 720)) * spec.Width,
                                  (.5 + y / height) * spec.Height);
            };
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 6; ++col)
                {
                    auto p = point(-3.75 + 1.5 * col, 1.7 - 1.5 * row);
                    Check(
                        target->ReadPixel(1, p.x, p.y) ==
                            int(uint32_t(runtime.GetScene()->GetEntityByUUID(10 + row * 6 + col))),
                        "IBL saved grid picking/framebuffer scale");
                    samples.push_back(target->ReadColorPixel(0, p.x, p.y));
                }
            auto capture=FileSystem::GetEnvironmentPath("HAZEL_IBL_CAPTURE");
            if(scale==1 && std::string(name)=="Photo" && !capture.empty()) {
                auto outputSpec=spec;outputSpec.Attachments={FramebufferTextureFormat::RGBA8};auto output=Framebuffer::Create(outputSpec);target->EncodeOutput(output);output->Bind();
                GLint alignment=0;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);
                std::vector<unsigned char> rgb(size_t(spec.Width)*spec.Height*3);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,spec.Width,spec.Height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
                std::ofstream file(capture,std::ios::binary);Check(bool(file),"Cannot open IBL inspection capture");file<<"P6\n"<<spec.Width<<' '<<spec.Height<<"\n255\n";for(int y=int(spec.Height)-1;y>=0;--y)file.write(reinterpret_cast<const char*>(rgb.data()+size_t(y)*spec.Width*3),spec.Width*3);Check(bool(file),"Cannot write IBL inspection capture");output->Unbind();
            }
            runtime.Stop();
            NavigationCamera camera(45, float(spec.Width) / spec.Height, .1f, 100);
            camera.SetViewportSize(spec.Width, spec.Height);
            camera.RestoreOrbit({0, 0, 0}, 0, 0, 10);
            authored->OnViewportResize(1280, 720);
            SceneRenderView view;
            prepare();
            authored->OnUpdateEditor(0, camera, &view, target);
            Check(authored->RenderError().empty(), "Saved IBL editor rendering failed");
            size_t i = 0;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 6; ++col)
                {
                    auto p = point(-3.75 + 1.5 * col, 1.7 - 1.5 * row);
                    Check(glm::length(target->ReadColorPixel(0, p.x, p.y) - samples[i++]) < .002,
                          "Saved IBL editor/player divergence");
                }
            target->Unbind();
            Check(SceneSerializer(authored).CaptureAuthoredState() == state,
                  "IBL runtime/editor altered authored content");
        }
        // Copy only native runtime closure into a disposable project. No HDR or
        // producer/cache executable is available there, even incidentally.
        auto detached =
            std::filesystem::temp_directory_path() / ("hazel-ibl-source-free-" + AssetKey(UUID()));
        std::filesystem::create_directories(detached / "Assets");
        struct Cleanup
        {
            std::filesystem::path Root;
            ~Cleanup()
            {
                std::error_code ec;
                std::filesystem::remove_all(Root, ec);
            }
        } cleanup{detached};
        auto closure = AuditSpriteAssets(root, {path});
        closure.push_back(path);
        for (auto &p : closure)
        {
            Check(p.extension() != ".hdr" && p.extension() != ".glb",
                  "Producer input entered prepared runtime closure");
            auto target = detached / "Assets" / p;
            std::filesystem::create_directories(target.parent_path());
            std::filesystem::copy_file(root / p, target,
                                       std::filesystem::copy_options::overwrite_existing);
        }
        auto relocated = CreateRef<Project>();
        relocated->GetConfig() = project->GetConfig();
        relocated->GetConfig().StartScene = path;
        auto descriptor = detached / "Reference.hproj";
        FileSystem::WriteNewFile(descriptor, ProjectSerializer(relocated).SerializeText());
        relocated = Project::LoadCandidate(descriptor);
        auto scene = relocated->LoadScene(path);
        RuntimeSession::Validate(scene, false);
        Check(AuditEnvironment(relocated->GetAssetRoot(), env.Asset).size() == 6,
              "Source-free prepared environment closure failed");
    }
    Check(glGetError() == GL_NO_ERROR, "IBL Reference OpenGL error");
    std::cout
        << "PASS: frozen IBL_v1 DirectOnly/IBLOnly/Combined/Photo/yaw saved content, editor/player "
           "equivalence, 2x picking and HDR/parser-free native dependency closure\n";
}
