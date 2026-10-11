#include "hzpch.h"
#include "Entity.h"
#include <box2d/b2_body.h>
#include "Scene.h"
#include <algorithm>
#include <cmath>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <stdexcept>

namespace Hazel {
namespace {
void Finite(const glm::mat4 &m) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r]))
                throw std::runtime_error("Hierarchy transform is non-finite or overflowed");
}
void Compact(std::unordered_map<UUID, Relationship> &graph, UUID parent) {
    std::vector<UUID> ids;
    for (const auto &item : graph)
        if (item.second.Parent == parent)
            ids.push_back(item.first);
    std::sort(ids.begin(), ids.end(),
              [&](UUID a, UUID b) { return graph.at(a).Order < graph.at(b).Order; });
    for (size_t i = 0; i < ids.size(); ++i)
        graph.at(ids[i]).Order = uint32_t(i);
}
} // namespace
void Scene::CheckThread() const {
    if (std::this_thread::get_id() != m_Thread)
        throw std::logic_error("Scene hierarchy requires its owning thread");
}
void Scene::CheckEntity(Entity e) const {
    CheckThread();
    if (!e.BelongsTo(this) || !e || !IsEntityValid(e.GetUUID()))
        throw std::invalid_argument("Hierarchy operation requires a live entity in this scene");
}
Relationship Scene::GetRelationship(Entity e) const {
    CheckEntity(e);
    return m_Relationships.at(e.GetUUID());
}
std::vector<UUID> Scene::GetChildren(UUID parent) const {
    CheckThread();
    std::vector<UUID> result;
    auto it = m_Children.find(parent);
    if (it != m_Children.end())
        for (auto id : it->second)
            if (IsEntityValid(id))
                result.push_back(id);
    return result;
}
std::vector<UUID> Scene::GetSubtree(Entity e) const {
    CheckEntity(e);
    std::vector<UUID> result, pending{e.GetUUID()};
    while (!pending.empty()) {
        auto id = pending.back();
        pending.pop_back();
        result.push_back(id);
        if (result.size() > MaxEntities)
            throw std::runtime_error("Hierarchy subtree exceeds supported bounds");
        auto children = GetChildren(id);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
    return result;
}
glm::mat4 Scene::World(UUID id, const Relationships &graph, const Transforms &overrides) const {
    std::vector<UUID> path;
    std::set<UUID> seen;
    while (uint64_t(id)) {
        if (!seen.insert(id).second)
            throw std::runtime_error("Hierarchy contains a cycle");
        if (path.size() >= MaxDepth)
            throw std::runtime_error("Hierarchy depth exceeds 256");
        auto it = graph.find(id);
        if (it == graph.end() || !m_EntityMap.count(id))
            throw std::runtime_error("Hierarchy references a missing parent/entity");
        path.push_back(id);
        id = it->second.Parent;
    }
    glm::mat4 world(1);
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        const auto override = overrides.find(*it);
        const auto &local = override != overrides.end()
                                ? override->second
                                : m_Registry.get<TransformComponent>(m_EntityMap.at(*it));
        Finite(local.GetTransform());
        world *= local.GetTransform();
        Finite(world);
    }
    return world;
}
glm::mat4 Scene::GetWorldTransform(Entity e) const {
    CheckEntity(e);
    return World(e.GetUUID(), m_Relationships, {});
}
TransformComponent Scene::ExactTRS(const glm::mat4 &m) {
    Finite(m);
    constexpr float tolerance = 2e-4f;
    if (std::abs(m[0][3]) > tolerance || std::abs(m[1][3]) > tolerance ||
        std::abs(m[2][3]) > tolerance || std::abs(m[3][3] - 1) > tolerance)
        throw std::runtime_error("Transform is not affine TRS");
    TransformComponent value;
    value.Translation = glm::vec3(m[3]);
    glm::mat3 basis(m);
    for (int i = 0; i < 3; ++i) {
        value.Scale[i] = glm::length(basis[i]);
        if (!std::isfinite(value.Scale[i]) || value.Scale[i] < 1e-6f)
            throw std::runtime_error("Keep World cannot represent a singular transform");
        basis[i] /= value.Scale[i];
    }
    if (glm::determinant(basis) < 0)
        throw std::runtime_error(
            "Keep World rejects reflected transforms; use Keep Local explicitly");
    if (std::abs(glm::dot(basis[0], basis[1])) > tolerance ||
        std::abs(glm::dot(basis[0], basis[2])) > tolerance ||
        std::abs(glm::dot(basis[1], basis[2])) > tolerance)
        throw std::runtime_error("Keep World would require shear; local transforms support TRS "
                                 "only. Use Keep Local explicitly");
    value.Rotation = glm::eulerAngles(glm::normalize(glm::quat_cast(basis)));
    const auto rebuilt = value.GetTransform();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (std::abs(rebuilt[c][r] - m[c][r]) > tolerance * std::max(1.f, std::abs(m[c][r])))
                throw std::runtime_error(
                    "World transform cannot be reconstructed safely as local TRS");
    return value;
}
void Scene::ValidateGraph(const Relationships &graph, const Transforms &transforms) const {
    if (graph.size() != m_EntityMap.size() || graph.size() > MaxEntities)
        throw std::runtime_error("Invalid hierarchy entity count");
    std::unordered_map<UUID, std::set<uint32_t>> orders;
    for (const auto &[id, rel] : graph) {
        if (!m_EntityMap.count(id) || id == rel.Parent ||
            (uint64_t(rel.Parent) && !graph.count(rel.Parent)))
            throw std::runtime_error("Invalid hierarchy parent (missing or self)");
        if (!orders[rel.Parent].insert(rel.Order).second)
            throw std::runtime_error("Duplicate hierarchy sibling order");
        const auto handle = m_EntityMap.at(id);
        if (uint64_t(rel.Parent) && (m_Registry.has<Rigidbody2DComponent>(handle) ||
                                     m_Registry.has<BoxCollider2DComponent>(handle) ||
                                     m_Registry.has<CircleCollider2DComponent>(handle)))
            throw std::runtime_error(
                "Rigidbody/collider owners must be roots; visual children may follow a root body");
        const auto override = transforms.find(id);
        ValidatePhysics(Entity(handle, const_cast<Scene *>(this)),
                        override != transforms.end() ? override->second
                                                     : m_Registry.get<TransformComponent>(handle));
        const auto world = World(id, graph, transforms);
        if (m_Registry.has<CameraComponent>(handle) &&
            m_Registry.get<CameraComponent>(handle).Primary)
            Finite(glm::inverse(world));
    }
    for (const auto &[parent, values] : orders) {
        uint32_t next = 0;
        for (auto order : values)
            if (order != next++)
                throw std::runtime_error("Hierarchy sibling order must be contiguous from zero");
    }
}
void Scene::ValidateHierarchy() const {
    CheckThread();
    ValidateGraph(m_Relationships, {});
}
void Scene::RebuildChildren() {
    m_Children.clear();
    for (const auto &[id, rel] : m_Relationships)
        m_Children[rel.Parent].push_back(id);
    for (auto &[parent, ids] : m_Children)
        std::sort(ids.begin(), ids.end(), [&](UUID a, UUID b) {
            return m_Relationships.at(a).Order < m_Relationships.at(b).Order;
        });
}
void Scene::ValidateComponentPlacement(Entity e, bool physics) const {
    CheckThread();
    if (physics && m_Relationships.count(e.GetUUID()) &&
        uint64_t(m_Relationships.at(e.GetUUID()).Parent))
        throw std::runtime_error("Unparent this entity before adding a Rigidbody/collider; only "
                                 "root physics owners are supported");
}
void Scene::ValidatePhysics(Entity e, const TransformComponent &tc, const Rigidbody2DComponent *rb,
                            const BoxCollider2DComponent *box,
                            const CircleCollider2DComponent *circle) const {
    if (!rb && e.HasComponent<Rigidbody2DComponent>())
        rb = &e.GetComponent<Rigidbody2DComponent>();
    if (!box && e.HasComponent<BoxCollider2DComponent>())
        box = &e.GetComponent<BoxCollider2DComponent>();
    if (!circle && e.HasComponent<CircleCollider2DComponent>())
        circle = &e.GetComponent<CircleCollider2DComponent>();
    if (!rb && !box && !circle)
        return;
    Finite(tc.GetTransform());
    if (std::abs(tc.Rotation.x) > 1e-5f || std::abs(tc.Rotation.y) > 1e-5f || tc.Scale.x <= 0 ||
        tc.Scale.y <= 0)
        throw std::runtime_error("Physics needs planar Z rotation and positive XY scale: " +
                                 e.GetName());
    if (rb && (rb->Type != Rigidbody2DComponent::BodyType::Static &&
               rb->Type != Rigidbody2DComponent::BodyType::Dynamic &&
               rb->Type != Rigidbody2DComponent::BodyType::Kinematic))
        throw std::runtime_error("Invalid physics body type: " + e.GetName());
    if (rb && !std::isfinite(rb->GravityScale))
        throw std::runtime_error("Non-finite gravity scale: " + e.GetName());
    auto material = [&](float density, float friction, float restitution, float threshold) {
        if (!std::isfinite(density) || !std::isfinite(friction) || !std::isfinite(restitution) ||
            !std::isfinite(threshold) || density < 0 || friction < 0 || restitution < 0 ||
            threshold < 0)
            throw std::runtime_error("Invalid physics material: " + e.GetName());
    };
    if (box) {
        material(box->Density, box->Friction, box->Restitution, box->RestitutionThreshold);
        if (!std::isfinite(box->Offset.x) || !std::isfinite(box->Offset.y) ||
            !std::isfinite(box->Size.x * tc.Scale.x) || !std::isfinite(box->Size.y * tc.Scale.y) ||
            box->Size.x * tc.Scale.x <= 0 || box->Size.y * tc.Scale.y <= 0)
            throw std::runtime_error("Invalid box collider dimensions/offset: " + e.GetName());
    }
    if (circle) {
        material(circle->Density, circle->Friction, circle->Restitution,
                 circle->RestitutionThreshold);
        if (std::abs(tc.Scale.x - tc.Scale.y) > 1e-5f * std::max(tc.Scale.x, tc.Scale.y))
            throw std::runtime_error(
                "Circle physics requires uniform XY scale; ellipses are unsupported: " +
                e.GetName());
        if (!std::isfinite(circle->Offset.x) || !std::isfinite(circle->Offset.y) ||
            !std::isfinite(circle->Radius * tc.Scale.x) || circle->Radius * tc.Scale.x <= 0)
            throw std::runtime_error("Invalid circle collider dimensions/offset: " + e.GetName());
    }
}
void Scene::SetLocalTransform(Entity e, const TransformComponent &value) {
    CheckEntity(e);
    Transforms change{{e.GetUUID(), value}};
    // Topology is unchanged. Only this subtree's world matrices can change;
    // graph edits/load/copy retain full ValidateGraph. Avoid rescanning every
    // terrain tile for camera/actor/animation movement.
    for(auto id:GetSubtree(e)) {
        auto entity=GetEntityByUUID(id);
        const auto& local=id==e.GetUUID()?value:entity.GetComponent<TransformComponent>();
        ValidatePhysics(entity,local);
        auto world=World(id,m_Relationships,change);
        if(entity.HasComponent<CameraComponent>() && entity.GetComponent<CameraComponent>().Primary)Finite(glm::inverse(world));
    }
    const auto &accepted = e.GetComponent<TransformComponent>();
    if (e.HasComponent<Rigidbody2DComponent>() &&
        e.GetComponent<Rigidbody2DComponent>().RuntimeBody && value.Scale != accepted.Scale)
        throw std::runtime_error("Runtime physics scale is fixed at body creation; set initial "
                                 "placement before startup");
    e.GetComponent<TransformComponent>() = value;
    if (e.HasComponent<Rigidbody2DComponent>())
        if (auto *body = static_cast<b2Body *>(e.GetComponent<Rigidbody2DComponent>().RuntimeBody))
            body->SetTransform({value.Translation.x, value.Translation.y}, value.Rotation.z);
}
void Scene::SetWorldTransform(Entity e, const glm::mat4 &value) {
    CheckEntity(e);
    const auto parent = GetRelationship(e).Parent;
    auto local = value;
    if (uint64_t(parent)) {
        const auto world = World(parent, m_Relationships, {});
        const auto inverse = glm::inverse(world);
        Finite(inverse);
        local = inverse * value;
    }
    SetLocalTransform(e, ExactTRS(local));
}
void Scene::CheckReparent(UUID child, UUID parent, TransformPolicy mode, Relationships &graph,
                          Transforms &transforms) const {
    if (!IsEntityValid(child) || (uint64_t(parent) && !IsEntityValid(parent)))
        throw std::runtime_error("Reparent target has been destroyed or is missing");
    if (mode != TransformPolicy::KeepWorld && mode != TransformPolicy::KeepLocal)
        throw std::runtime_error("Unknown reparent transform policy");
    graph = m_Relationships;
    if (graph.at(child).Parent == parent)
        return;
    auto old = graph.at(child).Parent;
    graph.at(child).Parent = parent;
    uint32_t next = 0;
    for (const auto &item : graph)
        if (item.first != child && item.second.Parent == parent)
            ++next;
    graph.at(child).Order = next;
    Compact(graph, old);
    ValidateGraph(graph, {}); // Cycles/physics before any inverse or mutation.
    if (mode == TransformPolicy::KeepWorld) {
        const auto world = World(child, m_Relationships, {});
        auto local = world;
        if (uint64_t(parent)) {
            auto inverse = glm::inverse(World(parent, m_Relationships, {}));
            Finite(inverse);
            local = inverse * world;
        }
        transforms[child] = ExactTRS(local);
    }
    ValidateGraph(graph, transforms);
}
void Scene::ApplyReparent(UUID child, UUID parent, TransformPolicy mode) {
    Relationships graph;
    Transforms transforms;
    CheckReparent(child, parent, mode, graph, transforms);
    m_Relationships = std::move(graph);
    for (const auto &item : transforms)
        m_Registry.get<TransformComponent>(m_EntityMap.at(item.first)) = item.second;
    RebuildChildren();
}
uint64_t Scene::Reparent(Entity child, Entity parent, TransformPolicy mode) {
    CheckEntity(child);
    if (!parent.BelongsTo(nullptr))
        CheckEntity(parent);
    if (m_Stopping)
        throw std::runtime_error("Cannot parent while the scene is stopping");
    const UUID target = parent ? parent.GetUUID() : UUID(0);
    Relationships graph;
    Transforms transforms;
    CheckReparent(child.GetUUID(), target, mode, graph, transforms);
    if (!m_IsRunning) {
        ApplyReparent(child.GetUUID(), target, mode);
        return 0;
    }
    if (m_Parenting.size() >= 256)
        throw std::runtime_error("Too many pending parenting requests (256)");
    while (m_ParentingResults.size() >= 128) {
        auto it = std::find_if(
            m_ParentingResults.begin(), m_ParentingResults.end(),
            [](const auto &item) { return item.second.State != ParentingState::Pending; });
        if (it == m_ParentingResults.end())
            break;
        m_ParentingResults.erase(it);
    }
    const auto request = ++m_NextParenting;
    m_ParentingResults[request] = {ParentingState::Pending, {}};
    m_Parenting.push_back({request, child.GetUUID(), target, mode});
    return request;
}
ParentingResult Scene::GetParentingResult(uint64_t request) const {
    CheckThread();
    auto it = m_ParentingResults.find(request);
    return it == m_ParentingResults.end() ? ParentingResult{} : it->second;
}
void Scene::FlushParenting() {
    auto pending = std::move(m_Parenting);
    m_Parenting.clear();
    for (const auto &command : pending)
        try {
            ApplyReparent(command.Child, command.Parent, command.Mode);
            m_ParentingResults[command.Request] = {ParentingState::Applied, {}};
        } catch (const std::exception &e) {
            m_ParentingResults[command.Request] = {ParentingState::Rejected, e.what()};
            HZ_CORE_WARN("Parenting request {} rejected: {}", command.Request, e.what());
        }
    while (m_ParentingResults.size() > 128)
        m_ParentingResults.erase(m_ParentingResults.begin());
}
std::vector<UUID> Scene::OrderedForCleanup() const {
    std::vector<UUID> result, pending;
    auto roots = m_Children.find(UUID(0));
    if (roots != m_Children.end())
        pending.assign(roots->second.rbegin(), roots->second.rend());
    while (!pending.empty()) {
        auto id = pending.back();
        pending.pop_back();
        result.push_back(id);
        auto children = m_Children.find(id);
        if (children != m_Children.end())
            pending.insert(pending.end(), children->second.rbegin(), children->second.rend());
        if (result.size() > MaxEntities)
            throw std::runtime_error("Invalid cleanup hierarchy bounds");
    }
    std::reverse(result.begin(), result.end());
    return result;
}
void Scene::RemoveRelationship(UUID id) {
    const auto it = m_Relationships.find(id);
    if (it == m_Relationships.end())
        return;
    const auto parent = it->second.Parent;
    m_Relationships.erase(it);
    Compact(m_Relationships, parent);
    RebuildChildren();
}
} // namespace Hazel
