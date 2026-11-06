#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Assets/ImportedAsset.h"
#include "UI/PropertyUI.h"
#include <imgui.h>
#include <algorithm>
namespace Hazel
{
void AuthoringPanel::BeginImport(const std::filesystem::path &folder,
                                 const std::filesystem::path &source)
{
    if (!Require(EditorAction::EditAsset))
        return;
    std::string file = source.generic_u8string();
    if (file.empty())
        file = FileDialogs::OpenFile("glTF Binary (*.glb)\0*.glb\0");
    if (file.empty())
        return;
    m_ImportSource = file;
    auto name = std::filesystem::u8path(file).stem().generic_u8string();
    for (auto &c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            c = '_';
    if (name.empty())
        name = "Model";
    m_ImportDestination = (folder / "Imported" / name).generic_u8string();
    m_ImportScene = -1;
    m_ImportError.clear();
    m_ShowImport = true;
}
bool AuthoringPanel::StartImport(bool reimport, const std::filesystem::path &manifest)
{
    if (!Require(EditorAction::StartTool) || !Require(EditorAction::EditAsset) ||
        !Project::GetActive())
        return false;
    try
    {
        m_Editor.m_ProjectFile.Check();
        ToolRequest request;
        request.NativeExecutable = ProjectTools::ImportTool();
        request.Project = m_Editor.m_ProjectPath;
        request.Generation = m_ProjectGeneration;
        request.Completion = ToolCompletion::RefreshImportedAssets;
        request.Label = reimport ? "Reimport GLB" : "Import GLB";
        if (reimport)
        {
            AssetPath(manifest, ".himport");
            request.Arguments = {"reimport-glb", request.Project.generic_u8string(),
                                 manifest.generic_u8string()};
            request.Artifact = Project::GetAssetDirectory() / manifest;
        }
        else
        {
            auto source = std::filesystem::u8path(m_ImportSource);
            if (!source.is_absolute() || source.extension() != ".glb" ||
                !std::filesystem::is_regular_file(source))
                throw std::runtime_error("Choose an existing absolute .glb source file");
            auto destination = std::filesystem::u8path(m_ImportDestination);
            AssetPath(destination);
            if (std::filesystem::exists(
                    Project::ResolveOwnedAsset(Project::GetAssetDirectory(), destination)))
                throw std::runtime_error("Destination exists; choose a new folder or select "
                                         "Import.himport for Reimport");
            request.Arguments = {"import-glb",
                                 request.Project.generic_u8string(),
                                 source.generic_u8string(),
                                 destination.generic_u8string(),
                                 "--scene",
                                 std::to_string(m_ImportScene)};
            request.Artifact = Project::GetAssetDirectory() / destination / "Import.himport";
        }
        if (!m_Tools.Start(std::move(request)))
            throw std::runtime_error("A tool operation is already running");
        m_ImportError.clear();
        ShowConsole();
        return true;
    }
    catch (const std::exception &e)
    {
        m_ImportError = e.what();
        m_Editor.ActionFailed(m_ImportError);
        return false;
    }
}
void AuthoringPanel::ImportAssets()
{
    if (m_ShowImport)
    {
        ImGui::SetNextWindowSize({560, 0}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Import GLB", &m_ShowImport, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(
                "Static triangles, UV0 and embedded 8-bit PNG/JPEG. Unsupported "
                "content is rejected with diagnostics. Produces native meshes, "
                "materials and a detached prefab. Lit materials use scene-referred presentation.");
            ImGui::BeginDisabled(m_Tools.Busy() || !Availability(EditorAction::EditAsset));
            PropertyUI::ReadOnly("source", "Source", m_ImportSource.c_str());
            PropertyUI::Text("destination", "New asset folder", m_ImportDestination);
            PropertyUI::DragInt("scene", "Scene index", m_ImportScene, 1, -1, 9999);
            ImGui::TextDisabled("-1 selects the declared default or sole scene.");
            if (ImGui::Button("Import"))
                StartImport(false);
            ImGui::EndDisabled();
            if (m_Tools.Busy() && !m_Tools.Request().NativeExecutable.empty())
            {
                ImGui::TextWrapped("%s — progress and results appear in Console.",
                                   m_Tools.Request().Label.c_str());
                if (ImGui::Button("Cancel import"))
                    m_Tools.Cancel();
            }
            PropertyUI::Validation(m_ImportError.c_str());
        }
        ImGui::End();
    }
    if (!m_ShowMesh || !Project::GetActive())
        return;
    ImGui::SetNextWindowSize({580, 520}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Mesh / Import metadata", &m_ShowMesh))
        try
        {
            auto assets = Project::GetActive()->GetAssets();
            if (m_MetadataReference != m_MeshReference || m_MetadataEpoch != assets->Epoch())
            {
                const bool different = m_MetadataReference != m_MeshReference;
                uint64_t selected =
                    !different && m_Metadata ? m_Metadata->Meshes[size_t(m_MetadataMesh)].ID : 0;
                if (different)
                {
                    m_Metadata.reset();
                    m_MetadataAccepted.reset();
                    m_MetadataImport.clear();
                }
                m_MetadataReference = m_MeshReference;
                m_MetadataEpoch = assets->Epoch();
                m_MetadataError.clear();
                try
                {
                    auto reference = m_MeshReference;
                    auto import =
                        reference.extension() == ".himport"
                            ? reference
                            : ReadMesh(Project::ResolveOwnedAsset(assets->Root(), reference))
                                  .Import;
                    auto candidate = ReadImport(Project::ResolveOwnedAsset(assets->Root(), import));
                    int choice = 0;
                    for (size_t i = 0; i < candidate.Meshes.size(); ++i)
                        if (candidate.Meshes[i].Output == reference ||
                            candidate.Meshes[i].ID == selected)
                            choice = int(i);
                    m_Metadata = std::move(candidate);
                    m_MetadataImport = import;
                    m_MetadataMesh = choice;
                }
                catch (const std::exception &e)
                {
                    m_MetadataError = std::string(e.what()).substr(0, 1024);
                }
            }
            if (ImGui::Button("Refresh metadata"))
                assets->Refresh();
            PropertyUI::Validation(m_MetadataError.c_str());
            if (!m_Metadata)
                throw std::runtime_error(
                    "No accepted import metadata; correct the reported file and Refresh metadata");
            auto &manifest = *m_Metadata;
            PropertyUI::ReadOnly("manifest", "Import", m_MetadataImport.generic_u8string().c_str());
            PropertyUI::ReadOnly("source", "Owned source",
                                 manifest.Source.generic_u8string().c_str());
            PropertyUI::ReadOnly("generation", "Accepted generation",
                                 AssetKey(manifest.Generation).c_str());
            if (!manifest.SourceNotice.empty() && ImGui::TreeNode("Source copyright / attribution"))
            {
                ImGui::TextWrapped("%s", manifest.SourceNotice.c_str());
                ImGui::TreePop();
            }
            ImGui::TextWrapped("Instances share accepted geometry resources. Slot bindings are "
                               "independent of source array order.");
            ImGui::BeginDisabled(!Availability(EditorAction::StartTool) || m_Tools.Busy() ||
                                 !m_MetadataError.empty());
            if (ImGui::Button("Reimport owned source"))
                StartImport(true, m_MetadataImport);
            ImGui::EndDisabled();
            PropertyUI::WrapButton("Open detached prefab");
            if (ImGui::Button("Open detached prefab"))
                SelectAsset(assets->Root() / manifest.Prefab);
            PropertyUI::WrapButton("Instantiate prefab");
            ImGui::BeginDisabled(!Availability(EditorAction::EditScene));
            if (ImGui::Button("Instantiate prefab"))
                InstantiatePrefab(assets->Root() / manifest.Prefab);
            ImGui::EndDisabled();
            if (manifest.Meshes.size() > 1)
            {
                PropertyUI::Row row("mesh", "Mesh source mapping");
                if (ImGui::BeginCombo("##value",
                                      manifest.Meshes[size_t(m_MetadataMesh)].SourceKey.c_str()))
                {
                    for (size_t i = 0; i < manifest.Meshes.size(); ++i)
                        if (ImGui::Selectable((manifest.Meshes[i].SourceKey + " — " +
                                               AssetKey(manifest.Meshes[i].ID))
                                                  .c_str(),
                                              int(i) == m_MetadataMesh))
                            m_MetadataMesh = int(i);
                    ImGui::EndCombo();
                }
            }
            auto &output = manifest.Meshes[size_t(m_MetadataMesh)];
            auto resolution = assets->Mesh(output.Output);
            m_MetadataAccepted = resolution.Accepted;
            PropertyUI::Validation(resolution.Error.c_str());
            if (resolution.Accepted)
            {
                auto &mesh = *resolution.Accepted;
                ImGui::Separator();
                ImGui::TextUnformatted(mesh.Definition.Name.c_str());
                PropertyUI::ReadOnly("mesh-file", "Mesh asset",
                                     output.Output.generic_u8string().c_str());
                ImGui::Text("%zu vertices | %zu indices | %zu primitives",
                            mesh.Geometry.Vertices.size(), mesh.Geometry.Indices.size(),
                            mesh.Geometry.Primitives.size());
                auto &b = mesh.Geometry.Bounds;
                ImGui::Text("Local bounds: [%.3g, %.3g, %.3g] .. [%.3g, %.3g, %.3g]", b.Min.x,
                            b.Min.y, b.Min.z, b.Max.x, b.Max.y, b.Max.z);
                for (auto &primitive : mesh.Geometry.Primitives)
                {
                    auto slot =
                        std::find_if(mesh.Definition.Slots.begin(), mesh.Definition.Slots.end(),
                                     [&](auto &s) { return s.ID == primitive.Slot; });
                    ImGui::PushID(AssetKey(slot->ID).c_str());
                    ImGui::Text("%s — %s", slot->Name.c_str(), AssetKey(slot->ID).c_str());
                    ImGui::Text("Indices %u..%u | UV0 %s | COLOR0 %s", primitive.FirstIndex,
                                primitive.FirstIndex + primitive.IndexCount - 1,
                                (primitive.Attributes & 1) ? "yes" : "no",
                                (primitive.Attributes & 2) ? "yes" : "no");
                    auto material = assets->Material(slot->Material);
                    if (material.Accepted)
                    {
                        if (ImGui::SmallButton(material.Accepted->Name.c_str()))
                            SelectAsset(assets->Root() / slot->Material);
                    }
                    else
                        PropertyUI::Validation(material.Error.c_str());
                    ImGui::PopID();
                }
            }
            if (m_Tools.Busy() && !m_Tools.Request().NativeExecutable.empty() &&
                ImGui::Button("Cancel import"))
                m_Tools.Cancel();
            PropertyUI::Validation(m_ImportError.c_str());
        }
        catch (const std::exception &e)
        {
            PropertyUI::Validation(e.what());
        }
    ImGui::End();
}
} // namespace Hazel
