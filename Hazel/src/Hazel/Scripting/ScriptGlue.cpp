#include "hzpch.h"
#include "ScriptGlue.h"
#include "ManagedString.h"
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

	static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;

#define HZ_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Hazel.InternalCalls::" #Name, reinterpret_cast<const void*>(Name))

    static RuntimeStorage& CheckedStorage() {
        auto* session=ScriptEngine::GetRuntimeSession();
        if(!session)throw std::runtime_error("SaveData requires an active runtime session");
        return session->Storage();
    }
    static bool SaveData_IsPersistent() {
        MonoException* failure = nullptr;
        bool result = false;
        try {
            if (ScriptEngine::GetRuntimeSession())
                result = CheckedStorage().IsPersistent();
        } catch (const std::exception& e) {
            failure = mono_get_exception_invalid_operation(e.what());
        }
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static MonoString* SaveData_Read(MonoString* slot) {
        MonoException* failure = nullptr;
        MonoString* result = nullptr;
        try {
            const auto name = ManagedStrings::Read(slot, 96, 96, "Save slot");
            const auto payload = CheckedStorage().Read(name);
            result = mono_string_new(mono_domain_get(), payload.c_str());
        } catch (const std::exception& e) {
            failure = mono_get_exception_invalid_operation(e.what());
        }
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static void SaveData_Write(MonoString* slot, MonoString* payload) {
        MonoException* failure = nullptr;
        try {
            const auto name = ManagedStrings::Read(slot, 96, 96, "Save slot");
            const auto text = ManagedStrings::Read(payload, RuntimeStorage::MaximumPayload,
                                                   RuntimeStorage::MaximumPayload, "Save payload");
            CheckedStorage().Write(name, text);
        } catch (const std::exception& e) {
            failure = mono_get_exception_invalid_operation(e.what());
        }
        if (failure)
            mono_raise_exception(failure);
    }

    static void NativeLog(MonoString* value, int parameter) {
        MonoException* failure = nullptr;
        try {
            const auto text = ManagedStrings::Read(value, 32768, 131072, "Log message",
                                                   ManagedStrings::NulPolicy::Preserve);
            Log::GetClientLogger()->log(spdlog::source_loc{"Managed",0,""}, spdlog::level::info,
                                        "{}, {}", text, parameter);
        } catch (const std::exception& e) {
            failure = mono_get_exception_argument("message", e.what());
        }
        if (failure)
            mono_raise_exception(failure);
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
    static uint64_t Entity_Instantiate(MonoString* path, glm::vec3* position, glm::vec3* rotation,
                                       glm::vec3* scale, bool replaceRotationAndScale) {
        MonoException* failure = nullptr;
        uint64_t result = 0;
        try {
            const auto reference = ManagedStrings::Read(path, ManagedStrings::PathUnits,
                                                        ManagedStrings::PathBytes, "Prefab path");
            auto* scene = ScriptEngine::GetSceneContext();
            if (!scene || !Project::GetActive())
                throw std::runtime_error("Instantiation needs an active main-thread runtime project");
            TransformComponent transform;
            transform.Translation = *position;
            transform.Rotation = *rotation;
            transform.Scale = *scale;
            result = Prefab::Instantiate(Project::GetAssetDirectory(), std::filesystem::u8path(reference),
                                         *scene, transform, replaceRotationAndScale).GetUUID();
        } catch (const std::exception& error) {
            HZ_CORE_ERROR("Instantiate prefab: {}", error.what());
            failure = mono_get_exception_invalid_operation(error.what());
        }
        if (failure)
            mono_raise_exception(failure);
        return result;
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
    static void SpriteRendererComponent_SetSprite(uint64_t id, MonoString* sheet, uint64_t region) {
        MonoException* failure = nullptr;
        try {
            auto entity = CheckedSpriteEntity<SpriteRendererComponent>(id);
            const auto path = ManagedStrings::Read(sheet, ManagedStrings::PathUnits,
                                                   ManagedStrings::PathBytes, "Sprite sheet path");
            SpriteReference reference{std::filesystem::u8path(path), region};
            auto* scene = ScriptEngine::GetSceneContext();
            if (!scene->GetAssets())
                throw std::runtime_error("Project assets unavailable");
            scene->GetAssets()->Resolve(reference);
            entity.GetComponent<SpriteRendererComponent>().Source = reference;
        } catch (const std::exception& e) {
            failure = mono_get_exception_invalid_operation(e.what());
        }
        if (failure)
            mono_raise_exception(failure);
    }
    static void SpriteAnimationComponent_Play(uint64_t id, MonoString* sheet, uint64_t clip) {
        MonoException* failure = nullptr;
        try {
            auto entity = CheckedSpriteEntity<SpriteAnimationComponent>(id);
            const auto path = ManagedStrings::Read(sheet, ManagedStrings::PathUnits,
                                                   ManagedStrings::PathBytes, "Animation sheet path");
            AnimationReference reference{std::filesystem::u8path(path), clip};
            if (!ScriptEngine::GetSceneContext()->PlayAnimation(entity, reference))
                throw std::runtime_error("Cannot play clip; see native asset diagnostic");
        } catch (const std::exception& e) {
            failure = mono_get_exception_invalid_operation(e.what());
        }
        if (failure)
            mono_raise_exception(failure);
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
        if (failure)
            mono_raise_exception(failure);
        return result;
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
        if (failure)
            mono_raise_exception(failure);
        return result;
    }

    static uint64_t Entity_FindEntityByName(MonoString* name) {
        MonoException* failure = nullptr;
        uint64_t result = 0;
        try {
            const auto text = ManagedStrings::Read(name, 32768, 131072, "Entity name");
            auto* scene = ScriptEngine::GetSceneContext();
            if (!scene)
                throw std::runtime_error("Entity lookup needs a runtime scene");
            auto entity = scene->FindEntityByName(text);
            if (entity)
                result = entity.GetUUID();
        } catch (const std::exception& e) {
            failure = mono_get_exception_argument("name", e.what());
        }
        if (failure)
            mono_raise_exception(failure);
        return result;
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
		if(entity.HasComponent<UITextEntryComponent>())return ScriptEngine::CreateString(ScriptEngine::GetSceneContext()->GetUITextEntry(entityID).c_str());
		return ScriptEngine::CreateString(tc.TextString.c_str());
	}

    static void TextComponent_SetText(UUID entityID, MonoString* textString)
    {
        MonoException* failure=nullptr;
        try {
            auto* scene=ScriptEngine::GetSceneContext();
            if (!scene)
                throw std::invalid_argument("Text needs a runtime scene");
            scene->CheckThread();
            auto entity=scene->GetEntityByUUID(entityID);
            if(!entity||!scene->IsEntityValid(entityID)||!entity.HasComponent<TextComponent>())throw std::invalid_argument("Text needs a live Text component");
            const auto text = ManagedStrings::Read(textString, 32768, 65536, "Text");
            scene->SetDisplayText(entityID,text);
        }catch(const std::exception& e){failure=mono_get_exception_argument("text",e.what());}
        if(failure)mono_raise_exception(failure);
    }

    struct TextSpanWire {uint32_t Begin,End,Flags;int32_t Face;glm::vec4 Color;};
    static_assert(sizeof(TextSpanWire)==32,"Managed emphasis ABI");
    static void TextComponent_SetRichText(UUID entityID,MonoString* source,MonoArray* array) {
        MonoException* failure=nullptr;
        try {
            const auto text = ManagedStrings::Read(source, 32768, 65536, "Display text");
            const auto map = Text::SourceMap::Analyze(text);
            const auto count=array?mono_array_length(array):0;if(count>Text::MaxSpans)throw std::invalid_argument("Display emphasis exceeds 128 spans");
            std::vector<Text::Span> spans;spans.reserve(count);
            for(size_t i=0;i<count;++i){auto wire=mono_array_get(array,TextSpanWire,i);if(wire.Flags&~3u)throw std::invalid_argument("Invalid emphasis flags");Text::Span s;
                s.Begin=map.Scalar(wire.Begin,Text::SourceUnit::UTF16);s.End=map.Scalar(wire.End,Text::SourceUnit::UTF16);
                if(wire.Flags&1){if(wire.Face<0||wire.Face>3)throw std::invalid_argument("Invalid emphasis face");s.Face=FontStyle(wire.Face);}if(wire.Flags&2)s.Color=wire.Color;spans.push_back(s);}
            auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("Display text needs a runtime scene");scene->SetDisplayText(entityID,text,spans);
        }catch(const std::exception& e){failure=mono_get_exception_argument("emphasis",e.what());}
        if(failure)mono_raise_exception(failure);
    }
    static int TextComponent_GetEmphasisCount(UUID entityID){return int(CheckedSpriteEntity<TextComponent>(entityID).GetComponent<TextComponent>().Spans.size());}
    static void TextComponent_GetEmphasis(UUID entityID,MonoArray* array) {
        MonoException* failure=nullptr;
        try {const auto& c=CheckedSpriteEntity<TextComponent>(entityID).GetComponent<TextComponent>();
            if(!array||mono_array_length(array)!=c.Spans.size()||c.Spans.size()>Text::MaxSpans)throw std::invalid_argument("Emphasis array length changed; retry");
            auto map=Text::SourceMap::Analyze(c.TextString);Text::NormalizeSpans(map,c.Spans);
            for(size_t i=0;i<c.Spans.size();++i){const auto& s=c.Spans[i];TextSpanWire wire{map.Offset(s.Begin,Text::SourceUnit::UTF16),map.Offset(s.End,Text::SourceUnit::UTF16),uint32_t((s.Face?1:0)|(s.Color?2:0)),s.Face?int(*s.Face):0,s.Color.value_or(glm::vec4(1))};mono_array_set(array,TextSpanWire,i,wire);}
        }catch(const std::exception& e){failure=mono_get_exception_argument("emphasis",e.what());}
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
            const auto path = ManagedStrings::Read(family, 1024, 1024, "Font family");
            FontSelection selection{std::filesystem::u8path(path), FontStyle(style)};
            selection.Validate();
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
            if (!session || !session->RequestSceneLoad(std::filesystem::u8path(ManagedStrings::Read(path, ManagedStrings::PathUnits, ManagedStrings::PathBytes, "Scene path"))))
                HZ_CORE_ERROR("Scene.LoadScene request rejected (no running project session or conflicting/invalid request)");
        } catch (const std::exception& error) { HZ_CORE_ERROR("Scene.LoadScene request: {}", error.what()); }
    }
    static RuntimeSession& UI_Session(){auto* s=ScriptEngine::GetRuntimeSession();if(!s)throw std::logic_error("UI bindings require an active session");return *s;}
    static void UI_Binding(int action,int* key,int* alternate,int* pad,int* shift,bool write) {
        MonoException* failure=nullptr;
        try{if(action<1||action>=int(GameUI::UIAction::Count)||!GameUI::IsBindable(GameUI::UIAction(action)))throw std::invalid_argument("Invalid digital UI action");auto& session=UI_Session();auto b=session.GetUIBindings();
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
    static float Runtime_AudioVolume(float value,bool write){
        MonoException* failure=nullptr;float result=1;
        try {auto &session=UI_Session();if(write)session.SetAudioVolume(value);result=session.GetAudioVolume();}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static void Runtime_Exit(){
        MonoException* failure=nullptr;try{UI_Session().RequestExit();}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if(failure)mono_raise_exception(failure);
    }
    static void UI_ResetBindings(){MonoException* failure=nullptr;try{UI_Session().ResetUIBindings();}catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}if(failure)mono_raise_exception(failure);}
    static bool UI_Focus(uint64_t identity,uint64_t id){MonoException* failure=nullptr;bool result=false;try{auto* scene=ScriptEngine::GetSceneContext();if(scene&&scene->GetIdentity()==identity)result=scene->FocusUI(id);}catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static bool UI_Screen(uint64_t identity,uint64_t id,int operation) {
        MonoException* failure=nullptr;bool result=false;
        try{auto* scene=ScriptEngine::GetSceneContext();if(scene&&scene->GetIdentity()==identity){scene->CheckThread();if(operation==-1){scene->RefreshUI();result=scene->IsUIScreenOpen(id);}else if(operation>=0&&operation<=3)result=scene->RequestUIScreen(GameUI::ScreenOperation(operation),id);else throw std::invalid_argument("Invalid UI screen operation");}}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static bool UI_ConsumeEvent(uint64_t identity,uint64_t* screen,uint64_t* source,int* kind,uint64_t* sequence,MonoString** name,bool* previousChecked,bool* checked,uint64_t* previousSelection,uint64_t* selection,uint64_t* recipient,float* previousValue,float* value,float* startValue,uint64_t* gesture,bool* interrupted,MonoString** previousText,MonoString** text) {
        *previousText=*text=nullptr;
        *previousValue=*value=*startValue=0;*gesture=0;*interrupted=false;
        *recipient=0;*previousChecked=*checked=false;*previousSelection=*selection=0;
        *screen=*source=*sequence=0;*kind=0;*name=nullptr;MonoException* failure=nullptr;bool result=false;
        try{auto* scene=ScriptEngine::GetSceneContext();auto owner=ScriptEngine::GetExecutingEntity();
            if(!owner)throw std::invalid_argument("Scoped UI events must be consumed from the declared owner's lifecycle callback");
            if(scene&&scene->GetIdentity()==identity){auto entity=scene->GetEntityByUUID(owner);auto instance=ScriptEngine::GetEntityScriptInstance(owner);
                if(!entity||!entity.HasComponent<ScriptComponent>()||!instance||instance->GetScriptClass()!=ScriptEngine::GetEntityClass(entity.GetComponent<ScriptComponent>().ClassName))return false;
                GameUI::Action event;if(scene->ConsumeUIEvent(owner,event)){*screen=event.Screen;*source=event.Entity;*kind=int(event.Kind);*sequence=event.Sequence;*name=ScriptEngine::CreateString(event.Name.c_str());*previousChecked=event.PreviousChecked;*checked=event.Checked;*previousSelection=event.PreviousSelection;*selection=event.Selection;*recipient=event.Owner;*previousValue=event.PreviousValue;*value=event.Value;*startValue=event.StartValue;*gesture=event.Gesture;*interrupted=event.Interrupted;*previousText=ScriptEngine::CreateString(event.PreviousText.c_str());*text=ScriptEngine::CreateString(event.TextValue.c_str());result=true;}}}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static void UI_Style(uint64_t id,bool write,MonoString* asset,MonoString* variant,bool enabled,MonoString** actualAsset,MonoString** actualVariant,bool* actualEnabled){
        *actualAsset=*actualVariant=nullptr;*actualEnabled=false;MonoException* failure=nullptr;
        try{auto scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("Style requires a runtime scene");scene->CheckThread();if(write)scene->SetUIStyle(id,{std::filesystem::u8path(ManagedStrings::Read(asset, 1024, 1024, "UI style asset")),ManagedStrings::Read(variant, 1024, 128, "UI style variant")},enabled);auto selection=scene->GetUIStyle(id);*actualAsset=ScriptEngine::CreateString(selection.Asset.generic_u8string().c_str());*actualVariant=ScriptEngine::CreateString(selection.Variant.c_str());*actualEnabled=scene->GetEntityByUUID(id).GetComponent<UIAppearanceComponent>().Styled;}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}if(failure)mono_raise_exception(failure);
    }
    static uint32_t UI_StyleOverrides(uint64_t id,uint32_t groups,bool enable){
        uint32_t result=0;MonoException* failure=nullptr;
        try{auto scene=ScriptEngine::GetSceneContext();if(!scene||scene->IsStopping())throw std::invalid_argument("Overrides need a live runtime scene");scene->CheckThread();auto entity=scene->GetEntityByUUID(id);if(!entity||!scene->IsEntityValid(id)||!entity.HasComponent<UIAppearanceComponent>()||(groups&~UIStyleAll))throw std::invalid_argument("Invalid UI override target/groups");auto& c=entity.GetComponent<UIAppearanceComponent>();auto before=c.Overrides;if(enable){c.Overrides|=groups;try{scene->RefreshUI();}catch(...){c.Overrides=before;try{scene->RefreshUI();}catch(...){}throw;}}result=c.Overrides;}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static void UI_StyleReset(uint64_t id,uint32_t groups){MonoException* failure=nullptr;try{auto scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("Style requires a runtime scene");scene->ResetUIStyleOverrides(id,groups);}catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}if(failure)mono_raise_exception(failure);}
    static MonoString* UI_TextEntry(uint64_t id, bool write, MonoString* value) {
        MonoException* failure = nullptr;
        MonoString* result = nullptr;
        try {
            auto* scene = ScriptEngine::GetSceneContext();
            if (!scene)
                throw std::invalid_argument("Text Entry requires a runtime scene");
            if (write) {
                const auto text = ManagedStrings::Read(value, Text::EditBuffer::MaxBytes,
                                                       Text::EditBuffer::MaxBytes, "Text entry");
                scene->SetUITextEntry(id, text);
            }
            result = ScriptEngine::CreateString(scene->GetUITextEntry(id).c_str());
        } catch (const std::exception& e) {
            failure = mono_get_exception_invalid_operation(e.what());
        }
        if (failure)
            mono_raise_exception(failure);
        return result;
    }

    static bool UI_Checked(uint64_t id,bool write,bool value){
        MonoException* failure=nullptr;bool result=false;
        try{auto* s=ScriptEngine::GetSceneContext();if(!s)throw std::invalid_argument("Control requires a runtime scene");if(write)s->SetUIChecked(id,value);result=s->IsUIChecked(id);}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static uint64_t UI_Selection(uint64_t id,bool write,uint64_t value){
        MonoException* failure=nullptr;uint64_t result=0;
        try{auto* s=ScriptEngine::GetSceneContext();if(!s)throw std::invalid_argument("Control requires a runtime scene");if(write)s->SetUISelection(id,value);result=s->GetUISelection(id);}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static float UI_Progress(uint64_t id,bool write,float value){
        MonoException* failure=nullptr;float result=0;
        try{auto* s=ScriptEngine::GetSceneContext();if(!s)throw std::invalid_argument("Control requires a runtime scene");if(write)s->SetUIProgress(id,value);result=s->GetUIProgress(id);}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static uint64_t UI_PresentationReference(uint64_t id,int kind){MonoException* failure=nullptr;uint64_t result=0;try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI reference requires a live scene");auto e=scene->GetEntityByUUID(id);if(!e||!scene->IsEntityValid(id))throw std::invalid_argument("UI source retired");if(kind==0&&e.HasComponent<UITabComponent>())result=e.GetComponent<UITabComponent>().Page;else if(kind==1&&e.HasComponent<UITooltipComponent>())result=e.GetComponent<UITooltipComponent>().Body;else throw std::invalid_argument("UI presentation component missing");}catch(const std::exception &e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static bool UI_Dropdown(uint64_t id,int operation){MonoException* failure=nullptr;bool result=false;try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("Dropdown requires a live scene");if(operation< -1||operation>1)throw std::invalid_argument("Invalid Dropdown operation");result=operation<0?scene->IsUIDropdownOpen(id):scene->RequestUIDropdown(id,operation!=0);}catch(const std::exception &e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static void UI_ScrollInfo(uint64_t id,glm::vec2* offset,glm::vec2* range,glm::vec2* viewport,glm::vec2* extent){
        MonoException* failure=nullptr;try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("ScrollView requires a scene");auto g=scene->GetUIScroll(id);*offset=g.Offset;*range=g.Range;*viewport=g.Viewport.Size;*extent=g.Extent;}catch(const std::exception&e){failure=mono_get_exception_invalid_operation(e.what());}if(failure)mono_raise_exception(failure);
    }
    static void UI_ScrollTo(uint64_t id,glm::vec2* offset,bool normalized){MonoException* failure=nullptr;try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("ScrollView requires a scene");scene->ScrollUITo(id,*offset,normalized);}catch(const std::exception&e){failure=mono_get_exception_invalid_operation(e.what());}if(failure)mono_raise_exception(failure);}
    static bool UI_EnsureVisible(uint64_t id,uint64_t target,bool focus){MonoException* failure=nullptr;bool result=false;try{auto* scene=ScriptEngine::GetSceneContext();if(scene)result=scene->EnsureUIVisible(id,target,focus);}catch(const std::exception&e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static float UI_Slider(uint64_t id,bool write,float value){
        MonoException* failure=nullptr;float result=0;
        try{auto* s=ScriptEngine::GetSceneContext();if(!s)throw std::invalid_argument("Control requires a runtime scene");if(write)s->SetUISliderValue(id,value);result=s->GetUISliderValue(id);}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static uint64_t UI_FocusScope(){auto* scene=ScriptEngine::GetSceneContext();return scene?scene->GetUIDiagnostics().Scope:0;}
    static bool UI_Cancel(uint64_t identity,uint64_t scope){MonoException* failure=nullptr;bool result=false;try{auto* scene=ScriptEngine::GetSceneContext();if(scene&&scene->GetIdentity()==identity)result=scene->ConsumeUICancel(scope);}catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static bool UI_IsPointerCaptured(){auto* scene=ScriptEngine::GetSceneContext();return scene&&scene->IsUICapturingPointer();}
    static bool UI_TextInputOwned(){auto scene=ScriptEngine::GetSceneContext();return scene&&scene->IsUITextInputOwned();}
    static bool UI_IsKeyDown(KeyCode key) {
        auto* session=ScriptEngine::GetRuntimeSession();return session&&session->IsInputEnabled()&&session->GetScene()&&!session->GetScene()->IsUITextInputOwned()&&Input::IsKeyPressed(key);
    }
    static bool UI_Consume(uint64_t identity,uint64_t id,uint64_t* entity,MonoString** name) {
        *entity=0;*name=nullptr;MonoException* failure=nullptr;bool result=false;
        try{auto* scene=ScriptEngine::GetSceneContext();if(scene){scene->CheckThread();if(scene->GetIdentity()==identity){GameUI::Action action;if(scene->ConsumeUIAction(id,action)){*entity=action.Entity;*name=ScriptEngine::CreateString(action.Name.c_str());result=true;}}}}
        catch(const std::exception& e){failure=mono_get_exception_invalid_operation(e.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static uint32_t UI_LayoutVersion(uint64_t id) {
        MonoException* failure=nullptr;uint32_t result=0;
        try{auto* scene=ScriptEngine::GetSceneContext();if(!scene)throw std::invalid_argument("UI requires a runtime scene");scene->CheckThread();auto e=scene->GetEntityByUUID(id);if(!e||!scene->IsEntityValid(id)||!e.HasComponent<UIRectComponent>())throw std::invalid_argument("Sizing needs a live UI Rect");result=GameUI::RectLayout::SchemaVersion;}
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if (failure)
            mono_raise_exception(failure);
        return result;
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
        catch(const std::exception& error){failure=mono_get_exception_invalid_operation(error.what());}
        if (failure)
            mono_raise_exception(failure);
        return count;
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
        if (failure)
            mono_raise_exception(failure);
        return result;
    }
    static void UI_SetValue(uint64_t id,int property,bool value) {
        MonoException* failure=nullptr;
        try {
            auto* scene=ScriptEngine::GetSceneContext();
            if(!scene)throw std::runtime_error("UI requires a runtime scene");
            scene->CheckThread();
            auto* target=UI_Value(id,property);
            if(!target)throw std::runtime_error("UI property needs a live UI component");
            if(*target!=value) {
                const bool before=*target;
                *target=value;
                try {
                    for(auto child:scene->GetSubtree(scene->GetEntityByUUID(id)))scene->RetireUI(child);
                    scene->RefreshUI();
                } catch(...) {
                    *target=before;
                    // Restore authored state, but never resurrect retired input.
                    // An invalid host surface stays fail-closed until repaired.
                    try { scene->RefreshUI(); } catch(...) {}
                    throw;
                }
            }
        }
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
        RegisterComponent<UIRadioGroupComponent>("Hazel.UIRadioGroupComponent");
        RegisterComponent<UIProgressComponent>("Hazel.UIProgressComponent");
        RegisterComponent<UISliderComponent>("Hazel.UISliderComponent");
        RegisterComponent<UIScrollViewComponent>("Hazel.UIScrollViewComponent");
        RegisterComponent<UIAppearanceComponent>("Hazel.UIAppearanceComponent");
        RegisterComponent<UITextEntryComponent>("Hazel.UITextEntryComponent");
        RegisterComponent<UITabsComponent>("Hazel.UITabsComponent");
        RegisterComponent<UITabComponent>("Hazel.UITabComponent");
        RegisterComponent<UITooltipComponent>("Hazel.UITooltipComponent");
        RegisterComponent<UIDropdownComponent>("Hazel.UIDropdownComponent");

	}

	void ScriptGlue::ValidateComponents(MonoImage* image)
	{
		for (const char* name : { "Hazel.TransformComponent", "Hazel.Rigidbody2DComponent", "Hazel.TextComponent", "Hazel.CameraComponent", "Hazel.SpriteRendererComponent", "Hazel.SpriteAnimationComponent", "Hazel.AudioSourceComponent", "Hazel.UICanvasComponent", "Hazel.UIRectComponent", "Hazel.UILayoutComponent", "Hazel.UITextStyleComponent", "Hazel.UINineSliceComponent", "Hazel.UIButtonComponent", "Hazel.UIRadioGroupComponent", "Hazel.UIProgressComponent", "Hazel.UISliderComponent", "Hazel.UIScrollViewComponent", "Hazel.UIDropdownComponent", "Hazel.UITabsComponent", "Hazel.UITabComponent", "Hazel.UITooltipComponent", "Hazel.UIAppearanceComponent" })
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
        HZ_ADD_INTERNAL_CALL(Runtime_AudioVolume);
        HZ_ADD_INTERNAL_CALL(Runtime_Exit);
        HZ_ADD_INTERNAL_CALL(UI_Focus);
        HZ_ADD_INTERNAL_CALL(UI_Screen);
        HZ_ADD_INTERNAL_CALL(UI_ConsumeEvent);
        HZ_ADD_INTERNAL_CALL(UI_Style);
        HZ_ADD_INTERNAL_CALL(UI_StyleReset);
        HZ_ADD_INTERNAL_CALL(UI_StyleOverrides);
        HZ_ADD_INTERNAL_CALL(UI_TextEntry);
        HZ_ADD_INTERNAL_CALL(UI_TextInputOwned);
        HZ_ADD_INTERNAL_CALL(UI_Checked);
        HZ_ADD_INTERNAL_CALL(UI_Selection);
        HZ_ADD_INTERNAL_CALL(UI_Progress);
        HZ_ADD_INTERNAL_CALL(UI_Slider);
        HZ_ADD_INTERNAL_CALL(UI_Dropdown);
        HZ_ADD_INTERNAL_CALL(UI_PresentationReference);
        HZ_ADD_INTERNAL_CALL(UI_ScrollInfo);HZ_ADD_INTERNAL_CALL(UI_ScrollTo);HZ_ADD_INTERNAL_CALL(UI_EnsureVisible);
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
        HZ_ADD_INTERNAL_CALL(TextComponent_SetRichText);
        HZ_ADD_INTERNAL_CALL(TextComponent_GetEmphasisCount);
        HZ_ADD_INTERNAL_CALL(TextComponent_GetEmphasis);
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
