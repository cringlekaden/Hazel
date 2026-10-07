#pragma once

#include "Authoring/EditorActions.h"
#include "Hazel/Core/Base.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Scene.h"
#include "SpriteWidgets.h"
#include <functional>
#include <unordered_map>
#include <map>
#include <set>

namespace Hazel
{

class SceneHierarchyPanel
{
  public:
    SceneHierarchyPanel() = default;
    SceneHierarchyPanel(const Ref<Scene> &scene);

    void SetContext(const Ref<Scene> &scene);

    void OnImGuiRender();
    bool HierarchyVisible=true, PropertiesVisible=true;
    bool PrefabDocument=false;
    void DrawHierarchy(); // Embedded by the prefab document; stable scene/UUID IDs.
    bool AddChild(Entity parent, const std::string& name="Child Entity");
    bool ReparentEntity(uint64_t scene, uint64_t child, uint64_t parent, TransformPolicy mode);
    bool DuplicateSelected();
    bool ClearStoredEntityReference(Entity entity, const std::string& field);
    void RequestDelete();
    bool ConfirmDelete(uint64_t scene, uint64_t entity, size_t expectedCount,
                       DestroyPolicy policy, TransformPolicy mode);
    void RevealSelected();
    std::map<std::string,bool> Sections() const {auto e=GetSelectedEntity();return e && uint64_t(e.GetUUID())==m_SectionEntity?m_Sections:std::map<std::string,bool>{};}
    void RestoreSections(const std::map<std::string,bool>& values){auto e=GetSelectedEntity();if(!e)return;m_SectionEntity=e.GetUUID();m_Sections=values;m_RestoreSections.clear();for(const auto& item:values)m_RestoreSections.insert(item.first);}
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
    bool DeleteSelected(DestroyPolicy policy=DestroyPolicy::Subtree, TransformPolicy mode=TransformPolicy::KeepWorld);

    Entity GetSelectedEntity() const {
        auto e=m_SelectionContext;
        return m_Context && e.BelongsTo(m_Context.get()) && e && m_Context->IsEntityValid(e.GetUUID())?e:Entity{};
    }
    bool SetSelectedEntity(Entity entity);
    static bool AssignSpriteTexture(SpriteRendererComponent &component, const std::filesystem::path &path);

  private:
    template <typename T> void DisplayAddComponentEntry(const std::string &entryName);

    void DrawEntityNode(Entity entity);
    void DrawComponents(Entity entity);
    void ParentProperties(Entity entity);
    std::string ReparentPreview(uint64_t scene,uint64_t child,uint64_t parent,TransformPolicy mode) const;
    void DrawDeleteDialog();
    bool HierarchyFailed(const std::string& reason);
    std::set<uint64_t> m_Matches, m_Reveal;
    std::string m_HierarchyError;
    int m_ParentMode=0, m_DeleteMode=0;
    bool m_DeleteWanted=false;
    uint64_t m_DeleteScene=0, m_DeleteEntity=0;
    size_t m_DeleteCount=0;
    bool CanEdit(bool report = false) const;

  private:
    Ref<Scene> m_Context;
    Entity m_SelectionContext;
    std::string m_Search;
    std::vector<std::string> m_PrefabChoices;
    bool m_PrefabChoicesReady = false;
    bool m_Focused = false;
    uint64_t m_SectionEntity=0;
    std::map<std::string,bool> m_Sections;
    std::set<std::string> m_RestoreSections;
    std::unordered_map<uint32_t, SpritePickerState> m_Pickers;
};

} // namespace Hazel
