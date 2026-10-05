#pragma once
#include "Authoring/EditorActions.h"
#include "Authoring/EditorDocuments.h"
namespace Hazel
{
static void EditorDocumentChecks()
{
    std::vector<DocumentInfo> docs = {{EditorDocument::Scene, 10, "Scene", true, {}},
                                      {EditorDocument::Prefab, 20, "Prefab", true, {}},
                                      {EditorDocument::Sheet, 30, "Sheet", true, {}}};
    EditorDocuments guard;
    int operations = 0, saves = 0;
    auto action = [&]
    {
        ++operations;
        return true;
    };
    auto save = [&](const DocumentInfo &doc)
    {
        ++saves;
        return DocumentSaveResult{doc, SaveOutcome::Saved, "Saved"};
    };
    guard.Request(OperationIntent::Play, docs, action);
    Check(guard.Affected().size() == 2 && guard.Affected()[0].Kind != EditorDocument::Scene,
          "Play guard included scene draft");
    Check(!guard.Resolve(GuardChoice::Discard, docs, save) && !operations,
          "Play allowed destructive discard");
    Check(guard.Resolve(GuardChoice::UseSaved, docs, save) && operations == 1 && !saves &&
              docs[0].Dirty && docs[1].Dirty && docs[2].Dirty,
          "Use Saved altered drafts or invoked saves");
    guard.Request(OperationIntent::Export, docs, action);
    Check(guard.Resolve(GuardChoice::UseSaved, docs, save) && operations == 2 && !saves,
          "Export saved files saved/discarded drafts");
    guard.Request(OperationIntent::OpenScene, docs, action);
    Check(guard.Affected().size() == 1 && guard.Affected()[0].Kind == EditorDocument::Scene,
          "Open Scene guarded independent asset drafts");
    Check(!guard.Resolve(GuardChoice::Cancel, docs, save) && !guard.Pending() && operations == 2,
          "Cancel executed action");
    guard.Request(OperationIntent::OpenProject, docs, [] { return false; });
    Check(!guard.Resolve(GuardChoice::Discard, docs, save) && guard.Pending() && docs[0].Dirty &&
              docs[1].Dirty && docs[2].Dirty,
          "Rejected Open discarded drafts or completed guard");
    guard.Resolve(GuardChoice::Cancel, docs, save);
    guard.Request(OperationIntent::SaveAll, docs, action);
    auto partial = [&](const DocumentInfo &doc)
    {
        ++saves;
        const auto success = doc.Kind == EditorDocument::Scene;
        if (success)
            docs[0].Dirty = false;
        return DocumentSaveResult{doc,
                                  success                              ? SaveOutcome::Saved
                                  : doc.Kind == EditorDocument::Prefab ? SaveOutcome::Failed
                                                                       : SaveOutcome::Cancelled,
                                  success ? "Saved" : "Draft retained"};
    };
    Check(!guard.Resolve(GuardChoice::SaveAndContinue, docs, partial) && guard.Pending() &&
              operations == 2 && guard.Results().size() == 3 && !docs[0].Dirty && docs[1].Dirty &&
              docs[2].Dirty,
          "Partial Save All lost drafts or ran action");
    Check(guard.Results()[1].Outcome == SaveOutcome::Failed &&
              guard.Results()[2].Outcome == SaveOutcome::Cancelled,
          "Failure and cancelled saves conflated");
    saves = 0;
    Check(guard.Resolve(GuardChoice::SaveAndContinue, docs, save) && saves == 2 && operations == 3,
          "Retry rewrote already-saved document or missed pending saves");
    docs[0].Dirty = true;
    guard.Request(OperationIntent::OpenScene, docs, action);
    docs[0].Identity = 11;
    Check(!guard.Resolve(GuardChoice::Discard, docs, save) && operations == 3 && !guard.Error().empty(),
          "Guard ignored changed document identity");
    guard.Resolve(GuardChoice::Cancel, docs, save);
    guard.Request(OperationIntent::CloseSheet, docs,
                  [&]
                  {
                      docs.erase(docs.begin() + 2);
                      return true;
                  });
    Check(guard.Resolve(GuardChoice::Discard, docs, save) && docs.size() == 2,
          "Close discard retained the closed sheet draft");
    guard.Request(OperationIntent::OpenScene, docs, action);
    Check(!guard.Resolve(GuardChoice::SaveAndContinue, docs,
                         [](const DocumentInfo &) -> DocumentSaveResult
                         { throw std::runtime_error("Controlled save exception"); }) &&
              guard.Pending() && guard.Results().size() == 1 &&
              guard.Results()[0].Outcome == SaveOutcome::Failed && operations == 3,
          "Thrown save lost draft, escaped guard, or continued operation");
    guard.Resolve(GuardChoice::Cancel, docs, save);
    const SceneTarget target{90, 123};
    Check(target.Matches(90, 123) && !target.Matches(91, 123) && !target.Matches(90, 124) &&
              !SceneTarget{}.Matches(0, 0),
          "Assignment target ignored scene/entity identity");
    EditorActionState state{EditorMode::Edit, false, true, true};
    for (auto actionType :
         {EditorAction::EditScene, EditorAction::EditAsset, EditorAction::SaveScene,
          EditorAction::SaveAsset, EditorAction::StartTool, EditorAction::Play, EditorAction::Simulate})
        Check(bool(EditorActionAvailability(state, actionType)), "Edit action unexpectedly unavailable");
    state.ToolBusy = true;
    for (auto actionType :
         {EditorAction::EditScene, EditorAction::EditAsset, EditorAction::SaveScene,
          EditorAction::SaveAsset, EditorAction::StartTool, EditorAction::CreateProject,
          EditorAction::ReplaceProject, EditorAction::ReplaceScene, EditorAction::ReloadScripts,
          EditorAction::Play, EditorAction::Simulate})
        Check(!EditorActionAvailability(state, actionType),
              "Tool job left mutation/lifecycle action available");
    for (auto actionType : {EditorAction::Browse, EditorAction::Preview, EditorAction::OpenAsset})
        Check(bool(EditorActionAvailability(state, actionType)),
              "Tool job blocked safe browsing/preview");
    state.ToolBusy = false;
    for (auto mode : {EditorMode::Play, EditorMode::Simulate})
    {
        state.Mode = mode;
        for (auto actionType : {EditorAction::EditScene, EditorAction::EditAsset,
                                EditorAction::SaveAsset, EditorAction::StartTool})
            Check(!EditorActionAvailability(state, actionType),
                  "Runtime left authored mutations available");
        Check(bool(EditorActionAvailability(state, EditorAction::SaveScene)) &&
                  bool(EditorActionAvailability(state, EditorAction::RuntimeControl)),
              "Runtime blocked retained scene save/Stop");
    }
    state.Mode = EditorMode::Edit;
    state.GuardPending = true;
    Check(!EditorActionAvailability(state, EditorAction::EditScene) &&
              !EditorActionAvailability(state, EditorAction::EditAsset),
          "Pending guard allowed draft mutations");
    std::cout
        << "PASS: document scopes, Use Saved retention, cancel/rejected Open, partial/failed/cancelled "
           "Save All, retries, identity checks, close discard and shared action availability\n";
}
} // namespace Hazel
