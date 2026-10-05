#pragma once

#include "Hazel/Core/Timestep.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Renderer/EditorCamera.h"
#include "Hazel/Scripting/ScriptField.h"

#include "entt.hpp"
#include <unordered_set>
#include <vector>
#include "Components.h"

class b2World;

namespace Hazel {

	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(Ref<Scene> other);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);
        Entity InstantiateEntity(Entity source, const TransformComponent& transform);
        bool IsEntityValid(UUID id) const;
        void CancelPendingLifecycle();

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		void OnViewportResize(uint32_t width, uint32_t height);

		Entity DuplicateEntity(Entity entity);

		Entity FindEntityByName(std::string_view name);
		Entity GetEntityByUUID(UUID uuid);

		Entity GetPrimaryCameraEntity();
		void SetAssets(const Ref<ProjectAssets>& assets) {m_Assets=assets;}
		const Ref<ProjectAssets>& GetAssets() const {return m_Assets;}
		void PrepareSprites(bool strict = false);
		void ValidateSprites();
		bool PlayAnimation(Entity entity,const AnimationReference& reference);
		const ResolvedSprite* RenderedSprite(Entity entity) const;

		uint64_t GetIdentity() const { return m_Identity; }
        const std::string& GetName() const { return m_Name; }
        void SetName(std::string name) { m_Name=std::move(name); }
        bool IsStopping() const { return m_Stopping; }

		bool IsRunning() const { return m_IsRunning; }
		bool IsPaused() const { return m_IsPaused; }

		void SetPaused(bool paused) { m_IsPaused = paused; }

		void Step(int frames = 1);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
		template<typename T> void OnComponentRemoving(Entity entity);
		void DestroyNativeScript(Entity entity);
        void DestroyEntityNow(Entity entity);
        void FlushLifecycle();
		void DestroyPhysicsBody(Entity entity);
		void DestroyPhysicsFixture(Entity entity, bool circle);

		void OnPhysics2DStart();
		void OnPhysics2DStop();
		void SynchronizePhysics2D();

		void RenderScene(EditorCamera& camera);
		void AdvanceAnimations(double timestep);
	private:
		entt::registry m_Registry;
		Ref<ProjectAssets> m_Assets;
        UUID m_Identity;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
        std::string m_Name = "Untitled";
		bool m_IsRunning = false;
        bool m_Stopping = false;
        std::vector<UUID> m_PendingStart;
        std::unordered_set<UUID> m_PendingDestroy, m_Destroying;
		bool m_IsPaused = false;
		int m_StepFrames = 0;

		Scope<b2World> m_PhysicsWorld;

		std::unordered_map<UUID, entt::entity> m_EntityMap;
		std::unordered_map<UUID, ScriptFieldMap> m_ScriptFields;

		friend class Entity;
		friend class ScriptEngine;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};

}
