#pragma once
#include <glad/glad.h>
#include <algorithm>
#include "Hazel/Assets/AssetData.h"
#include "Hazel/Assets/Geometry.h"
#include "Hazel/Core/FileDocument.h"
// Both native camera paths consume the same saved, frozen Direct_v1 content.
static void DirectReferenceChecks()
{
    auto descriptor = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    if (descriptor.empty())
    {
        std::cout << "NOT EXECUTED: Direct_v1 (no project selector)\n";
        return;
    }
    RenderCommand::StateScope scope;
    auto project = Project::LoadCandidate(descriptor);
    auto root = project->GetAssetRoot();
    const auto conditions =
        YAML::Load(FileDocument::Read(root / "Reference/Direct_v1/conditions.json"));
    Check(conditions["Version"].as<int>() == 1 && conditions["Root"].as<std::string>() == "Assets",
          "Direct condition contract changed");
    for (auto file : conditions["Files"])
    {
        const auto reference = std::filesystem::u8path(file.first.as<std::string>());
        Check(AssetHash(ReadAssetBytes(Project::ResolveOwnedAsset(root, reference),
                                       GeometryByteLimit)) == file.second.as<std::string>(),
              "Frozen Direct_v1 geometry/material/image/reference condition changed");
    }
    auto metadata = YAML::Load(FileDocument::Read(root / "Reference/Direct_v1/outputs.json"));
    for (auto name : {"PerspectivePBR", "OrthographicPBR", "PerspectiveUnlit"})
    {
        auto reference =
            std::filesystem::path("Scenes") / (std::string("Direct_v1-") + name + ".hazel");
        Check(AssetHash(FileDocument::Read(root / reference)) ==
                  metadata["Scenes"][reference.generic_u8string()].as<std::string>(),
              "Frozen Direct_v1 scene bytes changed");
        auto authored = project->LoadScene(reference);
        auto before = SceneSerializer(authored).CaptureAuthoredState();
        for (unsigned scale : {1u, 2u})
        {
            unsigned width = 1280 * scale, height = 720 * scale;
            FramebufferSpecification spec;
            spec.Width = width;
            spec.Height = height;
            spec.Attachments = {FramebufferTextureFormat::RGBA16F,
                                FramebufferTextureFormat::RED_INTEGER,
                                FramebufferTextureFormat::Depth};
            auto composite = Framebuffer::Create(spec);
            SceneRenderView editorView;
            auto prepare = [&] {
                composite->Bind();
                auto raster = RenderCommand::GetRasterState();
                raster.DepthTest = raster.DepthWrite = true;
                raster.Compare = DepthCompare::Less;
                raster.ClipEnabled = false;
                raster.ColorMask = raster.PickingMask = glm::bvec4(true);
                RenderCommand::SetRasterState(raster);
                RenderCommand::SetClearColor(authored->Presentation().Background);
                RenderCommand::Clear();
                composite->ClearAttachment(1, -1);
            };
            RuntimeSession runtime;
            runtime.Resize(1280, 720);
            runtime.ResizeFramebuffer(width, height);
            runtime.SetRenderTarget(composite);
            runtime.Start(project, authored);
            runtime.SetInput({-100, -100}, false);
            prepare();
            runtime.Update(0);
            Check(runtime.GetError().empty(), "Direct Reference runtime failed");
            bool ortho = std::string(name) == "OrthographicPBR";
            double h = ortho ? 10. : 20 * std::tan(std::acos(-1.) / 8);
            auto point = [&](double x, double y) {
                return glm::ivec2((.5 + x / (h * 1280 / 720)) * width, (.5 + y / h) * height);
            };
            std::vector<glm::vec4> samples;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 6; ++col)
                {
                    auto p = point(-3.75 + 1.5 * col, 1.7 - 1.5 * row);
                    Check(
                        composite->ReadPixel(1, p.x, p.y) ==
                            int(uint32_t(runtime.GetScene()->GetEntityByUUID(10 + row * 6 + col))),
                        "Direct Reference sphere picking/framebuffer scale");
                    samples.push_back(composite->ReadColorPixel(0, p.x, p.y));
                }
            if (scale == 1 && std::string(name) == "PerspectivePBR")
            {
                auto path = FileSystem::GetEnvironmentPath("HAZEL_DIRECT_CAPTURE");
                if (!path.empty())
                {
                    spec.Attachments = {FramebufferTextureFormat::RGBA8};
                    auto output = Framebuffer::Create(spec);
                    composite->EncodeOutput(output);
                    output->Bind();
                    std::vector<unsigned char> rgb(width * height * 3);
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
                    std::ofstream file(path, std::ios::binary);
                    file << "P6\n" << width << ' ' << height << "\n255\n";
                    for (int y = int(height) - 1; y >= 0; --y)
                        file.write(reinterpret_cast<char *>(rgb.data() + y * width * 3), width * 3);
                }
            }
            runtime.Stop();
            authored->OnViewportResize(1280, 720);
            EditorCamera camera(45, float(width) / height, .1f, 100);
            camera.SetViewportSize(width, height);
            camera.RestoreOrbit({0, 0, 0}, 0, 0, 10);
            if (ortho)
            {
                camera.RestorePlanar({0, 0}, 10);
                camera.SetNavigation(EditorNavigation::Planar);
            }
            prepare();
            authored->OnUpdateEditor(0, camera, &editorView, composite);
            Check(authored->RenderError().empty(), "Direct Reference editor draw failed");
            unsigned i = 0;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 6; ++col)
                {
                    auto p = point(-3.75 + 1.5 * col, 1.7 - 1.5 * row);
                    auto color = composite->ReadColorPixel(0, p.x, p.y);
                    Check(glm::length(color - samples[i++]) < .002f,
                          "Editor/player saved direct content diverged");
                }
            composite->Unbind();
            Check(SceneSerializer(authored).CaptureAuthoredState() == before,
                  "Direct Reference Play/editor modified authored state");
        }
    }
    auto surfaces =
        YAML::Load(FileDocument::Read(root / "Reference/Direct_v1/Surfaces/outputs.json"));
    auto surfacePath = std::filesystem::u8path(surfaces["Scene"].as<std::string>());
    Check(AssetHash(FileDocument::Read(root / surfacePath)) ==
              surfaces["SceneHash"].as<std::string>(),
          "Frozen surface scene changed");
    auto authored = project->LoadScene(surfacePath);
    auto state = SceneSerializer(authored).CaptureAuthoredState();
    for (unsigned scale : {1u, 2u})
    {
        FramebufferSpecification spec;
        spec.Width = 1280 * scale;
        spec.Height = 720 * scale;
        spec.Attachments = {FramebufferTextureFormat::RGBA16F,
                            FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth};
        auto target = Framebuffer::Create(spec);
        RuntimeSession runtime;
        runtime.Resize(1280, 720);
        runtime.ResizeFramebuffer(spec.Width, spec.Height);
        runtime.SetRenderTarget(target);
        runtime.Start(project, authored);
        target->Bind();
        RenderCommand::SetClearColor(authored->Presentation().Background);
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        runtime.Update(0);
        std::vector<int> ids(size_t(spec.Width) * spec.Height);
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(0, 0, spec.Width, spec.Height, GL_RED_INTEGER, GL_INT, ids.data());
        for (uint64_t id : {10, 11, 12, 13, 14, 15, 40})
        {
            auto entity = runtime.GetScene()->GetEntityByUUID(id);
            Check(entity && std::find(ids.begin(), ids.end(), int(uint32_t(entity))) != ids.end(),
                  "Surface reference instance missing / unpickable");
        }
        runtime.Stop();
        target->Unbind();
        Check(SceneSerializer(authored).CaptureAuthoredState() == state,
              "Surface Play modified authored state");
    }
    Check(glGetError() == GL_NO_ERROR, "Direct Reference OpenGL error");
    std::cout << "PASS: frozen Direct_v1 saved orthographic PBR/perspective PBR/unlit, same "
                 "editor/player submissions, sphere grid, mapped/mirrored/masked/COLOR0 surface "
                 "cases and 2x picking\n";
}
