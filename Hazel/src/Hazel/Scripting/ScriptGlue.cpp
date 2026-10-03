#include "hzpch.h"
#include "ScriptGlue.h"
#include <cmath>
#include "ScriptEngine.h"
#include "Hazel/Scene/RuntimeSession.h"

#include "Hazel/Core/UUID.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Core/Input.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"

#include "Hazel/Physics/Physics2D.h"

#include "mono/metadata/object.h"
#include "mono/metadata/reflection.h"

#include "box2d/b2_body.h"
#include <glm/gtx/string_cast.hpp>

namespace Hazel {

	namespace Utils {

		std::string MonoStringToString(MonoString* string)
		{
			if (!string) return {};
			char* cStr = mono_string_to_utf8(string);
			std::string str(cStr);
			mono_free(cStr);
			return str;
		}

	}

	static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;

#define HZ_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Hazel.InternalCalls::" #Name, reinterpret_cast<const void*>(Name))

	static void NativeLog(MonoString* string, int parameter)
	{
		std::string str = Utils::MonoStringToString(string);
		std::cout << str << ", " << parameter << std::endl;
	}

	static void NativeLog_Vector(glm::vec3* parameter, glm::vec3* outResult)
	{
		HZ_CORE_WARN("Value: {0}", glm::to_string(*parameter));
		*outResult = glm::normalize(*parameter);
	}

	static float NativeLog_VectorDot(glm::vec3* parameter)
	{
		HZ_CORE_WARN("Value: {0}", glm::to_string(*parameter));
		return glm::dot(*parameter, *parameter);
	}

	static MonoObject* GetScriptInstance(UUID entityID)
	{
		return ScriptEngine::GetManagedInstance(entityID);
	}

	static bool Entity_HasComponent(UUID entityID, MonoReflectionType* componentType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		MonoType* managedType = mono_reflection_type_get_type(componentType);
		HZ_CORE_ASSERT(s_EntityHasComponentFuncs.find(managedType) != s_EntityHasComponentFuncs.end());
		return s_EntityHasComponentFuncs.at(managedType)(entity);
	}

	static uint64_t Entity_FindEntityByName(MonoString* name)
	{
		char* nameCStr = mono_string_to_utf8(name);

		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->FindEntityByName(nameCStr);
		mono_free(nameCStr);

		if (!entity)
			return 0;

		return entity.GetUUID();
	}

	static void TransformComponent_GetTranslation(UUID entityID, glm::vec3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		*outTranslation = entity.GetComponent<TransformComponent>().Translation;
	}

	static void TransformComponent_SetTranslation(UUID entityID, glm::vec3* translation)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		if (!std::isfinite(translation->x) || !std::isfinite(translation->y) || !std::isfinite(translation->z)) {
            HZ_CORE_ERROR("Translation must be finite"); return;
        }
        auto& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = *translation;
        if (entity.HasComponent<Rigidbody2DComponent>())
            if (auto* body = static_cast<b2Body*>(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody))
                body->SetTransform(b2Vec2(translation->x, translation->y), transform.Rotation.z);
	}

	static void Rigidbody2DComponent_ApplyLinearImpulse(UUID entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		body->ApplyLinearImpulse(b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
	}

	static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(UUID entityID, glm::vec2* impulse, bool wake)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		body->ApplyLinearImpulseToCenter(b2Vec2(impulse->x, impulse->y), wake);
	}

	static void Rigidbody2DComponent_GetLinearVelocity(UUID entityID, glm::vec2* outLinearVelocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		const b2Vec2& linearVelocity = body->GetLinearVelocity();
		*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
	}

    static void Rigidbody2DComponent_SetLinearVelocity(UUID entityID, glm::vec2* velocity) {
        if (!std::isfinite(velocity->x) || !std::isfinite(velocity->y)) {
            HZ_CORE_ERROR("Linear velocity must be finite"); return;
        }
        auto entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        HZ_CORE_ASSERT(entity);
        auto* body = static_cast<b2Body*>(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        HZ_CORE_ASSERT(body);
        body->SetLinearVelocity(b2Vec2(velocity->x, velocity->y));
    }
    static float CameraComponent_GetOrthographicSize(UUID entityID) {
        auto entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        HZ_CORE_ASSERT(entity);
        return entity.GetComponent<CameraComponent>().Camera.GetOrthographicSize();
    }
    static void CameraComponent_SetOrthographicSize(UUID entityID, float size) {
        if (!std::isfinite(size) || size <= 0) { HZ_CORE_ERROR("Orthographic size must be positive and finite"); return; }
        auto entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        HZ_CORE_ASSERT(entity);
        entity.GetComponent<CameraComponent>().Camera.SetOrthographicSize(size);
    }
    static float CameraComponent_GetAspectRatio(UUID entityID) {
        auto entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        HZ_CORE_ASSERT(entity);
        return entity.GetComponent<CameraComponent>().Camera.GetAspectRatio();
    }

	static Rigidbody2DComponent::BodyType Rigidbody2DComponent_GetType(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		return Utils::Rigidbody2DTypeFromBox2DBody(body->GetType());
	}

	static void Rigidbody2DComponent_SetType(UUID entityID, Rigidbody2DComponent::BodyType bodyType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = (b2Body*)rb2d.RuntimeBody;
		body->SetType(Utils::Rigidbody2DTypeToBox2DBody(bodyType));
	}

	static MonoString* TextComponent_GetText(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		return ScriptEngine::CreateString(tc.TextString.c_str());
	}

	static void TextComponent_SetText(UUID entityID, MonoString* textString)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		tc.TextString = Utils::MonoStringToString(textString);
	}

	static void TextComponent_GetColor(UUID entityID, glm::vec4* color)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		*color = tc.Color;
	}

	static void TextComponent_SetColor(UUID entityID, glm::vec4* color)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		tc.Color = *color;
	}

	static float TextComponent_GetKerning(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		return tc.Kerning;
	}

	static void TextComponent_SetKerning(UUID entityID, float kerning)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		tc.Kerning = kerning;
	}

	static float TextComponent_GetLineSpacing(UUID entityID)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		return tc.LineSpacing;
	}

	static void TextComponent_SetLineSpacing(UUID entityID, float lineSpacing)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);
		HZ_CORE_ASSERT(entity.HasComponent<TextComponent>());

		auto& tc = entity.GetComponent<TextComponent>();
		tc.LineSpacing = lineSpacing;
	}

    static void Scene_LoadScene(MonoString* path) {
        auto* session = ScriptEngine::GetRuntimeSession();
        try {
            if (!session || !session->RequestSceneLoad(std::filesystem::u8path(Utils::MonoStringToString(path))))
                HZ_CORE_ERROR("Scene.LoadScene request rejected (no running project session or conflicting/invalid request)");
        } catch (const std::exception& error) { HZ_CORE_ERROR("Scene.LoadScene request: {}", error.what()); }
    }
    static bool Input_IsKeyDown(KeyCode keycode) {
        auto* session = ScriptEngine::GetRuntimeSession();
        return (!session || session->IsInputEnabled()) && Input::IsKeyPressed(keycode);
    }
    static bool Input_IsMouseButtonDown(MouseCode button) {
        auto* session = ScriptEngine::GetRuntimeSession();
        return session && session->IsInputEnabled() && Input::IsMouseButtonPressed(button);
    }
    static bool Input_GetMouseWorldPosition(glm::vec2* position) {
        auto* session = ScriptEngine::GetRuntimeSession();
        *position = {};
        return session && session->GetMouseWorldPosition(*position);
    }

	template<typename Component>
	static void RegisterComponent(const char* managedTypename)
	{
		MonoType* type = mono_reflection_type_from_name(const_cast<char*>(managedTypename), ScriptEngine::GetCoreAssemblyImage());
		if (!type) throw std::runtime_error(std::string("Missing managed component: ") + managedTypename);
		s_EntityHasComponentFuncs[type] = [](Entity entity) { return entity.HasComponent<Component>(); };
	}

	void ScriptGlue::RegisterComponents()
	{
		s_EntityHasComponentFuncs.clear();
		// Explicit public component names.
		// Preserve the API without depending on compiler RTTI spelling.
		RegisterComponent<TransformComponent>("Hazel.TransformComponent");
		RegisterComponent<Rigidbody2DComponent>("Hazel.Rigidbody2DComponent");
		RegisterComponent<TextComponent>("Hazel.TextComponent");
        RegisterComponent<CameraComponent>("Hazel.CameraComponent");
	}

	void ScriptGlue::ValidateComponents(MonoImage* image)
	{
		for (const char* name : { "Hazel.TransformComponent", "Hazel.Rigidbody2DComponent", "Hazel.TextComponent", "Hazel.CameraComponent" })
			if (!mono_reflection_type_from_name(const_cast<char*>(name), image))
				throw std::runtime_error(std::string("Missing managed component: ") + name);
	}

	void ScriptGlue::RegisterFunctions()
	{
		HZ_ADD_INTERNAL_CALL(NativeLog);
		HZ_ADD_INTERNAL_CALL(NativeLog_Vector);
		HZ_ADD_INTERNAL_CALL(NativeLog_VectorDot);

		HZ_ADD_INTERNAL_CALL(GetScriptInstance);

		HZ_ADD_INTERNAL_CALL(Entity_HasComponent);
		HZ_ADD_INTERNAL_CALL(Entity_FindEntityByName);

		HZ_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		HZ_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);

		HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse);
		HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulseToCenter);
		HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetLinearVelocity);
        HZ_ADD_INTERNAL_CALL(CameraComponent_GetOrthographicSize);
        HZ_ADD_INTERNAL_CALL(CameraComponent_SetOrthographicSize);
        HZ_ADD_INTERNAL_CALL(CameraComponent_GetAspectRatio);
		HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetType);
		HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetType);

		HZ_ADD_INTERNAL_CALL(TextComponent_GetText);
		HZ_ADD_INTERNAL_CALL(TextComponent_SetText);
		HZ_ADD_INTERNAL_CALL(TextComponent_GetColor);
		HZ_ADD_INTERNAL_CALL(TextComponent_SetColor);
		HZ_ADD_INTERNAL_CALL(TextComponent_GetKerning);
		HZ_ADD_INTERNAL_CALL(TextComponent_SetKerning);
		HZ_ADD_INTERNAL_CALL(TextComponent_GetLineSpacing);
		HZ_ADD_INTERNAL_CALL(TextComponent_SetLineSpacing);

		HZ_ADD_INTERNAL_CALL(Input_IsKeyDown);
        HZ_ADD_INTERNAL_CALL(Input_IsMouseButtonDown);
        HZ_ADD_INTERNAL_CALL(Input_GetMouseWorldPosition);
        HZ_ADD_INTERNAL_CALL(Scene_LoadScene);
	}

}
