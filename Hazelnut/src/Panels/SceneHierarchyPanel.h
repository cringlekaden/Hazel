#pragma once

#include "Authoring/EditorActions.h"
#include "Hazel/Core/Base.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Scene.h"
#include "SpriteWidgets.h"
#include "Authoring/UIInspectorEditing.h"
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
    bool MoveSelected(int direction);
    bool DropEntity(uint64_t scene, uint64_t entity, uint64_t parent);
    std::string DropReason(uint64_t scene, uint64_t entity, uint64_t parent) const;
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
    Ref<Scene> AuthoringScene() const {return m_Context;}
    bool AddEntity(const std::string &name = "Empty Entity");
    bool AddUI(int kind);
    void UIAddMenu();
    bool DeleteSelected(DestroyPolicy policy=DestroyPolicy::Subtree, TransformPolicy mode=TransformPolicy::KeepWorld);

    Entity GetSelectedEntity() const {
        auto e=m_SelectionContext;
        return m_Context && e.BelongsTo(m_Context.get()) && e && m_Context->IsEntityValid(e.GetUUID())?e:Entity{};
    }
    bool SetSelectedEntity(Entity entity);
    void BindHistory(DocumentHistory *history) { if(history!=m_History)FinishUIEdit();m_History = history; }
    void FinishUIEdit(bool cancel = true);
    bool Structural(const std::string& name,const std::function<bool()>& operation);
    std::function<void()> BeforeStructure; // finish viewport edits / retire borrowed hover
    void RestoreHistorySelection(uint64_t id);
    template<class T> bool AddAuthoredComponent(uint64_t id){return Structural("Add component",[&]{auto e=m_Context->GetEntityByUUID(id);if(!e)throw std::runtime_error("Component target retired");e.AddComponent<T>();return true;});}
    template<class T> bool RemoveAuthoredComponent(uint64_t id){return Structural("Remove component",[&]{auto e=m_Context->GetEntityByUUID(id);if(!e)throw std::runtime_error("Component target retired");if constexpr(std::is_same_v<T,TransformComponent>)throw std::runtime_error("Transform is required");else e.RemoveComponent<T>();return true;});}
    static bool AssignSpriteTexture(SpriteRendererComponent &component, const std::filesystem::path &path);

  private:
    template<typename T,class F> void DrawComponent(const std::string& name,Entity entity,std::map<std::string,bool>& sections,std::set<std::string>& restore,F ui,std::function<void(const std::string&)> report={},std::function<void()> beforeRemove={});
    template <typename T> void DisplayAddComponentEntry(const std::string &entryName);

    void DrawEntityNode(Entity entity);
    void DrawComponents(Entity entity);
    template<class T,class F> void DrawUIComponent(const std::string &name, Entity entity,
        std::map<std::string,bool> &sections, std::set<std::string> &restore, F ui,
        std::function<void(const std::string&)> report = {});
    void DropTarget(uint64_t parent, const std::string& name);
    void ParentMenu(Entity entity);
    std::string ReparentPreview(uint64_t scene,uint64_t child,uint64_t parent,TransformPolicy mode) const;
    void DrawDeleteDialog();
    bool HierarchyFailed(const std::string& reason);
    std::set<uint64_t> m_Matches, m_Reveal;
    std::string m_HierarchyError;
    uint64_t m_DragHover=0;
    double m_DragHoverSince=0;
    bool m_DeleteWanted=false;
    uint64_t m_DeleteScene=0, m_DeleteEntity=0;
    size_t m_DeleteCount=0;
    bool CanEdit(bool report = false) const;

  private:
    bool m_Structuring=false;
    Ref<Scene> m_Context;
    DocumentHistory *m_History = nullptr; // document-owned; detached on document retirement
    UIInspectorEditing m_UIInspectorEdit;
    Entity m_SelectionContext;
    std::string m_Search;
    std::vector<std::string> m_PrefabChoices;
    bool m_PrefabChoicesReady = false;
    bool m_Focused = false;
    uint64_t m_SectionEntity=0;
    std::map<std::string,bool> m_Sections;
    std::set<std::string> m_RestoreSections;
    std::unordered_map<uint32_t, SpritePickerState> m_Pickers;
    std::array<SpritePickerState,5> m_UIStatePickers;
    std::string m_UIEmphasisNotice;
    std::string m_UIFontDraft;
    std::filesystem::path m_UIFontBound;
    bool m_UIFontInitialized=false;
};

} // namespace Hazel
