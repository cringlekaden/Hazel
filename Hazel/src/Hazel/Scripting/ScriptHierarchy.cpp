#include "hzpch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"
#include "Hazel/Scene/Entity.h"
#include "mono/metadata/appdomain.h"
#include "mono/metadata/exception.h"
#include "mono/metadata/object.h"
#include <cmath>
namespace Hazel {
namespace {
template <typename F> static void HierarchyCall(F &&action) {
    MonoException *failure = nullptr;
    try {
        action();
    } catch (const std::exception &e) {
        failure = mono_get_exception_invalid_operation(e.what());
    }
    // C++ exceptions and their owners have unwound before entering Mono's managed exception path.
    if (failure)
        mono_raise_exception(failure);
}
static Entity HierarchyEntity(uint64_t id) {
    auto *scene = ScriptEngine::GetSceneContext();
    if (!scene)
        throw std::runtime_error("Scene has retired");
    scene->CheckThread();
    auto entity = scene->GetEntityByUUID(id);
    if (!entity)
        throw std::runtime_error("Hierarchy entity has been destroyed");
    return entity;
}
static uint64_t Hierarchy_GetParent(uint64_t id) {
    uint64_t result = 0;
    HierarchyCall([&] {
        auto entity = HierarchyEntity(id);
        result = ScriptEngine::GetSceneContext()->GetRelationship(entity).Parent;
    });
    return result;
}
static MonoArray *Hierarchy_GetChildren(uint64_t id) {
    MonoArray *result = nullptr;
    HierarchyCall([&] {
        auto entity = HierarchyEntity(id);
        auto ids = ScriptEngine::GetSceneContext()->GetChildren(entity.GetUUID());
        result = mono_array_new(mono_domain_get(), mono_get_uint64_class(), ids.size());
        for (size_t i = 0; i < ids.size(); ++i)
            mono_array_set(result, uint64_t, i, uint64_t(ids[i]));
    });
    return result;
}
static uint64_t Hierarchy_SetParent(uint64_t id, uint64_t parent, int mode) {
    uint64_t result = 0;
    HierarchyCall([&] {
        auto entity = HierarchyEntity(id);
        auto target = parent ? HierarchyEntity(parent) : Entity{};
        result = ScriptEngine::GetSceneContext()->Reparent(entity, target, TransformPolicy(mode));
    });
    return result;
}
static int Hierarchy_GetStatus(uint64_t identity, uint64_t request) {
    int result = int(ParentingState::Expired);
    HierarchyCall([&] {
        auto *scene = ScriptEngine::GetSceneContext();
        if (scene && scene->GetIdentity() == identity) {
            scene->CheckThread();
            result = int(scene->GetParentingResult(request).State);
        }
    });
    return result;
}
static MonoString *Hierarchy_GetReason(uint64_t identity, uint64_t request) {
    MonoString *result = nullptr;
    HierarchyCall([&] {
        auto *scene = ScriptEngine::GetSceneContext();
        std::string reason = "Scene retired or parenting result expired";
        if (scene && scene->GetIdentity() == identity) {
            scene->CheckThread();
            auto value = scene->GetParentingResult(request);
            reason = value.State == ParentingState::Expired ? reason : value.Reason;
        }
        result = mono_string_new(mono_domain_get(), reason.c_str());
    });
    return result;
}
static void Transform_GetLocal(uint64_t id, int axis, glm::vec3 *value) {
    HierarchyCall([&] {
        auto &t = HierarchyEntity(id).GetComponent<TransformComponent>();
        if (axis == 0)
            *value = t.Translation;
        else if (axis == 1)
            *value = t.Rotation;
        else if (axis == 2)
            *value = t.Scale;
        else
            throw std::runtime_error("Unknown local transform property");
    });
}
static void Transform_SetLocal(uint64_t id, int axis, glm::vec3 *value) {
    HierarchyCall([&] {
        auto e = HierarchyEntity(id);
        auto t = e.GetComponent<TransformComponent>();
        if (axis == 0)
            t.Translation = *value;
        else if (axis == 1)
            t.Rotation = *value;
        else if (axis == 2)
            t.Scale = *value;
        else
            throw std::runtime_error("Unknown local transform property");
        ScriptEngine::GetSceneContext()->SetLocalTransform(e, t);
    });
}
static void Transform_GetWorldMatrix(uint64_t id, glm::mat4 *matrix) {
    HierarchyCall(
        [&] { *matrix = ScriptEngine::GetSceneContext()->GetWorldTransform(HierarchyEntity(id)); });
}
static void TransformComponent_GetScale(uint64_t id, glm::vec3 *scale) {
    HierarchyCall([&] {
        auto e = HierarchyEntity(id);
        auto *scene = ScriptEngine::GetSceneContext();
        *scale = !uint64_t(scene->GetRelationship(e).Parent)
                     ? e.GetComponent<TransformComponent>().Scale
                     : Scene::ExactTRS(scene->GetWorldTransform(e)).Scale;
    });
}
static void TransformComponent_SetScale(uint64_t id, glm::vec3 *scale) {
    HierarchyCall([&] {
        auto e = HierarchyEntity(id);
        if (e.HasComponent<Rigidbody2DComponent>())
            throw std::runtime_error("Set physics scale in initial placement before startup");
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite((*scale)[i]) || (*scale)[i] <= 0)
                throw std::runtime_error("World scale must be finite and positive");
        auto *scene = ScriptEngine::GetSceneContext();
        if (!uint64_t(scene->GetRelationship(e).Parent)) {
            auto t = e.GetComponent<TransformComponent>();
            t.Scale = *scale;
            scene->SetLocalTransform(e, t);
        } else {
            auto t = Scene::ExactTRS(scene->GetWorldTransform(e));
            t.Scale = *scale;
            scene->SetWorldTransform(e, t.GetTransform());
        }
    });
}
static void TransformComponent_GetTranslation(UUID id, glm::vec3 *value) {
    HierarchyCall([&] {
        auto e = HierarchyEntity(id);
        *value = glm::vec3(ScriptEngine::GetSceneContext()->GetWorldTransform(e)[3]);
    });
}
static void TransformComponent_SetTranslation(UUID id, glm::vec3 *value) {
    HierarchyCall([&] {
        auto e = HierarchyEntity(id);
        auto *scene = ScriptEngine::GetSceneContext();
        auto t = e.GetComponent<TransformComponent>();
        auto parent = scene->GetRelationship(e).Parent;
        t.Translation =
            uint64_t(parent)
                ? glm::vec3(glm::inverse(scene->GetWorldTransform(scene->GetEntityByUUID(parent))) *
                            glm::vec4(*value, 1))
                : *value;
        scene->SetLocalTransform(e, t);
    });
}

} // namespace
void ScriptGlue::RegisterHierarchyFunctions() {
#define HZ_HIERARCHY_CALL(Name)                                                                    \
    mono_add_internal_call("Hazel.InternalCalls::" #Name, reinterpret_cast<const void *>(Name))
    HZ_HIERARCHY_CALL(Hierarchy_GetParent);
    HZ_HIERARCHY_CALL(Hierarchy_GetChildren);
    HZ_HIERARCHY_CALL(Hierarchy_SetParent);
    HZ_HIERARCHY_CALL(Hierarchy_GetStatus);
    HZ_HIERARCHY_CALL(Hierarchy_GetReason);
    HZ_HIERARCHY_CALL(Transform_GetLocal);
    HZ_HIERARCHY_CALL(Transform_SetLocal);
    HZ_HIERARCHY_CALL(Transform_GetWorldMatrix);
    HZ_HIERARCHY_CALL(TransformComponent_GetTranslation);
    HZ_HIERARCHY_CALL(TransformComponent_GetScale);
    HZ_HIERARCHY_CALL(TransformComponent_SetScale);
    HZ_HIERARCHY_CALL(TransformComponent_SetTranslation);
#undef HZ_HIERARCHY_CALL
}
} // namespace Hazel
