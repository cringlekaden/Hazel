#pragma once

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scripting/ScriptField.h"
#include "Hazel/Scene/Entity.h"

#include <filesystem>
#include <functional>
#include <string>
#include <map>
#include <cstring>
#include <cstddef>
#include <cstdint>

extern "C" {
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoClassField MonoClassField;
	typedef struct _MonoString MonoString;
}

namespace Hazel {
	struct ScriptEngineData;
    class ScriptAssemblyCandidate
    {
    public:
        ~ScriptAssemblyCandidate();
    private:
        ScriptAssemblyCandidate();
        Scope<ScriptEngineData> m_Data;
        uint64_t m_Generation=0;
        friend class ScriptEngine;
    };
    class RuntimeSession;


	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);

		MonoObject* Instantiate();
		MonoMethod* GetMethod(const std::string& name, int parameterCount);
		MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

		const std::map<std::string, ScriptField>& GetFields() const { return m_Fields; }
	private:
		std::string m_ClassNamespace;
		std::string m_ClassName;

		std::map<std::string, ScriptField> m_Fields;

		MonoClass* m_MonoClass = nullptr;

		friend class ScriptEngine;
	};

	class ScriptInstance
	{
	public:
		ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);
		~ScriptInstance();

		void InvokeOnCreate();
        void InvokeOnDestroy();
		void InvokeOnUpdate(float ts);

		Ref<ScriptClass> GetScriptClass() { return m_ScriptClass; }

		template<typename T>
		T GetFieldValue(const std::string& name)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");

			alignas(std::max_align_t) uint8_t buffer[16]{};
			bool success = GetFieldValueInternal(name, buffer);
			if (!success)
				return T();

			T value{};
			std::memcpy(&value, buffer, sizeof(T));
			return value;
		}

		template<typename T>
		void SetFieldValue(const std::string& name, T value)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");

			SetFieldValueInternal(name, &value);
		}

		MonoObject* GetManagedObject();
	private:
		void Invalidate();
		bool GetFieldValueInternal(const std::string& name, void* buffer);
		bool SetFieldValueInternal(const std::string& name, const void* value);
	private:
		Ref<ScriptClass> m_ScriptClass;

		uint32_t m_GCHandle = 0;
		MonoMethod* m_Constructor = nullptr;
		MonoMethod* m_OnCreateMethod = nullptr;
		MonoMethod* m_OnUpdateMethod = nullptr;
        MonoMethod* m_OnDestroyMethod = nullptr;
        bool m_Created = false;


		friend class ScriptEngine;
		friend struct ScriptFieldInstance;
	};

	class ScriptEngine
	{
	public:
		static void Init(const std::filesystem::path& applicationAssembly = {}, const std::function<void()>& beforeReplacement = {});
        static Scope<ScriptAssemblyCandidate> StageAssembly(const std::filesystem::path& applicationAssembly);
        static void CommitAssembly(Scope<ScriptAssemblyCandidate> candidate,const std::function<void()>& beforeReplacement = {});
		static void Shutdown();
        // Retire project metadata/watcher while keeping Mono's root reusable.
        static void ClearApplicationAssembly();
		static bool IsInitialized();

		static bool LoadAssembly(const std::filesystem::path& filepath);
		static bool LoadAppAssembly(const std::filesystem::path& filepath);

		static void ReloadAssembly();

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static bool EntityClassExists(const std::string& fullClassName);
		static void OnCreateEntity(Entity entity);
		static void OnDestroyEntity(UUID entityID);
		static void OnUpdateEntity(Entity entity, Timestep ts);

		static Scene* GetSceneContext();
        // Borrowed until session Stop; never retained by managed code.
        static RuntimeSession* GetRuntimeSession();

		static Ref<ScriptInstance> GetEntityScriptInstance(UUID entityID);

		static Ref<ScriptClass> GetEntityClass(const std::string& name);
		static std::unordered_map<std::string, Ref<ScriptClass>> GetEntityClasses();
		static ScriptFieldMap& GetScriptFieldMap(Entity entity);

		static MonoImage* GetCoreAssemblyImage();

		static MonoObject* GetManagedInstance(UUID uuid);

		static MonoString* CreateString(const char* string);
	private:
		static void SetRuntimeSession(RuntimeSession* session);
        friend class RuntimeSession;
        static void ReleaseDomainMetadata();
		static void InitMono();
		static void ShutdownMono();

		static MonoObject* InstantiateClass(MonoClass* monoClass);
		static void LoadAssemblyClasses(ScriptEngineData& data);
		static Scope<ScriptEngineData> PrepareDomain(const std::filesystem::path& core, const std::filesystem::path& app);
		static void ReplaceAssembly(const std::filesystem::path& core, const std::filesystem::path& app, bool preserveRuntime, const std::function<void()>& beforeReplacement = {});

		friend class ScriptClass;
		friend class ScriptGlue;
	};

	namespace Utils {

		inline const char* ScriptFieldTypeToString(ScriptFieldType fieldType)
		{
			switch (fieldType)
			{
				case ScriptFieldType::None:    return "None";
				case ScriptFieldType::Float:   return "Float";
				case ScriptFieldType::Double:  return "Double";
				case ScriptFieldType::Bool:    return "Bool";
				case ScriptFieldType::Char:    return "Char";
				case ScriptFieldType::Byte:    return "Byte";
				case ScriptFieldType::Short:   return "Short";
				case ScriptFieldType::Int:     return "Int";
				case ScriptFieldType::Long:    return "Long";
				case ScriptFieldType::UByte:   return "UByte";
				case ScriptFieldType::UShort:  return "UShort";
				case ScriptFieldType::UInt:    return "UInt";
				case ScriptFieldType::ULong:   return "ULong";
				case ScriptFieldType::Vector2: return "Vector2";
				case ScriptFieldType::Vector3: return "Vector3";
				case ScriptFieldType::Vector4: return "Vector4";
				case ScriptFieldType::Entity:  return "Entity";
                case ScriptFieldType::Prefab: return "Prefab";
                case ScriptFieldType::Sprite: return "Sprite";
                case ScriptFieldType::SpriteAnimation: return "SpriteAnimation";
			}
			HZ_CORE_ASSERT(false, "Unknown ScriptFieldType");
			return "None";
		}

		inline ScriptFieldType ScriptFieldTypeFromString(std::string_view fieldType)
		{
			if (fieldType == "None")    return ScriptFieldType::None;
			if (fieldType == "Float")   return ScriptFieldType::Float;
			if (fieldType == "Double")  return ScriptFieldType::Double;
			if (fieldType == "Bool")    return ScriptFieldType::Bool;
			if (fieldType == "Char")    return ScriptFieldType::Char;
			if (fieldType == "Byte")    return ScriptFieldType::Byte;
			if (fieldType == "Short")   return ScriptFieldType::Short;
			if (fieldType == "Int")     return ScriptFieldType::Int;
			if (fieldType == "Long")    return ScriptFieldType::Long;
			if (fieldType == "UByte")   return ScriptFieldType::UByte;
			if (fieldType == "UShort")  return ScriptFieldType::UShort;
			if (fieldType == "UInt")    return ScriptFieldType::UInt;
			if (fieldType == "ULong")   return ScriptFieldType::ULong;
			if (fieldType == "Vector2") return ScriptFieldType::Vector2;
			if (fieldType == "Vector3") return ScriptFieldType::Vector3;
			if (fieldType == "Vector4") return ScriptFieldType::Vector4;
			if (fieldType == "Entity")  return ScriptFieldType::Entity;
            if (fieldType == "Prefab") return ScriptFieldType::Prefab;
            if (fieldType == "Sprite") return ScriptFieldType::Sprite;
            if (fieldType == "SpriteAnimation") return ScriptFieldType::SpriteAnimation;

			throw std::invalid_argument("Unknown stored ScriptFieldType: " + std::string(fieldType));
		}

	}

}
