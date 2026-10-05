#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "UI/PropertyUI.h"
#include <algorithm>
#include <imgui.h>
namespace Hazel
{
void AuthoringPanel::DocumentGuard()
{
    if (m_Documents.Pending())
        ImGui::OpenPopup("Document operation");
    auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({std::min(640.f, viewport->WorkSize.x - 24.f), 0}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Document operation", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    const auto intent = m_Documents.Intent();
    ImGui::TextWrapped("%s", EditorDocuments::Description(intent));
    ImGui::Separator();
    ImGui::TextUnformatted("Affected dirty documents:");
    const auto current = Documents();
    for (const auto &doc : m_Documents.Affected())
    {
        const auto found =
            std::find_if(current.begin(), current.end(), [&](const auto &now)
                         { return now.Kind == doc.Kind && now.Identity == doc.Identity; });
        ImGui::BulletText("%s%s", doc.Name.c_str(),
                          found != current.end() && !found->Dirty ? " (saved)" : " *");
        if (found != current.end() && !found->SaveUnavailable.empty())
            ImGui::TextWrapped("Save unavailable: %s", found->SaveUnavailable.c_str());
    }
    if (intent == OperationIntent::Play || intent == OperationIntent::Simulate)
        ImGui::TextWrapped("The current scene draft runs in a copy. Referenced "
                           "assets come from disk. Use "
                           "Saved Assets retains these asset drafts unchanged.");
    else if (intent == OperationIntent::Export)
        ImGui::TextWrapped("Export packages saved files. Export Saved Files "
                           "retains every unsaved draft; it "
                           "does not discard work.");
    else if (EditorDocuments::AllowsDiscard(intent))
        ImGui::TextWrapped("Discard applies only when the replacement or close "
                           "succeeds. A failed or "
                           "cancelled Open keeps the current session and drafts.");
    else
        ImGui::TextWrapped("Each file is saved independently. Successful saves "
                           "remain saved if another file "
                           "fails or Save As is cancelled.");
    PropertyUI::Validation(m_Documents.Error().c_str());
    for (const auto &result : m_Documents.Results())
        ImGui::TextWrapped("%s: %s", result.Document.Name.c_str(), result.Message.c_str());
    auto resolve = [&](GuardChoice choice)
    {
        m_ResolvingDocumentAction = true;
        const bool complete = m_Documents.Resolve(choice, Documents(),
                                                  [this](const auto &doc) { return SaveDocument(doc); });
        m_ResolvingDocumentAction = false;
        if (complete || !m_Documents.Pending())
            ImGui::CloseCurrentPopup();
    };
    const char *saveLabel = intent == OperationIntent::SaveAll    ? "Save Listed Documents"
                            : intent == OperationIntent::Export   ? "Save All and Export"
                            : intent == OperationIntent::Play     ? "Save Assets and Play"
                            : intent == OperationIntent::Simulate ? "Save Assets and Simulate"
                                                                  : "Save and Continue";
    if (ImGui::Button(saveLabel))
        resolve(GuardChoice::SaveAndContinue);
    if (EditorDocuments::AllowsSaved(intent))
    {
        if (ImGui::Button(intent == OperationIntent::Export ? "Export Saved Files (Keep Drafts)"
                                                            : "Use Saved Assets (Keep Drafts)"))
            resolve(GuardChoice::UseSaved);
    }
    if (EditorDocuments::AllowsDiscard(intent))
    {
        if (ImGui::Button("Discard Listed Drafts on Success"))
            resolve(GuardChoice::Discard);
    }
    if (ImGui::Button("Cancel"))
        resolve(GuardChoice::Cancel);
    ImGui::EndPopup();
}
} // namespace Hazel
