#pragma once

#include "Authoring/EditorActions.h"
#include "Hazel/Core/Base.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Scene.h"
#include "SpriteWidgets.h"
#include <functional>
#include <unordered_map>

namespace Hazel
{

class SceneHierarchyPanel
{
  public:
    SceneHierarchyPanel() = default;
    SceneHierarchyPanel(const Ref<Scene> &scene);

    void SetContext(const Ref<Scene> &scene);

    void OnImGuiRender();
    std::function<void(Entity)> CreatePrefab;
    std::function<void(const std::string &)> ReportError;
    std::function<void(const std::string &)> EditScript;
    OpenAssetAction OpenAsset;
    std::function<ActionAvailability(EditorAction)> Availability;
    EditorAction EditOperation = EditorAction::EditScene;
    void DrawAssetProperties(Entity entity);
    bool Focused() const
    {
        return m_Focused;
    }
    bool AddEntity(const std::string &name = "Empty Entity");
    bool DeleteSelected();

    Entity GetSelectedEntity() const
    {
        return m_Context && m_SelectionContext.BelongsTo(m_Context.get()) && m_SelectionContext
                   ? m_SelectionContext
                   : Entity{};
    }
    bool SetSelectedEntity(Entity entity);
    static bool AssignSpriteTexture(SpriteRendererComponent &component, const std::filesystem::path &path);

  private:
    template <typename T> void DisplayAddComponentEntry(const std::string &entryName);

    void DrawEntityNode(Entity entity);
    void DrawComponents(Entity entity);
    bool CanEdit(bool report = false) const;

  private:
    Ref<Scene> m_Context;
    Entity m_SelectionContext;
    std::string m_Search;
    std::vector<std::string> m_PrefabChoices;
    bool m_PrefabChoicesReady = false;
    bool m_Focused = false;
    std::unordered_map<uint32_t, SpritePickerState> m_Pickers;
};

} // namespace Hazel
