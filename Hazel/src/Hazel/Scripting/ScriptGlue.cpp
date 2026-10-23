#include "hzpch.h"
#include "ScriptGlue.h"
#include <cmath>
#include "Hazel/Utils/UTF8.h"
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

    static bool Entity_HasComponent(UUID entityID,MonoReflectionType* componentType) {
        MonoException* failure=nullptr;bool result=false;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::runtime_error("Component lookup needs a runtime scene");scene->CheckThread();auto entity=scene->GetEntityByUUID(entityID);if(!entity||!scene->IsEntityValid(entityID)||!componentType)throw std::runtime_error("Component lookup needs a live entity and type");auto* type=mono_reflection_type_get_type(componentType);auto found=s_EntityHasComponentFuncs.find(type);if(found==s_EntityHasComponentFuncs.end())throw std::runtime_error("Component type has no native binding");result=found->second(entity);}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);return result;
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
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
		return ScriptEngine::CreateString(tc.TextString.c_str());
	}

    static void TextComponent_SetText(UUID entityID, MonoString* textString)
    {
        MonoException* failure=nullptr;
        try {
            auto* scene=ScriptEngine::GetSceneContext();
            if(!scene)throw std::invalid_argument("Text needs a runtime scene");scene->CheckThread();
            auto entity=scene->GetEntityByUUID(entityID);
            if(!entity||!scene->IsEntityValid(entityID)||!entity.HasComponent<TextComponent>())throw std::invalid_argument("Text needs a live Text component");
            if(textString) {
                auto chars=mono_string_chars(textString);auto length=mono_string_length(textString);
                for(int i=0;i<length;++i) {
                    auto c=chars[i];
                    if(!c)throw std::invalid_argument("Text cannot contain embedded NUL");
                    if(c>=0xd800&&c<=0xdbff) {
                        if(++i>=length||chars[i]<0xdc00||chars[i]>0xdfff)throw std::invalid_argument("Text contains an unpaired UTF-16 surrogate");
                    } else if(c>=0xdc00&&c<=0xdfff)throw std::invalid_argument("Text contains an unpaired UTF-16 surrogate");
                }
            }
            auto text=Utils::MonoStringToString(textString);UTF8::Validate(text);
            if(entity.HasComponent<UIRectComponent>()&&text.size()>65536)throw std::invalid_argument("UI text exceeds 64 KiB");
            entity.GetComponent<TextComponent>().TextString=std::move(text);
        }catch(const std::exception& e){failure=mono_get_exception_argument("text",e.what());}
        if(failure)mono_raise_exception(failure);
    }

    static MonoString* TextComponent_GetFontFamily(UUID entityID) {
        return ScriptEngine::CreateString(CheckedSpriteEntity<TextComponent>(entityID).GetComponent<TextComponent>().Font.Family.generic_u8string().c_str());
    }
    static int TextComponent_GetFontStyle(UUID entityID) {
        return int(CheckedSpriteEntity<TextComponent>(entityID).GetComponent<TextComponent>().Font.Style);
    }
    static void TextComponent_SetFont(UUID entityID,MonoString* family,int style) {
        auto entity=CheckedSpriteEntity<TextComponent>(entityID);MonoException* failure=nullptr;
        try {
            if(style<0 || style>3)throw std::invalid_argument("Font style must be Regular, Bold, Italic or BoldItalic");
            if(family) {
                auto chars=mono_string_chars(family);int length=mono_string_length(family);
                if(length>1024)throw std::invalid_argument("Family reference exceeds 1024 UTF-16 units");
                for(int i=0;i<length;++i){auto c=chars[i];if(!c)throw std::invalid_argument("Family reference contains NUL");
                    if(c>=0xd800 && c<=0xdbff){if(++i>=length || chars[i]<0xdc00 || chars[i]>0xdfff)throw std::invalid_argument("Invalid UTF-16 family reference");}
                    else if(c>=0xdc00 && c<=0xdfff)throw std::invalid_argument("Invalid UTF-16 family reference");}
            }
            FontSelection selection{std::filesystem::u8path(Utils::MonoStringToString(family)),FontStyle(style)};selection.Validate();
            auto* scene=ScriptEngine::GetSceneContext();scene->CheckThread();
            if(!selection.Family.empty()) {auto assets=scene->GetAssets();if(!assets)throw std::invalid_argument("Family selection needs project assets");(void)Project::ResolveOwnedAsset(assets->Root(),selection.Family);}
            entity.GetComponent<TextComponent>().Font=std::move(selection);
        }catch(const std::exception& error){failure=mono_get_exception_argument("font",error.what());}
        if(failure)mono_raise_exception(failure);
    }

	static void TextComponent_GetColor(UUID entityID, glm::vec4* color)
	{
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
		*color = tc.Color;
	}

	static void TextComponent_SetColor(UUID entityID, glm::vec4* color)
	{
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
        for(int i=0;i<4;i++)if(!std::isfinite((*color)[i]))mono_raise_exception(mono_get_exception_argument("color","Text color must be finite"));
		tc.Color = *color;
	}

	static float TextComponent_GetKerning(UUID entityID)
	{
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
		return tc.Kerning;
	}

	static void TextComponent_SetKerning(UUID entityID, float kerning)
	{
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
        if(!std::isfinite(kerning))mono_raise_exception(mono_get_exception_argument("kerning","Text spacing must be finite"));
		tc.Kerning = kerning;
	}

	static float TextComponent_GetLineSpacing(UUID entityID)
	{
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
		return tc.LineSpacing;
	}

	static void TextComponent_SetLineSpacing(UUID entityID, float lineSpacing)
	{
        Entity entity=CheckedSpriteEntity<TextComponent>(entityID);

		auto& tc = entity.GetComponent<TextComponent>();
        if(!std::isfinite(lineSpacing))mono_raise_exception(mono_get_exception_argument("lineSpacing","Text spacing must be finite"));
		tc.LineSpacing = lineSpacing;
	}

    static void Scene_LoadScene(MonoString* path) {
        auto* session = ScriptEngine::GetRuntimeSession();
        try {
            if (!session || !session->RequestSceneLoad(std::filesystem::u8path(Utils::MonoStringToString(path))))
                HZ_CORE_ERROR("Scene.LoadScene request rejected (no running project session or conflicting/invalid request)");
        } catch (const std::exception& error) { HZ_CORE_ERROR("Scene.LoadScene request: {}", error.what()); }
    }
    static RuntimeSession& UI_Session(){auto* s=ScriptEngine::GetRuntimeSession();if(!s)throw std::logic_error("UI bindings require an active session");return *s;}
    static void UI_Binding(int action,int* key,int* alternate,int* pad,int* shift,bool write) {
        MonoException* failure=nullptr;
        try{if(action<1||action>=int(GameUI::UIAction::Count))throw std::invalid_argument("Invalid digital UI action");auto& session=UI_Session();auto b=session.GetUIBindings();
            if(write){if(*shift<0||*shift>2)throw std::invalid_argument("Invalid Shift match");b.Actions[size_t(action)]={*key,*alternate,*pad,GameUI::ShiftMatch(*shift)};session.ApplyUIBindings(b);}
            else {auto x=b.Actions[size_t(action)];*key=x.Key;*alternate=x.Alternate;*pad=x.Pad;*shift=int(x.Shift);}}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    struct UIInputSettings {int PointerButton,Stick;float DeadZone,NeutralZone,RepeatDelay,RepeatInterval;};
    static void UI_InputSettings(UIInputSettings* value,bool write) {
        MonoException* failure=nullptr;
        try{auto& session=UI_Session();auto b=session.GetUIBindings();if(write){b.PointerButton=value->PointerButton;b.Stick=value->Stick;b.DeadZone=value->DeadZone;b.NeutralZone=value->NeutralZone;b.RepeatDelay=value->RepeatDelay;b.RepeatInterval=value->RepeatInterval;session.ApplyUIBindings(b);}
            else *value={b.PointerButton,b.Stick,b.DeadZone,b.NeutralZone,b.RepeatDelay,b.RepeatInterval};}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_ResetBindings(){MonoException* failure=nullptr;try{UI_Session().ResetUIBindings();}catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);}
    static uint64_t UI_FocusScope(){auto* scene=ScriptEngine::GetSceneContext();return scene?scene->GetUIDiagnostics().Scope:0;}
    static bool UI_Cancel(uint64_t identity,uint64_t scope){MonoException* failure=nullptr;bool result=false;try{auto* scene=ScriptEngine::GetSceneContext();if(scene&&scene->GetIdentity()==identity)result=scene->ConsumeUICancel(scope);}catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);return result;}
    static bool UI_IsPointerCaptured(){auto* scene=ScriptEngine::GetSceneContext();return scene&&scene->IsUICapturingPointer();}
    static bool UI_IsKeyDown(KeyCode key) {
        auto* session=ScriptEngine::GetRuntimeSession();return session&&session->IsInputEnabled()&&Input::IsKeyPressed(key);
    }
    static bool UI_Consume(uint64_t identity,uint64_t id,uint64_t* entity,MonoString** name) {
        *entity=0;*name=nullptr;MonoException* failure=nullptr;bool result=false;
        try{auto* scene=ScriptEngine::GetSceneContext();if(scene){scene->CheckThread();if(scene->GetIdentity()==identity){GameUI::Action action;if(scene->ConsumeUIAction(id,action)){*entity=action.Entity;*name=ScriptEngine::CreateString(action.Name.c_str());result=true;}}}}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);return result;
    }
    static uint32_t UI_LayoutVersion(uint64_t id) {
        MonoException* failure=nullptr;uint32_t result=0;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");scene->CheckThread();auto e=scene->GetEntityByUUID(id);if(!e||!scene->IsEntityValid(id)||!e.HasComponent<UIRectComponent>())throw std::invalid_argument("Sizing needs a live UI Rect");result=GameUI::RectLayout::SchemaVersion;}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if(failure)mono_raise_exception(failure);return result;
    }
    static void UI_GetSizing(uint64_t id,int axis,int* mode,float* size,float* minimum,int* hasMaximum,float* maximum) {
        MonoException* failure=nullptr;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");scene->CheckThread();auto e=scene->GetEntityByUUID(id);if(!e||!scene->IsEntityValid(id)||!e.HasComponent<UIRectComponent>()||axis<0||axis>1)throw std::invalid_argument("Sizing needs a live UI Rect and valid axis");const auto& r=e.GetComponent<UIRectComponent>();*mode=int(axis?r.HeightMode:r.WidthMode);*size=r.FixedSize[axis];*minimum=r.MinimumSize[axis];*hasMaximum=r.HasMaximum[axis]?1:0;*maximum=r.MaximumSize[axis];}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if(failure)mono_raise_exception(failure);
    }
    static void UI_SetSizing(uint64_t id,int axis,int mode,float size,float minimum,int hasMaximum,float maximum) {
        MonoException* failure=nullptr;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");if(axis<0||axis>1||hasMaximum<0||hasMaximum>1)throw std::invalid_argument("Invalid sizing axis/maximum flag");scene->SetUISizing(id,GameUI::Axis(axis),GameUI::SizeMode(mode),size,minimum,hasMaximum?std::optional<float>(maximum):std::optional<float>{});}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if(failure)mono_raise_exception(failure);
    }
    static void UI_TextOverflow(uint64_t id,int* mode,uint32_t* lines,bool write) {
        MonoException* failure=nullptr;
        try {
            auto* scene=ScriptEngine::GetSceneContext();if(!scene || scene->IsStopping())throw std::invalid_argument("Text overflow requires a runtime scene");
            scene->CheckThread();auto e=scene->GetEntityByUUID(id);
            if(!e || !scene->IsEntityValid(id) || !e.HasComponent<UITextStyleComponent>())throw std::invalid_argument("Text overflow needs a live UI Text Style");
            auto& style=e.GetComponent<UITextStyleComponent>();
            if(write){if(*mode<0 || *mode>1 || *lines>4096)throw std::invalid_argument("Overflow must be Clip/Ellipsis; MaxLines must be 0 to 4096");style.Overflow=Text::Overflow(*mode);style.MaxLines=*lines;}
            else {*mode=int(style.Overflow);*lines=style.MaxLines;}
        } catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if(failure)mono_raise_exception(failure);
    }
    // Blittable wire values: int flags, never native bool or retained managed references.
    struct UILayoutWire { int Kind,Direction,Main,Cross,LineAlign;float Gap,LineGap;glm::vec2 GridGap; };
    struct UITrackWire { int Mode;float Value,Minimum,Maximum;int HasMaximum; };
    static_assert(sizeof(UILayoutWire)==36 && sizeof(UITrackWire)==20,"Managed UI layout ABI");
    static Entity UI_LayoutEntity(uint64_t id) {
        auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");scene->CheckThread();
        auto e=scene->GetEntityByUUID(id);if(!e||!scene->IsEntityValid(id)||!e.HasComponent<UILayoutComponent>())throw std::invalid_argument("Container needs a live UI Layout");return e;
    }
    static void UI_GetLayout(uint64_t id,UILayoutWire* result) {
        MonoException* failure=nullptr;try{auto &v=UI_LayoutEntity(id).GetComponent<UILayoutComponent>();*result={int(v.Kind),int(v.Direction),int(v.Main),int(v.Cross),int(v.LineAlign),v.Gap,v.LineGap,v.GridGap};}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static int UI_TrackCount(uint64_t id,bool rows) {
        int count=0;MonoException* failure=nullptr;try{auto &v=UI_LayoutEntity(id).GetComponent<UILayoutComponent>();count=int(rows?v.Rows.size():v.Columns.size());}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);return count;
    }
    static void UI_GetTrack(uint64_t id,bool rows,int index,UITrackWire* result) {
        MonoException* failure=nullptr;try{auto &v=UI_LayoutEntity(id).GetComponent<UILayoutComponent>();auto &tracks=rows?v.Rows:v.Columns;if(index<0||size_t(index)>=tracks.size())throw std::invalid_argument("Track index out of range");auto &t=tracks[index];*result={int(t.Mode),t.Value,t.Minimum,t.Maximum,t.HasMaximum?1:0};}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_SetLayout(uint64_t id,UILayoutWire* wire,MonoArray* columns,MonoArray* rows) {
        MonoException* failure=nullptr;try{auto e=UI_LayoutEntity(id);auto next=e.GetComponent<UILayoutComponent>();auto &v=*wire;
            next.Kind=GameUI::ContainerKind(v.Kind);next.Direction=GameUI::Axis(v.Direction);next.Main=GameUI::Align(v.Main);next.Cross=GameUI::Align(v.Cross);next.LineAlign=GameUI::Align(v.LineAlign);next.Gap=v.Gap;next.LineGap=v.LineGap;next.GridGap=v.GridGap;
            auto read=[](MonoArray* array,std::vector<GameUI::Track> &tracks){if(!array)return;auto count=mono_array_length(array);if(!count||count>64)throw std::invalid_argument("Grid needs 1..64 tracks");tracks.clear();tracks.reserve(count);
                for(uintptr_t i=0;i<count;++i){auto &t=*reinterpret_cast<UITrackWire*>(mono_array_addr_with_size(array,sizeof(UITrackWire),i));if(t.HasMaximum<0||t.HasMaximum>1)throw std::invalid_argument("Invalid track maximum flag");tracks.push_back({GameUI::TrackMode(t.Mode),t.Value,t.Minimum,t.Maximum,t.HasMaximum!=0});}};
            read(columns,next.Columns);read(rows,next.Rows);ScriptEngine::GetSceneContext()->SetUILayout(id,next);
        }catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_GetCell(uint64_t id,int* row,int* column,int* rowSpan,int* columnSpan) {
        MonoException* failure=nullptr;try{auto e=CheckedSpriteEntity<UIRectComponent>(id);auto &r=e.GetComponent<UIRectComponent>();*row=r.GridRow;*column=r.GridColumn;*rowSpan=int(r.RowSpan);*columnSpan=int(r.ColumnSpan);}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_SetCell(uint64_t id,int row,int column,int rowSpan,int columnSpan) {
        MonoException* failure=nullptr;try{if(rowSpan<1||columnSpan<1)throw std::invalid_argument("Cell spans must be positive");auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");scene->SetUIGridCell(id,row,column,unsigned(rowSpan),unsigned(columnSpan));}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_GetImageFit(uint64_t id,int* fit,glm::vec2* alignment) {
        MonoException* failure=nullptr;try{auto e=CheckedSpriteEntity<UIRectComponent>(id);auto &r=e.GetComponent<UIRectComponent>();*fit=int(r.Fit);*alignment=r.ImageAlignment;}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_SetImageFit(uint64_t id,int fit,glm::vec2* alignment) {
        MonoException* failure=nullptr;try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");scene->SetUIImageFit(id,GameUI::ImageFit(fit),*alignment);}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);
    }
    static bool* UI_Value(uint64_t id,int property) {
        auto* scene=ScriptEngine::GetSceneContext();auto e=scene?scene->GetEntityByUUID(id):Entity{};
        if(!e||!scene->IsEntityValid(id))return nullptr;
        if(property==3&&e.HasComponent<UIButtonComponent>())return &e.GetComponent<UIButtonComponent>().Interactable;
        if((property==4||property==5)&&e.HasComponent<UIRectComponent>()) {
            auto& r=e.GetComponent<UIRectComponent>();return property==4?&r.Visible:&r.Enabled;
        }
        if(e.HasComponent<UICanvasComponent>()) {
            auto& c=e.GetComponent<UICanvasComponent>();if(property==0)return &c.Visible;if(property==1)return &c.Enabled;if(property==2)return &c.Modal;
        }
        if(e.HasComponent<UIRectComponent>()) {
            auto& c=e.GetComponent<UIRectComponent>();if(property==0)return &c.Visible;if(property==1)return &c.Enabled;
        }
        return nullptr;
    }
    static bool UI_GetValue(uint64_t id,int property) {
        MonoException* failure=nullptr;bool result=false;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::runtime_error("UI requires a runtime scene");scene->CheckThread();auto* value=UI_Value(id,property);if(!value)throw std::runtime_error("UI property needs a live UI component");result=*value;}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);return result;
    }
    static void UI_SetValue(uint64_t id,int property,bool value) {
        MonoException* failure=nullptr;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::runtime_error("UI requires a runtime scene");scene->CheckThread();auto* target=UI_Value(id,property);if(!target)throw std::runtime_error("UI property needs a live UI component");if(*target!=value){*target=value;for(auto child:scene->GetSubtree(scene->GetEntityByUUID(id)))scene->RetireUI(child);scene->RefreshUI();}}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if(failure)mono_raise_exception(failure);
    }
    static bool Input_IsKeyDown(KeyCode keycode) {
        auto* session = ScriptEngine::GetRuntimeSession();
        return (!session || session->IsGameplayInputEnabled()) && Input::IsKeyPressed(keycode);
    }
    static bool Input_IsMouseButtonDown(MouseCode button) {
        auto* session = ScriptEngine::GetRuntimeSession();
        return session && session->IsGameplayMouseEnabled() && Input::IsMouseButtonPressed(button);
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
        RegisterComponent<UICanvasComponent>("Hazel.UICanvasComponent");
        RegisterComponent<UIRectComponent>("Hazel.UIRectComponent");
        RegisterComponent<UILayoutComponent>("Hazel.UILayoutComponent");
        RegisterComponent<UITextStyleComponent>("Hazel.UITextStyleComponent");
        RegisterComponent<UINineSliceComponent>("Hazel.UINineSliceComponent");
        RegisterComponent<UIButtonComponent>("Hazel.UIButtonComponent");

	}

	void ScriptGlue::ValidateComponents(MonoImage* image)
	{
		for (const char* name : { "Hazel.TransformComponent", "Hazel.Rigidbody2DComponent", "Hazel.TextComponent", "Hazel.CameraComponent", "Hazel.SpriteRendererComponent", "Hazel.SpriteAnimationComponent", "Hazel.AudioSourceComponent", "Hazel.UICanvasComponent", "Hazel.UIRectComponent", "Hazel.UILayoutComponent", "Hazel.UITextStyleComponent", "Hazel.UINineSliceComponent", "Hazel.UIButtonComponent" })
			if (!mono_reflection_type_from_name(const_cast<char*>(name), image))
				throw std::runtime_error(std::string("Missing managed component: ") + name);
	}

	void ScriptGlue::RegisterFunctions()
	{
        RegisterHierarchyFunctions();
        HZ_ADD_INTERNAL_CALL(UI_IsKeyDown);
        HZ_ADD_INTERNAL_CALL(UI_Binding);
        HZ_ADD_INTERNAL_CALL(UI_InputSettings);
        HZ_ADD_INTERNAL_CALL(UI_ResetBindings);
        HZ_ADD_INTERNAL_CALL(UI_FocusScope);
        HZ_ADD_INTERNAL_CALL(UI_Cancel);
        HZ_ADD_INTERNAL_CALL(UI_IsPointerCaptured);
        HZ_ADD_INTERNAL_CALL(UI_Consume);
        HZ_ADD_INTERNAL_CALL(UI_GetValue);
        HZ_ADD_INTERNAL_CALL(UI_SetValue);
        HZ_ADD_INTERNAL_CALL(UI_LayoutVersion);
        HZ_ADD_INTERNAL_CALL(UI_GetImageFit);
        HZ_ADD_INTERNAL_CALL(UI_SetImageFit);
        HZ_ADD_INTERNAL_CALL(UI_GetLayout);
        HZ_ADD_INTERNAL_CALL(UI_TrackCount);
        HZ_ADD_INTERNAL_CALL(UI_GetTrack);
        HZ_ADD_INTERNAL_CALL(UI_SetLayout);
        HZ_ADD_INTERNAL_CALL(UI_GetCell);
        HZ_ADD_INTERNAL_CALL(UI_SetCell);

        HZ_ADD_INTERNAL_CALL(UI_TextOverflow);
        HZ_ADD_INTERNAL_CALL(UI_GetSizing);
        HZ_ADD_INTERNAL_CALL(UI_SetSizing);
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
        HZ_ADD_INTERNAL_CALL(TextComponent_GetFontFamily);
        HZ_ADD_INTERNAL_CALL(TextComponent_GetFontStyle);
        HZ_ADD_INTERNAL_CALL(TextComponent_SetFont);
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
