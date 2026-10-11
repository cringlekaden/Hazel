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
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Project/Project.h"
#include "mono/metadata/exception.h"
#include "mono/metadata/appdomain.h"

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

    static RuntimeStorage& CheckedStorage() {
        auto* session=ScriptEngine::GetRuntimeSession();
        if(!session)throw std::runtime_error("SaveData requires an active runtime session");
        return session->Storage();
    }
    static bool SaveData_IsPersistent() {
        MonoException* failure=nullptr;bool result=false;
        try {if(ScriptEngine::GetRuntimeSession())result=CheckedStorage().IsPersistent();}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);return result;
    }
    static MonoString* SaveData_Read(MonoString* slot) {
        MonoException* failure=nullptr;MonoString* result=nullptr;
        try {auto payload=CheckedStorage().Read(Utils::MonoStringToString(slot));result=mono_string_new(mono_domain_get(),payload.c_str());}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);return result;
    }
    static void SaveData_Write(MonoString* slot,MonoString* payload) {
        MonoException* failure=nullptr;
        try {CheckedStorage().Write(Utils::MonoStringToString(slot),Utils::MonoStringToString(payload));}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);
    }

	static void NativeLog(MonoString* string, int parameter)
	{
		std::string str = Utils::MonoStringToString(string);
		Log::GetClientLogger()->log(spdlog::source_loc{"Managed",0,""},spdlog::level::info,"{}, {}",str,parameter);
	}

	static void NativeLog_Vector(glm::vec3* parameter, glm::vec3* outResult)
	{
		Log::GetCoreLogger()->log(spdlog::source_loc{"Managed",0,""},spdlog::level::warn,"Value: {}",glm::to_string(*parameter));
		*outResult = glm::normalize(*parameter);
	}

	static float NativeLog_VectorDot(glm::vec3* parameter)
	{
		Log::GetCoreLogger()->log(spdlog::source_loc{"Managed",0,""},spdlog::level::warn,"Value: {}",glm::to_string(*parameter));
		return glm::dot(*parameter, *parameter);
	}

    static uint64_t Entity_GetSceneIdentity() {
        auto* scene=ScriptEngine::GetSceneContext(); return scene ? scene->GetIdentity():0;
    }
    static bool Entity_IsValid(uint64_t id,uint64_t identity) {
        auto* scene=ScriptEngine::GetSceneContext(); return scene && scene->GetIdentity()==identity && scene->IsEntityValid(id);
    }
    static void Entity_Destroy(uint64_t id,uint64_t identity) {
        auto* scene=ScriptEngine::GetSceneContext();
        if(scene && scene->GetIdentity()==identity && scene->IsEntityValid(id)) scene->DestroyEntity(scene->GetEntityByUUID(id));
    }
    static uint64_t Entity_Instantiate(MonoString* path,glm::vec3* position,glm::vec3* rotation,glm::vec3* scale,bool replaceRotationAndScale) {
        try {
            auto* scene=ScriptEngine::GetSceneContext();
            if(!scene || !Project::GetActive()) throw std::runtime_error("Instantiation needs an active main-thread runtime project");
            TransformComponent transform; transform.Translation=*position; transform.Rotation=*rotation; transform.Scale=*scale;
            auto result=Prefab::Instantiate(Project::GetAssetDirectory(),std::filesystem::u8path(Utils::MonoStringToString(path)),*scene,transform,replaceRotationAndScale);
            return result.GetUUID();
        } catch(const std::exception& error) {
            HZ_CORE_ERROR("Instantiate prefab: {}",error.what());
            mono_raise_exception(mono_get_exception_invalid_operation(error.what())); return 0;
        }
    }
	static MonoObject* GetScriptInstance(UUID entityID)
	{
		return ScriptEngine::GetManagedInstance(entityID);
	}

    template<typename T> static Entity CheckedSpriteEntity(uint64_t id) {
        auto* scene=ScriptEngine::GetSceneContext();auto entity=scene?scene->GetEntityByUUID(id):Entity{};
        if(!entity || !entity.HasComponent<T>()) {mono_raise_exception(mono_get_exception_invalid_operation("Sprite operation requires a live entity with the requested component"));return {};}
        return entity;
    }
    static void SpriteRendererComponent_SetSprite(uint64_t id,MonoString* sheet,uint64_t region) {
        auto entity=CheckedSpriteEntity<SpriteRendererComponent>(id);if(!entity)return;
        std::string error;
        try {SpriteReference reference{std::filesystem::u8path(Utils::MonoStringToString(sheet)),region};
            auto* scene=ScriptEngine::GetSceneContext();if(!scene->GetAssets())throw std::runtime_error("Project assets unavailable");
            scene->GetAssets()->Resolve(reference);entity.GetComponent<SpriteRendererComponent>().Source=reference;
        }catch(const std::exception& e){error=e.what();}
        if(!error.empty())mono_raise_exception(mono_get_exception_invalid_operation(error.c_str()));
    }
    static void SpriteAnimationComponent_Play(uint64_t id,MonoString* sheet,uint64_t clip) {
        auto entity=CheckedSpriteEntity<SpriteAnimationComponent>(id);if(!entity)return;
        AnimationReference reference{std::filesystem::u8path(Utils::MonoStringToString(sheet)),clip};
        if(!ScriptEngine::GetSceneContext()->PlayAnimation(entity,reference))mono_raise_exception(mono_get_exception_invalid_operation("Cannot play clip; see native asset diagnostic"));
    }
    static void SpriteAnimationComponent_Control(uint64_t id,int action) {
        auto entity=CheckedSpriteEntity<SpriteAnimationComponent>(id);if(!entity)return;
        auto& a=entity.GetComponent<SpriteAnimationComponent>();
        if(action==0)a.Playback.Playing=false;
        else if(action==2)a.Playback.Reset();
        else if(action==1 && a.Resolved){if(a.Playback.Finished)a.Playback.Reset();a.Playback.Playing=true;}
        else mono_raise_exception(mono_get_exception_invalid_operation("Cannot resume an unassigned or broken animation"));
    }
    static bool AudioSourceComponent_Play(uint64_t id) {
        MonoException* failure=nullptr;bool result=false;
        try {auto entity=CheckedSpriteEntity<AudioSourceComponent>(id);if(entity)result=ScriptEngine::GetSceneContext()->PlayAudio(entity);}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);return result;
    }
    static void AudioSourceComponent_Stop(uint64_t id) {
        auto entity=CheckedSpriteEntity<AudioSourceComponent>(id);if(entity)ScriptEngine::GetSceneContext()->StopAudio(entity);
    }
    static bool SpriteAnimationComponent_State(uint64_t id,bool finished) {
        auto entity=CheckedSpriteEntity<SpriteAnimationComponent>(id);if(!entity)return false;
        auto& a=entity.GetComponent<SpriteAnimationComponent>();return finished?a.Playback.Finished:a.Playback.Playing;
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

    static b2Body* CheckedBody(Entity entity) {
        auto* body=static_cast<b2Body*>(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if(!body) mono_raise_exception(mono_get_exception_invalid_operation("Rigidbody physics is unavailable; operate on a started runtime entity"));
        return body;
    }
	static void Rigidbody2DComponent_ApplyLinearImpulse(UUID entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		b2Body* body = CheckedBody(entity);
		body->ApplyLinearImpulse(b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
	}

	static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(UUID entityID, glm::vec2* impulse, bool wake)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		b2Body* body = CheckedBody(entity);
		body->ApplyLinearImpulseToCenter(b2Vec2(impulse->x, impulse->y), wake);
	}

	static void Rigidbody2DComponent_GetLinearVelocity(UUID entityID, glm::vec2* outLinearVelocity)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		b2Body* body = CheckedBody(entity);
		const b2Vec2& linearVelocity = body->GetLinearVelocity();
		*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
	}

    static void Rigidbody2DComponent_SetLinearVelocity(UUID entityID, glm::vec2* velocity) {
        if (!std::isfinite(velocity->x) || !std::isfinite(velocity->y)) {
            HZ_CORE_ERROR("Linear velocity must be finite"); return;
        }
        auto entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        HZ_CORE_ASSERT(entity);
        auto* body = CheckedBody(entity);
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

		b2Body* body = CheckedBody(entity);
		return Utils::Rigidbody2DTypeFromBox2DBody(body->GetType());
	}

	static void Rigidbody2DComponent_SetType(UUID entityID, Rigidbody2DComponent::BodyType bodyType)
	{
		Scene* scene = ScriptEngine::GetSceneContext();
		HZ_CORE_ASSERT(scene);
		Entity entity = scene->GetEntityByUUID(entityID);
		HZ_CORE_ASSERT(entity);

		auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
		b2Body* body = CheckedBody(entity);
        if(bodyType!=Rigidbody2DComponent::BodyType::Static && bodyType!=Rigidbody2DComponent::BodyType::Dynamic && bodyType!=Rigidbody2DComponent::BodyType::Kinematic)
            mono_raise_exception(mono_get_exception_argument("bodyType","Select a valid Rigidbody2D BodyType"));
        rb2d.Type=bodyType;
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
        RegisterComponent<SpriteRendererComponent>("Hazel.SpriteRendererComponent");
        RegisterComponent<SpriteAnimationComponent>("Hazel.SpriteAnimationComponent");
        RegisterComponent<AudioSourceComponent>("Hazel.AudioSourceComponent");
	}

	void ScriptGlue::ValidateComponents(MonoImage* image)
	{
		for (const char* name : { "Hazel.TransformComponent", "Hazel.Rigidbody2DComponent", "Hazel.TextComponent", "Hazel.CameraComponent", "Hazel.SpriteRendererComponent", "Hazel.SpriteAnimationComponent", "Hazel.AudioSourceComponent" })
			if (!mono_reflection_type_from_name(const_cast<char*>(name), image))
				throw std::runtime_error(std::string("Missing managed component: ") + name);
	}

	void ScriptGlue::RegisterFunctions()
	{
        RegisterHierarchyFunctions();
        HZ_ADD_INTERNAL_CALL(SpriteRendererComponent_SetSprite);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_Play);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_Stop);
        HZ_ADD_INTERNAL_CALL(SpriteAnimationComponent_Play);
        HZ_ADD_INTERNAL_CALL(SpriteAnimationComponent_Control);
        HZ_ADD_INTERNAL_CALL(SpriteAnimationComponent_State);
		HZ_ADD_INTERNAL_CALL(SaveData_IsPersistent);
        HZ_ADD_INTERNAL_CALL(SaveData_Read);
        HZ_ADD_INTERNAL_CALL(SaveData_Write);
        HZ_ADD_INTERNAL_CALL(NativeLog);
		HZ_ADD_INTERNAL_CALL(NativeLog_Vector);
		HZ_ADD_INTERNAL_CALL(NativeLog_VectorDot);

		HZ_ADD_INTERNAL_CALL(GetScriptInstance);

        HZ_ADD_INTERNAL_CALL(Entity_HasComponent);
        HZ_ADD_INTERNAL_CALL(Entity_GetSceneIdentity);
        HZ_ADD_INTERNAL_CALL(Entity_IsValid);
        HZ_ADD_INTERNAL_CALL(Entity_Destroy);
        HZ_ADD_INTERNAL_CALL(Entity_Instantiate);
        HZ_ADD_INTERNAL_CALL(Entity_FindEntityByName);

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
