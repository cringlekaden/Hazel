#pragma once

#include "Hazel/Core/UUID.h"
#include "Scene.h"
#include "Components.h"

#include "entt.hpp"
#include <type_traits>
#include <stdexcept>

namespace Hazel {

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene);
		Entity(const Entity& other) = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			if (!*this || HasComponent<T>()) throw std::logic_error("Invalid entity or duplicate component");
			m_Scene->ValidateComponentPlacement(*this, std::is_same_v<T, Rigidbody2DComponent> ||
                std::is_same_v<T, BoxCollider2DComponent> || std::is_same_v<T, CircleCollider2DComponent>);
            ValidatePhysicsCandidate<T>(args...);
            T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			m_Scene->OnComponentAdded<T>(*this, component);
			return component;
		}

		template<typename T, typename... Args>
		T& AddOrReplaceComponent(Args&&... args)
		{
			if (!*this) throw std::logic_error("Invalid entity");
			m_Scene->ValidateComponentPlacement(*this, std::is_same_v<T, Rigidbody2DComponent> ||
                std::is_same_v<T, BoxCollider2DComponent> || std::is_same_v<T, CircleCollider2DComponent>);
            ValidatePhysicsCandidate<T>(args...);
            if (HasComponent<T>()) m_Scene->OnComponentRemoving<T>(*this);
			T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
			m_Scene->OnComponentAdded<T>(*this, component);
			return component;
		}

		template<typename T>
		T& GetComponent()
		{
			if (!HasComponent<T>()) throw std::logic_error("Invalid entity or missing component");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		bool HasComponent()
		{
			return *this && m_Scene->m_Registry.has<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent()
		{
			if (!HasComponent<T>()) throw std::logic_error("Invalid entity or missing component");
			m_Scene->OnComponentRemoving<T>(*this);
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		operator bool() const { return m_Scene && m_Scene->m_Registry.valid(m_EntityHandle); }
		// Pointer comparison only: safe even when an observation's former scene has died.
		bool BelongsTo(const Scene* scene) const { return m_Scene == scene; }
		operator entt::entity() const { return m_EntityHandle; }
		operator uint32_t() const { return (uint32_t)m_EntityHandle; }

		UUID GetUUID() { return GetComponent<IDComponent>().ID; }
		const std::string& GetName() { return GetComponent<TagComponent>().Tag; }

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}
    private:
        template<typename T,typename... Args> void ValidatePhysicsCandidate(Args&&... args) {
            if constexpr(std::is_same_v<T,Rigidbody2DComponent> || std::is_same_v<T,BoxCollider2DComponent> || std::is_same_v<T,CircleCollider2DComponent>) {
                const T candidate(std::forward<Args>(args)...);
                const auto& transform=GetComponent<TransformComponent>();
                if constexpr(std::is_same_v<T,Rigidbody2DComponent>)m_Scene->ValidatePhysics(*this,transform,&candidate);
                if constexpr(std::is_same_v<T,BoxCollider2DComponent>)m_Scene->ValidatePhysics(*this,transform,nullptr,&candidate);
                if constexpr(std::is_same_v<T,CircleCollider2DComponent>)m_Scene->ValidatePhysics(*this,transform,nullptr,nullptr,&candidate);
            }
        }
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr;
		friend class Scene;
		friend class ScriptEngine;
	};

	template<typename T> void Scene::OnComponentRemoving(Entity entity)
	{
		if constexpr (std::is_same_v<T, NativeScriptComponent>) DestroyNativeScript(entity);
		if constexpr (std::is_same_v<T, Rigidbody2DComponent>) DestroyPhysicsBody(entity);
		if constexpr (std::is_same_v<T, BoxCollider2DComponent>) DestroyPhysicsFixture(entity, false);
		if constexpr (std::is_same_v<T, CircleCollider2DComponent>) DestroyPhysicsFixture(entity, true);
	}

}
