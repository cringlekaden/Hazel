#pragma once
#include <cstdint>
namespace Hazel
{
enum class EditorMode
{
    Edit,
    Play,
    Simulate
};
enum class EditorAction
{
    Browse,
    Preview,
    EditScene,
    EditAsset,
    SaveScene,
    SaveAsset,
    ReplaceScene,
    ReplaceProject,
    OpenAsset,
    StartTool,
    CreateProject,
    ReloadScripts,
    Play,
    Simulate,
    RuntimeControl
};
struct EditorActionState
{
    EditorMode Mode = EditorMode::Edit;
    bool ToolBusy = false, HasProject = false, HasScene = false, GuardPending = false;
};
struct ActionAvailability
{
    const char *Reason = nullptr;
    explicit operator bool() const
    {
        return Reason == nullptr;
    }
};
// Shared by presentation and apply-time checks. Read-only navigation/preview never
// touches tool inputs. Scene Save always writes the retained editor scene.
inline ActionAvailability EditorActionAvailability(EditorActionState state, EditorAction action)
{
    switch (action)
    {
    case EditorAction::Browse:
    case EditorAction::Preview:
        return {};
    case EditorAction::OpenAsset:
        return {state.HasProject ? nullptr : "Open a project first"};
    default:
        break;
    }
    if (state.GuardPending &&
        (action == EditorAction::EditScene || action == EditorAction::EditAsset ||
         action == EditorAction::StartTool || action == EditorAction::CreateProject ||
         action == EditorAction::ReplaceScene || action == EditorAction::ReplaceProject ||
         action == EditorAction::ReloadScripts || action == EditorAction::Play ||
         action == EditorAction::Simulate))
        return {"Resolve the pending document operation first"};
    if (action == EditorAction::RuntimeControl)
        return {state.Mode == EditorMode::Edit ? "Start Play / Simulate first" : nullptr};
    if (state.ToolBusy)
        return {"Wait for the tool job to finish; its inputs are protected"};
    switch (action)
    {
    case EditorAction::CreateProject:
        return {state.Mode == EditorMode::Edit ? nullptr : "Stop Play / Simulate before creating a project"};
    case EditorAction::EditScene:
        if (!state.HasScene)
            return {"Open a scene first"};
        if (state.Mode != EditorMode::Edit)
            return {"Stop Play / Simulate to edit authored content"};
        return {};
    case EditorAction::EditAsset:
    case EditorAction::SaveAsset:
    case EditorAction::StartTool:
        if (!state.HasProject)
            return {"Open a project first"};
        if (state.Mode != EditorMode::Edit)
            return {"Stop Play / Simulate before changing saved assets or running tools"};
        return {};
    case EditorAction::SaveScene:
        return {state.HasScene ? nullptr : "Open a scene first"};
    case EditorAction::Play:
    case EditorAction::Simulate:
        if (!state.HasScene)
            return {"Open a scene first"};
        if (state.Mode != EditorMode::Edit)
            return {"Stop the current session first"};
        return {};
    case EditorAction::RuntimeControl:
        return {state.Mode == EditorMode::Edit ? "Start Play / Simulate first" : nullptr};
    default:
        return {};
    }
}
// An observation, not an owning Entity/Scene pointer; revalidate at application.
struct SceneTarget
{
    uint64_t Scene = 0, Entity = 0;
    bool Matches(uint64_t scene, uint64_t entity) const
    {
        return Scene && Entity && Scene == scene && Entity == entity;
    }
};
} // namespace Hazel
