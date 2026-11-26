#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Renderer/Scene/ReflectionProbeCapture.h"

namespace Hazel
{
AuthoringPanel::~AuthoringPanel()
{
    m_Editor.m_SceneHierarchyPanel.BakeProbe = {};
    m_Editor.m_SceneHierarchyPanel.ProbeBakeStatus = {};
    m_Editor.m_SceneHierarchyPanel.CancelProbeBake = {};
    if (m_ProbeBake)
    {
        m_Tools.Shutdown();
        FinishProbeBake(false);
    }
}

void AuthoringPanel::StartProbeBake(Entity entity)
{
    if (!Require(EditorAction::StartTool) || !Require(EditorAction::EditScene))
        return;
    try
    {
        auto scene = m_Editor.m_EditorScene;
        if (!scene || !entity.BelongsTo(scene.get()) || !entity.HasComponent<ReflectionProbeComponent>())
            throw std::runtime_error("Select a probe in the active scene");
        const auto &probe = entity.GetComponent<ReflectionProbeComponent>();
        probe.Validate();
        m_Editor.m_ProjectFile.Check();
        auto bake = CreateScope<ProbeBake>();
        bake->Scene = scene->GetIdentity();
        bake->Entity = entity.GetUUID();
        bake->Component = scene->GetAuthoringComponentVersion(bake->Entity, typeid(ReflectionProbeComponent));
        bake->Position = glm::vec3(scene->GetWorldTransform(entity)[3]);
        bake->Previous = probe.Environment;
        bake->Resolution = probe.BakeResolution;
        if (!scene->GetAssets())
            throw std::runtime_error("Probe capture requires project assets");
        bake->Root = scene->GetAssets()->Root();
        bake->Capture = CreateRef<ReflectionProbeCapture>(Scene::CopyForAuthoring(scene), bake->Position, bake->Resolution);
        m_ProbeBake = std::move(bake);
    }
    catch (const std::exception &error)
    {
        m_Editor.ActionFailed(error.what());
    }
}

void AuthoringPanel::AdvanceProbeBake()
{
    if (!m_ProbeBake || !m_ProbeBake->Capture)
        return;
    try
    {
        auto &bake = *m_ProbeBake;
        if (!bake.Capture->Advance())
            return;

        const auto parent = std::filesystem::path("CapturedReflections");
        std::filesystem::create_directories(Project::ResolveOwnedAsset(bake.Root, parent));
        const auto directory = parent / AssetKey(UUID());
        if (!std::filesystem::create_directory(Project::ResolveOwnedAsset(bake.Root, directory)))
            throw std::runtime_error("Could not reserve a new probe bake asset");
        bake.Reference = directory / "Probe.henv";
        EnvironmentDefinition descriptor;
        descriptor.Name = "Captured Reflection Probe";
        descriptor.Image = directory / "Capture.hlight";
        descriptor.SceneCapture = true;
        descriptor.Profile = {16, bake.Resolution, bake.Resolution == 128 ? 32u : 64u, 1024};
        FileSystem::WriteNewFile(Project::ResolveOwnedAsset(bake.Root, descriptor.Image),
                                 EncodeLightingImage(bake.Capture->Image()));
        FileSystem::WriteNewFile(Project::ResolveOwnedAsset(bake.Root, bake.Reference),
                                 WriteEnvironmentText(descriptor));
        bake.Capture.reset();

        ToolRequest request;
        request.NativeExecutable = ProjectTools::ImportTool();
        request.Project = m_Editor.m_ProjectPath;
        request.Generation = m_ProjectGeneration;
        request.Completion = ToolCompletion::ApplyReflectionBake;
        request.Label = "Prepare captured reflection probe";
        request.Artifact = bake.Root / bake.Reference;
        request.Arguments = {"prepare-environment", request.Project.generic_u8string(), bake.Reference.generic_u8string()};
        if (!m_Tools.Start(std::move(request)))
            throw std::runtime_error("A tool operation is already running");
        ShowConsole();
    }
    catch (const std::exception &error)
    {
        FinishProbeBake(false);
        m_Editor.ActionFailed(error.what());
    }
}

void AuthoringPanel::FinishProbeBake(bool prepared)
{
    auto bake = std::move(m_ProbeBake);
    if (!bake)
        return;
    if (!prepared)
    {
        if (!bake->Reference.empty())
        {
            try
            {
                const auto directory = Project::ResolveOwnedAsset(bake->Root, bake->Reference.parent_path());
                std::filesystem::remove_all(directory);
            }
            catch (const std::exception &error)
            {
                HZ_CORE_WARN("Could not remove failed probe bake assets: {}", error.what());
            }
        }
        return;
    }

    auto scene = m_Editor.m_EditorScene;
    if (m_Editor.m_ContentBrowserPanel)
        m_Editor.m_ContentBrowserPanel->Refresh();
    if (!scene || scene->GetIdentity() != bake->Scene || !Require(EditorAction::EditScene))
        throw std::runtime_error("Probe baked successfully. The originating scene is no longer editable");
    auto entity = scene->GetEntityByUUID(bake->Entity);
    if (!entity || !entity.HasComponent<ReflectionProbeComponent>() ||
        scene->GetAuthoringComponentVersion(bake->Entity, typeid(ReflectionProbeComponent)) != bake->Component ||
        glm::vec3(scene->GetWorldTransform(entity)[3]) != bake->Position)
        throw std::runtime_error("Probe baked successfully. The original probe moved or was replaced");
    auto before = UIEditValues::Capture(entity);
    if (before.Probe->Environment != bake->Previous || before.Probe->BakeResolution != bake->Resolution)
        throw std::runtime_error("Probe baked successfully. The original probe assignment or resolution changed");

    auto result = scene->GetAssets()->Environment(bake->Reference);
    if (!result.Accepted || !result.Error.empty())
        throw std::runtime_error(result.Error);
    ValidateReflectionProbeEnvironment(*result.Accepted);
    CancelEdits();
    before = UIEditValues::Capture(entity);
    auto after = before;
    after.Probe->Environment = bake->Reference;
    PropertyUI::EditActivity committed;
    committed.Changed = committed.Committed = true;
    UIInspectorEditing edit;
    if (!edit.Edit(*scene, &m_SceneHistory, bake->Entity, "Bake Static Probe", before, after, committed, 0))
        throw std::runtime_error("Probe prepared successfully. The assignment could not enter scene history");
    m_Editor.m_ActionError.clear();
}

void AuthoringPanel::CancelProbeBake()
{
    if (!m_ProbeBake)
        return;
    if (m_ProbeBake->Capture)
        FinishProbeBake(false);
    else
        m_Tools.Cancel();
}

std::string AuthoringPanel::ProbeBakeStatus() const
{
    if (!m_ProbeBake)
        return {};
    if (!m_ProbeBake->Capture)
        return "Preparing probe. Progress is in the Console.";
    return "Capturing probe face " + std::to_string(m_ProbeBake->Capture->FacesComplete() + 1) + " of 6";
}
}
