#pragma once
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
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

namespace Hazel {
static void SubtreePrefabChecks() {
    auto scene = CreateRef<Scene>();
    auto root = scene->CreateEntityWithUUID(300, "Assembly");
    auto child = scene->CreateEntityWithUUID(301, "Lamp");
    auto grandchild = scene->CreateEntityWithUUID(302, "Spark");
    root.GetComponent<TransformComponent>().Translation = {2, 3, 0};
    child.GetComponent<TransformComponent>().Translation = {1, 0, 0};
    grandchild.GetComponent<TransformComponent>().Translation = {0, 2, 0};
    scene->Reparent(child, root, TransformPolicy::KeepLocal);
    scene->Reparent(grandchild, child, TransformPolicy::KeepLocal);
    root.AddComponent<ScriptComponent>().ClassName = "Uncompiled.Assembly";
    child.AddComponent<ScriptComponent>().ClassName = "Uncompiled.Lamp";
    auto setRef = [](Entity e, const char *name, uint64_t value) {
        auto &field = ScriptEngine::GetScriptFieldMap(e)[name];
        field.Field = {ScriptFieldType::Entity, name, nullptr};
        field.SetValue<uint64_t>(value);
    };
    setRef(root, "Lamp", 301);
    setRef(child, "Assembly", 300);
    setRef(child, "Spark", 302);
    auto external = scene->CreateEntityWithUUID(399, "External");
    setRef(root, "External", 399);
    auto duplicate = scene->DuplicateEntity(root);
    auto duplicateChild = scene->GetEntityByUUID(scene->GetChildren(duplicate.GetUUID()).front());
    Check(scene->GetSubtree(duplicate).size() == 3 &&
              ScriptEngine::GetScriptFieldMap(duplicate).at("Lamp").GetValue<uint64_t>() ==
                  uint64_t(duplicateChild.GetUUID()) &&
              ScriptEngine::GetScriptFieldMap(duplicate).at("External").GetValue<uint64_t>() ==
                  399 &&
              scene->GetRelationship(duplicate).Order == scene->GetRelationship(root).Order + 1,
          "Subtree duplicate lost internal/external remap or sibling order");
    duplicateChild.GetComponent<TransformComponent>().Translation.x = 9;
    Check(child.GetComponent<TransformComponent>().Translation.x == 1,
          "Duplicate subtree aliases original authored state");
    const auto before = SceneSerializer(scene).SerializeAuthoredSnapshot();
    bool rejected = false;
    const auto folder =
        std::filesystem::temp_directory_path() /
        std::filesystem::u8path("hazel subtree-é-" + std::to_string(uint64_t(UUID())));
    std::filesystem::create_directories(folder);
    struct Cleanup {
        std::filesystem::path Folder;
        ~Cleanup() {
            std::error_code e;
            std::filesystem::remove_all(Folder, e);
        }
    } cleanup{folder};
    try {
        Prefab::Serialize(folder, scene, root, true);
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected && SceneSerializer(scene).SerializeAuthoredSnapshot() == before,
          "External prefab reference was redirected/cleared or changed source");
    ScriptEngine::GetScriptFieldMap(root).erase("External");
    Prefab::Save(folder, "Assembly.hprefab", scene, root, true, WriteMode::CreateNew);
    const auto saved = FileDocument::Read(folder / "Assembly.hprefab");
    auto asset = Prefab::Load(folder, "Assembly.hprefab", true, ResourceLoading::MetadataOnly);
    Check(YAML::Load(saved)["PrefabVersion"].as<int>() == 2 &&
              asset->GetSubtree(Prefab::GetEntity(asset)).size() == 3,
          "Detached prefab v2 lost connected subtree");
    auto target = CreateRef<Scene>();
    target->CreateEntityWithUUID(300, "Same numeric UUID, unrelated");
    auto first = Prefab::Instantiate(folder, "Assembly.hprefab", *target);
    auto second = Prefab::Instantiate(folder, "Assembly.hprefab", *target);
    auto firstChild = target->GetEntityByUUID(target->GetChildren(first.GetUUID()).front());
    auto secondChild = target->GetEntityByUUID(target->GetChildren(second.GetUUID()).front());
    Check(first.GetUUID() != second.GetUUID() && firstChild.GetUUID() != secondChild.GetUUID() &&
              target->GetSubtree(first).size() == 3 &&
              ScriptEngine::GetScriptFieldMap(first).at("Lamp").GetValue<uint64_t>() ==
                  uint64_t(firstChild.GetUUID()) &&
              ScriptEngine::GetScriptFieldMap(firstChild).at("Assembly").GetValue<uint64_t>() ==
                  uint64_t(first.GetUUID()) &&
              ScriptEngine::GetScriptFieldMap(secondChild).at("Assembly").GetValue<uint64_t>() ==
                  uint64_t(second.GetUUID()),
          "Repeated prefab instances share identities or bind to unrelated destination entity");
    target->DestroyEntity(first);
    Check(!firstChild && secondChild && target->GetEntityByUUID(300),
          "Deleting one instance damaged another or left its subtree");
    auto old = YAML::Load(SceneSerializer(scene).SerializeText(root));
    old["PrefabVersion"] = 1;
    old["SceneVersion"] = 1;
    old["Entities"][0].remove("Relationship");
    // Legacy v1 remains single-entity; clear entity references outside that asset explicitly in the
    // fixture.
    old["Entities"][0]["ScriptComponent"].remove("ScriptFields");
    YAML::Emitter legacy;
    legacy << old;
    FileSystem::WriteNewFile(folder / "Legacy.hprefab", legacy.c_str());
    Check(Prefab::Load(folder, "Legacy.hprefab", true, ResourceLoading::MetadataOnly)
                  ->GetAllEntitiesWith<IDComponent>()
                  .size() == 1,
          "Single-entity prefab compatibility lost");
    auto invalid = YAML::Load(saved);
    invalid["PrefabRoot"] = 399;
    YAML::Emitter broken;
    broken << invalid;
    FileSystem::WriteNewFile(folder / "Bad.hprefab", broken.c_str());
    const auto retained = SceneSerializer(target).SerializeAuthoredSnapshot();
    rejected = false;
    try {
        Prefab::Instantiate(folder, "Bad.hprefab", *target);
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected && SceneSerializer(target).SerializeAuthoredSnapshot() == retained,
          "Malformed prefab partially inserted into target");
    auto body = scene->CreateEntity("Physics");
    body.AddComponent<Rigidbody2DComponent>();
    body.AddComponent<CircleCollider2DComponent>();
    auto transform = body.GetComponent<TransformComponent>();
    transform.Scale.y = 2;
    rejected = false;
    try {
        scene->SetLocalTransform(body, transform);
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected && body.GetComponent<TransformComponent>().Scale == glm::vec3(1),
          "Circle nonuniform scale was approximated or published");
    transform = body.GetComponent<TransformComponent>();
    transform.Rotation.x = .1f;
    rejected = false;
    try {
        scene->SetLocalTransform(body, transform);
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected && body.GetComponent<TransformComponent>().Rotation == glm::vec3(0),
          "Nonplanar Box2D transform was published");
    std::cout << "PASS: subtree duplication/internal/external remap, independent v2 prefab "
                 "instances, legacy v1 import, source/destination failure retention and "
                 "planar/uniform-circle physics\n";
}
} // namespace Hazel
