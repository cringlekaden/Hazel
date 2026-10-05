#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "UI/PropertyUI.h"
#include <imgui.h>
#include <algorithm>

namespace Hazel
{
void AuthoringPanel::ReportOpen(const std::filesystem::path& path,const DocumentLoadReport& report)
{
    if(report.State==DocumentLoadState::Rejected || report.State==DocumentLoadState::NeedsDecision)
        return; // ActionFailed records the rejection once; the Console keeps typed actions below.
    Notify("Opened "+path.generic_u8string()+
           (report.Problems.empty()?"":" with editable problems; authored references retained"));
    if(report.Migration)Notify("Known legacy/default encoding loaded in memory. Save explicitly to write the supported format; the original will be preserved first.",spdlog::level::warn);
    for(const auto& problem:report.Problems)
        Notify(problem.Property+": "+problem.Message,spdlog::level::warn);
}
void AuthoringPanel::RecoveryControls()
{
    const auto& report=m_Editor.m_OpenLoad;
    const bool accepted=report.State==DocumentLoadState::Ready || report.State==DocumentLoadState::EditableWithProblems;
    const bool currentRecovery=m_Editor.m_SceneFile.NeedsBackup() || m_Editor.m_ProjectFile.NeedsBackup() || m_PrefabFile.NeedsBackup() || (m_Sprites.File() && m_Sprites.File()->NeedsBackup());
    if(m_Editor.m_OpenPath.empty() && !currentRecovery)return;
    if(!ImGui::CollapsingHeader("Open / document problems"))return;
    ImGui::PushID("document-problems");
    ImGui::TextWrapped("%s",m_Editor.m_OpenPath.generic_u8string().c_str());
    if(!accepted)ImGui::TextWrapped("%s",report.Error.c_str());
    ImGui::TextWrapped("Findings from Open. Retry Open refreshes them; saving unresolved references does not resolve resources.");
    if(report.Migration || currentRecovery)
        ImGui::TextWrapped("Recovery/known migration: authored references remain intact. First overwrite preserves original bytes in the editor's recovery folder.");
    if(ImGui::Button("Open original file"))
        if(!FileDialogs::OpenPath(m_Editor.m_OpenPath.generic_u8string()))
            m_Editor.ActionFailed("Cannot open original file: "+m_Editor.m_OpenPath.generic_u8string());
    PropertyUI::WrapButton("Open containing folder");
    if(ImGui::Button("Open containing folder"))
        if(!FileDialogs::OpenPath(m_Editor.m_OpenPath.parent_path().generic_u8string()))
            m_Editor.ActionFailed("Original folder is unavailable: "+m_Editor.m_OpenPath.parent_path().generic_u8string());
    PropertyUI::WrapButton("Copy path");
    if(ImGui::Button("Copy path"))ImGui::SetClipboardText(m_Editor.m_OpenPath.generic_u8string().c_str());
    PropertyUI::WrapButton("Copy diagnostic");
    if(ImGui::Button("Copy diagnostic")) {
        std::string text=m_Editor.m_OpenPath.generic_u8string()+"\n"+report.Error;
        for(const auto& problem:report.Problems)text+="\n"+problem.Property+": "+problem.Message;
        ImGui::SetClipboardText(text.c_str());
    }
    const auto available=Availability(m_Editor.m_OpenIsProject?EditorAction::ReplaceProject:EditorAction::ReplaceScene);
    ImGui::BeginDisabled(!available);
    PropertyUI::WrapButton("Retry Open");
    if(ImGui::Button("Retry Open")) {
        const auto path=m_Editor.m_OpenPath;
        const bool project=m_Editor.m_OpenIsProject;
        Guard(project?OperationIntent::OpenProject:OperationIntent::OpenScene,
              [this,path,project]{return project?m_Editor.OpenProject(path):m_Editor.OpenScene(path);});
    }
    ImGui::EndDisabled();PropertyUI::Help(available.Reason);
    if(report.State==DocumentLoadState::NeedsDecision && m_Editor.m_OpenIsProject) {
        ImGui::TextWrapped("No candidate has replaced the current session. Choose an explicit location or open an empty workspace; the descriptor changes only on Save Project.");
        // Choosing a recovery action explicitly cancels the unsuccessful pending request;
        // the replacement below receives a fresh, normal dirty-document decision.
        const bool failedOpenPending=m_Documents.Pending() && m_Documents.Intent()==OperationIntent::OpenProject && !m_Documents.Error().empty();
        const auto choose=[this,failedOpenPending](EditorLayer::ProjectOpenOptions options) {
            const auto path=m_Editor.m_OpenPath;
            if(m_Documents.Pending()) { if(!failedOpenPending)return; m_Documents.Resolve(GuardChoice::Cancel,Documents(),{}); }
            Guard(OperationIntent::OpenProject,[this,path,options]{return m_Editor.OpenProject(path,options);});
        };
        const auto canChoose=EditorActionAvailability({static_cast<EditorMode>(m_Editor.m_SceneState),m_Tools.Busy(),bool(Project::GetActive()),bool(m_Editor.m_EditorScene),m_Documents.Pending() && !failedOpenPending},EditorAction::ReplaceProject);
        ImGui::BeginDisabled(!canChoose);
        if(ImGui::Button("Locate Assets")) {
            const auto path=FileDialogs::SelectFolder();
            if(!path.empty()){EditorLayer::ProjectOpenOptions options;options.Assets=std::filesystem::u8path(path);choose(options);}
        }
        PropertyUI::WrapButton("Locate startup scene");
        if(ImGui::Button("Locate startup scene")) {
            const auto path=FileDialogs::OpenFile("Hazel scene\0*.hazel\0");
            if(!path.empty()){EditorLayer::ProjectOpenOptions options;options.Scene=std::filesystem::u8path(path);choose(options);}
        }
        PropertyUI::WrapButton("Open workspace without scene");
        if(ImGui::Button("Open workspace without scene")){EditorLayer::ProjectOpenOptions options;options.WithoutScene=true;choose(options);}
        ImGui::EndDisabled();PropertyUI::Help(canChoose.Reason);
    }
    for(size_t index=0;index<report.Problems.size();++index) {
        const auto& problem=report.Problems[index];
        ImGui::PushID(static_cast<int>(index));
        ImGui::TextWrapped("%s: %s",problem.Property.c_str(),problem.Message.c_str());
        if(problem.Entity && accepted) {
            auto entity=m_Editor.m_EditorScene->GetEntityByUUID(problem.Entity);
            ImGui::BeginDisabled(!entity || m_Editor.m_SceneState!=EditorLayer::SceneState::Edit);
            if(ImGui::SmallButton("Select affected entity"))m_Editor.m_SceneHierarchyPanel.SetSelectedEntity(entity);
            ImGui::EndDisabled();
            PropertyUI::Help("Use the selected entity's typed reference fields to choose and explicitly apply a replacement. No name-based replacement is inferred.");
        }
        if(!problem.Path.empty()) {
            if(ImGui::SmallButton("Reveal resource"))
                if(!FileDialogs::OpenPath(problem.Path.parent_path().generic_u8string()))
                    m_Editor.ActionFailed("Resource folder is unavailable: "+problem.Path.parent_path().generic_u8string());
        }
        ImGui::PopID();
    }
    for(const auto* file:{static_cast<const FileDocument*>(&m_Editor.m_SceneFile),static_cast<const FileDocument*>(&m_Editor.m_ProjectFile),static_cast<const FileDocument*>(&m_PrefabFile),m_Sprites.File()})
        if(file && !file->Backup().empty()) {
            ImGui::TextWrapped("Original: %s",file->Backup().generic_u8string().c_str());
            ImGui::PushID(file);
            if(ImGui::SmallButton("Open recovery folder"))FileDialogs::OpenPath(file->Backup().parent_path().generic_u8string());
            ImGui::PopID();
        }
    ImGui::PopID();
}
void AuthoringPanel::SaveConflictControls()
{
    if(!m_ShowSaveConflict)return;
    ImGui::SetNextWindowSize({520,230},ImGuiCond_FirstUseEver);
    ImGui::OpenPopup("Document changed externally");
    if(!ImGui::BeginPopupModal("Document changed externally",nullptr))return;
    ImGui::TextWrapped("The draft and external file are retained. Save a new copy or reopen through the normal unsaved-document guard. No automatic merge/overwrite.");
    const auto docs=Documents();
    const bool same=std::any_of(docs.begin(),docs.end(),[this](const auto& doc){return doc.Kind==m_ConflictDocument && doc.Identity==m_ConflictIdentity;});
    const auto available=Availability(m_ConflictDocument==EditorDocument::Scene?EditorAction::SaveScene:EditorAction::SaveAsset);
    ImGui::BeginDisabled(!same || !available);
    if(ImGui::Button("Save Copy")) {
        const auto path=FileDialogs::SaveFile("Hazel document\0*\0");
        if(!path.empty())try {
            const auto target=std::filesystem::u8path(path);
            std::string text;
            if(m_ConflictDocument==EditorDocument::Scene)text=SceneSerializer(m_Editor.m_EditorScene).SerializeText();
            else if(m_ConflictDocument==EditorDocument::Prefab)text=Prefab::Serialize(Project::GetAssetDirectory(),m_PrefabScene,Prefab::GetEntity(m_PrefabScene),true);
            else text=m_Sprites.CopyDraftText();
            if(!Require(m_ConflictDocument==EditorDocument::Scene?EditorAction::SaveScene:EditorAction::SaveAsset))throw std::runtime_error("Save unavailable; draft retained");
            FileSystem::WriteFileAtomically(target,[&](auto& out){out<<text;},WriteMode::CreateNew);
            Notify("Saved copy: "+target.generic_u8string()+"; original draft retained");
            m_ShowSaveConflict=false;ImGui::CloseCurrentPopup();
        }catch(const std::exception& error){m_Editor.ActionFailed(error.what());}
    }
    ImGui::SameLine();
    if(ImGui::Button("Reopen")) {
        const auto kind=m_ConflictDocument;
        const auto path=kind==EditorDocument::Scene?m_Editor.m_EditorScenePath:kind==EditorDocument::Prefab?m_PrefabFile.Path():Project::GetAssetDirectory()/std::filesystem::u8path(m_Sprites.Name());
        m_ShowSaveConflict=false;ImGui::CloseCurrentPopup();
        if(m_Documents.Pending())m_Documents.Resolve(GuardChoice::Cancel,Documents(),{});
        if(kind==EditorDocument::Scene)Guard(OperationIntent::OpenScene,[this,path]{return m_Editor.OpenScene(path);});
        else SelectAsset(path);
    }
    ImGui::EndDisabled();PropertyUI::Help(!same?"The document identity changed; cancel this outdated conflict":available.Reason);
    ImGui::SameLine();
    if(ImGui::Button("Cancel")){m_ShowSaveConflict=false;ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
}
}
