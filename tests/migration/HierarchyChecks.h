#pragma once
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel {
static bool Near(const glm::mat4 &a, const glm::mat4 &b) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (std::abs(a[c][r] - b[c][r]) > 1e-3f)
                return false;
    return true;
}
static void HierarchyChecks() {
    auto scene = CreateRef<Scene>();
    auto parent = scene->CreateEntityWithUUID(101, "Parent");
    auto child = scene->CreateEntityWithUUID(102, "Child");
    auto leaf = scene->CreateEntityWithUUID(103, "Leaf");
    parent.GetComponent<TransformComponent>().Translation = {3, 2, 0};
    child.GetComponent<TransformComponent>().Translation = {1, 0, 0};
    leaf.GetComponent<TransformComponent>().Translation = {0, 4, 0};
    scene->Reparent(child, parent, TransformPolicy::KeepLocal);
    scene->Reparent(leaf, child, TransformPolicy::KeepLocal);
    Check(glm::length(glm::vec3(scene->GetWorldTransform(leaf)[3]) - glm::vec3(4, 6, 0)) < 1e-4f,
          "Nested local transforms did not compose to world");
    Check(scene->GetChildren() == std::vector<UUID>{101} &&
              scene->GetChildren(101) == std::vector<UUID>{102},
          "Ordered roots/children incorrect");
    const auto authored = SceneSerializer(scene).SerializeAuthoredSnapshot();
    for (auto candidate : {parent, child, leaf}) {
        bool reject = false;
        try {
            scene->Reparent(candidate, leaf, TransformPolicy::KeepLocal);
        } catch (const std::exception &) {
            reject = true;
        }
        Check(reject && SceneSerializer(scene).SerializeAuthoredSnapshot() == authored,
              "Cycle/self rejection mutated hierarchy");
    }
    auto foreign = CreateRef<Scene>();
    auto alien = foreign->CreateEntityWithUUID(101, "Other scene");
    bool reject = false;
    try {
        scene->Reparent(child, alien, TransformPolicy::KeepLocal);
    } catch (const std::exception &) {
        reject = true;
    }
    Check(reject && SceneSerializer(scene).SerializeAuthoredSnapshot() == authored,
          "Cross-scene parenting mutated source");
    const auto before = scene->GetWorldTransform(child);
    scene->Reparent(child, {}, TransformPolicy::KeepWorld);
    Check(Near(before, scene->GetWorldTransform(child)) &&
              !uint64_t(scene->GetRelationship(child).Parent),
          "Keep World detach moved subtree");
    scene->Reparent(child, parent, TransformPolicy::KeepWorld);
    Check(Near(before, scene->GetWorldTransform(child)), "Keep World reparent moved entity");
    auto copy = Scene::Copy(scene);
    copy->GetEntityByUUID(101).GetComponent<TransformComponent>().Translation.x = 10;
    Check(copy->GetRelationship(copy->GetEntityByUUID(102)).Parent == UUID(101) &&
              glm::vec3(scene->GetWorldTransform(leaf)[3]) !=
                  glm::vec3(copy->GetWorldTransform(copy->GetEntityByUUID(103))[3]),
          "Play copy hierarchy/transforms alias authored scene");
    const auto text = SceneSerializer(scene).SerializeText();
    auto loaded = CreateRef<Scene>();
    SceneSerializer reader(loaded);
    Check(reader.DeserializeText(text) &&
              Near(loaded->GetWorldTransform(loaded->GetEntityByUUID(103)),
                   scene->GetWorldTransform(leaf)),
          "Versioned hierarchy save/reopen changed world transform");
    const auto retained = reader.SerializeAuthoredSnapshot();
    for (const auto &bad :
         {"{Parent: 102, Order: 0}", "{Parent: 9999, Order: 0}", "{Parent: 0, Order: 17}"}) {
        auto node = YAML::Load(text);
        for (auto n : node["Entities"])
            if (n["Entity"].as<uint64_t>() == 101)
                n["Relationship"] = YAML::Load(bad);
        YAML::Emitter out;
        out << node;
        Check(!reader.DeserializeText(out.c_str()) &&
                  reader.SerializeAuthoredSnapshot() == retained,
              "Malformed graph replaced valid scene");
    }
    auto future = YAML::Load(text);
    future["SceneVersion"] = 99;
    YAML::Emitter out;
    out << future;
    Check(!reader.DeserializeText(out.c_str()) && reader.SerializeAuthoredSnapshot() == retained,
          "Future hierarchy data replaced valid scene");
    auto legacy = CreateRef<Scene>();
    Check(SceneSerializer(legacy).DeserializeText(
              "Scene: Legacy\nEntities: [{Entity: 7, TransformComponent: {Translation: [2, 3, 0], "
              "Rotation: [0, 0, 0], Scale: [1, 1, 1]}}]\n") &&
              glm::vec3(legacy->GetWorldTransform(legacy->GetEntityByUUID(7))[3]) ==
                  glm::vec3(2, 3, 0),
          "Legacy flat scene did not remain a root at original world position");
    auto singular = scene->CreateEntity("Singular");
    singular.GetComponent<TransformComponent>().Scale.x = 0;
    const auto keep = SceneSerializer(scene).SerializeAuthoredSnapshot();
    reject = false;
    try {
        scene->Reparent(child, singular, TransformPolicy::KeepWorld);
    } catch (const std::exception &) {
        reject = true;
    }
    Check(reject && SceneSerializer(scene).SerializeAuthoredSnapshot() == keep,
          "Singular Keep World operation mutated draft");
    auto skew = CreateRef<Scene>();
    auto stretch = skew->CreateEntity("Stretch");
    auto rotated = skew->CreateEntity("Rotated");
    stretch.GetComponent<TransformComponent>().Scale = {2, 1, 1};
    rotated.GetComponent<TransformComponent>().Rotation.z = .6f;
    skew->Reparent(rotated, stretch, TransformPolicy::KeepLocal);
    const auto exact = stretch.GetComponent<TransformComponent>().GetTransform() *
                       rotated.GetComponent<TransformComponent>().GetTransform();
    Check(Near(skew->GetWorldTransform(rotated), exact), "Visual shear was silently approximated");
    const auto skewDraft = SceneSerializer(skew).SerializeAuthoredSnapshot();
    reject = false;
    try {
        skew->Reparent(rotated, {}, TransformPolicy::KeepWorld);
    } catch (const std::exception &) {
        reject = true;
    }
    Check(reject && SceneSerializer(skew).SerializeAuthoredSnapshot() == skewDraft,
          "Sheared Keep World detach approximated/mutated data");
    reject = false;
    try {
        skew->DestroyEntity(stretch, DestroyPolicy::KeepChildren);
    } catch (const std::exception &) {
        reject = true;
    }
    Check(reject && SceneSerializer(skew).SerializeAuthoredSnapshot() == skewDraft,
          "Delete/keep children partially detached on failure");
    skew->DestroyEntity(stretch, DestroyPolicy::KeepChildren, TransformPolicy::KeepLocal);
    Check(!stretch && rotated && !uint64_t(skew->GetRelationship(rotated).Parent),
          "Explicit Keep Local children deletion failed");
    auto body = scene->CreateEntity("Body");
    body.AddComponent<Rigidbody2DComponent>();
    reject = false;
    try {
        scene->Reparent(body, parent, TransformPolicy::KeepLocal);
    } catch (const std::exception &) {
        reject = true;
    }
    Check(reject && !uint64_t(scene->GetRelationship(body).Parent),
          "Parented physics owner accepted");
    reject = false;
    try {
        child.AddComponent<BoxCollider2DComponent>();
    } catch (const std::exception &) {
        reject = true;
    }
    Check(reject && !child.HasComponent<BoxCollider2DComponent>(),
          "Child collider addition mutated entity");
    scene->DestroyEntity(parent);
    Check(!scene->GetEntityByUUID(101) && !scene->GetEntityByUUID(102) &&
              !scene->GetEntityByUUID(103) && body,
          "Subtree deletion orphaned members or deleted unrelated root");
    std::cout << "PASS: hierarchy ordered roots/local-world/TRS, cycle/cross-scene atomic "
                 "rejection, save/reopen/legacy/future retention, independent Play copy, "
                 "shear/singular failure, subtree/keep-child deletion and root-only physics\n";
}
} // namespace Hazel
