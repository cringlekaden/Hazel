#pragma once

#include "Hazel/Core/Timestep.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Renderer/EditorCamera.h"
#include "Hazel/Scripting/ScriptField.h"
#include "Hazel/UI/RuntimeUISnapshot.h"

#include "entt.hpp"
#include <unordered_set>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <typeindex>
#include <optional>
#include "Components.h"

class b2World;

namespace Hazel {
    namespace Text { class Clipboard; }
    class AudioPlayback;
    class RuntimeUI;

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
        // Editor transaction staging preserves the entity allocator/generations.
        static Ref<Scene> CopyForAuthoring(const Ref<Scene>& source);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity, DestroyPolicy policy = DestroyPolicy::Subtree,
                           TransformPolicy transform = TransformPolicy::KeepWorld);
        static constexpr size_t MaxEntities = 10000, MaxDepth = 256;
        Relationship GetRelationship(Entity entity) const;
        std::vector<UUID> GetChildren(UUID parent = UUID(0)) const;
        const std::vector<UUID>& GetChildrenView(UUID parent) const;
        void SetDisplayText(uint64_t entity,const std::string& source,const std::vector<Text::Span>& spans={});
        UIStyleValues GetUIPresentation(uint64_t entity);
        UIStyleSelection GetUIStyle(uint64_t entity);
        void SetUIStyle(uint64_t entity,const UIStyleSelection& selection,bool enabled=true);
        void ResetUIStyleOverrides(uint64_t entity,uint32_t groups);
        void UpdateUIInput(const GameUI::InputFrame& frame);
        bool ConsumeUIAction(uint64_t entity,GameUI::Action& action);
        bool ConsumeUICancel(uint64_t scope);
        bool RequestUIScreen(GameUI::ScreenOperation operation,uint64_t screen=0);
        void CommitUIScreens();
        bool IsUIScreenOpen(uint64_t screen) const;
        bool ConsumeUIEvent(uint64_t owner,GameUI::Action &event);
    bool IsUIChecked(uint64_t id);
    void SetUIChecked(uint64_t id,bool value);
    uint64_t GetUISelection(uint64_t group);
    void PreviewUITab(uint64_t group,uint64_t tab);
    void PreviewUITooltip(uint64_t source,bool open);
    bool RequestUIDropdown(uint64_t dropdown,bool open);
    bool IsUIDropdownOpen(uint64_t dropdown) const;
    void PreviewUIDropdown(uint64_t dropdown,bool open);
    void SetUISelection(uint64_t group,uint64_t member);
    GameUI::ScrollGeometry GetUIScroll(uint64_t id);
    void ScrollUITo(uint64_t id,glm::vec2 offset,bool normalized=false);
    bool EnsureUIVisible(uint64_t view,uint64_t target,bool focus=false);
    float GetUISliderValue(uint64_t id);
    void SetUISliderValue(uint64_t id,float value);
    bool IsUISliderEditing() const;
    float GetUIProgress(uint64_t id);
    void SetUIProgress(uint64_t id,float value);
    void ValidateUIControls(uint64_t target=0) const;
    bool GetUIControlVisual(uint64_t id,GameUI::Rect &bounds,bool &visible) const;
        bool FocusUI(uint64_t target);
        void SetUIPreviewTarget(uint64_t selected); // transient Edit-only screen isolation
        bool IsGameplayInputBlocked() const;
        bool IsGameplayPointerBlocked() const;
        bool IsUICapturingPointer() const;
        bool GetUIBounds(uint64_t entity,GameUI::Rect& pixels) const;
        void ResetUI();
        void RetireUI(uint64_t entity);
        void RetireUISubtree(uint64_t entity);
        std::string GetUITextEntry(uint64_t entity);
        void SetUITextEntry(uint64_t entity,const std::string& value);
        bool IsUITextEditing() const;
        bool IsUITextInputOwned() const;
        void SetUIClipboard(Ref<Text::Clipboard> clipboard);
        void CancelUIInput();
        void SetUIFramebufferSize(glm::vec2 size);
        const GameUI::ResolvedSnapshot& GetUISnapshot() const;
        GameUI::RuntimeUIDiagnostics GetUIDiagnostics() const;
        Ref<const Text::Layout> GetUITextLayout(uint64_t entity) const; // last completed paint, never resolves
        bool GetUIImageVisual(uint64_t entity,GameUI::UIImageVisual &result) const;
        void SetUIImageFit(uint64_t entity,GameUI::ImageFit fit,glm::vec2 alignment={.5f,.5f});
        void RefreshUI();
        void SetUILayout(uint64_t entity, const GameUI::FlowLayout &layout);
        void SetUIGridCell(uint64_t entity,int row,int column,unsigned rowSpan=1,unsigned columnSpan=1);
        void SetUISizing(uint64_t entity, GameUI::Axis axis, GameUI::SizeMode mode,
                         float fixedSize, float minimum = 0, std::optional<float> maximum = {});
        void ValidateUI(bool resources=true, bool controls=true);
        void ValidateUILayoutDependencies(); // CPU-only, including hidden/zero-surface documents
        const std::string& GetUIError() const {return m_UIError;}

        std::vector<UUID> GetSubtree(Entity entity) const;
        glm::mat4 GetWorldTransform(Entity entity) const;
        void SetLocalTransform(Entity entity, const TransformComponent& value);
        void SetWorldTransform(Entity entity, const glm::mat4& value);
        static TransformComponent ExactTRS(const glm::mat4& value);
        // Edit commands apply immediately. Runtime requests return a bounded result ID;
        // getters retain committed relationships until the next lifecycle boundary.
        uint64_t Reparent(Entity entity, Entity parent, TransformPolicy mode = TransformPolicy::KeepWorld);
        void MoveSibling(Entity entity, int direction); // Edit-only adjacent order, stable IDs
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
        // Transient identity guard for authoring commands. A replaced component,
        // reparent or recreated UUID must not accept commands for its old lifetime.
        uint64_t GetAuthoringStructureVersion(UUID id) const;
        uint64_t GetAuthoringLifetime(UUID id) const;
        uint64_t GetAuthoringComponentVersion(UUID id, std::type_index type) const;
        const std::string& GetName() const { return m_Name; }
        void SetName(std::string name) { m_Name=std::move(name); }
        bool IsStopping() const { return m_Stopping; }

		bool IsRunning() const { return m_IsRunning; }
        bool IsSimulating() const { return !m_IsRunning && bool(m_PhysicsWorld); }
		bool IsPaused() const { return m_IsPaused; }

		void SetPaused(bool paused) { m_IsPaused = paused; }

		void Step(int frames = 1);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}
	private:
        // Only the document command can publish preflighted Edit state. Ordinary
        // native replacement must use the serializer's fresh document lifetimes.
        void PatchAuthoring(const Ref<Scene>& decoded,const std::vector<uint64_t>& affected);
        void PublishAuthoring(Scene& staged);
        friend class StructuralCommand;
        void ChangedAuthoringStructure(UUID id);
        void ChangedAuthoringStructure(UUID id, std::type_index type);
        std::unordered_map<UUID, uint64_t> m_AuthoringLifetimes;
        std::unordered_map<UUID, std::unordered_map<std::type_index,uint64_t>> m_AuthoringComponentVersions;
        std::unordered_map<UUID, uint64_t> m_AuthoringStructureVersions;
        uint64_t m_NextAuthoringStructureVersion = 0;
        Scope<AudioPlayback> m_Audio;
        Scope<RuntimeUI> m_UI;
        glm::vec2 m_UIFramebufferSize{};
        bool m_UIFramebufferExplicit = false;
        std::string m_UIError;
        bool m_UIScriptCallbacks=false;
        void RenderUI();
        mutable bool m_HierarchyValidationDirty=true;
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
