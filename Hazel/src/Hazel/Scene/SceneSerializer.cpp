#include "hzpch.h"
#include <cmath>
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileDocument.h"
#include "Prefab.h"

#include "Hazel/Project/Project.h"

#include <fstream>
#include <limits>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace YAML {

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<Hazel::UUID>
	{
		static Node encode(const Hazel::UUID& uuid)
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, Hazel::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
			return true;
		}
	};

}

namespace Hazel {

#define WRITE_SCRIPT_FIELD(FieldType, Type)           \
			case ScriptFieldType::FieldType:          \
				out << scriptField.GetValue<Type>();  \
				break

#define READ_SCRIPT_FIELD(FieldType, Type)             \
	case ScriptFieldType::FieldType:                   \
	{                                                  \
		Type data = scriptField["Data"].as<Type>();    \
		fieldInstance.SetValue(data);                  \
		break;                                         \
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	static std::string RigidBody2DBodyTypeToString(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
			case Rigidbody2DComponent::BodyType::Static:    return "Static";
			case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
			case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
		}

		throw std::invalid_argument("Unknown rigid body type");
	}

	static Rigidbody2DComponent::BodyType RigidBody2DBodyTypeFromString(const std::string& bodyTypeString)
	{
		if (bodyTypeString == "Static")    return Rigidbody2DComponent::BodyType::Static;
		if (bodyTypeString == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
		if (bodyTypeString == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;

		throw std::invalid_argument("Unknown rigid body type: " + bodyTypeString);
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: SceneSerializer(scene, Project::GetActive() ? Project::GetAssetDirectory() : std::filesystem::path{})
	{
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene, const std::filesystem::path& assetRoot, bool repair, const Ref<ProjectAssets>& assets)
		: m_Scene(scene), m_AssetRoot(assetRoot), m_Repair(repair) {
        if(assets) scene->SetAssets(assets);
        else if(!scene->GetAssets()) {
            // Standalone legacy scenes used cwd-relative/absolute TexturePath values
            // before project assets existed. Keep that loading convention intact.
            const auto root=assetRoot.empty()?std::filesystem::current_path():assetRoot;
            auto active=Project::GetActive();
            scene->SetAssets(active && std::filesystem::weakly_canonical(active->GetAssetRoot())==std::filesystem::weakly_canonical(root)?active->GetAssets():CreateRef<ProjectAssets>(root));
        }
    }

	static void SerializeEntity(YAML::Emitter& out, Entity entity, const std::filesystem::path& assetRoot, bool portablePaths)
	{
		HZ_CORE_ASSERT(entity.HasComponent<IDComponent>());

		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			auto& scriptComponent = entity.GetComponent<ScriptComponent>();

			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent
			out << YAML::Key << "ClassName" << YAML::Value << scriptComponent.ClassName;

			// Fields
			// Stored metadata also survives a temporarily unavailable/renamed class.
			// Keep its published field names/types/data rather than silently dropping it.
			const auto& fields = ScriptEngine::GetScriptFieldMap(entity);
			if (fields.size() > 0)
			{
				out << YAML::Key << "ScriptFields" << YAML::Value;
				auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);
				out << YAML::BeginSeq;
				for (const auto& [name, stored] : fields)
				{
					const auto& field = stored.Field;
					if (field.Type == ScriptFieldType::None) continue;
					if (entityFields.find(name) == entityFields.end())
						continue;

					out << YAML::BeginMap; // ScriptField
					out << YAML::Key << "Name" << YAML::Value << name;
					out << YAML::Key << "Type" << YAML::Value << Utils::ScriptFieldTypeToString(field.Type);

					out << YAML::Key << "Data" << YAML::Value;
					ScriptFieldInstance& scriptField = entityFields.at(name);

					switch (field.Type)
					{
						WRITE_SCRIPT_FIELD(Float,   float     );
						WRITE_SCRIPT_FIELD(Double,  double    );
						WRITE_SCRIPT_FIELD(Bool,    bool      );
						WRITE_SCRIPT_FIELD(Char,    uint16_t  );
						case ScriptFieldType::Byte: out << static_cast<int>(scriptField.GetValue<int8_t>()); break;
						WRITE_SCRIPT_FIELD(Short,   int16_t   );
						WRITE_SCRIPT_FIELD(Int,     int32_t   );
						WRITE_SCRIPT_FIELD(Long,    int64_t   );
						case ScriptFieldType::UByte: out << static_cast<unsigned>(scriptField.GetValue<uint8_t>()); break;
						WRITE_SCRIPT_FIELD(UShort,  uint16_t  );
						WRITE_SCRIPT_FIELD(UInt,    uint32_t  );
						WRITE_SCRIPT_FIELD(ULong,   uint64_t  );
						WRITE_SCRIPT_FIELD(Vector2, glm::vec2 );
						WRITE_SCRIPT_FIELD(Vector3, glm::vec3 );
						WRITE_SCRIPT_FIELD(Vector4, glm::vec4 );
						WRITE_SCRIPT_FIELD(Entity,  UUID      );
                        case ScriptFieldType::Prefab: out << scriptField.AssetReference; break;
                        case ScriptFieldType::Sprite: if(scriptField.AssetID)WriteSpriteReference(out,{std::filesystem::u8path(scriptField.AssetReference),scriptField.AssetID});else out<<YAML::Null;break;
                        case ScriptFieldType::SpriteAnimation: if(scriptField.AssetID)WriteAnimationReference(out,{std::filesystem::u8path(scriptField.AssetReference),scriptField.AssetID});else out<<YAML::Null;break;
						case ScriptFieldType::None: break;
					}
					out << YAML::EndMap; // ScriptFields
				}
				out << YAML::EndSeq;
			}

			out << YAML::EndMap; // ScriptComponent
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent

			auto& spriteRendererComponent = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;
			auto source=spriteRendererComponent.Source;
            if(auto t=std::get_if<TextureSpriteSource>(&source);portablePaths && t && !t->Texture.empty() && spriteRendererComponent.Resolved.Error.empty())
                t->Texture=Project::MakeAssetReference(assetRoot,Project::ResolveAssetPath(assetRoot,t->Texture));
            WriteSpriteSource(out,source);

			out << YAML::EndMap; // SpriteRendererComponent
		}

        if(entity.HasComponent<SpriteAnimationComponent>()) {
            const auto& a=entity.GetComponent<SpriteAnimationComponent>();
            out<<YAML::Key<<"SpriteAnimationComponent"<<YAML::Value;
            WriteSpriteAnimationSettings(out,a);
        }
		if (entity.HasComponent<CircleRendererComponent>())
		{
			out << YAML::Key << "CircleRendererComponent";
			out << YAML::BeginMap; // CircleRendererComponent

			auto& circleRendererComponent = entity.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << circleRendererComponent.Color;
			out << YAML::Key << "Thickness" << YAML::Value << circleRendererComponent.Thickness;
			out << YAML::Key << "Fade" << YAML::Value << circleRendererComponent.Fade;

			out << YAML::EndMap; // CircleRendererComponent
		}

		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap; // Rigidbody2DComponent

			auto& rb2dComponent = entity.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << RigidBody2DBodyTypeToString(rb2dComponent.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << rb2dComponent.FixedRotation;
			out << YAML::Key << "GravityScale" << YAML::Value << rb2dComponent.GravityScale;

			out << YAML::EndMap; // Rigidbody2DComponent
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap; // BoxCollider2DComponent

			auto& bc2dComponent = entity.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << bc2dComponent.Offset;
			out << YAML::Key << "Size" << YAML::Value << bc2dComponent.Size;
			out << YAML::Key << "Density" << YAML::Value << bc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << bc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << bc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << bc2dComponent.RestitutionThreshold;

			out << YAML::EndMap; // BoxCollider2DComponent
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap; // CircleCollider2DComponent

			auto& cc2dComponent = entity.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << cc2dComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << cc2dComponent.Radius;
			out << YAML::Key << "Density" << YAML::Value << cc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << cc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << cc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << cc2dComponent.RestitutionThreshold;

			out << YAML::EndMap; // CircleCollider2DComponent
		}

		if (entity.HasComponent<TextComponent>())
		{
			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap; // TextComponent

			auto& textComponent = entity.GetComponent<TextComponent>();
			out << YAML::Key << "TextString" << YAML::Value << textComponent.TextString;
			// TODO: textComponent.FontAsset
			out << YAML::Key << "Color" << YAML::Value << textComponent.Color;
			out << YAML::Key << "Kerning" << YAML::Value << textComponent.Kerning;
			out << YAML::Key << "LineSpacing" << YAML::Value << textComponent.LineSpacing;

			out << YAML::EndMap; // TextComponent
		}

		out << YAML::EndMap; // Entity
	}

    std::string SceneSerializer::SerializeText(Entity only) { return SerializeTextImpl(only, true); }
    std::string SceneSerializer::SerializeAuthoredSnapshot(Entity only) { return SerializeTextImpl(only, false); }
	std::string SceneSerializer::SerializeTextImpl(Entity only, bool portablePaths)
	{
		YAML::Emitter out;
		out.SetFloatPrecision(std::numeric_limits<float>::max_digits10);
		out.SetDoublePrecision(std::numeric_limits<double>::max_digits10);
		out << YAML::BeginMap;
        out << YAML::Key << "SceneVersion" << YAML::Value << 1;
		out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		m_Scene->m_Registry.each([&](auto entityID)
		{
			Entity entity = { entityID, m_Scene.get() };
			if (!entity)
				return;

			if (!only || only == entity) SerializeEntity(out, entity, m_AssetRoot, portablePaths);
		});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		if (!out.good()) throw std::runtime_error(out.GetLastError());
		return out.c_str();
    }

    void SceneSerializer::Serialize(const std::string& filepath) {
        auto text=SerializeText();
        FileSystem::WriteFileAtomically(std::filesystem::u8path(filepath), [&](std::ostream& stream) { stream << text; });
	}

	bool SceneSerializer::Deserialize(const std::string& filepath) {
        m_Report = {};
        try{return DeserializeText(FileDocument::Read(std::filesystem::u8path(filepath)),std::filesystem::u8path(filepath).extension()==".hprefab");}
        catch(const std::exception& error){m_Report.Error=error.what();return false;}
    }

    bool SceneSerializer::DeserializeText(const std::string& text, bool prefabDocument)
	{
        m_Report = {};
		if (m_Scene->m_IsRunning || m_Scene->m_PhysicsWorld) {
			HZ_CORE_ERROR("Stop scene runtime/simulation before deserializing"); return false;
		}
		YAML::Node data;
		try
		{
            data = YAML::Load(text);
            m_Report.Migration = DocumentSchema::Scene(data,prefabDocument);
            if (!data["SceneVersion"]) m_Report.Problems.push_back({0,"Encoding","Save adds SceneVersion: 1; original bytes are preserved first",{},true});
            if (!data["Entities"]) m_Report.Problems.push_back({0,"Entities","Known legacy missing entity list becomes an empty list on Save",{},true});
            for (auto node : data["Entities"]) {
                const auto id=node["Entity"].as<uint64_t>();
                if (!node["TagComponent"]) m_Report.Problems.push_back({id,"Name","Known legacy default: Entity; Save writes TagComponent explicitly",{},true});
                if (!node["TransformComponent"]) m_Report.Problems.push_back({id,"Transform","Known legacy default: identity transform; Save writes it explicitly",{},true});
                if (node["Rigidbody2DComponent"] && !node["Rigidbody2DComponent"]["GravityScale"])
                    m_Report.Problems.push_back({id,"Gravity scale","Known legacy default: 1; Save writes it explicitly",{},true});
                if(auto animation=node["SpriteAnimationComponent"]) {
                    if(!animation["Autoplay"])m_Report.Problems.push_back({id,"Autoplay","Known default: true; Save writes it explicitly",{},true});
                    if(!animation["Speed"])m_Report.Problems.push_back({id,"Animation speed","Known default: 1; Save writes it explicitly",{},true});
                    if(!animation["DefaultClip"])m_Report.Problems.push_back({id,"Default clip","Known default: unassigned; Save writes it explicitly",{},true});
                }
                if(auto sprite=node["SpriteRendererComponent"]) {
                    if(sprite["Source"] && sprite["Source"]["Type"].as<std::string>()=="Texture" && !sprite["Source"]["TilingFactor"])
                        m_Report.Problems.push_back({id,"Tiling factor","Known default: 1; Save writes it explicitly",{},true});
                }
                if (node["SpriteRendererComponent"] && !node["SpriteRendererComponent"]["Source"])
                    m_Report.Problems.push_back({id,"Sprite encoding","Legacy TexturePath becomes an explicit Source on Save; its reference is retained",{},true});
            }
			auto staged = CreateRef<Scene>();
            staged->SetAssets(m_Scene->GetAssets());
			staged->m_ViewportWidth = m_Scene->m_ViewportWidth;
			staged->m_ViewportHeight = m_Scene->m_ViewportHeight;
			std::unordered_map<UUID, ScriptFieldMap> stagedScriptFields;

		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		HZ_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();

				HZ_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);

				Entity deserializedEntity = staged->CreateEntityWithUUID(uuid, name);
                if(tagComponent) { deserializedEntity.GetComponent<TagComponent>().Tag=name; }

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent)
				{
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<glm::vec3>();
					tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
					tc.Scale = transformComponent["Scale"].as<glm::vec3>();
				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					auto cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());

					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());

					cc.Primary = cameraComponent["Primary"].as<bool>();
					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					sc.ClassName = scriptComponent["ClassName"].as<std::string>();

					auto scriptFields = scriptComponent["ScriptFields"];
					if (scriptFields)
					{
						// Stored fields are independent of the currently loaded project/domain.
						{
							auto& entityFields = stagedScriptFields[deserializedEntity.GetUUID()];

							for (auto scriptField : scriptFields)
							{
								std::string name = scriptField["Name"].as<std::string>();
								std::string typeString = scriptField["Type"].as<std::string>();
								ScriptFieldType type = Utils::ScriptFieldTypeFromString(typeString);

								ScriptFieldInstance& fieldInstance = entityFields[name];

								fieldInstance.Field = { type, name, nullptr };


								switch (type)
								{
									READ_SCRIPT_FIELD(Float, float);
									READ_SCRIPT_FIELD(Double, double);
									READ_SCRIPT_FIELD(Bool, bool);
									case ScriptFieldType::Char: {
										// Write a UTF-16 code unit. Accept original ASCII char scalars too.
										const auto value = scriptField["Data"].Scalar();
										uint16_t code;
										if (value.size() == 1 && (value[0] < '0' || value[0] > '9')) code = static_cast<unsigned char>(value[0]);
										else code = scriptField["Data"].as<uint16_t>();
										fieldInstance.SetValue<uint16_t>(code); break;
									}
									READ_SCRIPT_FIELD(Byte, int8_t);
									READ_SCRIPT_FIELD(Short, int16_t);
									READ_SCRIPT_FIELD(Int, int32_t);
									READ_SCRIPT_FIELD(Long, int64_t);
									READ_SCRIPT_FIELD(UByte, uint8_t);
									READ_SCRIPT_FIELD(UShort, uint16_t);
									READ_SCRIPT_FIELD(UInt, uint32_t);
									READ_SCRIPT_FIELD(ULong, uint64_t);
									READ_SCRIPT_FIELD(Vector2, glm::vec2);
									READ_SCRIPT_FIELD(Vector3, glm::vec3);
									READ_SCRIPT_FIELD(Vector4, glm::vec4);
									READ_SCRIPT_FIELD(Entity, UUID);
                                    case ScriptFieldType::Prefab: fieldInstance.AssetReference=scriptField["Data"].as<std::string>(); break;
                                    case ScriptFieldType::Sprite: {auto r=ReadSpriteReference(scriptField["Data"]);fieldInstance.AssetReference=r.Sheet.generic_u8string();fieldInstance.AssetID=r.Region;break;}
                                    case ScriptFieldType::SpriteAnimation: {auto r=ReadAnimationReference(scriptField["Data"]);fieldInstance.AssetReference=r.Sheet.generic_u8string();fieldInstance.AssetID=r.Clip;break;}
									case ScriptFieldType::None: throw std::runtime_error("Unsupported stored script field type");
								}
							}
						}
					}

				}

				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<glm::vec4>();
					src.Source=ReadSpriteSource(spriteRendererComponent);
				}

                if(auto node=entity["SpriteAnimationComponent"]) {
                    auto& a=deserializedEntity.AddComponent<SpriteAnimationComponent>();
                    static_cast<SpriteAnimationSettings&>(a)=ReadSpriteAnimationSettings(node);
                    if(!deserializedEntity.HasComponent<SpriteRendererComponent>()) throw std::runtime_error("Sprite animation requires SpriteRendererComponent");
                }

				auto circleRendererComponent = entity["CircleRendererComponent"];
				if (circleRendererComponent)
				{
					auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
					crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
					crc.Thickness = circleRendererComponent["Thickness"].as<float>();
					crc.Fade = circleRendererComponent["Fade"].as<float>();
				}

				auto rigidbody2DComponent = entity["Rigidbody2DComponent"];
				if (rigidbody2DComponent)
				{
					auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
					rb2d.Type = RigidBody2DBodyTypeFromString(rigidbody2DComponent["BodyType"].as<std::string>());
					rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();
					if (auto gravity = rigidbody2DComponent["GravityScale"]) rb2d.GravityScale = gravity.as<float>();
				}

				auto boxCollider2DComponent = entity["BoxCollider2DComponent"];
				if (boxCollider2DComponent)
				{
					auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
					bc2d.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
					bc2d.Size = boxCollider2DComponent["Size"].as<glm::vec2>();
					bc2d.Density = boxCollider2DComponent["Density"].as<float>();
					bc2d.Friction = boxCollider2DComponent["Friction"].as<float>();
					bc2d.Restitution = boxCollider2DComponent["Restitution"].as<float>();
					bc2d.RestitutionThreshold = boxCollider2DComponent["RestitutionThreshold"].as<float>();
				}

				auto circleCollider2DComponent = entity["CircleCollider2DComponent"];
				if (circleCollider2DComponent)
				{
					auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
					cc2d.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>();
					cc2d.Radius = circleCollider2DComponent["Radius"].as<float>();
					cc2d.Density = circleCollider2DComponent["Density"].as<float>();
					cc2d.Friction = circleCollider2DComponent["Friction"].as<float>();
					cc2d.Restitution = circleCollider2DComponent["Restitution"].as<float>();
					cc2d.RestitutionThreshold = circleCollider2DComponent["RestitutionThreshold"].as<float>();
				}

				auto textComponent = entity["TextComponent"];
				if (textComponent)
				{
					auto& tc = deserializedEntity.AddComponent<TextComponent>();
					tc.TextString = textComponent["TextString"].as<std::string>();
					// tc.FontAsset // TODO
					tc.Color = textComponent["Color"].as<glm::vec4>();
					tc.Kerning = textComponent["Kerning"].as<float>();
					tc.LineSpacing = textComponent["LineSpacing"].as<float>();
				}
			}
		}

        staged->PrepareSprites(!m_Repair);
        for(const auto& [id,fields]:stagedScriptFields)
            for(const auto& [name,field]:fields) {
                std::filesystem::path resource;
                if(field.Field.Type==ScriptFieldType::Prefab && !field.AssetReference.empty())
                    resource=Prefab::Resolve(m_AssetRoot,std::filesystem::u8path(field.AssetReference));
                else if((field.Field.Type==ScriptFieldType::Sprite || field.Field.Type==ScriptFieldType::SpriteAnimation) && field.AssetID)
                    resource=Project::ResolveOwnedAsset(m_AssetRoot,std::filesystem::u8path(field.AssetReference));
                if(resource.empty())continue;
                try {
                    if(!std::filesystem::is_regular_file(resource))throw std::runtime_error("Missing referenced asset: "+resource.generic_u8string());
                    if(field.Field.Type==ScriptFieldType::Sprite)staged->GetAssets()->Resolve(SpriteReference{std::filesystem::u8path(field.AssetReference),field.AssetID});
                    if(field.Field.Type==ScriptFieldType::SpriteAnimation)staged->GetAssets()->Clip({std::filesystem::u8path(field.AssetReference),field.AssetID});
                }catch(const std::exception& error){
                    m_Report.Problems.push_back({id,"Script field "+name,error.what(),resource});
                }
            }
        for (auto handle : staged->GetAllEntitiesWith<SpriteRendererComponent>())
        {
            Entity entity(handle, staged.get());
            const auto& error = entity.GetComponent<SpriteRendererComponent>().Resolved.Error;
            if (!error.empty()) {
                std::filesystem::path resource;
                const auto& source=entity.GetComponent<SpriteRendererComponent>().Source;
                if(auto texture=std::get_if<TextureSpriteSource>(&source))resource=Project::ResolveAssetPath(m_AssetRoot,texture->Texture);
                if(auto region=std::get_if<SpriteReference>(&source))resource=m_AssetRoot/region->Sheet;
                m_Report.Problems.push_back({entity.GetUUID(), "Sprite source", error, resource});
            }
        }
        for (auto handle : staged->GetAllEntitiesWith<SpriteAnimationComponent>())
        {
            Entity entity(handle, staged.get());
            const auto& error = entity.GetComponent<SpriteAnimationComponent>().Error;
            if (!error.empty()) m_Report.Problems.push_back({entity.GetUUID(), "Animation", error, m_AssetRoot/entity.GetComponent<SpriteAnimationComponent>().DefaultClip.Sheet});
        }
		// Commit only after parsing every component and loading every referenced asset.
		// Existing scene/field data remains untouched on malformed input or asset failure.
		std::vector<Entity> oldEntities;
		m_Scene->m_Registry.each([&](auto id) { oldEntities.emplace_back(id, m_Scene.get()); });
		for (auto entity : oldEntities) m_Scene->DestroyEntity(entity);
		m_Scene->m_Registry = std::move(staged->m_Registry);
		m_Scene->m_EntityMap = std::move(staged->m_EntityMap);
		m_Scene->m_ScriptFields = std::move(stagedScriptFields);
        m_Scene->SetName(sceneName);
        m_Report.State = m_Report.Problems.empty() ? DocumentLoadState::Ready : DocumentLoadState::EditableWithProblems;
		return true;
		}
		catch (const std::runtime_error& error) {
            m_Report.Error=error.what();m_Report.State=DocumentLoadState::Rejected;
			HZ_CORE_ERROR("Failed to load .hazel file '{}': {}", "scene document", error.what());
			return false;
		}
		catch (const std::invalid_argument& error) {
            m_Report.Error=error.what();m_Report.State=DocumentLoadState::Rejected;
			HZ_CORE_ERROR("Failed to load .hazel file '{}': {}", "scene document", error.what());
			return false;
		}
	}

}
