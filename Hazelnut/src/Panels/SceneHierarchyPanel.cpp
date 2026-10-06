#include "SceneHierarchyPanel.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Scene/Components.h"

#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/UI/UI.h"
#include "SpriteWidgets.h"
#include "UI/PropertyUI.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <type_traits>

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
#define _CRT_SECURE_NO_WARNINGS
#endif

namespace Hazel
{

SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene> &context) { SetContext(context); }

void SceneHierarchyPanel::SetContext(const Ref<Scene> &context)
{
    m_SelectionContext = {};
    m_Pickers.clear();
    m_PrefabChoices.clear();
    m_PrefabChoicesReady = false;
    m_Context = context;
}

bool SceneHierarchyPanel::CanEdit(bool report) const
{
    const auto available =
        Availability
            ? Availability(EditOperation)
            : ActionAvailability{m_Context && !m_Context->IsRunning() ? nullptr : "Stop Play to edit"};
    if (!available && report && ReportError)
        ReportError(available.Reason);
    return bool(available);
}
bool SceneHierarchyPanel::AddEntity(const std::string &name)
{
    if (!m_Context || !CanEdit(true))
        return false;
    return SetSelectedEntity(m_Context->CreateEntity(name));
}
bool SceneHierarchyPanel::DeleteSelected()
{
    auto entity = GetSelectedEntity();
    if (!entity || !CanEdit(true))
        return false;
    m_SelectionContext = {};
    m_Pickers.clear();
    m_Context->DestroyEntity(entity);
    return true;
}
void SceneHierarchyPanel::DrawAssetProperties(Entity entity)
{
    if (entity != m_SelectionContext)
    {
        m_Pickers.clear();
        m_SelectionContext = entity;
    }
    DrawComponents(entity);
}
void SceneHierarchyPanel::OnImGuiRender()
{
    m_Focused=false;
    const auto availability = Availability ? Availability(EditOperation) : ActionAvailability{};
    if(HierarchyVisible) {
    ImGui::Begin("Scene Hierarchy",&HierarchyVisible);
    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::SetNextItemWidth(-ImGui::GetFrameHeightWithSpacing());
    ImGui::InputTextWithHint("##search", "Find entity...", &m_Search);
    ImGui::SameLine();
    if (ImGui::Button("X##clear-search"))
        m_Search.clear();
    PropertyUI::Help("Clear entity search");
    ImGui::BeginDisabled(!CanEdit());
    if (ImGui::Button("Add Entity"))
        AddEntity();
    ImGui::EndDisabled();
    PropertyUI::Help(availability.Reason);
    ImGui::BeginChild("Entity list", {0, 0}, false);
    if (m_Context)
    {
        std::vector<Entity> observed;
        m_Context->m_Registry.each([&](auto id) { observed.emplace_back(id, m_Context.get()); });
        std::sort(observed.begin(), observed.end(),
                  [](Entity a, Entity b)
                  {
                      return a.GetName() == b.GetName() ? uint64_t(a.GetUUID()) < uint64_t(b.GetUUID())
                                                        : a.GetName() < b.GetName();
                  });
        size_t count = 0;
        for (auto entity : observed)
            if (entity)
            {
                auto name = entity.GetName(), query = m_Search;
                auto lower = [](std::string &text)
                {
                    std::transform(text.begin(), text.end(), text.begin(),
                                   [](unsigned char c) { return char(std::tolower(c)); });
                };
                lower(name);
                lower(query);
                if (!query.empty() && name.find(query) == std::string::npos)
                    continue;
                DrawEntityNode(entity);
                ++count;
            }
        if (!count)
            ImGui::TextDisabled(m_Search.empty() ? "No entities. Add one above."
                                                 : "No matching entities. Clear search to see all.");
        // A dedicated item covers only remaining content space. Row clicks,
        // scrollbars, headers, menus and text entry cannot clear selection.
        const auto space = ImGui::GetContentRegionAvail();
        if (space.y > 0 && ImGui::InvisibleButton("##blank", {std::max(space.x, 1.f), space.y}))
            SetSelectedEntity({});
        if (ImGui::BeginPopupContextItem("Blank entity actions"))
        {
            ImGui::BeginDisabled(!CanEdit());
            if (ImGui::MenuItem("Create Empty Entity"))
                AddEntity();
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::End();
    }
    if(!PropertiesVisible)return;
    ImGui::Begin("Properties",&PropertiesVisible);
    m_Focused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (auto entity = GetSelectedEntity())
    {
        ImGui::TextWrapped("Scene Entity: %s", entity.GetName().c_str());
        ImGui::BeginDisabled(!CanEdit());
        if (CreatePrefab && ImGui::Button("Create Prefab..."))
            CreatePrefab(entity);
        ImGui::EndDisabled();
        PropertyUI::Help(availability.Reason);
        DrawComponents(entity);
    }
    else
        ImGui::TextWrapped("Select a scene entity here or in the viewport. Asset editing retains that "
                           "assignment target.");
    ImGui::End();
}

bool SceneHierarchyPanel::SetSelectedEntity(Entity entity)
{
    if (!entity.BelongsTo(m_Context.get()))
    {
        m_SelectionContext = {};
        return false;
    }
    if (!m_Context || !entity)
    {
        m_SelectionContext = {};
        return false;
    }
    if (m_SelectionContext != entity)
        m_Pickers.clear();
    m_SelectionContext = entity;
    return true;
}

bool SceneHierarchyPanel::AssignSpriteTexture(SpriteRendererComponent &component,
                                              const std::filesystem::path &path)
{
    try
    {
        auto texture = Texture2D::Create(path.generic_u8string());
        component.SetTexture(std::move(texture));
        return true;
    }
    catch (const std::runtime_error &error)
    {
        HZ_ERROR("Texture assignment '{}': {}", path.generic_u8string(), error.what());
        return false;
    }
}

void SceneHierarchyPanel::DrawEntityNode(Entity entity)
{
    ImGui::PushID(std::to_string(m_Context->GetIdentity()).c_str());
    ImGui::PushID(std::to_string(uint64_t(entity.GetUUID())).c_str());
    if (ImGui::Selectable((entity.GetName() + "###entity").c_str(), m_SelectionContext == entity))
        SetSelectedEntity(entity);
    if (ImGui::BeginPopupContextItem())
    {
        SetSelectedEntity(entity);
        ImGui::BeginDisabled(!CanEdit());
        if (CreatePrefab && ImGui::MenuItem("Create Prefab..."))
            CreatePrefab(entity);
        if (ImGui::MenuItem("Delete Entity"))
            DeleteSelected();
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::PopID();
}

template <typename T, typename UIFunction>
static void DrawComponent(const std::string &name, Entity entity, std::map<std::string,bool>& sections,std::set<std::string>& restore,UIFunction uiFunction)
{
    const ImGuiTreeNodeFlags treeNodeFlags =
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
    if (entity.HasComponent<T>())
    {
        ImGui::PushID(name.c_str());
        auto &component = entity.GetComponent<T>();
        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
        float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImGui::Separator();
        if(restore.erase(name))ImGui::SetNextItemOpen(sections[name],ImGuiCond_Always);
        bool open = ImGui::TreeNodeEx("##component", treeNodeFlags, "%s", name.c_str());
        sections[name]=open;
        ImGui::PopStyleVar();
        ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
        if (ImGui::Button("+", ImVec2{lineHeight, lineHeight}))
        {
            ImGui::OpenPopup("ComponentSettings");
        }

        bool removeComponent = false;
        if (ImGui::BeginPopup("ComponentSettings"))
        {
            if (ImGui::MenuItem("Remove component", nullptr, false,
                                !std::is_same_v<T, TransformComponent>))
                removeComponent = true;

            ImGui::EndPopup();
        }

        if (open)
        {
            uiFunction(component);
            ImGui::TreePop();
        }

        if (removeComponent)
            entity.RemoveComponent<T>();
        ImGui::PopID();
    }
}

void SceneHierarchyPanel::DrawComponents(Entity entity)
{
    if(m_SectionEntity!=uint64_t(entity.GetUUID())){m_Sections.clear();m_RestoreSections.clear();m_SectionEntity=entity.GetUUID();}
    if (!entity)
        return;
    ImGui::PushID(std::to_string(m_Context->GetIdentity()).c_str());
    ImGui::PushID(std::to_string(uint64_t(entity.GetUUID())).c_str());
    const bool editable = CanEdit();
    if (!editable)
        ImGui::TextWrapped("%s", Availability ? Availability(EditOperation).Reason
                                              : "Stop Play to edit authored content");
    ImGui::BeginDisabled(!editable);
    if (entity.HasComponent<TagComponent>())
        PropertyUI::Text("tag", "Name", entity.GetComponent<TagComponent>().Tag);

    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponent");

    if (ImGui::BeginPopup("AddComponent"))
    {
        DisplayAddComponentEntry<CameraComponent>("Camera");
        DisplayAddComponentEntry<ScriptComponent>("Script");
        DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
        if (entity.HasComponent<SpriteRendererComponent>())
            DisplayAddComponentEntry<SpriteAnimationComponent>("Sprite Animation");
        DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
        DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
        DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
        DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
        DisplayAddComponentEntry<TextComponent>("Text Component");

        ImGui::EndPopup();
    }

    DrawComponent<TransformComponent>(
        "Transform", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const TransformComponent defaults;
            PropertyUI::Vector("translation", "Position", glm::value_ptr(component.Translation), 3, .1f,
                               glm::value_ptr(defaults.Translation));
            glm::vec3 rotation = glm::degrees(component.Rotation);
            if (PropertyUI::Vector("rotation", "Rotation (deg)", glm::value_ptr(rotation), 3, .5f,
                                   glm::value_ptr(defaults.Rotation)))
                component.Rotation = glm::radians(rotation);
            PropertyUI::Vector("scale", "Scale", glm::value_ptr(component.Scale), 3, .1f,
                               glm::value_ptr(defaults.Scale));
        });

    DrawComponent<CameraComponent>(
        "Camera", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            auto &camera = component.Camera;

            PropertyUI::Checkbox("Primary", "Primary", component.Primary);

            const char *projectionTypeStrings[] = {"Perspective", "Orthographic"};
            const char *currentProjectionTypeString =
                projectionTypeStrings[(int)camera.GetProjectionType()];
            {
                PropertyUI::Row projectionRow("projection", "Projection");
                if (ImGui::BeginCombo("##value", currentProjectionTypeString))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
                        if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
                        {
                            currentProjectionTypeString = projectionTypeStrings[i];
                            camera.SetProjectionType((SceneCamera::ProjectionType)i);
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }
            if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
            {
                float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
                if (PropertyUI::DragFloat("vertical-fov", "Vertical FOV (deg)", perspectiveVerticalFov))
                    camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

                float perspectiveNear = camera.GetPerspectiveNearClip();
                if (PropertyUI::DragFloat("Near", "Near", perspectiveNear))
                    camera.SetPerspectiveNearClip(perspectiveNear);

                float perspectiveFar = camera.GetPerspectiveFarClip();
                if (PropertyUI::DragFloat("Far", "Far", perspectiveFar))
                    camera.SetPerspectiveFarClip(perspectiveFar);
            }

            if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
            {
                float orthoSize = camera.GetOrthographicSize();
                if (PropertyUI::DragFloat("Size", "Size", orthoSize))
                    camera.SetOrthographicSize(orthoSize);

                float orthoNear = camera.GetOrthographicNearClip();
                if (PropertyUI::DragFloat("Near", "Near", orthoNear))
                    camera.SetOrthographicNearClip(orthoNear);

                float orthoFar = camera.GetOrthographicFarClip();
                if (PropertyUI::DragFloat("Far", "Far", orthoFar))
                    camera.SetOrthographicFarClip(orthoFar);

                PropertyUI::Checkbox("Fixed Aspect Ratio", "Fixed Aspect Ratio",
                                     component.FixedAspectRatio);
            }
        });

    DrawComponent<ScriptComponent>(
        "Script", entity, m_Sections,m_RestoreSections,
        [this, entity](auto &component) mutable
        {
            if (m_Context && m_Context->IsRunning())
            {
                ImGui::TextWrapped(
                    "Runtime script values are temporary. Stop Play to edit authored fields.");
                return;
            }
            auto classes = ScriptEngine::GetEntityClasses();
            std::vector<std::string> names;
            for (auto &[name, type] : classes)
                names.push_back(name);
            std::sort(names.begin(), names.end());
            {
                PropertyUI::Row classRow("class", "Class");
                if (ImGui::BeginCombo("##value", component.ClassName.empty()
                                                     ? "Select compiled class"
                                                     : component.ClassName.c_str()))
                {
                    if (ImGui::Selectable("None", component.ClassName.empty()))
                    {
                        component.ClassName.clear();
                        ScriptEngine::GetScriptFieldMap(entity).clear();
                    }
                    for (auto &name : names)
                        if (ImGui::Selectable(name.c_str(), name == component.ClassName))
                        {
                            if (component.ClassName != name)
                                ScriptEngine::GetScriptFieldMap(entity).clear();
                            component.ClassName = name;
                        }
                    ImGui::EndCombo();
                }
            }
            if (EditScript && !component.ClassName.empty() && ImGui::Button("Open Script"))
                EditScript(component.ClassName);
            if (ImGui::CollapsingHeader("Script authoring help"))
                ImGui::TextWrapped(
                    "Project > Create Script / Build Scripts. Fields are authored overrides; "
                    "unset fields use C# defaults, which are not evaluated here.");
            auto type = ScriptEngine::GetEntityClass(component.ClassName);
            if (!type)
            {
                if (!component.ClassName.empty())
                    ImGui::TextWrapped(
                        "Class unavailable. Build Scripts or select an existing compiled class.");
                return;
            }
            auto &values = ScriptEngine::GetScriptFieldMap(entity);
            for (auto &[name, field] : type->GetFields())
            {
                if (field.Type == ScriptFieldType::None)
                    continue;
                ImGui::PushID(name.c_str());
                auto found = values.find(name);
                bool authored = found != values.end() && found->second.Field.Type == field.Type;
                if (!authored)
                {
                    {
                        PropertyUI::Row row("default", name.c_str());
                        ImGui::TextWrapped("C# default (value not evaluated)");
                        if (ImGui::SmallButton("Create override..."))
                            ImGui::OpenPopup("New override");
                        if (ImGui::BeginPopup("New override"))
                        {
                            ImGui::TextWrapped(
                                "An explicit override starts at zero / unassigned. This is "
                                "not the C# constructor value.");
                            if (ImGui::Button("Create explicit override"))
                            {
                                ScriptFieldInstance initial;
                                initial.Field = {field.Type, name, nullptr};
                                values[name] = initial;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                    continue;
                }
                ScriptFieldInstance value;
                if (authored)
                    value = found->second;
                value.Field = {field.Type, name, nullptr};
                bool changed = false;
                switch (field.Type)
                {
                case ScriptFieldType::Float:
                {
                    auto x = value.GetValue<float>();
                    changed = PropertyUI::DragFloat("value", name.c_str(), x, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Double:
                {
                    auto x = value.GetValue<double>();
                    changed = PropertyUI::Double("value", name.c_str(), x);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Bool:
                {
                    auto x = value.GetValue<bool>();
                    changed = PropertyUI::Checkbox("value", name.c_str(), x);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Vector2:
                {
                    auto x = value.GetValue<glm::vec2>();
                    changed = PropertyUI::Vector("value", name.c_str(), glm::value_ptr(x), 2, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Vector3:
                {
                    auto x = value.GetValue<glm::vec3>();
                    changed = PropertyUI::Vector("value", name.c_str(), glm::value_ptr(x), 3, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Vector4:
                {
                    auto x = value.GetValue<glm::vec4>();
                    changed = PropertyUI::Vector("value", name.c_str(), glm::value_ptr(x), 4, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Entity:
                {
                    PropertyUI::Row row("value", name.c_str());
                    auto id = value.GetValue<uint64_t>();
                    auto ref = m_Context ? m_Context->GetEntityByUUID(id) : Entity{};
                    const auto referenceName = ref  ? ref.GetName()
                                               : id ? "Missing entity (" + std::to_string(id) + ")"
                                                    : "None";
                    if (ImGui::BeginCombo("##value", referenceName.c_str()))
                    {
                        if (ImGui::Selectable("None", !id))
                        {
                            value.SetValue<uint64_t>(0);
                            changed = true;
                        }
                        if (m_Context)
                            for (auto e : m_Context->GetAllEntitiesWith<IDComponent>())
                            {
                                Entity choice{e, m_Context.get()};
                                ImGui::PushID(static_cast<int>((entt::entity)choice));
                                if (ImGui::Selectable(choice.GetName().c_str(), choice.GetUUID() == id))
                                {
                                    value.SetValue<uint64_t>(choice.GetUUID());
                                    changed = true;
                                }
                                if (ImGui::IsItemHovered())
                                {
                                    auto p = choice.GetComponent<TransformComponent>().Translation;
                                    ImGui::SetTooltip("Position: %.2f, %.2f, %.2f", p.x, p.y, p.z);
                                }
                                ImGui::PopID();
                            }
                        ImGui::EndCombo();
                    }
                    break;
                }
                case ScriptFieldType::Prefab:
                {
                    PropertyUI::Row row("value", name.c_str());
                    if (ImGui::BeginCombo("##value", value.AssetReference.empty()
                                                         ? "None"
                                                         : value.AssetReference.c_str()))
                    {
                        if (ImGui::Selectable("None", value.AssetReference.empty()))
                        {
                            value.AssetReference.clear();
                            changed = true;
                        }
                        auto root = Project::GetAssetDirectory();
                        if (!m_PrefabChoicesReady || ImGui::Button("Refresh prefabs"))
                        {
                            m_PrefabChoicesReady = true;
                            m_PrefabChoices.clear();
                            std::error_code error;
                            for (auto it = std::filesystem::recursive_directory_iterator(root, error);
                                 !error && it != std::filesystem::recursive_directory_iterator();
                                 it.increment(error))
                                if (it->is_regular_file(error) && it->path().extension() == ".hprefab")
                                    m_PrefabChoices.push_back(
                                        it->path().lexically_relative(root).generic_u8string());
                            std::sort(m_PrefabChoices.begin(), m_PrefabChoices.end());
                        }
                        for (auto &path : m_PrefabChoices)
                            if (ImGui::Selectable(path.c_str(), path == value.AssetReference))
                            {
                                try
                                {
                                    Prefab::Load(root, std::filesystem::u8path(path));
                                    value.AssetReference = path;
                                    changed = true;
                                }
                                catch (const std::exception &error)
                                {
                                    if (ReportError)
                                        ReportError(error.what());
                                    else
                                        HZ_ERROR("Select prefab: {}", error.what());
                                }
                            }
                        ImGui::EndCombo();
                    }
                    break;
                }
                case ScriptFieldType::Sprite:
                {
                    SpriteReference r{std::filesystem::u8path(value.AssetReference), value.AssetID};
                    changed = SpritePicker(name.c_str(), r, m_Pickers[ImGui::GetID("sprite-field")],
                                           OpenAsset, [this] { return CanEdit(); });
                    value.AssetReference = r.Sheet.generic_u8string();
                    value.AssetID = r.Region;
                    break;
                }
                case ScriptFieldType::SpriteAnimation:
                {
                    AnimationReference r{std::filesystem::u8path(value.AssetReference), value.AssetID};
                    changed = ClipPicker(name.c_str(), r, m_Pickers[ImGui::GetID("clip-field")],
                                         OpenAsset, [this] { return CanEdit(); });
                    value.AssetReference = r.Sheet.generic_u8string();
                    value.AssetID = r.Clip;
                    break;
                }
#define HZ_FIELD_NUMBER(Type, Cpp, Gui)                                                                 \
    case ScriptFieldType::Type:                                                                         \
    {                                                                                                   \
        auto x = value.GetValue<Cpp>();                                                                 \
        changed = PropertyUI::Scalar("value", name.c_str(), Gui, &x);                                   \
        value.SetValue(x);                                                                              \
        break;                                                                                          \
    }
                    HZ_FIELD_NUMBER(Char, uint16_t, ImGuiDataType_U16)
                    HZ_FIELD_NUMBER(Byte, int8_t, ImGuiDataType_S8)
                    HZ_FIELD_NUMBER(Short, int16_t, ImGuiDataType_S16)
                    HZ_FIELD_NUMBER(Int, int32_t, ImGuiDataType_S32)
                    HZ_FIELD_NUMBER(Long, int64_t, ImGuiDataType_S64)
                    HZ_FIELD_NUMBER(UByte, uint8_t, ImGuiDataType_U8)
                    HZ_FIELD_NUMBER(UShort, uint16_t, ImGuiDataType_U16)
                    HZ_FIELD_NUMBER(UInt, uint32_t, ImGuiDataType_U32)
                    HZ_FIELD_NUMBER(ULong, uint64_t, ImGuiDataType_U64)
#undef HZ_FIELD_NUMBER
                case ScriptFieldType::None:
                    break;
                }
                if (changed)
                    values[name] = value;
                {
                    PropertyUI::Row resetRow("default", "Override");
                    if (ImGui::SmallButton("Use C# default"))
                        values.erase(name);
                }
                ImGui::PopID();
            }
        });

    DrawComponent<SpriteRendererComponent>(
        "Sprite Renderer", entity, m_Sections,m_RestoreSections,
        [this](auto &component)
        {
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));

            SpriteSourceEditor(component, m_Pickers[ImGui::GetID("source-picker")], OpenAsset,
                               [this] { return CanEdit(); });
        });
    DrawComponent<SpriteAnimationComponent>(
        "Sprite Animation", entity, m_Sections,m_RestoreSections,
        [this](auto &component)
        {
            if (ClipPicker("Clip", component.DefaultClip, m_Pickers[ImGui::GetID("animation-picker")],
                           OpenAsset, [this] { return CanEdit(); }))
                component.ResetRuntime();
            PropertyUI::Checkbox("Autoplay", "Autoplay", component.Autoplay);
            if (PropertyUI::Double("speed", "Speed", component.Speed, .1))
                component.PreparedEpoch = 0;
            ImGui::TextWrapped(
                "Animation supplies the rendered sprite while assigned. Clearing/removing it "
                "restores the static sprite source. Speed zero holds the frame.");
            if (!component.Error.empty())
                ImGui::TextColored({1, .4f, .3f, 1}, "%s", component.Error.c_str());
        });

    DrawComponent<CircleRendererComponent>(
        "Circle Renderer", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));
            PropertyUI::DragFloat("Thickness", "Thickness", component.Thickness, 0.025f, 0.0f, 1.0f);
            PropertyUI::DragFloat("Fade", "Fade", component.Fade, 0.00025f, 0.0f, 1.0f);
        });

    DrawComponent<Rigidbody2DComponent>(
        "Rigidbody 2D", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const Rigidbody2DComponent defaults;
            const char *bodyTypeStrings[] = {"Static", "Dynamic", "Kinematic"};
            const char *currentBodyTypeString = bodyTypeStrings[(int)component.Type];
            {
                PropertyUI::Row bodyRow("body-type", "Body Type");
                if (ImGui::BeginCombo("##value", currentBodyTypeString))
                {
                    for (int i = 0; i < 3; i++)
                    {
                        bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
                        if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
                        {
                            currentBodyTypeString = bodyTypeStrings[i];
                            component.Type = (Rigidbody2DComponent::BodyType)i;
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }
            PropertyUI::Checkbox("Fixed Rotation", "Fixed Rotation", component.FixedRotation,
                                 &defaults.FixedRotation);
            PropertyUI::DragFloat("Gravity Scale", "Gravity Scale", component.GravityScale, 0.05f,
                                  -10.0f, 10.0f, "%.2f", &defaults.GravityScale);
        });

    DrawComponent<BoxCollider2DComponent>(
        "Box Collider 2D", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const BoxCollider2DComponent defaults;
            PropertyUI::Vector("Offset", "Offset", glm::value_ptr(component.Offset), 2, .1f,
                               glm::value_ptr(defaults.Offset));
            PropertyUI::Vector("Size", "Size", glm::value_ptr(component.Size), 2, .1f,
                               glm::value_ptr(defaults.Size));
            PropertyUI::DragFloat("Density", "Density", component.Density, .01f, 0, 1, "%.3f",
                                  &defaults.Density);
            PropertyUI::DragFloat("Friction", "Friction", component.Friction, .01f, 0, 1, "%.3f",
                                  &defaults.Friction);
            PropertyUI::DragFloat("Restitution", "Restitution", component.Restitution, .01f, 0, 1,
                                  "%.3f", &defaults.Restitution);
            PropertyUI::DragFloat("Restitution Threshold", "Restitution Threshold",
                                  component.RestitutionThreshold, .01f, 0, 0, "%.3f",
                                  &defaults.RestitutionThreshold);
        });

    DrawComponent<CircleCollider2DComponent>(
        "Circle Collider 2D", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const CircleCollider2DComponent defaults;
            PropertyUI::Vector("Offset", "Offset", glm::value_ptr(component.Offset), 2, .1f,
                               glm::value_ptr(defaults.Offset));
            PropertyUI::DragFloat("Radius", "Radius", component.Radius, .1f, 0, 0, "%.3f",
                                  &defaults.Radius);
            PropertyUI::DragFloat("Density", "Density", component.Density, .01f, 0, 1, "%.3f",
                                  &defaults.Density);
            PropertyUI::DragFloat("Friction", "Friction", component.Friction, .01f, 0, 1, "%.3f",
                                  &defaults.Friction);
            PropertyUI::DragFloat("Restitution", "Restitution", component.Restitution, .01f, 0, 1,
                                  "%.3f", &defaults.Restitution);
            PropertyUI::DragFloat("Restitution Threshold", "Restitution Threshold",
                                  component.RestitutionThreshold, .01f, 0, 0, "%.3f",
                                  &defaults.RestitutionThreshold);
        });

    DrawComponent<TextComponent>(
        "Text Renderer", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            PropertyUI::Multiline("text", "Text", component.TextString);
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));
            PropertyUI::DragFloat("Kerning", "Kerning", component.Kerning, 0.025f);
            PropertyUI::DragFloat("Line Spacing", "Line Spacing", component.LineSpacing, 0.025f);
        });

    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::PopID();
}

template <typename T> void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string &entryName)
{
    if (!m_SelectionContext.HasComponent<T>())
    {
        if (ImGui::MenuItem(entryName.c_str()) && CanEdit(true))
        {
            m_SelectionContext.AddComponent<T>();
            ImGui::CloseCurrentPopup();
        }
    }
}

} // namespace Hazel
