#pragma once

#include "Hazel/Core/Timestep.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Renderer/EditorCamera.h"
#include "Hazel/Scripting/ScriptField.h"

#include "entt.hpp"
#include <unordered_set>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include "Components.h"

class b2World;

namespace Hazel {
    class AudioPlayback;

	class Entity;
    enum class TransformPolicy { KeepWorld, KeepLocal };
    enum class DestroyPolicy { Subtree, KeepChildren };
    struct Relationship { UUID Parent{0}; uint32_t Order = 0; };
    enum class ParentingState { Pending, Applied, Rejected, Expired };
    struct ParentingResult { ParentingState State = ParentingState::Expired; std::string Reason; };

	class Scene
	{
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(Ref<Scene> other);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity, DestroyPolicy policy = DestroyPolicy::Subtree,
                           TransformPolicy transform = TransformPolicy::KeepWorld);
        static constexpr size_t MaxEntities = 10000, MaxDepth = 256;
        Relationship GetRelationship(Entity entity) const;
        std::vector<UUID> GetChildren(UUID parent = UUID(0)) const;
        std::vector<UUID> GetSubtree(Entity entity) const;
        glm::mat4 GetWorldTransform(Entity entity) const;
        void SetLocalTransform(Entity entity, const TransformComponent& value);
        void SetWorldTransform(Entity entity, const glm::mat4& value);
        static TransformComponent ExactTRS(const glm::mat4& value);
        // Edit commands apply immediately. Runtime requests return a bounded result ID;
        // getters retain committed relationships until the next lifecycle boundary.
        uint64_t Reparent(Entity entity, Entity parent, TransformPolicy mode = TransformPolicy::KeepWorld);
        ParentingResult GetParentingResult(uint64_t request) const;
        void ValidateHierarchy() const;
        void CheckThread() const;
        void ValidateComponentPlacement(Entity entity, bool physics) const;
        Entity InstantiateEntity(Entity source, const TransformComponent& transform);
        static Ref<Scene> ExtractSubtree(Entity root);
        void ValidatePhysics(Entity entity, const TransformComponent& transform,
                             const Rigidbody2DComponent* body = nullptr,
                             const BoxCollider2DComponent* box = nullptr,
                             const CircleCollider2DComponent* circle = nullptr) const;
        bool IsEntityValid(UUID id) const;
        void CancelPendingLifecycle();

		bool PlayAudio(Entity entity);
        void StopAudio(Entity entity);
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
        Scope<AudioPlayback> m_Audio;
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
		template<typename T> void OnComponentRemoving(Entity entity);
		void DestroyNativeScript(Entity entity);
        void DestroyEntityNow(Entity entity);
        void FlushLifecycle();
		void DestroyPhysicsBody(Entity entity);
		void DestroyPhysicsFixture(Entity entity, bool circle);

        using Relationships = std::unordered_map<UUID, Relationship>;
        using Transforms = std::unordered_map<UUID, TransformComponent>;
        glm::mat4 World(UUID id, const Relationships& relations, const Transforms& overrides) const;
        void ValidateGraph(const Relationships& relations, const Transforms& overrides) const;
        void RebuildChildren();
        void ApplyReparent(UUID child, UUID parent, TransformPolicy mode);
        void CheckReparent(UUID child, UUID parent, TransformPolicy mode,
                           Relationships& relations, Transforms& transforms) const;
        void FlushParenting();
        void RemoveRelationship(UUID id);
        std::vector<UUID> OrderedForCleanup() const;
        void CheckEntity(Entity entity) const;
        Entity CloneSubtree(Entity source, const TransformComponent* placement,
                            bool preserveIDs, bool retainExternal, bool sibling);
		void OnPhysics2DStart();
		void OnPhysics2DStop();
		void SynchronizePhysics2D();

		void RenderScene(EditorCamera& camera);
		void AdvanceAnimations(double timestep);
	private:
        std::thread::id m_Thread=std::this_thread::get_id();
        Relationships m_Relationships;
        std::unordered_map<UUID, std::vector<UUID>> m_Children;
        struct ParentingCommand { uint64_t Request; UUID Child, Parent; TransformPolicy Mode; };
        std::deque<ParentingCommand> m_Parenting;
        std::map<uint64_t, ParentingResult> m_ParentingResults;
        uint64_t m_NextParenting = 0;
        std::vector<UUID> m_DestroyOrder;
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
