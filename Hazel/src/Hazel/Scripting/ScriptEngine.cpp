#include "hzpch.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Core/Resources.h"
#include "ScriptEngine.h"

#include "ScriptGlue.h"

#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/object.h"
#include "mono/metadata/attrdefs.h"
#include "mono/metadata/tokentype.h"
#include "mono/metadata/row-indexes.h"
#include "mono/metadata/mono-debug.h"
#include "mono/metadata/threads.h"

#include "Hazel/Utils/FileWatcher.h"
#include "mono/metadata/mono-config.h"
#include <atomic>
#include <limits>
#include <stdexcept>

#include "Hazel/Core/Application.h"
#include "Hazel/Core/Timer.h"
#include "Hazel/Core/Buffer.h"
#include "Hazel/Core/FileSystem.h"

#include "Hazel/Project/Project.h"

namespace Hazel {
    static RuntimeSession* s_RuntimeSession = nullptr;
    static const std::thread::id s_RuntimeThread = std::this_thread::get_id();
    RuntimeSession* ScriptEngine::GetRuntimeSession() {
        // Reject managed worker requests before observing the borrowed owner.
        return std::this_thread::get_id() == s_RuntimeThread ? s_RuntimeSession : nullptr;
    }
    void ScriptEngine::SetRuntimeSession(RuntimeSession* session) {
        if (std::this_thread::get_id() != s_RuntimeThread) throw std::logic_error("Runtime session binding requires the main thread");
        s_RuntimeSession = session;
    }

	static std::unordered_map<std::string, ScriptFieldType> s_ScriptFieldTypeMap =
	{
		{ "System.Single", ScriptFieldType::Float },
		{ "System.Double", ScriptFieldType::Double },
		{ "System.Boolean", ScriptFieldType::Bool },
		{ "System.Char", ScriptFieldType::Char },
		{ "System.Int16", ScriptFieldType::Short },
		{ "System.Int32", ScriptFieldType::Int },
		{ "System.Int64", ScriptFieldType::Long },
		{ "System.SByte", ScriptFieldType::Byte },
		{ "System.Byte", ScriptFieldType::UByte },
		{ "System.UInt16", ScriptFieldType::UShort },
		{ "System.UInt32", ScriptFieldType::UInt },
		{ "System.UInt64", ScriptFieldType::ULong },

		{ "Hazel.Vector2", ScriptFieldType::Vector2 },
		{ "Hazel.Vector3", ScriptFieldType::Vector3 },
		{ "Hazel.Vector4", ScriptFieldType::Vector4 },

		{ "Hazel.Entity", ScriptFieldType::Entity },
        { "Hazel.Prefab", ScriptFieldType::Prefab },
	};

	namespace Utils {

		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath, bool loadPDB = false, ScopedBuffer* snapshot = nullptr)
		{
			ScopedBuffer owned(snapshot ? Buffer{} : FileSystem::ReadFileBinary(assemblyPath));
			ScopedBuffer& fileData = snapshot ? *snapshot : owned;
			if (!fileData || fileData.Size() > std::numeric_limits<uint32_t>::max()) {
				HZ_CORE_ERROR("Cannot read managed assembly {}", assemblyPath.generic_u8string());
				return nullptr;
			}

			// NOTE: We can't use this image for anything other than loading the assembly because this image doesn't have a reference to the assembly
			MonoImageOpenStatus status;
			MonoImage* image = mono_image_open_from_data_full(fileData.As<char>(), static_cast<uint32_t>(fileData.Size()), 1, &status, 0);

			if (status != MONO_IMAGE_OK)
			{
				const char* errorMessage = mono_image_strerror(status);
				HZ_CORE_ERROR("Managed assembly {}: {}", assemblyPath.generic_u8string(), errorMessage);
				return nullptr;
			}

			if (loadPDB)
			{
				std::filesystem::path pdbPath = assemblyPath;
				pdbPath.replace_extension(".pdb");

				if (std::filesystem::exists(pdbPath))
				{
					ScopedBuffer pdbFileData = FileSystem::ReadFileBinary(pdbPath);
					mono_debug_open_image_from_memory(image, pdbFileData.As<const mono_byte>(), pdbFileData.Size());
					HZ_CORE_INFO("Loaded PDB {}", pdbPath.generic_u8string());
				}
			}

			std::string pathString = assemblyPath.generic_u8string();
			MonoAssembly* assembly = mono_assembly_load_from_full(image, pathString.c_str(), &status, 0);
			mono_image_close(image);

			return assembly;
		}

		void PrintAssemblyTypes(MonoAssembly* assembly)
		{
			MonoImage* image = mono_assembly_get_image(assembly);
			const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
			int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

			for (int32_t i = 0; i < numTypes; i++)
			{
				uint32_t cols[MONO_TYPEDEF_SIZE];
				mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

				const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
				const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);
				HZ_CORE_TRACE("{}.{}", nameSpace, name);
			}
		}

		ScriptFieldType MonoTypeToScriptFieldType(MonoType* monoType)
		{
			char* allocatedName = mono_type_get_name(monoType);
			std::string typeName = allocatedName;
			mono_free(allocatedName);

			auto it = s_ScriptFieldTypeMap.find(typeName);
			if (it == s_ScriptFieldTypeMap.end())
			{
				HZ_CORE_ERROR("Unknown type: {}", typeName);
				return ScriptFieldType::None;
			}

			return it->second;
		}

	}

	struct ScriptEngineData
	{
		~ScriptEngineData() {
			ShuttingDown = true;
			AppAssemblyFileWatcher.reset();
			if (AppDomain) {
				auto* previous = mono_domain_get();
				mono_domain_set(RootDomain, false);
				mono_domain_unload(AppDomain);
				if (previous != AppDomain) mono_domain_set(previous, true);
			}
		}
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;

		MonoAssembly* AppAssembly = nullptr;
		MonoImage* AppAssemblyImage = nullptr;

		std::filesystem::path CoreAssemblyFilepath;
		std::filesystem::path AppAssemblyFilepath;

		ScriptClass EntityClass;

		std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;
		std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
		std::unordered_map<UUID, ScriptFieldMap> ReloadFields; // Live snapshots, never editor defaults.

		Scope<FileWatcher> AppAssemblyFileWatcher;
		std::atomic_bool AssemblyReloadPending{false};
		std::atomic_bool ShuttingDown{false};
		uint64_t Generation = 0;

#ifdef HZ_DEBUG
		bool EnableDebugging = true;
#else
		bool EnableDebugging = false;
#endif
		bool Initialized = false;
		// Runtime

		Scene* SceneContext = nullptr;
	};

	static Scope<ScriptEngineData> s_Data;
	static uint64_t s_Generation = 0;

	static void OnAppAssemblyFileSystemEvent(const std::filesystem::path&, FileWatchEvent change_type)
	{
		if (!s_Data || s_Data->ShuttingDown) return;
		if (change_type != FileWatchEvent::modified && change_type != FileWatchEvent::added && change_type != FileWatchEvent::renamed_new) return;
		if (s_Data->AssemblyReloadPending.exchange(true)) return;
		const auto generation = s_Data->Generation;
		if (auto* application = Application::TryGet())
			application->SubmitToMainThread([generation]() {
				if (!s_Data || s_Data->Generation != generation || s_Data->ShuttingDown) return;
				try { ScriptEngine::ReloadAssembly(); }
				catch (const std::runtime_error& error) { HZ_CORE_ERROR("Assembly reload: {}", error.what()); }
				s_Data->AssemblyReloadPending = false;
			});
		else s_Data->AssemblyReloadPending = false; // Hosts without an application explicitly call ReloadAssembly.
	}

	void ScriptEngine::Init(const std::filesystem::path& applicationAssembly, const std::function<void()>& beforeReplacement)
	{
		auto appPath = applicationAssembly;
		if (appPath.empty()) {
			if (!Project::GetActive()) throw std::logic_error("Scripting requires an active project");
			appPath = Project::GetAssetFileSystemPath(Project::GetActive()->GetConfig().ScriptModulePath);
		}
		if (!s_Data) {
			s_Data = CreateScope<ScriptEngineData>();
			s_Data->Generation = ++s_Generation;
		}
		if (!s_Data->RootDomain) { InitMono(); ScriptGlue::RegisterFunctions(); }
		ReplaceAssembly(Resources::Resolve("Scripts/Hazel-ScriptCore.dll"), appPath, false, beforeReplacement);
	}

	void ScriptEngine::Shutdown()
	{
        if (auto* session = GetRuntimeSession()) session->Stop();
		if (!s_Data) return;
		s_Data->ShuttingDown = true;
		s_Data->AppAssemblyFileWatcher.reset();
		ReleaseDomainMetadata();
		ShutdownMono();
		s_Data.reset();
	}
	bool ScriptEngine::IsInitialized() { return s_Data && s_Data->Initialized; }

	void ScriptEngine::InitMono()
	{
		const auto root = Resources::Get().MonoRoot;
        const auto assemblies = (root / "lib").generic_u8string();
        const auto config = (root / "etc/mono/config").generic_u8string();
        if (!std::filesystem::is_regular_file(root / "lib/mono/4.5/mscorlib.dll") ||
            !std::filesystem::is_regular_file(root / "etc/mono/config"))
            throw std::runtime_error("Incomplete Mono runtime at " + root.generic_u8string());
        mono_set_dirs(assemblies.c_str(), (root / "etc").generic_u8string().c_str());
        mono_set_assemblies_path(assemblies.c_str());
        mono_config_parse(config.c_str());

		if (s_Data->EnableDebugging)
		{
			const char* argv[2] = {
				// Mono's debugger owns its logfile until process exit. Use its
				// supported stdout sink so project directories remain removable
				// after engine shutdown, while preserving debugger diagnostics.
				"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3",
				"--soft-breakpoints"
			};

			mono_jit_parse_options(2, (char**)argv);
			mono_debug_init(MONO_DEBUG_FORMAT_MONO);
		}

		MonoDomain* rootDomain = mono_jit_init("HazelJITRuntime");
		if (!rootDomain) throw std::runtime_error("Could not initialize Mono JIT runtime");

		// Store the root domain pointer
		s_Data->RootDomain = rootDomain;

		if (s_Data->EnableDebugging)
			mono_debug_domain_create(s_Data->RootDomain);

		mono_thread_set_main(mono_thread_current());
	}

	void ScriptEngine::ShutdownMono()
	{
		if (!s_Data->RootDomain) return;
		mono_domain_set(mono_get_root_domain(), false);

		if (s_Data->AppDomain) mono_domain_unload(s_Data->AppDomain);
		s_Data->AppDomain = nullptr;

		mono_jit_cleanup(s_Data->RootDomain);
		s_Data->RootDomain = nullptr;
	}

	bool ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		// Create an App Domain
		s_Data->AppDomain = mono_domain_create_appdomain(const_cast<char*>("HazelScriptRuntime"), nullptr);
		if (!s_Data->AppDomain) throw std::runtime_error("Could not create script domain");
		mono_domain_set(s_Data->AppDomain, true);

		s_Data->CoreAssemblyFilepath = filepath;
		s_Data->CoreAssembly = Utils::LoadMonoAssembly(filepath, s_Data->EnableDebugging);
		if (s_Data->CoreAssembly == nullptr)
			return false;

		s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
		return true;
	}

	bool ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
	{
		s_Data->AppAssemblyFilepath = filepath;
		s_Data->AppAssembly = Utils::LoadMonoAssembly(filepath, s_Data->EnableDebugging);
		if (s_Data->AppAssembly == nullptr)
			return false;

		s_Data->AppAssemblyImage = mono_assembly_get_image(s_Data->AppAssembly);

		s_Data->AppAssemblyFileWatcher = FileWatcher::Create(filepath, OnAppAssemblyFileSystemEvent);
		s_Data->AssemblyReloadPending = false;
		return true;
	}

	Scope<ScriptEngineData> ScriptEngine::PrepareDomain(const std::filesystem::path& corePath, const std::filesystem::path& appPath)
	{
		ScopedBuffer core(FileSystem::ReadFileBinary(corePath)), app(FileSystem::ReadFileBinary(appPath));
		if (!core || !app) throw std::runtime_error("Missing or empty script assembly: " + corePath.generic_u8string() + " / " + appPath.generic_u8string());
		auto candidate = CreateScope<ScriptEngineData>();
		candidate->RootDomain = s_Data->RootDomain;
		candidate->EnableDebugging = s_Data->EnableDebugging;
		candidate->CoreAssemblyFilepath = corePath;
		candidate->AppAssemblyFilepath = appPath;
		candidate->Generation = ++s_Generation;
		candidate->ShuttingDown = true; // Candidate watcher cannot publish into the old session.
		MonoDomain* previous = mono_domain_get();
		try {
			candidate->AppDomain = mono_domain_create_appdomain(const_cast<char*>("HazelScriptRuntime"), nullptr);
			if (!candidate->AppDomain) throw std::runtime_error("Could not create candidate script domain");
			mono_domain_set(candidate->AppDomain, true);
			candidate->CoreAssembly = Utils::LoadMonoAssembly(corePath, candidate->EnableDebugging, &core);
			candidate->AppAssembly = Utils::LoadMonoAssembly(appPath, candidate->EnableDebugging, &app);
			if (!candidate->CoreAssembly || !candidate->AppAssembly) throw std::runtime_error("Invalid script assembly: " + corePath.generic_u8string() + " / " + appPath.generic_u8string());
			candidate->CoreAssemblyImage = mono_assembly_get_image(candidate->CoreAssembly);
			candidate->AppAssemblyImage = mono_assembly_get_image(candidate->AppAssembly);
			LoadAssemblyClasses(*candidate);
			ScriptGlue::ValidateComponents(candidate->CoreAssemblyImage);
			candidate->EntityClass.m_ClassNamespace = "Hazel";
			candidate->EntityClass.m_ClassName = "Entity";
			candidate->EntityClass.m_MonoClass = mono_class_from_name(candidate->CoreAssemblyImage, "Hazel", "Entity");
			if (!candidate->EntityClass.GetMethod(".ctor", 1)) throw std::runtime_error("ScriptCore is missing Hazel.Entity(ulong)");
			auto* owner = candidate.get();
			candidate->AppAssemblyFileWatcher = FileWatcher::Create(appPath, [owner](const auto& path, auto event) {
				if (!owner->ShuttingDown) OnAppAssemblyFileSystemEvent(path, event);
			});
			candidate->Initialized = true;
			mono_domain_set(previous, true);
			return candidate;
		} catch (...) {
			candidate->AppAssemblyFileWatcher.reset();
			mono_domain_set(s_Data->RootDomain, false);
			if (candidate->AppDomain) mono_domain_unload(candidate->AppDomain);
			candidate->AppDomain = nullptr;
			mono_domain_set(previous, true);
			throw;
		}
	}

	void ScriptEngine::ReplaceAssembly(const std::filesystem::path& core, const std::filesystem::path& app, bool preserveRuntime, const std::function<void()>& beforeReplacement)
	{
		// Fully load/reflect/validate and reserve the watcher before touching the old domain.
		auto candidate = PrepareDomain(core, app);
		Scene* scene = preserveRuntime ? s_Data->SceneContext : nullptr;
		candidate->SceneContext = scene;
		if (scene) {
			candidate->ReloadFields = s_Data->ReloadFields;
			for (auto& [id, instance] : s_Data->EntityInstances)
				for (const auto& [name, field] : instance->GetScriptClass()->GetFields()) {
					if (field.Type == ScriptFieldType::None) continue;
					auto& saved = candidate->ReloadFields[id][name];
					saved.Field = { field.Type, name, nullptr };
					instance->GetFieldValueInternal(name, field.Type==ScriptFieldType::Prefab ? static_cast<void*>(&saved.AssetReference) : saved.m_Buffer);
				}
		}
		// Project replacement stops the old scene only after validation, while its domain is still alive.
		if (beforeReplacement) beforeReplacement();
		s_Data->ShuttingDown = true;
		s_Data->AppAssemblyFileWatcher.reset(); // Join before changing the global owner/metadata.
		if(scene) { scene->m_Stopping=true; scene->CancelPendingLifecycle(); OnRuntimeStop(); scene->CancelPendingLifecycle(); candidate->SceneContext=scene; }
        ReleaseDomainMetadata();
		mono_domain_set(s_Data->RootDomain, false);
		if (s_Data->AppDomain) mono_domain_unload(s_Data->AppDomain);
		s_Data->AppDomain = nullptr;
		s_Data = std::move(candidate);
		mono_domain_set(s_Data->AppDomain, true);
		ScriptGlue::RegisterComponents(); // These types were validated in PrepareDomain.
		s_Data->ShuttingDown = false;
		if (scene) {
            scene->m_Stopping=false;
            std::vector<UUID> ids; for(auto e:scene->GetAllEntitiesWith<ScriptComponent>()) ids.push_back(Entity{e,scene}.GetUUID());
            for(auto id:ids) if(scene->IsEntityValid(id)) OnCreateEntity(scene->GetEntityByUUID(id));
        }
	}

	void ScriptEngine::ReloadAssembly()
	{
		if (!IsInitialized()) throw std::runtime_error("Open a valid project before reloading scripts");
		ReplaceAssembly(s_Data->CoreAssemblyFilepath, s_Data->AppAssemblyFilepath, true);
	}

	void ScriptEngine::OnRuntimeStart(Scene* scene)
	{
		if (s_Data) s_Data->SceneContext = scene;
	}

	bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
	{
		return s_Data && s_Data->EntityClasses.find(fullClassName) != s_Data->EntityClasses.end();
	}

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		if (ScriptEngine::EntityClassExists(sc.ClassName))
		{
			UUID entityID = entity.GetUUID();
            if(s_Data->EntityInstances.count(entityID)) return;

			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(s_Data->EntityClasses[sc.ClassName], entity);
			s_Data->EntityInstances[entityID] = instance;

			// Authored values belong to this scene; live reload snapshots are separate.
			ScriptFieldMap fieldMap = GetScriptFieldMap(entity);
			if (auto snapshots = s_Data->ReloadFields.find(entityID); snapshots != s_Data->ReloadFields.end())
				for (const auto& [name, field] : snapshots->second) fieldMap[name] = field;
			for (const auto& [name, fieldInstance] : fieldMap)
				if (auto known = instance->GetScriptClass()->GetFields().find(name);
					known != instance->GetScriptClass()->GetFields().end() && known->second.Type == fieldInstance.Field.Type)
					instance->SetFieldValueInternal(name, fieldInstance.Field.Type==ScriptFieldType::Prefab ? static_cast<const void*>(&fieldInstance.AssetReference) : fieldInstance.m_Buffer);

			instance->InvokeOnCreate();
		}
	}

	void ScriptEngine::OnDestroyEntity(UUID entityID)
	{
		if (!s_Data) return;
		s_Data->ReloadFields.erase(entityID);
		auto instance = s_Data->EntityInstances.find(entityID);
		if (instance != s_Data->EntityInstances.end()) {
			auto owned=instance->second; s_Data->EntityInstances.erase(instance);
            owned->InvokeOnDestroy(); owned->Invalidate();
		}
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
	{
		UUID entityUUID = entity.GetUUID();
		if (s_Data->EntityInstances.find(entityUUID) != s_Data->EntityInstances.end())
		{
			Ref<ScriptInstance> instance = s_Data->EntityInstances[entityUUID];
			instance->InvokeOnUpdate((float)ts);
		}
		else
		{
			HZ_CORE_ERROR("Could not find ScriptInstance for entity {}", static_cast<uint64_t>(entityUUID));
		}
	}

	Scene* ScriptEngine::GetSceneContext()
	{
		return std::this_thread::get_id()==s_RuntimeThread && s_Data ? s_Data->SceneContext : nullptr;
	}

	Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
	{
		if (!s_Data) return nullptr;
		auto it = s_Data->EntityInstances.find(entityID);
		if (it == s_Data->EntityInstances.end())
			return nullptr;

		return it->second;
	}


	Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& name)
	{
		if (!s_Data) return nullptr;
		if (s_Data->EntityClasses.find(name) == s_Data->EntityClasses.end())
			return nullptr;

		return s_Data->EntityClasses.at(name);
	}

	void ScriptEngine::OnRuntimeStop()
	{
		if (!s_Data) return;
		std::vector<UUID> ids; for(auto& [id,instance]:s_Data->EntityInstances) ids.push_back(id);
        for(auto id:ids) OnDestroyEntity(id);
        s_Data->SceneContext = nullptr;
		s_Data->EntityInstances.clear();
		s_Data->ReloadFields.clear();
	}

	std::unordered_map<std::string, Ref<ScriptClass>> ScriptEngine::GetEntityClasses()
	{
		return s_Data ? s_Data->EntityClasses : std::unordered_map<std::string, Ref<ScriptClass>>{};
	}

	ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entity)
	{
		if (!entity) throw std::logic_error("Script fields require a valid entity");

		UUID entityID = entity.GetUUID();
		return entity.m_Scene->m_ScriptFields[entityID];
	}

	void ScriptEngine::LoadAssemblyClasses(ScriptEngineData& data)
	{
		data.EntityClasses.clear();

		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(data.AppAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);
		MonoClass* entityClass = mono_class_from_name(data.CoreAssemblyImage, "Hazel", "Entity");
		if (!entityClass) throw std::runtime_error("ScriptCore is missing Hazel.Entity");

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(data.AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* className = mono_metadata_string_heap(data.AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);
			std::string fullName;
			if (strlen(nameSpace) != 0)
				fullName = fmt::format("{}.{}", nameSpace, className);
			else
				fullName = className;

			// TypeDef rows include nested/compiler-generated classes which cannot
            // be resolved by namespace/name alone. Resolve the actual metadata token.
            MonoClass* monoClass = mono_class_get(data.AppAssemblyImage, MONO_TOKEN_TYPE_DEF | (i + 1));
            if (!monoClass) throw std::runtime_error("Unable to resolve managed TypeDef: " + fullName);

			if (monoClass == entityClass)
				continue;

			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
			if (!isEntity)
				continue;

			Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>();
			scriptClass->m_ClassNamespace = nameSpace; scriptClass->m_ClassName = className;
			scriptClass->m_MonoClass = monoClass;
			data.EntityClasses[fullName] = scriptClass;


			// This routine is an iterator routine for retrieving the fields in a class.
			// You must pass a gpointer that points to zero and is treated as an opaque handle
			// to iterate over all of the elements. When no more values are available, the return value is NULL.

			int fieldCount = mono_class_num_fields(monoClass);
			HZ_CORE_TRACE("{} has {} fields:", className, fieldCount);
			void* iterator = nullptr;
			while (MonoClassField* field = mono_class_get_fields(monoClass, &iterator))
			{
				const char* fieldName = mono_field_get_name(field);
				uint32_t flags = mono_field_get_flags(field);
				if ((flags & MONO_FIELD_ATTR_FIELD_ACCESS_MASK) == MONO_FIELD_ATTR_PUBLIC && !(flags & MONO_FIELD_ATTR_STATIC))
				{
					MonoType* type = mono_field_get_type(field);
					ScriptFieldType fieldType = Utils::MonoTypeToScriptFieldType(type);
					HZ_CORE_TRACE("  {} ({})", fieldName, Utils::ScriptFieldTypeToString(fieldType));

					scriptClass->m_Fields[fieldName] = { fieldType, fieldName, field };
				}
			}

		}


	}

	MonoImage* ScriptEngine::GetCoreAssemblyImage()
	{
		return s_Data ? s_Data->CoreAssemblyImage : nullptr;
	}


	MonoObject* ScriptEngine::GetManagedInstance(UUID uuid)
	{
		HZ_CORE_ASSERT(s_Data->EntityInstances.find(uuid) != s_Data->EntityInstances.end());
		return s_Data->EntityInstances.at(uuid)->GetManagedObject();
	}

	MonoString* ScriptEngine::CreateString(const char* string)
	{
		return mono_string_new(s_Data->AppDomain, string);
	}

	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
		: m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		m_MonoClass = mono_class_from_name(isCore ? s_Data->CoreAssemblyImage : s_Data->AppAssemblyImage, classNamespace.c_str(), className.c_str());
	}

	MonoObject* ScriptClass::Instantiate()
	{
		return ScriptEngine::InstantiateClass(m_MonoClass);
	}

	MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
	{
		return m_MonoClass ? mono_class_get_method_from_name(m_MonoClass, name.c_str(), parameterCount) : nullptr;
	}

	MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
	{
		if (!method || !m_MonoClass) throw std::logic_error("Managed method/class is no longer valid");
		MonoObject* exception = nullptr;
		auto* result = mono_runtime_invoke(method, instance, params, &exception);
		if (exception) {
			// Report the original exception without invoking another managed method
			// (ToString can itself throw). Read Exception._message through its base.
			auto* type = mono_object_get_class(exception);
			MonoString* message = nullptr;
			for (auto* base = type; base && !message; base = mono_class_get_parent(base))
				if (auto* field = mono_class_get_field_from_name(base, "_message")) mono_field_get_value(exception, field, &message);
			char* text = message ? mono_string_to_utf8(message) : nullptr;
			HZ_CORE_ERROR("Managed exception {}.{}: {}", mono_class_get_namespace(type), mono_class_get_name(type), text ? text : "(no message)");
			mono_free(text);
		}
		return result;
	}

	ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
		: m_ScriptClass(scriptClass)
	{
		m_GCHandle = mono_gchandle_new(scriptClass->Instantiate(), true);

		m_Constructor = s_Data->EntityClass.GetMethod(".ctor", 1);
		m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
		m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);
        m_OnDestroyMethod = scriptClass->GetMethod("OnDestroy", 0);

		// Call Entity constructor
		{
			UUID entityID = entity.GetUUID();
			void* param = &entityID;
			m_ScriptClass->InvokeMethod(GetManagedObject(), m_Constructor, &param);
		}
	}

	ScriptInstance::~ScriptInstance() { Invalidate(); }
	void ScriptInstance::Invalidate()
	{
		if (m_GCHandle) mono_gchandle_free(m_GCHandle);
		m_GCHandle = 0;
		m_ScriptClass.reset();
		m_Constructor = m_OnCreateMethod = m_OnUpdateMethod = m_OnDestroyMethod = nullptr;
	}
	MonoObject* ScriptInstance::GetManagedObject() { return m_GCHandle ? mono_gchandle_get_target(m_GCHandle) : nullptr; }

	void ScriptEngine::ReleaseDomainMetadata()
	{
		s_Data->Initialized = false;
		for (auto& [id, instance] : s_Data->EntityInstances) instance->Invalidate();
		s_Data->EntityInstances.clear();
		for (auto& [name, type] : s_Data->EntityClasses) {
			type->m_MonoClass = nullptr;
			type->m_Fields.clear();
		}
		s_Data->EntityClasses.clear();
		s_Data->EntityClass = {};
		for (auto& [id, fields] : s_Data->ReloadFields)
			for (auto& [name, field] : fields) field.Field.ClassField = nullptr;
	}

	void ScriptInstance::InvokeOnCreate()
	{
        m_Created=true;
		if (m_OnCreateMethod)
			m_ScriptClass->InvokeMethod(GetManagedObject(), m_OnCreateMethod);
	}

    void ScriptInstance::InvokeOnDestroy() {
        if(!m_Created) return; m_Created=false;
        if(m_OnDestroyMethod && GetManagedObject()) m_ScriptClass->InvokeMethod(GetManagedObject(),m_OnDestroyMethod);
    }

	void ScriptInstance::InvokeOnUpdate(float ts)
	{
		if (m_OnUpdateMethod)
		{
			void* param = &ts;
			m_ScriptClass->InvokeMethod(GetManagedObject(), m_OnUpdateMethod, &param);
		}
	}

	bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* buffer)
	{
		if (!m_GCHandle || !m_ScriptClass) return false;
		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		if(field.Type==ScriptFieldType::Prefab) {
            MonoObject* reference=nullptr; mono_field_get_value(GetManagedObject(),field.ClassField,&reference);
            auto& path=*static_cast<std::string*>(buffer); path.clear();
            if(reference) {
                auto* type=mono_class_from_name(s_Data->CoreAssemblyImage,"Hazel","Prefab"); MonoString* text=nullptr;
                mono_field_get_value(reference,mono_class_get_field_from_name(type,"Path"),&text);
                if(text) { auto* utf8=mono_string_to_utf8(text); path=utf8; mono_free(utf8); }
            }
        } else if (field.Type == ScriptFieldType::Entity) {
            MonoObject* reference = nullptr;
			mono_field_get_value(GetManagedObject(), field.ClassField, &reference);
			uint64_t id = 0;
			if (reference) {
				MonoClass* type = mono_class_from_name(s_Data->CoreAssemblyImage, "Hazel", "Entity");
				mono_field_get_value(reference, mono_class_get_field_from_name(type, "ID"), &id);
			}
			std::memcpy(buffer, &id, sizeof(id));
		} else mono_field_get_value(GetManagedObject(), field.ClassField, buffer);
		return true;
	}

	bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
	{
		if (!m_GCHandle || !m_ScriptClass) return false;
		const auto& fields = m_ScriptClass->GetFields();
		auto it = fields.find(name);
		if (it == fields.end())
			return false;

		const ScriptField& field = it->second;
		if(field.Type==ScriptFieldType::Prefab) {
            const auto& path=*static_cast<const std::string*>(value);
            auto* type=mono_class_from_name(s_Data->CoreAssemblyImage,"Hazel","Prefab");
            auto* reference=mono_object_new(s_Data->AppDomain,type); auto* text=ScriptEngine::CreateString(path.c_str());
            void* parameters[]{text}; MonoObject* exception=nullptr;
            mono_runtime_invoke(mono_class_get_method_from_name(type,".ctor",1),reference,parameters,&exception);
            if(exception) return false;
            mono_field_set_value(GetManagedObject(),field.ClassField,reference);
        } else if (field.Type == ScriptFieldType::Entity) {
            uint64_t id = 0; std::memcpy(&id, value, sizeof(id));
			MonoObject* reference = nullptr;
			if (id) {
				if (auto instance = ScriptEngine::GetEntityScriptInstance(id)) reference = instance->GetManagedObject();
				else {
					reference = s_Data->EntityClass.Instantiate();
					void* arguments[]{ &id };
					s_Data->EntityClass.InvokeMethod(reference, s_Data->EntityClass.GetMethod(".ctor", 1), arguments);
				}
			}
			// Mono's published embedding sample passes reference pointers directly.
			mono_field_set_value(GetManagedObject(), field.ClassField, reference);
		} else mono_field_set_value(GetManagedObject(), field.ClassField, const_cast<void*>(value));
		return true;
	}

}
