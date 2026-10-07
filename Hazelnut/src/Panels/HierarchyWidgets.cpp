#include "SceneHierarchyPanel.h"
#include "UI/PropertyUI.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <sstream>
namespace Hazel {
namespace {
struct EntityDrop {
    uint64_t Scene, Entity;
};
std::string Lower(std::string text) {
    for (auto &c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::string Types(Entity e) {
    std::string result;
    if (e.HasComponent<SpriteRendererComponent>())
        result += " sprite";
    if (e.HasComponent<CircleRendererComponent>())
        result += " circle";
    if (e.HasComponent<TextComponent>())
        result += " text";
    if (e.HasComponent<CameraComponent>())
        result += " camera";
    if (e.HasComponent<ScriptComponent>())
        result += " script " + e.GetComponent<ScriptComponent>().ClassName;
    if (e.HasComponent<Rigidbody2DComponent>())
        result += " rigidbody physics";
    if (e.HasComponent<BoxCollider2DComponent>() || e.HasComponent<CircleCollider2DComponent>())
        result += " collider";
    return result;
}
} // namespace
bool SceneHierarchyPanel::HierarchyFailed(const std::string &reason) {
    if (m_HierarchyError != reason && ReportError)
        ReportError(reason);
    m_HierarchyError = reason;
    return false;
}
void SceneHierarchyPanel::RevealSelected() {
    auto e = GetSelectedEntity();
    if (!e)
        return;
    auto id = e.GetUUID();
    while (uint64_t(id)) {
        m_Reveal.insert(id);
        id = m_Context->GetRelationship(m_Context->GetEntityByUUID(id)).Parent;
    }
}
bool SceneHierarchyPanel::AddChild(Entity parent, const std::string &name) {
    if (!m_Context || !CanEdit(true) || !parent.BelongsTo(m_Context.get()) || !parent)
        return false;
    Entity child;
    try {
        child = m_Context->CreateEntity(name);
        m_Context->Reparent(child, parent, TransformPolicy::KeepLocal);
        m_HierarchyError.clear();
        return SetSelectedEntity(child);
    } catch (const std::exception &error) {
        if (child)
            m_Context->DestroyEntity(child);
        return HierarchyFailed(error.what());
    }
}
std::string SceneHierarchyPanel::ReparentPreview(uint64_t scene, uint64_t child, uint64_t parent,
                                                 TransformPolicy mode) const {
    if (!m_Context)
        return "Document unavailable";
    if (!CanEdit())
        return Availability ? Availability(EditOperation).Reason : "Stop runtime before editing";
    try {
        if (scene != m_Context->GetIdentity())
            throw std::runtime_error("Cannot reparent across scenes or a replaced document");
        auto entity = m_Context->GetEntityByUUID(child),
             target = m_Context->GetEntityByUUID(parent);
        if (!entity || (parent && !target))
            throw std::runtime_error("Reparent entity/target no longer exists");
        if (PrefabDocument && (!parent || !uint64_t(m_Context->GetRelationship(entity).Parent)))
            throw std::runtime_error(
                "Prefab keeps its sole root; parent only children inside this asset");
        Scene::Relationships graph;
        Scene::Transforms transforms;
        m_Context->CheckReparent(entity.GetUUID(), target ? target.GetUUID() : UUID(0), mode, graph,
                                 transforms);
        return {};
    } catch (const std::exception &error) {
        return error.what();
    }
}
bool SceneHierarchyPanel::ReparentEntity(uint64_t scene, uint64_t child, uint64_t parent,
                                         TransformPolicy mode) {
    if (!m_Context || !CanEdit(true))
        return false;
    try {
        if (scene != m_Context->GetIdentity())
            throw std::runtime_error("Cannot reparent across scenes or a replaced document");
        auto entity = m_Context->GetEntityByUUID(child),
             target = m_Context->GetEntityByUUID(parent);
        if (!entity || (parent && !target))
            throw std::runtime_error("Reparent entity/target no longer exists");
        if (PrefabDocument && (!parent || !uint64_t(m_Context->GetRelationship(entity).Parent)))
            throw std::runtime_error("Prefab keeps its sole root. Move children under an entity in "
                                     "this asset; the root cannot be parented");
        m_Context->Reparent(entity, target, mode);
        m_HierarchyError.clear();
        RevealSelected();
        return true;
    } catch (const std::exception &error) {
        return HierarchyFailed(error.what());
    }
}
bool SceneHierarchyPanel::DuplicateSelected() {
    auto e = GetSelectedEntity();
    if (!e || !CanEdit(true))
        return false;
    try {
        if (PrefabDocument && !uint64_t(m_Context->GetRelationship(e).Parent))
            throw std::runtime_error(
                "A prefab keeps one root; duplicate one of its child subtrees");
        auto duplicate = m_Context->DuplicateEntity(e);
        m_HierarchyError.clear();
        return SetSelectedEntity(duplicate);
    } catch (const std::exception &error) {
        return HierarchyFailed(error.what());
    }
}
bool SceneHierarchyPanel::ClearStoredEntityReference(Entity entity, const std::string &name) {
    if (!m_Context || !CanEdit(true) || !entity.BelongsTo(m_Context.get()) || !entity ||
        !m_Context->IsEntityValid(entity.GetUUID()))
        return false;
    auto &fields = ScriptEngine::GetScriptFieldMap(entity);
    auto found = fields.find(name);
    if (found == fields.end() || found->second.Field.Type != ScriptFieldType::Entity)
        return HierarchyFailed("Stored entity reference is no longer available");
    found->second.SetValue<uint64_t>(0);
    m_HierarchyError.clear();
    return true;
}
void SceneHierarchyPanel::RequestDelete() {
    auto e = GetSelectedEntity();
    if (!e || !CanEdit(true))
        return;
    if (PrefabDocument && !uint64_t(m_Context->GetRelationship(e).Parent)) {
        HierarchyFailed(
            "The prefab root owns this asset. Close the document or delete a child subtree");
        return;
    }
    m_DeleteScene = m_Context->GetIdentity();
    m_DeleteEntity = e.GetUUID();
    m_DeleteCount = m_Context->GetSubtree(e).size();
    m_DeleteMode = 0;
    m_DeleteWanted = true;
    HierarchyVisible = true;
}
bool SceneHierarchyPanel::ConfirmDelete(uint64_t scene, uint64_t id, size_t expectedCount,
                                        DestroyPolicy policy, TransformPolicy mode) {
    if (!m_Context || !CanEdit(true))
        return false;
    try {
        if (scene != m_Context->GetIdentity())
            throw std::runtime_error("Delete document has been replaced");
        auto e = m_Context->GetEntityByUUID(id);
        if (!e)
            throw std::runtime_error("Delete entity no longer exists");
        if (m_Context->GetSubtree(e).size() != expectedCount)
            throw std::runtime_error("Subtree changed; review a new delete confirmation");
        if (PrefabDocument && (!uint64_t(m_Context->GetRelationship(e).Parent) ||
                               policy == DestroyPolicy::KeepChildren))
            throw std::runtime_error(
                "Prefab must remain a connected subtree; delete a child subtree instead");
        m_Context->DestroyEntity(e, policy, mode);
        if (!GetSelectedEntity()) {
            m_SelectionContext = {};
            m_Pickers.clear();
        }
        m_HierarchyError.clear();
        return true;
    } catch (const std::exception &error) {
        return HierarchyFailed(error.what());
    }
}
void SceneHierarchyPanel::DrawDeleteDialog() {
    if (m_DeleteWanted) {
        ImGui::OpenPopup("Delete subtree");
        m_DeleteWanted = false;
    }
    ImGui::SetNextWindowSize({std::min(480.f, ImGui::GetMainViewport()->WorkSize.x - 40.f), 0},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Delete subtree", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto entity = m_Context->GetEntityByUUID(m_DeleteEntity);
        ImGui::TextWrapped("Delete '%s' and its %zu descendant(s)? %zu entities will be removed.",
                           entity ? entity.GetName().c_str() : "Unavailable", m_DeleteCount - 1,
                           m_DeleteCount);
        const auto can = Availability ? Availability(EditOperation) : ActionAvailability{};
        ImGui::BeginDisabled(!CanEdit());
        if (ImGui::Button("Delete Subtree"))
            if (ConfirmDelete(m_DeleteScene, m_DeleteEntity, m_DeleteCount, DestroyPolicy::Subtree,
                              TransformPolicy::KeepWorld))
                ImGui::CloseCurrentPopup();
        if (!PrefabDocument && m_DeleteCount > 1) {
            ImGui::Separator();
            ImGui::TextWrapped("Alternatively, delete only the parent and move immediate children "
                               "to roots. Keep World may fail for shear/singular transforms.");
            PropertyUI::Combo("delete-policy", "Child transforms", m_DeleteMode,
                              "Keep World\0Keep Local (moves children)\0");
            if (ImGui::Button("Delete Parent / Keep Children"))
                if (ConfirmDelete(m_DeleteScene, m_DeleteEntity, m_DeleteCount,
                                  DestroyPolicy::KeepChildren, TransformPolicy(m_DeleteMode)))
                    ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        PropertyUI::Help(can.Reason);
        if (!m_HierarchyError.empty())
            PropertyUI::Validation(m_HierarchyError.c_str());
        if (ImGui::Button("Cancel")) {
            m_DeleteEntity = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void SceneHierarchyPanel::DrawHierarchy() {
    if (!m_Context)
        return;
    ImGui::PushID(std::to_string(m_Context->GetIdentity()).c_str());
    ImGui::SetNextItemWidth(-ImGui::GetFrameHeightWithSpacing());
    ImGui::InputTextWithHint("##search", "Find name or component...", &m_Search);
    ImGui::SameLine();
    if (ImGui::Button("X##clear-search"))
        m_Search.clear();
    PropertyUI::Help("Reset the entity filter; matching ancestors remain visible while searching");
    const auto can = Availability ? Availability(EditOperation) : ActionAvailability{};
    ImGui::BeginDisabled(!CanEdit());
    if (PrefabDocument) {
        if (ImGui::Button("Create Child")) {
            auto e = GetSelectedEntity();
            if (!e)
                e = m_Context->GetEntityByUUID(m_Context->GetChildren().front());
            AddChild(e);
        }
    } else {
        if (ImGui::Button("Create Root"))
            AddEntity();
        PropertyUI::WrapButton("Create Child");
        ImGui::BeginDisabled(!GetSelectedEntity());
        if (ImGui::Button("Create Child"))
            AddChild(GetSelectedEntity());
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    PropertyUI::Help(can.Reason);
    PropertyUI::Help("Children begin at the parent origin with local identity; root entities use "
                     "world coordinates");
    if (!m_HierarchyError.empty())
        PropertyUI::Validation(m_HierarchyError.c_str());
    m_Matches.clear();
    const auto query = Lower(m_Search);
    if (!query.empty())
        for (auto handle : m_Context->GetAllEntitiesWith<IDComponent>()) {
            Entity e(handle, m_Context.get());
            if (!m_Context->IsEntityValid(e.GetUUID()))
                continue;
            if (Lower(e.GetName() + Types(e)).find(query) != std::string::npos) {
                auto id = e.GetUUID();
                while (uint64_t(id)) {
                    m_Matches.insert(id);
                    id = m_Context->GetRelationship(m_Context->GetEntityByUUID(id)).Parent;
                }
            }
        }
    ImGui::BeginChild("Entity list", {0, 0}, false);
    for (auto root : m_Context->GetChildren())
        if (query.empty() || m_Matches.count(root))
            DrawEntityNode(m_Context->GetEntityByUUID(root));
    if (m_Context->GetChildren().empty())
        ImGui::TextWrapped("No entities. Create a root above.");
    else if (!query.empty() && m_Matches.empty())
        ImGui::TextWrapped("No matching entities. Clear search to see all.");
    if (!PrefabDocument) {
        ImGui::BeginDisabled(!CanEdit());
        const bool moveRoot = ImGui::Selectable("Root / Drop to Unparent##rootdrop", false, 0,
                                                {0, ImGui::GetFrameHeightWithSpacing()});
        ImGui::EndDisabled();
        if (moveRoot)
            if (auto e = GetSelectedEntity())
                ReparentEntity(m_Context->GetIdentity(), e.GetUUID(), 0,
                               ImGui::GetIO().KeyShift ? TransformPolicy::KeepLocal
                                                       : TransformPolicy::KeepWorld);
        PropertyUI::Help("Click to detach selection or drop here. Keep World is default; Shift "
                         "explicitly keeps local values and may move the entity");
        if (ImGui::BeginDragDropTarget()) {
            if (auto *payload = ImGui::AcceptDragDropPayload(
                    "HAZEL_HIERARCHY_ENTITY", ImGuiDragDropFlags_AcceptBeforeDelivery))
                if (payload->DataSize == sizeof(EntityDrop)) {
                    auto drop = *static_cast<const EntityDrop *>(payload->Data);
                    const auto mode = ImGui::GetIO().KeyShift ? TransformPolicy::KeepLocal
                                                              : TransformPolicy::KeepWorld;
                    const auto reason = ReparentPreview(drop.Scene, drop.Entity, 0, mode);
                    ImGui::BeginTooltip();
                    if (!reason.empty())
                        PropertyUI::Validation(reason.c_str());
                    else
                        ImGui::TextUnformatted(mode == TransformPolicy::KeepWorld
                                                   ? "Move to Root · Keep World"
                                                   : "Move to Root · Keep Local");
                    ImGui::EndTooltip();
                    if (payload->IsDelivery())
                        ReparentEntity(drop.Scene, drop.Entity, 0,
                                       ImGui::GetIO().KeyShift ? TransformPolicy::KeepLocal
                                                               : TransformPolicy::KeepWorld);
                }
            ImGui::EndDragDropTarget();
        }
    }
    auto blank = ImGui::GetContentRegionAvail();
    if (blank.y > 0) {
        if (ImGui::InvisibleButton("##blank", {std::max(1.f, blank.x), blank.y}))
            SetSelectedEntity({});
        if (ImGui::BeginPopupContextItem("Blank entity actions")) {
            ImGui::BeginDisabled(!CanEdit());
            if (ImGui::MenuItem(PrefabDocument ? "Create Child of Root" : "Create Root")) {
                if (PrefabDocument)
                    AddChild(m_Context->GetEntityByUUID(m_Context->GetChildren().front()));
                else
                    AddEntity();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
        ImGui::IsKeyPressed(ImGuiKey_F)) {
        m_Search.clear();
        RevealSelected();
    }
    ImGui::EndChild();
    DrawDeleteDialog();
    ImGui::PopID();
}
void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
    if (!entity || !m_Context->IsEntityValid(entity.GetUUID()))
        return;
    ImGui::PushID(std::to_string(uint64_t(entity.GetUUID())).c_str());
    auto children = m_Context->GetChildren(entity.GetUUID());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (GetSelectedEntity() == entity)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (!m_Search.empty() || m_Reveal.count(entity.GetUUID()))
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    const bool open = ImGui::TreeNodeEx("##entity", flags, "%s", entity.GetName().c_str());
    if ((ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) ||
        (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
        SetSelectedEntity(entity);
    if (ImGui::BeginPopupContextItem()) {
        SetSelectedEntity(entity);
        const auto can = Availability ? Availability(EditOperation) : ActionAvailability{};
        ImGui::BeginDisabled(!CanEdit());
        if (ImGui::MenuItem("Create Child"))
            AddChild(entity);
        if (ImGui::MenuItem("Duplicate Subtree"))
            DuplicateSelected();
        if (CreatePrefab && ImGui::MenuItem("Create Prefab from Subtree..."))
            CreatePrefab(entity);
        if (!PrefabDocument && uint64_t(m_Context->GetRelationship(entity).Parent)) {
            if (ImGui::MenuItem("Move to Root · Keep World"))
                ReparentEntity(m_Context->GetIdentity(), entity.GetUUID(), 0,
                               TransformPolicy::KeepWorld);
            if (ImGui::MenuItem("Move to Root · Keep Local"))
                ReparentEntity(m_Context->GetIdentity(), entity.GetUUID(), 0,
                               TransformPolicy::KeepLocal);
        }
        if (ImGui::MenuItem("Delete Subtree..."))
            RequestDelete();
        ImGui::EndDisabled();
        PropertyUI::Help(can.Reason);
        ImGui::EndPopup();
    }
    if (CanEdit() && ImGui::BeginDragDropSource()) {
        EntityDrop drop{m_Context->GetIdentity(), entity.GetUUID()};
        ImGui::SetDragDropPayload("HAZEL_HIERARCHY_ENTITY", &drop, sizeof(drop));
        ImGui::Text("%s · %zu entities", entity.GetName().c_str(),
                    m_Context->GetSubtree(entity).size());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("HAZEL_HIERARCHY_ENTITY",
                                                         ImGuiDragDropFlags_AcceptBeforeDelivery))
            if (payload->DataSize == sizeof(EntityDrop)) {
                auto drop = *static_cast<const EntityDrop *>(payload->Data);
                const auto mode = ImGui::GetIO().KeyShift ? TransformPolicy::KeepLocal
                                                          : TransformPolicy::KeepWorld;
                const auto reason =
                    ReparentPreview(drop.Scene, drop.Entity, entity.GetUUID(), mode);
                ImGui::BeginTooltip();
                if (!reason.empty())
                    PropertyUI::Validation(reason.c_str());
                else
                    ImGui::Text("Parent under %s · %s", entity.GetName().c_str(),
                                mode == TransformPolicy::KeepWorld ? "Keep World" : "Keep Local");
                ImGui::EndTooltip();
                if (payload->IsDelivery())
                    ReparentEntity(drop.Scene, drop.Entity, entity.GetUUID(), mode);
            }
        ImGui::EndDragDropTarget();
    }
    if (m_Reveal.count(entity.GetUUID()) && GetSelectedEntity() == entity)
        ImGui::SetScrollHereY(.5f);
    m_Reveal.erase(entity.GetUUID());
    if (open && !children.empty()) {
        for (auto id : children)
            if (m_Search.empty() || m_Matches.count(id))
                DrawEntityNode(m_Context->GetEntityByUUID(id));
        ImGui::TreePop();
    }
    ImGui::PopID();
}
void SceneHierarchyPanel::ParentProperties(Entity e) {
    if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto edge = m_Context->GetRelationship(e);
        auto parent = m_Context->GetEntityByUUID(edge.Parent);
        const bool rootAsset = PrefabDocument && !uint64_t(edge.Parent);
        PropertyUI::Combo("parent-policy", "Reparent mode", m_ParentMode,
                          "Keep World\0Keep Local (moves entity)\0",
                          {"Keep World preserves placement or rejects nonrepresentable TRS. Keep "
                           "Local intentionally keeps authored values."});
        {
            PropertyUI::Row row("parent", "Parent");
            ImGui::BeginDisabled(rootAsset);
            if (ImGui::BeginCombo("##value", parent ? parent.GetName().c_str() : "Root")) {
                if (!PrefabDocument && ImGui::Selectable("Root", !parent))
                    ReparentEntity(m_Context->GetIdentity(), e.GetUUID(), 0,
                                   TransformPolicy(m_ParentMode));
                for (auto handle : m_Context->GetAllEntitiesWith<IDComponent>()) {
                    Entity target(handle, m_Context.get());
                    if (target == e || !m_Context->IsEntityValid(target.GetUUID()))
                        continue;
                    ImGui::PushID(std::to_string(uint64_t(target.GetUUID())).c_str());
                    if (ImGui::Selectable(target.GetName().c_str(), target == parent))
                        ReparentEntity(m_Context->GetIdentity(), e.GetUUID(), target.GetUUID(),
                                       TransformPolicy(m_ParentMode));
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            PropertyUI::Help(rootAsset
                                 ? "The prefab root defines this asset; parent only its children"
                                 : nullptr);
        }
        PropertyUI::ReadOnly("children-count", "Children",
                             std::to_string(m_Context->GetChildren(e.GetUUID()).size()).c_str());
        const auto world = m_Context->GetWorldTransform(e);
        const auto position = glm::vec3(world[3]);
        std::ostringstream text;
        text << position.x << ", " << position.y << ", " << position.z;
        PropertyUI::ReadOnly("world-position", "World position", text.str().c_str());
        try {
            Scene::ExactTRS(world);
        } catch (const std::exception &error) {
            ImGui::TextWrapped("World matrix: %s. Rendering stays exact; use local controls.",
                               error.what());
        }
        if (ImGui::TreeNode("World matrix (read-only)")) {
            for (int r = 0; r < 4; ++r)
                ImGui::Text("%.4g  %.4g  %.4g  %.4g", world[0][r], world[1][r], world[2][r],
                            world[3][r]);
            ImGui::TreePop();
        }
        if (e.HasComponent<Rigidbody2DComponent>() || e.HasComponent<BoxCollider2DComponent>() ||
            e.HasComponent<CircleCollider2DComponent>())
            ImGui::TextWrapped(
                "Physics owner: root only, Z rotation and positive XY scale. Circle colliders "
                "require uniform XY scale; visual children follow this body.");
        if (!m_HierarchyError.empty())
            PropertyUI::Validation(m_HierarchyError.c_str());
    }
}
} // namespace Hazel
