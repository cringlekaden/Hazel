#include "ContentBrowserPayload.h"
#include "SceneHierarchyPanel.h"
#include "Hazel/Scene/Components.h"

#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/UI/UI.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Prefab.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <algorithm>

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
  #define _CRT_SECURE_NO_WARNINGS
#endif

namespace Hazel {

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		m_SelectionContext = {};
		m_Context = context;
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		if (m_Context)
		{
			std::vector<Entity> observed;
            m_Context->m_Registry.each([&](auto id){observed.emplace_back(id,m_Context.get());});
            for(auto entity:observed) if(entity) DrawEntityNode(entity);

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			// Right-click on blank space
			if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Empty Entity"))
					m_Context->CreateEntity("Empty Entity");

				ImGui::EndPopup();
			}

		}
		ImGui::End();

		ImGui::Begin("Properties");
		if (GetSelectedEntity())
		{
            if(CreatePrefab && !m_Context->IsRunning() && ImGui::Button("Create Prefab...")) CreatePrefab(m_SelectionContext);
			DrawComponents(m_SelectionContext);
		}

		ImGui::End();
	}

	bool SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		if (!entity.BelongsTo(m_Context.get())) { m_SelectionContext = {}; return false; }
		if (!m_Context || !entity) { m_SelectionContext = {}; return false; }
		m_SelectionContext = entity;
		return true;
	}

	bool SceneHierarchyPanel::AssignSpriteTexture(SpriteRendererComponent& component, const std::filesystem::path& path)
	{
		try {
			auto texture = Texture2D::Create(path.generic_u8string());
			component.Texture = std::move(texture);
			return true;
		} catch (const std::runtime_error& error) {
			HZ_ERROR("Texture assignment '{}': {}", path.generic_u8string(), error.what());
			return false;
		}
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());
		if (ImGui::IsItemClicked())
		{
			m_SelectionContext = entity;
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if(CreatePrefab && !m_Context->IsRunning() && ImGui::MenuItem("Create Prefab...")) CreatePrefab(entity);
            if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			bool opened = ImGui::TreeNodeEx((void*)9817239, flags, "%s", tag.c_str());
			if (opened)
				ImGui::TreePop();
			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity)
				m_SelectionContext = {};
		}
	}

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::TextUnformatted(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, "%s", name.c_str());
			ImGui::PopStyleVar(
			);
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentSettings");
			}

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove component"))
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
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			std::snprintf(buffer, sizeof(buffer), "%s", tag.c_str());
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			DisplayAddComponentEntry<CameraComponent>("Camera");
			DisplayAddComponentEntry<ScriptComponent>("Script");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
			DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
			DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
			DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
			DisplayAddComponentEntry<TextComponent>("Text Component");

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
		{
			DrawVec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);
			DrawVec3Control("Scale", component.Scale, 1.0f);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
		{
			auto& camera = component.Camera;

			ImGui::Checkbox("Primary", &component.Primary);

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
			if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
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

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
					camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

				float perspectiveNear = camera.GetPerspectiveNearClip();
				if (ImGui::DragFloat("Near", &perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				if (ImGui::DragFloat("Far", &perspectiveFar))
					camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
			{
				float orthoSize = camera.GetOrthographicSize();
				if (ImGui::DragFloat("Size", &orthoSize))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				if (ImGui::DragFloat("Near", &orthoNear))
					camera.SetOrthographicNearClip(orthoNear);

				float orthoFar = camera.GetOrthographicFarClip();
				if (ImGui::DragFloat("Far", &orthoFar))
					camera.SetOrthographicFarClip(orthoFar);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			}
		});

        DrawComponent<ScriptComponent>("Script", entity, [this,entity](auto& component) mutable {
            if(m_Context && m_Context->IsRunning()) { ImGui::TextWrapped("Runtime script values are temporary. Stop Play to edit authored fields."); return; }
            auto classes=ScriptEngine::GetEntityClasses(); std::vector<std::string> names;
            for(auto& [name,type]:classes) names.push_back(name); std::sort(names.begin(),names.end());
            if(ImGui::BeginCombo("Class",component.ClassName.empty()?"Select compiled class":component.ClassName.c_str())) {
                if(ImGui::Selectable("None",component.ClassName.empty())) { component.ClassName.clear(); ScriptEngine::GetScriptFieldMap(entity).clear(); }
                for(auto& name:names) if(ImGui::Selectable(name.c_str(),name==component.ClassName)) {
                    if(component.ClassName!=name) ScriptEngine::GetScriptFieldMap(entity).clear();
                    component.ClassName=name;
                }
                ImGui::EndCombo();
            }
            if(EditScript && !component.ClassName.empty() && ImGui::Button("Open Script")) EditScript(component.ClassName);
            ImGui::TextWrapped("Create scripts in Project > Create Script, then Build Scripts. Fields below are authored overrides; unset fields use C# defaults.");
            auto type=ScriptEngine::GetEntityClass(component.ClassName);
            if(!type) { if(!component.ClassName.empty()) ImGui::TextWrapped("Class unavailable. Build Scripts or select an existing compiled class."); return; }
            auto& values=ScriptEngine::GetScriptFieldMap(entity);
            for(auto& [name,field]:type->GetFields()) {
                if(field.Type==ScriptFieldType::None) continue;
                ImGui::PushID(name.c_str()); auto found=values.find(name);
                bool authored=found!=values.end() && found->second.Field.Type==field.Type;
                ScriptFieldInstance value; if(authored) value=found->second; value.Field={field.Type,name,nullptr};
                bool changed=false;
                switch(field.Type) {
                    case ScriptFieldType::Float: { auto x=value.GetValue<float>(); changed=ImGui::DragFloat(name.c_str(),&x,.05f); value.SetValue(x); break; }
                    case ScriptFieldType::Double: { auto x=value.GetValue<double>(); changed=ImGui::InputDouble(name.c_str(),&x); value.SetValue(x); break; }
                    case ScriptFieldType::Bool: { auto x=value.GetValue<bool>(); changed=ImGui::Checkbox(name.c_str(),&x); value.SetValue(x); break; }
                    case ScriptFieldType::Vector2: { auto x=value.GetValue<glm::vec2>(); changed=ImGui::DragFloat2(name.c_str(),glm::value_ptr(x),.05f); value.SetValue(x); break; }
                    case ScriptFieldType::Vector3: { auto x=value.GetValue<glm::vec3>(); changed=ImGui::DragFloat3(name.c_str(),glm::value_ptr(x),.05f); value.SetValue(x); break; }
                    case ScriptFieldType::Vector4: { auto x=value.GetValue<glm::vec4>(); changed=ImGui::DragFloat4(name.c_str(),glm::value_ptr(x),.05f); value.SetValue(x); break; }
                    case ScriptFieldType::Entity: {
                        auto id=value.GetValue<uint64_t>(); auto ref=m_Context?m_Context->GetEntityByUUID(id):Entity{};
                        if(ImGui::BeginCombo(name.c_str(),ref?ref.GetName().c_str():"None")) {
                            if(ImGui::Selectable("None",!id)) {value.SetValue<uint64_t>(0);changed=true;}
                            if(m_Context) for(auto e:m_Context->GetAllEntitiesWith<IDComponent>()) { Entity choice{e,m_Context.get()};
                                ImGui::PushID(static_cast<int>((entt::entity)choice));
                                if(ImGui::Selectable(choice.GetName().c_str(),choice.GetUUID()==id)) {value.SetValue<uint64_t>(choice.GetUUID());changed=true;}
                                if(ImGui::IsItemHovered()) {auto p=choice.GetComponent<TransformComponent>().Translation;ImGui::SetTooltip("Position: %.2f, %.2f, %.2f",p.x,p.y,p.z);}
                                ImGui::PopID();
                            } ImGui::EndCombo();
                        } break;
                    }
                    case ScriptFieldType::Prefab: {
                        if(ImGui::BeginCombo(name.c_str(),value.AssetReference.empty()?"None":value.AssetReference.c_str())) {
                            if(ImGui::Selectable("None",value.AssetReference.empty())) {value.AssetReference.clear();changed=true;}
                            std::error_code error; auto root=Project::GetAssetDirectory(); std::vector<std::string> assets;
                            for(auto it=std::filesystem::recursive_directory_iterator(root,error); !error && it!=std::filesystem::recursive_directory_iterator(); it.increment(error))
                                if(it->is_regular_file(error) && it->path().extension()==".hprefab") assets.push_back(it->path().lexically_relative(root).generic_u8string());
                            std::sort(assets.begin(),assets.end());
                            for(auto& path:assets) if(ImGui::Selectable(path.c_str(),path==value.AssetReference)) {
                                try { Prefab::Load(root,std::filesystem::u8path(path)); value.AssetReference=path; changed=true; }
                                catch(const std::exception& error) { if(ReportError) ReportError(error.what()); else HZ_ERROR("Select prefab: {}",error.what()); }
                            } ImGui::EndCombo();
                        } break;
                    }
#define HZ_FIELD_NUMBER(Type,Cpp,Gui) case ScriptFieldType::Type: {auto x=value.GetValue<Cpp>();changed=ImGui::InputScalar(name.c_str(),Gui,&x);value.SetValue(x);break;}
                    HZ_FIELD_NUMBER(Char,uint16_t,ImGuiDataType_U16)
                    HZ_FIELD_NUMBER(Byte,int8_t,ImGuiDataType_S8)
                    HZ_FIELD_NUMBER(Short,int16_t,ImGuiDataType_S16)
                    HZ_FIELD_NUMBER(Int,int32_t,ImGuiDataType_S32)
                    HZ_FIELD_NUMBER(Long,int64_t,ImGuiDataType_S64)
                    HZ_FIELD_NUMBER(UByte,uint8_t,ImGuiDataType_U8)
                    HZ_FIELD_NUMBER(UShort,uint16_t,ImGuiDataType_U16)
                    HZ_FIELD_NUMBER(UInt,uint32_t,ImGuiDataType_U32)
                    HZ_FIELD_NUMBER(ULong,uint64_t,ImGuiDataType_U64)
#undef HZ_FIELD_NUMBER
                    case ScriptFieldType::None: break;
                }
                if(changed) values[name]=value;
                if(authored) { ImGui::SameLine(); if(ImGui::SmallButton("Use C# default")) values.erase(name); }
                else ImGui::TextDisabled("Using C# default until edited");
                ImGui::PopID();
            }
        });

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

			ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const auto texturePath = ContentBrowserPath(payload->Data, payload->DataSize);
					AssignSpriteTexture(component, texturePath);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::DragFloat("Tiling Factor", &component.TilingFactor, 0.1f, 0.0f, 100.0f);
		});

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			ImGui::DragFloat("Thickness", &component.Thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.Fade, 0.00025f, 0.0f, 1.0f);
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component)
		{
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic"};
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
			if (ImGui::BeginCombo("Body Type", currentBodyTypeString))
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

			ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
			ImGui::DragFloat("Gravity Scale", &component.GravityScale, 0.05f, -10.0f, 10.0f);
		});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat2("Size", glm::value_ptr(component.Size));
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
		});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat("Radius", &component.Radius);
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
		});

		DrawComponent<TextComponent>("Text Renderer", entity, [](auto& component)
		{
			ImGui::InputTextMultiline("Text String", &component.TextString);
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			ImGui::DragFloat("Kerning", &component.Kerning, 0.025f);
			ImGui::DragFloat("Line Spacing", &component.LineSpacing, 0.025f);
		});

	}

	template<typename T>
	void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string& entryName) {
		if (!m_SelectionContext.HasComponent<T>())
		{
			if (ImGui::MenuItem(entryName.c_str()))
			{
				m_SelectionContext.AddComponent<T>();
				ImGui::CloseCurrentPopup();
			}
		}
	}

}
