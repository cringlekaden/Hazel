#include "hzpch.h"
#include "Scene.h"
#include "Entity.h"

#include "Components.h"
#include "ScriptableEntity.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Physics/Physics2D.h"

#include <glm/glm.hpp>

#include "Entity.h"

// Box2D
#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"
#include <type_traits>
#include <stdexcept>
#include <cmath>

namespace Hazel {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
		if (m_IsRunning) OnRuntimeStop();
		std::vector<Entity> natives; for(auto e:m_Registry.view<NativeScriptComponent>())natives.emplace_back(e,this);
        for(auto entity:natives)if(entity)DestroyNativeScript(entity);
        CancelPendingLifecycle(); // Cleanup callbacks may retire gameplay-owned instances.
        m_IsRunning = false;
		OnPhysics2DStop();
	}

	// Runtime observations never become ownership in a copied editor scene.
	template<typename T> static T CopyForScene(const T& source)
	{
		T copy = source;
		if constexpr (std::is_same_v<T, Rigidbody2DComponent>) copy.RuntimeBody = nullptr;
		if constexpr (std::is_same_v<T, BoxCollider2DComponent> || std::is_same_v<T, CircleCollider2DComponent>) copy.RuntimeFixture = nullptr;
		if constexpr (std::is_same_v<T, NativeScriptComponent>) copy.Instance.reset();
        if constexpr (std::is_same_v<T, SpriteRendererComponent>) {copy.Resolved={};copy.PreparedEpoch=0;copy.PreparedSource=std::monostate{};}
        if constexpr (std::is_same_v<T, SpriteAnimationComponent>) copy.ResetRuntime();
		return copy;
	}

	template<typename... Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		([&]()
		{
			auto view = src.view<Component>();
			for (auto srcEntity : view)
			{
				entt::entity dstEntity = enttMap.at(src.get<IDComponent>(srcEntity).ID);

				auto& srcComponent = src.get<Component>(srcEntity);
				dst.emplace_or_replace<Component>(dstEntity, CopyForScene(srcComponent));
			}
		}(), ...);
	}

	template<typename... Component>
	static void CopyComponent(ComponentGroup<Component...>, entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		CopyComponent<Component...>(dst, src, enttMap);
	}

	template<typename... Component>
	static void CopyComponentIfExists(Entity dst, Entity src)
	{
		([&]()
		{
			if (src.HasComponent<Component>())
				dst.AddOrReplaceComponent<Component>(CopyForScene(src.GetComponent<Component>()));
		}(), ...);
	}

	template<typename... Component>
	static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
	{
		CopyComponentIfExists<Component...>(dst, src);
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other)
	{
		Ref<Scene> newScene = CreateRef<Scene>();
        newScene->m_Assets=other->m_Assets;

		newScene->m_ViewportWidth = other->m_ViewportWidth;
        newScene->m_Name = other->m_Name;
		newScene->m_ViewportHeight = other->m_ViewportHeight;

		auto& srcSceneRegistry = other->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;
		std::unordered_map<UUID, entt::entity> enttMap;

		// Create entities in new scene
		auto idView = srcSceneRegistry.view<IDComponent>();
		for (auto e : idView)
		{
			UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			enttMap[uuid] = (entt::entity)newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent(AllComponents{}, dstSceneRegistry, srcSceneRegistry, enttMap);
		newScene->m_ScriptFields = other->m_ScriptFields;

		return newScene;
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		if(m_Stopping)throw std::runtime_error("Scene is stopping; creation is unavailable during cleanup");
        if (m_EntityMap.find(uuid) != m_EntityMap.end()) throw std::invalid_argument("Duplicate entity UUID");
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		m_EntityMap[uuid] = entity;
        if(m_IsRunning)m_PendingStart.push_back(uuid);

		return entity;
	}

	bool Scene::IsEntityValid(UUID id) const {
        return m_EntityMap.count(id) && !m_PendingDestroy.count(id) && !m_Destroying.count(id);
    }
    void Scene::DestroyEntity(Entity entity) {
        if (!entity.BelongsTo(this)) throw std::invalid_argument("Entity belongs to another scene");
        if (!entity) return;
        if (m_IsRunning) { if(!m_Destroying.count(entity.GetUUID())) m_PendingDestroy.insert(entity.GetUUID()); return; }
        DestroyEntityNow(entity);
    }
    void Scene::CancelPendingLifecycle() {
        auto pending=std::move(m_PendingStart);m_PendingStart.clear();
        for(auto id:pending){auto entity=GetEntityByUUID(id);if(entity)DestroyEntityNow(entity);}
        auto retired=std::move(m_PendingDestroy);m_PendingDestroy.clear();
        for(auto id:retired){auto entity=GetEntityByUUID(id);if(entity)DestroyEntityNow(entity);}
    }
    void Scene::FlushLifecycle() {
        auto retired=std::move(m_PendingDestroy); m_PendingDestroy.clear();
        for(auto id:retired) { auto entity=GetEntityByUUID(id); if(entity) DestroyEntityNow(entity); }
        auto starts=std::move(m_PendingStart); m_PendingStart.clear();
        SynchronizePhysics2D();
        for(auto id:starts) { auto entity=GetEntityByUUID(id);
            if(entity && IsEntityValid(id) && entity.HasComponent<ScriptComponent>()) ScriptEngine::OnCreateEntity(entity);
        }
    }
    Entity Scene::InstantiateEntity(Entity source, const TransformComponent& transform) {
        if(m_Stopping) throw std::runtime_error("Cannot instantiate while the scene is stopping");
        if(!source) throw std::invalid_argument("Prefab source entity is invalid");
        for(int i=0;i<3;i++) if(!std::isfinite(transform.Translation[i]) || !std::isfinite(transform.Rotation[i]) || !std::isfinite(transform.Scale[i]) || transform.Scale[i]<=0)
            throw std::invalid_argument("Initial transform must be finite with positive scale");
        auto fields=ScriptEngine::GetScriptFieldMap(source);
        for(auto& [name,field]:fields) if(field.Field.Type==ScriptFieldType::Entity) {
            auto id=field.GetValue<uint64_t>(); if(id && id!=source.GetUUID()) throw std::runtime_error("Prefab contains an external entity reference: "+name);
        }
        auto instance=CreateEntity(source.GetName());
        try {
            CopyComponentIfExists(AllComponents{},instance,source);
            instance.GetComponent<TransformComponent>()=transform;
            for(auto& [name,field]:fields) if(field.Field.Type==ScriptFieldType::Entity && field.GetValue<uint64_t>()) field.SetValue<uint64_t>(instance.GetUUID());
            ScriptEngine::GetScriptFieldMap(instance)=std::move(fields);
            if(m_IsRunning) SynchronizePhysics2D(); // Initial transform/fields are complete; body is usable by the caller.
            return instance;
        } catch(...) { DestroyEntityNow(instance); throw; }
    }
    void Scene::DestroyEntityNow(Entity entity)
    {
		if (entity.m_Scene != this) throw std::invalid_argument("Entity belongs to another scene");
		if (!entity) return;
        auto id=entity.GetUUID(); if(m_Destroying.count(id))return; m_Destroying.insert(id);
		if (ScriptEngine::GetSceneContext() == this) ScriptEngine::OnDestroyEntity(entity.GetUUID());
        DestroyNativeScript(entity);
        DestroyPhysicsBody(entity);
		m_EntityMap.erase(entity.GetUUID());
		m_ScriptFields.erase(entity.GetUUID());
		m_Registry.destroy(entity);m_Destroying.erase(id);
	}

	void Scene::OnRuntimeStart()
	{
		if (m_IsRunning) return;
		if (!ScriptEngine::IsInitialized())
            for (auto handle : m_Registry.view<ScriptComponent>())
                if (!m_Registry.get<ScriptComponent>(handle).ClassName.empty())
                    throw std::logic_error("Managed scene scripts require an initialized project script engine");
		m_IsRunning = true; m_Stopping = false;
        for(auto e:m_Registry.view<SpriteAnimationComponent>())m_Registry.get<SpriteAnimationComponent>(e).ResetRuntime();
        PrepareSprites(true);

		OnPhysics2DStart();

		// Scripting
		{
			ScriptEngine::OnRuntimeStart(this);
            for(auto e:m_Registry.view<ScriptComponent>()) m_PendingStart.push_back(m_Registry.get<IDComponent>(e).ID);
            FlushLifecycle();
		}
	}

	void Scene::OnRuntimeStop()
	{
		m_Stopping = true; CancelPendingLifecycle();
        if (ScriptEngine::GetSceneContext() == this) ScriptEngine::OnRuntimeStop();

		std::vector<Entity> natives; for(auto e:m_Registry.view<NativeScriptComponent>())natives.emplace_back(e,this);
        for(auto entity:natives)if(entity)DestroyNativeScript(entity);
        CancelPendingLifecycle(); // Cleanup callbacks may retire gameplay-owned instances.
        m_IsRunning = false;
		OnPhysics2DStop();
        m_Stopping=false;
	}

	void Scene::OnSimulationStart()
	{
		OnPhysics2DStart();
        for(auto e:m_Registry.view<SpriteAnimationComponent>()) m_Registry.get<SpriteAnimationComponent>(e).ResetRuntime();
        PrepareSprites(true);
	}

	void Scene::OnSimulationStop()
	{
		OnPhysics2DStop();
        for(auto e:m_Registry.view<SpriteAnimationComponent>()) m_Registry.get<SpriteAnimationComponent>(e).ResetRuntime();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
        FlushLifecycle(); // Administrative work commits even while simulation is paused.
		if (!m_IsPaused || m_StepFrames > 0)
		{
			if (m_IsPaused) --m_StepFrames;
			SynchronizePhysics2D(); // Components added between frames are ready for scripts.
			// Update scripts
			{
				// C# Entity OnUpdate
                std::vector<UUID> updates;
                for(auto e:m_Registry.view<ScriptComponent>()) updates.push_back(m_Registry.get<IDComponent>(e).ID);
                for(auto id:updates) if(IsEntityValid(id) && ScriptEngine::GetEntityScriptInstance(id))
                    ScriptEngine::OnUpdateEntity(GetEntityByUUID(id),ts);

                std::vector<UUID> natives;
                for(auto e:m_Registry.view<NativeScriptComponent>()) natives.push_back(m_Registry.get<IDComponent>(e).ID);
                for(auto id:natives) {
                    if(!IsEntityValid(id)) continue;
                    auto entity=GetEntityByUUID(id); auto& nsc=entity.GetComponent<NativeScriptComponent>();
                    if(!nsc.Instance) {
                        if(!nsc.InstantiateScript) throw std::logic_error("Native script is not bound");
                        nsc.Instance=nsc.InstantiateScript(); nsc.Instance->m_Entity=entity; nsc.Instance->OnCreate();
                    }
                    if(IsEntityValid(id)) nsc.Instance->OnUpdate(ts);
                }
                // Creation receives physics/OnCreate here, then its first update next frame.
                FlushLifecycle();
			}

            AdvanceAnimations(ts);
			// Physics
			{
				SynchronizePhysics2D(); // Native callbacks may add entities/components too.
				const int32_t velocityIterations = 6;
				const int32_t positionIterations = 2;
				m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

				// Retrieve transform from Box2D
				auto view = m_Registry.view<Rigidbody2DComponent>();
				for (auto e : view)
				{
					Entity entity = { e, this };
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					b2Body* body = (b2Body*)rb2d.RuntimeBody;

					const auto& position = body->GetPosition();
					transform.Translation.x = position.x;
					transform.Translation.y = position.y;
					transform.Rotation.z = body->GetAngle();
				}
			}
		}

        PrepareSprites();
		// Render 2D
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = transform.GetTransform();
					break;
				}
			}
		}

		if (mainCamera)
		{
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			// Draw sprites
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

                    auto* draw=RenderedSprite(Entity(entity,this));
                    if(draw) Renderer2D::DrawSprite(transform.GetTransform(),*draw,sprite.Color,(int)entity);
                    else Renderer2D::DrawQuad(transform.GetTransform(),glm::vec4(1,0,1,1),(int)entity);
				}
			}

			// Draw circles
			{
				auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
				for (auto entity : view)
				{
					auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

					Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
				}
			}

			// Draw text
			{
				auto view = m_Registry.view<TransformComponent, TextComponent>();
				for (auto entity : view)
				{
					auto [transform, text] = view.get<TransformComponent, TextComponent>(entity);

					Renderer2D::DrawString(text.TextString, transform.GetTransform(), text, (int)entity);
				}
			}

			Renderer2D::EndScene();
		}

	}

	void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& camera)
	{
		if (!m_IsPaused || m_StepFrames > 0)
		{
			if (m_IsPaused) --m_StepFrames;
            AdvanceAnimations(ts);
			SynchronizePhysics2D();
			// Physics
			{
				const int32_t velocityIterations = 6;
				const int32_t positionIterations = 2;
				m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

				// Retrieve transform from Box2D
				auto view = m_Registry.view<Rigidbody2DComponent>();
				for (auto e : view)
				{
					Entity entity = { e, this };
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					b2Body* body = (b2Body*)rb2d.RuntimeBody;
					const auto& position = body->GetPosition();
					transform.Translation.x = position.x;
					transform.Translation.y = position.y;
					transform.Rotation.z = body->GetAngle();
				}
			}
		}

		// Render
		RenderScene(camera);
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
	{
		// Render
		RenderScene(camera);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (m_ViewportWidth == width && m_ViewportHeight == height)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize our non-FixedAspectRatio cameras
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity{entity, this};
		}
		return {};
	}

	void Scene::Step(int frames)
	{
		if (frames < 0) throw std::invalid_argument("Scene step count cannot be negative");
		m_StepFrames = frames;
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		// Copy name because we're going to modify component data structure
		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);
		CopyComponentIfExists(AllComponents{}, newEntity, entity);
		if (entity.HasComponent<ScriptComponent>())
			ScriptEngine::GetScriptFieldMap(newEntity) = ScriptEngine::GetScriptFieldMap(entity);
		return newEntity;
	}

	Entity Scene::FindEntityByName(std::string_view name)
	{
		auto view = m_Registry.view<TagComponent>();
		for (auto entity : view)
		{
			const TagComponent& tc = view.get<TagComponent>(entity);
			if (tc.Tag == name)
				return Entity{ entity, this };
		}
		return {};
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		// TODO(Yan): Maybe should be assert
		if (m_EntityMap.find(uuid) != m_EntityMap.end())
			return { m_EntityMap.at(uuid), this };

		return {};
	}

	void Scene::DestroyNativeScript(Entity entity)
	{
		if (!entity.HasComponent<NativeScriptComponent>()) return;
		auto& component = entity.GetComponent<NativeScriptComponent>();
		if (!component.Instance) return;
		NativeScriptComponent detached;
		detached.InstantiateScript = component.InstantiateScript;
		detached.DestroyScript = component.DestroyScript;
		detached.Instance = std::move(component.Instance); // Clear observer before callbacks.
		try { detached.Instance->OnDestroy(); }
		catch (const std::exception& error) { HZ_CORE_ERROR("Native script cleanup: {}", error.what()); }
		catch (...) { HZ_CORE_ERROR("Native script cleanup threw an unknown exception"); }
		if (detached.DestroyScript) detached.DestroyScript(&detached);
	}

	void Scene::DestroyPhysicsFixture(Entity entity, bool circle)
	{
		if (!m_PhysicsWorld) return;
		void*& observation = circle ? entity.GetComponent<CircleCollider2DComponent>().RuntimeFixture
		                           : entity.GetComponent<BoxCollider2DComponent>().RuntimeFixture;
		if (observation) {
			auto* fixture = static_cast<b2Fixture*>(observation);
			fixture->GetBody()->DestroyFixture(fixture);
			observation = nullptr;
		}
	}

	void Scene::DestroyPhysicsBody(Entity entity)
	{
		if (!entity.HasComponent<Rigidbody2DComponent>()) return;
		auto& body = entity.GetComponent<Rigidbody2DComponent>();
		if (m_PhysicsWorld && body.RuntimeBody) m_PhysicsWorld->DestroyBody(static_cast<b2Body*>(body.RuntimeBody));
		body.RuntimeBody = nullptr;
		if (entity.HasComponent<BoxCollider2DComponent>()) entity.GetComponent<BoxCollider2DComponent>().RuntimeFixture = nullptr;
		if (entity.HasComponent<CircleCollider2DComponent>()) entity.GetComponent<CircleCollider2DComponent>().RuntimeFixture = nullptr;
	}

	void Scene::OnPhysics2DStart()
	{
		if (m_PhysicsWorld) return;
		m_PhysicsWorld = CreateScope<b2World>(b2Vec2{ 0.0f, -9.8f });
		SynchronizePhysics2D();
	}

	void Scene::SynchronizePhysics2D()
	{
		if (!m_PhysicsWorld) return;
		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			if (!rb2d.RuntimeBody) {
				b2BodyDef bodyDef;
				bodyDef.type = Utils::Rigidbody2DTypeToBox2DBody(rb2d.Type);
				bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
				bodyDef.angle = transform.Rotation.z;
				bodyDef.gravityScale = rb2d.GravityScale;
				auto* body = m_PhysicsWorld->CreateBody(&bodyDef);
				body->SetFixedRotation(rb2d.FixedRotation);
				rb2d.RuntimeBody = body;
			}
			auto* body = static_cast<b2Body*>(rb2d.RuntimeBody);

			if (entity.HasComponent<BoxCollider2DComponent>() && !entity.GetComponent<BoxCollider2DComponent>().RuntimeFixture)
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2PolygonShape boxShape;
				boxShape.SetAsBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y, b2Vec2(bc2d.Offset.x, bc2d.Offset.y), 0.0f);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = bc2d.Density;
				fixtureDef.friction = bc2d.Friction;
				fixtureDef.restitution = bc2d.Restitution;
				fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;
				bc2d.RuntimeFixture = body->CreateFixture(&fixtureDef);
			}

			if (entity.HasComponent<CircleCollider2DComponent>() && !entity.GetComponent<CircleCollider2DComponent>().RuntimeFixture)
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2CircleShape circleShape;
				circleShape.m_p.Set(cc2d.Offset.x, cc2d.Offset.y);
				circleShape.m_radius = transform.Scale.x * cc2d.Radius;

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &circleShape;
				fixtureDef.density = cc2d.Density;
				fixtureDef.friction = cc2d.Friction;
				fixtureDef.restitution = cc2d.Restitution;
				fixtureDef.restitutionThreshold = cc2d.RestitutionThreshold;
				cc2d.RuntimeFixture = body->CreateFixture(&fixtureDef);
			}
		}
	}

	void Scene::OnPhysics2DStop()
	{
		for (auto e : m_Registry.view<Rigidbody2DComponent>()) m_Registry.get<Rigidbody2DComponent>(e).RuntimeBody = nullptr;
		for (auto e : m_Registry.view<BoxCollider2DComponent>()) m_Registry.get<BoxCollider2DComponent>(e).RuntimeFixture = nullptr;
		for (auto e : m_Registry.view<CircleCollider2DComponent>()) m_Registry.get<CircleCollider2DComponent>(e).RuntimeFixture = nullptr;
		m_PhysicsWorld.reset();
	}

	void Scene::RenderScene(EditorCamera& camera)
	{
        PrepareSprites();
		Renderer2D::BeginScene(camera);

		// Draw sprites
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

                auto* draw=RenderedSprite(Entity(entity,this));
                if(draw) Renderer2D::DrawSprite(transform.GetTransform(),*draw,sprite.Color,(int)entity);
                else Renderer2D::DrawQuad(transform.GetTransform(),glm::vec4(1,0,1,1),(int)entity);
			}
		}

		// Draw circles
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

				Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
			}
		}

		// Draw text
		{
			auto view = m_Registry.view<TransformComponent, TextComponent>();
			for (auto entity : view)
			{
				auto [transform, text] = view.get<TransformComponent, TextComponent>(entity);

				Renderer2D::DrawString(text.TextString, transform.GetTransform(), text, (int)entity);
			}
		}

		Renderer2D::EndScene();
	}

    void Scene::PrepareSprites(bool strict)
    {
        const uint64_t epoch=m_Assets?m_Assets->Epoch():1;
        for(auto e:m_Registry.view<SpriteRendererComponent>()) {
            auto& s=m_Registry.get<SpriteRendererComponent>(e);
            if(s.PreparedEpoch!=epoch || !(s.PreparedSource==s.Source)) {
                s.PreparedEpoch=epoch;s.PreparedSource=s.Source;s.Resolved={};
                try {
                    if(m_Assets) s.Resolved.Data=m_Assets->Resolve(s.Source);
                    else if(std::holds_alternative<std::monostate>(s.Source)) s.Resolved.Data=CreateRef<ResolvedSprite>();
                    else if(auto t=std::get_if<TextureSpriteSource>(&s.Source);t && t->Resource) {
                        auto draw=CreateRef<ResolvedSprite>();draw->Texture=t->Resource;draw->TilingFactor=t->TilingFactor;s.Resolved.Data=draw;
                    } else throw std::runtime_error("Sprite needs a project asset root");
                } catch(const std::exception& error) {s.Resolved.Error=error.what();HZ_CORE_ERROR("Sprite '{}': {}",m_Registry.get<TagComponent>(e).Tag,s.Resolved.Error);}
            }
            if(strict && !s.Resolved.Error.empty()) throw std::runtime_error(s.Resolved.Error);
        }
        for(auto e:m_Registry.view<SpriteAnimationComponent>()) {
            auto& a=m_Registry.get<SpriteAnimationComponent>(e);
            const bool runtime=m_IsRunning || static_cast<bool>(m_PhysicsWorld);
            if(!a.Initialized || (!runtime && !(a.Current==a.DefaultClip))) {
                a.ResetRuntime();a.Current=a.DefaultClip;a.Initialized=true;a.Playback.Reset(runtime && a.Autoplay && a.DefaultClip.Clip!=0);
            }
            if(a.PreparedEpoch!=epoch) {
                a.PreparedEpoch=epoch;a.Resolved.reset();a.Error.clear();
                try {
                    if(!Entity(e,this).HasComponent<SpriteRendererComponent>()) throw std::runtime_error("Animation requires Sprite Renderer");
                    if(!std::isfinite(a.Speed) || a.Speed<0) throw std::runtime_error("Animation speed must be finite and nonnegative");
                    if(a.Current.Clip) {
                        if(!m_Assets) throw std::runtime_error("Animation needs a project asset root");
                        a.Resolved=m_Assets->Clip(a.Current);a.Playback.Scrub(a.Resolved->Definition(),a.Playback.Time);
                    }
                } catch(const std::exception& error) {a.Error=error.what();HZ_CORE_ERROR("Animation '{}': {}",m_Registry.get<TagComponent>(e).Tag,a.Error);}
            }
            if(!std::isfinite(a.Speed) || a.Speed<0)a.Error="Animation speed must be finite and nonnegative";
            if(strict && !a.Error.empty()) throw std::runtime_error(a.Error);
        }
    }
    void Scene::ValidateSprites() {
        PrepareSprites(true);
        for(auto& entityFields:m_ScriptFields)for(auto& entry:entityFields.second){const auto& field=entry.second;
            if(!field.AssetID)continue;
            if(field.Field.Type==ScriptFieldType::Sprite){if(!m_Assets)throw std::runtime_error("Sprite field needs project assets");m_Assets->Resolve(SpriteReference{std::filesystem::u8path(field.AssetReference),field.AssetID});}
            if(field.Field.Type==ScriptFieldType::SpriteAnimation){if(!m_Assets)throw std::runtime_error("Animation field needs project assets");m_Assets->Clip({std::filesystem::u8path(field.AssetReference),field.AssetID});}
        }
    }
    bool Scene::PlayAnimation(Entity entity,const AnimationReference& reference)
    {
        try {
            if(!entity || !entity.BelongsTo(this) || !entity.HasComponent<SpriteAnimationComponent>()) throw std::runtime_error("Animation component is missing");
            if(!entity.HasComponent<SpriteRendererComponent>() || !m_Assets || !reference.Clip) throw std::runtime_error("Animation requires a valid clip and Sprite Renderer");
            auto resolved=m_Assets->Clip(reference);
            auto& a=entity.GetComponent<SpriteAnimationComponent>();
            a.Current=reference;a.Resolved=resolved;a.PreparedEpoch=m_Assets->Epoch();a.Initialized=true;a.Error.clear();a.Playback.Reset(true);return true;
        } catch(const std::exception& error) {HZ_CORE_ERROR("Play animation: {}",error.what());return false;}
    }
    void Scene::AdvanceAnimations(double timestep)
    {
        PrepareSprites();
        for(auto e:m_Registry.view<SpriteAnimationComponent>()) {
            auto& a=m_Registry.get<SpriteAnimationComponent>(e);
            if(a.Resolved) {
                try {a.Playback.Advance(a.Resolved->Definition(),timestep,a.Speed);}
                catch(const std::exception& error) {a.Error=error.what();a.Playback.Playing=false;}
            }
        }
    }
    const ResolvedSprite* Scene::RenderedSprite(Entity entity) const
    {
        if(!entity || !entity.HasComponent<SpriteRendererComponent>()) return nullptr;
        if(entity.HasComponent<SpriteAnimationComponent>()) {
            auto& a=entity.GetComponent<SpriteAnimationComponent>();
            if(!a.Error.empty()) return nullptr;
            if(a.Resolved && a.Playback.Frame<a.Resolved->Frames.size()) return a.Resolved->Frames[a.Playback.Frame].get();
        }
        return entity.GetComponent<SpriteRendererComponent>().Resolved.Data.get();
    }

    template<typename T>

	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		static_assert(sizeof(T) == 0);
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component)
	{
	}

    template<> void Scene::OnComponentAdded<SpriteAnimationComponent>(Entity entity, SpriteAnimationComponent& component) {component.ResetRuntime();}

	template<>
	void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component)
	{
		component.RuntimeBody = nullptr;
	}

	template<>
	void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component)
	{
		component.RuntimeFixture = nullptr;
	}

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component)
	{
		component.RuntimeFixture = nullptr;
	}

	template<>
	void Scene::OnComponentAdded<TextComponent>(Entity entity, TextComponent& component)
	{
	}

}
