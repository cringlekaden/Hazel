#include "hzpch.h"
#include "Prefab.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "RuntimeSession.h"
#include "SceneSerializer.h"
#include <yaml-cpp/yaml.h>
#include <set>
namespace Hazel {
std::filesystem::path Prefab::Resolve(const std::filesystem::path &root,
                                      const std::filesystem::path &reference) {
    auto relative = Project::NormalizeAssetPath(reference);
    if (relative.extension() != ".hprefab")
        throw std::runtime_error("Select a project-relative .hprefab asset");
    return Project::ResolveOwnedAsset(root, relative);
}
Entity Prefab::GetEntity(const Ref<Scene> &scene) {
    if (!scene)
        throw std::runtime_error("Prefab editing scene unavailable");
    const auto roots = scene->GetChildren();
    if (roots.size() != 1)
        throw std::runtime_error("Detached prefab requires exactly one root");
    auto root = scene->GetEntityByUUID(roots.front());
    if (scene->GetSubtree(root).size() != scene->GetAllEntitiesWith<IDComponent>().size())
        throw std::runtime_error("Prefab must be one connected subtree");
    return root;
}
void Prefab::Validate(const std::filesystem::path &root, const Ref<Scene> &scene,
                      bool requireResources) {
    scene->ValidateHierarchy();
    auto entity = GetEntity(scene);
    const auto ids = scene->GetSubtree(entity);
    std::set<uint64_t> local;
    for (auto id : ids)
        local.insert(id);
    for (auto id : ids) {
        auto node = scene->GetEntityByUUID(id);
        if (node.HasComponent<NativeScriptComponent>())
            throw std::runtime_error("Native script factories cannot be serialized into prefabs");
        for (auto &[name, field] : ScriptEngine::GetScriptFieldMap(node)) {
            if (field.Field.Type == ScriptFieldType::Entity) {
                auto value = field.GetValue<uint64_t>();
                if (value && !local.count(value))
                    throw std::runtime_error("External entity reference '" + name + "' on '" +
                                             node.GetName() +
                                             "'. Clear it or include the entity in this subtree");
            }
            if (field.Field.Type == ScriptFieldType::Prefab && !field.AssetReference.empty()) {
                auto path = Resolve(root, std::filesystem::u8path(field.AssetReference));
                if (requireResources && !std::filesystem::is_regular_file(path))
                    throw std::runtime_error("Missing prefab reference: " + field.AssetReference);
            }
        }
        if (node.HasComponent<SpriteRendererComponent>()) {
            const auto &source = node.GetComponent<SpriteRendererComponent>().Source;
            if (auto t = std::get_if<TextureSpriteSource>(&source))
                Project::ResolveOwnedAsset(root, t->Texture);
            if (auto r = std::get_if<SpriteReference>(&source))
                Project::ResolveOwnedAsset(root, r->Sheet);
        }
    }
    if (requireResources)
        RuntimeSession::Validate(
            scene, false); // Class availability is project/runtime readiness, not prefab structure.
}
Ref<Scene> Prefab::Load(const std::filesystem::path &root, const std::filesystem::path &reference,
                        bool repair, ResourceLoading resources) {
    const auto text = FileDocument::Read(Resolve(root, reference));
    auto data = YAML::Load(text);
    DocumentSchema::Scene(data, true);
    auto scene = CreateRef<Scene>();
    SceneSerializer loader(scene, root, repair);
    if (!loader.DeserializeText(text, true, resources))
        throw std::runtime_error(loader.Report().Error);
    Validate(root, scene, !repair && resources == ResourceLoading::Resolve);
    return scene;
}
std::string Prefab::Serialize(const std::filesystem::path &root, const Ref<Scene> &scene,
                              Entity entity, bool allowBroken) {
    if (!scene || !entity.BelongsTo(scene.get()) || !entity || scene->IsRunning())
        throw std::runtime_error("Select an authored subtree before creating a prefab");
    // A child selected as a detached root must preserve its world placement exactly.
    auto staged = Scene::ExtractSubtree(entity);
    Validate(root, staged, !allowBroken);
    auto rootEntity = GetEntity(staged);
    auto text = std::string("PrefabVersion: 2\nPrefabRoot: ") +
                std::to_string(uint64_t(rootEntity.GetUUID())) + "\n" +
                SceneSerializer(staged, root).SerializeText();
    DocumentSchema::Scene(YAML::Load(text), true);
    return text;
}
void Prefab::Save(const std::filesystem::path &root, const std::filesystem::path &reference,
                  const Ref<Scene> &scene, Entity entity, bool allowBroken, WriteMode mode) {
    auto path = Resolve(root, reference);
    auto text = Serialize(root, scene, entity, allowBroken);
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path, [&](auto &out) { out << text; }, mode);
}
Entity Prefab::Instantiate(const std::filesystem::path &root,
                           const std::filesystem::path &reference, Scene &target) {
    if (target.GetAssets() && target.GetAssets()->Root() != std::filesystem::weakly_canonical(root))
        throw std::runtime_error("Prefab assets must belong to the destination scene's project");
    auto staged = Load(root, reference);
    auto source = GetEntity(staged);
    return target.InstantiateEntity(source, source.GetComponent<TransformComponent>());
}
Entity Prefab::Instantiate(const std::filesystem::path &root,
                           const std::filesystem::path &reference, Scene &target,
                           const TransformComponent &transform, bool replaceRotationAndScale) {
    if (target.GetAssets() && target.GetAssets()->Root() != std::filesystem::weakly_canonical(root))
        throw std::runtime_error("Prefab assets must belong to the destination scene's project");
    auto staged = Load(root, reference);
    auto source = GetEntity(staged);
    auto initial = source.GetComponent<TransformComponent>();
    if (replaceRotationAndScale)
        initial = transform;
    else
        initial.Translation = transform.Translation;
    staged->SetLocalTransform(source, initial);
    return target.InstantiateEntity(source, initial);
}
} // namespace Hazel
