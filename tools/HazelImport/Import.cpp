#include "Import.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileLease.h"
#include "Hazel/Core/OperationProgress.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Prefab.h"
#include <algorithm>
#include <set>
#include <sstream>
namespace Hazel::Import
{
namespace
{
void Require(bool yes, const std::string &why)
{
    if (!yes)
        throw std::runtime_error(why);
}
void Put(const std::filesystem::path &path, const std::string &bytes)
{
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteNewFile(path, bytes);
}
struct OwnedStage
{
    std::filesystem::path Path;
    explicit OwnedStage(const std::filesystem::path &parent)
    {
        Path = parent / ("stage-" + AssetKey(UUID()));
        Require(std::filesystem::create_directory(Path),
                "Cannot reserve exclusive import staging directory");
        Put(Path / "owner", "HazelImport.StaticMesh_v1\n");
    }
    ~OwnedStage()
    {
        std::error_code ec;
        std::filesystem::remove_all(Path, ec);
    }
};
uint64_t NewID(std::set<uint64_t> &ids)
{
    for (int i = 0; i < 32; ++i)
    {
        uint64_t id = UUID();
        if (id && ids.insert(id).second)
            return id;
    }
    throw std::runtime_error("Cannot allocate unique import identity");
}
std::string MaterialSource(const ImportedMaterialData &in,
                           const std::vector<ImportedImageData> &images)
{
    auto material = in.Values;
    material.Name = "Source semantic values";
    std::string evidence;
    for (unsigned i = 0; i < 5; ++i)
        if (in.Images[i] >= 0)
        {
            auto bytes = CookImage(images.at(size_t(in.Images[i])));
            evidence += std::to_string(i) + AssetHash(bytes);
        }
    return AssetHash(WriteMaterialText(material) + evidence);
}
std::string PrimitiveSource(const GeometryData &mesh, const MeshPrimitive &p,
                            const std::string &material)
{
    auto begin = mesh.Indices.begin() + p.FirstIndex, end = begin + p.IndexCount;
    auto range = std::minmax_element(begin, end);
    auto first = *range.first, last = *range.second;
    GeometryData isolated;
    isolated.Mesh = 1;
    isolated.Vertices.assign(mesh.Vertices.begin() + first, mesh.Vertices.begin() + last + 1);
    for (auto i = begin; i != end; ++i)
        isolated.Indices.push_back(*i - first);
    isolated.Primitives.push_back({1, 0, p.IndexCount, p.Attributes, {}});
    isolated.UpdateBounds();
    return AssetHash(EncodeGeometry(isolated) + material);
}
std::string MeshSource(std::vector<std::string> primitives)
{
    std::sort(primitives.begin(), primitives.end());
    std::string evidence;
    for (auto &value : primitives)
        evidence += value;
    return AssetHash(evidence);
}
std::string NodeSource(const ImportedSceneData &d, const std::vector<ImportMesh> &meshes,
                       std::vector<std::string> &signatures)
{
    signatures.resize(d.Nodes.size());
    // Order/name changes do not redirect native identities. Changed transforms,
    // topology or mesh assignments conflict with the detached authored prefab.
    std::function<std::string(int)> node = [&](int index) {
        auto &n = d.Nodes.at(size_t(index));
        std::vector<std::string> children;
        for (auto child : n.Children)
            children.push_back(node(child));
        std::sort(children.begin(), children.end());
        std::ostringstream values;
        values.precision(9);
        values << "Node_v1 ";
        for (auto v : {n.Translation, n.Rotation, n.Scale})
            for (int i = 0; i < 3; ++i)
                values << v[i] << ' ';
        values << (n.Mesh < 0 ? std::string("none") : AssetKey(meshes.at(size_t(n.Mesh)).ID));
        for (auto &child : children)
            values << child;
        return signatures[size_t(index)] = AssetHash(values.str());
    };
    std::vector<std::string> roots;
    for (auto i : d.Roots)
        roots.push_back(node(i));
    std::sort(roots.begin(), roots.end());
    std::string values;
    for (auto &root : roots)
        values += root;
    return AssetHash(values);
}
std::string PrefabBytes(const ImportedSceneData &data, const ImportManifest &manifest,
                        const std::filesystem::path &assets)
{
    auto scene = CreateRef<Scene>();
    scene->SetName("Imported geometry metadata");
    auto root = scene->CreateEntityWithUUID(UUID(manifest.PrefabRoot), "Imported model");
    std::vector<Entity> nodes;
    nodes.reserve(data.Nodes.size());
    for (size_t i = 0; i < data.Nodes.size(); ++i)
    {
        auto &n = data.Nodes[i];
        auto e = scene->CreateEntityWithUUID(UUID(manifest.Nodes.at(i).ID), n.Name);
        auto &t = e.GetComponent<TransformComponent>();
        t.Translation = n.Translation;
        t.Rotation = n.Rotation;
        t.Scale = n.Scale;
        if (n.Mesh >= 0)
            e.AddComponent<MeshRendererComponent>().Mesh =
                manifest.Meshes.at(size_t(n.Mesh)).Output;
        nodes.push_back(e);
    }
    for (size_t i = 0; i < data.Nodes.size(); ++i)
        for (auto child : data.Nodes[i].Children)
            scene->Reparent(nodes.at(size_t(child)), nodes[i], TransformPolicy::KeepLocal);
    for (auto i : data.Roots)
        scene->Reparent(nodes.at(size_t(i)), root, TransformPolicy::KeepLocal);
    return Prefab::Serialize(assets, scene, root, true);
}
void CheckSource(const Request &r, const std::string &bytes)
{
    Require(ReadAssetBytes(r.Source, 67108864) == bytes,
            "Source changed during import; retry after saving the source. Accepted generation "
            "retained");
}
void CheckMappings(const std::filesystem::path &root, const std::filesystem::path &manifestPath,
                   const ImportManifest &m)
{
    auto base = manifestPath.parent_path();
    Require(!base.empty(), "Import manifest must live in its owned folder");
    auto owned = [&](const auto &p) {
        auto rel = p.lexically_relative(base);
        Require(!rel.empty() && !rel.has_root_path() && *rel.begin() != "..",
                "Import output escapes its owned destination");
        Project::ResolveOwnedAsset(root, p);
    };
    owned(m.Source);
    owned(m.Prefab);
    for (auto &material : m.Materials)
        owned(material.Output);
    for (auto &mesh : m.Meshes)
    {
        owned(mesh.Output);
        owned(mesh.Geometry);
        Require(mesh.Geometry.parent_path() == base / "generations" / AssetKey(m.Generation),
                "Geometry is outside its accepted immutable generation");
    }
}
} // namespace
Result ImportGLB(const Request &r)
{
    auto bytes = ReadAssetBytes(r.Source, 67108864);
    auto data = ReadGLB(bytes, r.Scene, r.Cancelled);
    return Publish(r, std::move(data), bytes, false);
}
Result ReimportGLB(const std::filesystem::path &assets, const std::filesystem::path &reference,
                   const Cancel &cancel)
{
    AssetPath(reference, ".himport");
    auto m = ReadImport(Project::ResolveOwnedAsset(assets, reference));
    CheckMappings(assets, reference, m);
    Require(reference.filename() == "Import.himport",
            "Use the import folder's Import.himport manifest");
    Request r{assets, Project::ResolveOwnedAsset(assets, m.Source), reference.parent_path(),
              m.Scene, cancel};
    auto bytes = ReadAssetBytes(r.Source, 67108864);
    auto data = ReadGLB(bytes, r.Scene, cancel);
    return Publish(r, std::move(data), bytes, true);
}
Result Publish(const Request &r, ImportedSceneData d, const std::string &sourceBytes, bool reimport)
{
    ValidateImported(d);
    CheckCancelled(r.Cancelled);
    Require(r.Assets.is_absolute() && r.Source.is_absolute(),
            "Import requires absolute asset/source locations");
    AssetPath(r.Destination);
    Require(!r.Destination.empty() && r.Destination.begin()->generic_u8string() != "Intermediates",
            "Choose a content destination outside Intermediates");
    auto destination = Project::ResolveOwnedAsset(r.Assets, r.Destination);
    auto manifestPath = r.Destination / "Import.himport";
    Require(reimport ? std::filesystem::is_directory(destination)
                     : !std::filesystem::exists(destination),
            "Import destination already exists; use explicit Reimport or choose a new folder");
    auto working =
        Project::ResolveOwnedAsset(r.Assets, std::filesystem::path("Intermediates/NativeImports") /
                                                 AssetHash(r.Destination.generic_u8string()));
    std::filesystem::create_directories(working);
    auto lease = FileLease::Try(working / "operation.lock");
    Require(bool(lease), "Another import owns this destination; wait or cancel that operation");
    // Only clean our marked temporary stages under this destination's lease.
    for (auto &entry : std::filesystem::directory_iterator(working))
        if (entry.is_directory() &&
            entry.path().filename().generic_u8string().rfind("stage-", 0) == 0 &&
            std::filesystem::is_regular_file(entry.path() / "owner"))
            if (ReadAssetBytes(entry.path() / "owner", 128) == "HazelImport.StaticMesh_v1\n")
                std::filesystem::remove_all(entry.path());
    OwnedStage stage(working);
    const auto stagedRoot = stage.Path / "assets";
    std::filesystem::create_directory(stagedRoot);
    ImportManifest old, m;
    FileDocument manifestFile;
    std::map<std::filesystem::path, std::string> authored;
    if (reimport)
    {
        manifestFile.Open(Project::ResolveOwnedAsset(r.Assets, manifestPath));
        old = ReadImportText(manifestFile.Original());
        CheckMappings(r.Assets, manifestPath, old);
        Require(old.Producer == d.Adapter + "/Mikk-" HZ_IMPORT_MIKK_COMMIT,
                "Import producer revision changed; import into a new folder to inspect/reconcile "
                "the new recipe");
        Require(old.Source == r.Destination / "Source.glb" && old.Scene == r.Scene,
                "Import source/options conflict; import into a new destination");
        for (auto &material : old.Materials)
        {
            authored[material.Output] =
                ReadAssetBytes(Project::ResolveOwnedAsset(r.Assets, material.Output), 65536);
            AuditMaterial(r.Assets, ReadMaterialText(authored.at(material.Output)));
        }
        for (auto &mesh : old.Meshes)
        {
            authored[mesh.Output] =
                ReadAssetBytes(Project::ResolveOwnedAsset(r.Assets, mesh.Output), 4 * 1024 * 1024);
            AuditMesh(r.Assets, mesh.Output);
        }
        authored[old.Prefab] =
            ReadAssetBytes(Project::ResolveOwnedAsset(r.Assets, old.Prefab), 64 * 1024 * 1024);
        Prefab::Load(r.Assets, old.Prefab, false, ResourceLoading::MetadataOnly);
        if (old.SourceHash == AssetHash(sourceBytes))
        {
            CheckSource(r, sourceBytes);
            return {manifestPath, old.Prefab, old.Generation, true};
        }
    }
    std::set<uint64_t> ids;
    for (auto &material : old.Materials)
        ids.insert(material.ID);
    for (auto &mesh : old.Meshes)
    {
        ids.insert(mesh.ID);
        for (auto &slot : mesh.Slots)
            ids.insert(slot.ID);
    }
    if (old.PrefabRoot)
        ids.insert(old.PrefabRoot);
    for (auto &n : old.Nodes)
        ids.insert(n.ID);
    if (old.ID)
        ids.insert(old.ID);
    if (old.Generation)
        ids.insert(old.Generation);
    m.ID = reimport ? old.ID : NewID(ids);
    m.PrefabRoot = reimport ? old.PrefabRoot : NewID(ids);
    m.Generation = NewID(ids);
    m.Source = r.Destination / "Source.glb";
    m.SourceHash = AssetHash(sourceBytes);
    m.Producer = d.Adapter + "/Mikk-" HZ_IMPORT_MIKK_COMMIT;
    m.Scene = r.Scene;
    m.Prefab = r.Destination / "Model.hprefab";
    std::vector<std::string> materialSources;
    for (auto &material : d.Materials)
        materialSources.push_back(MaterialSource(material, d.Images));
    bool needsDefault = false;
    for (auto &mesh : d.Meshes)
        for (auto &p : mesh.Primitives)
            needsDefault |= p.Material < 0;
    if (needsDefault)
    {
        ImportedMaterialData def;
        def.SourceKey = "default";
        def.Values.Name = "Default glTF material";
        def.Values.Metallic = def.Values.Roughness = 1;
        d.Materials.push_back(def);
        materialSources.push_back(MaterialSource(def, d.Images));
        for (auto &mesh : d.Meshes)
            for (auto &p : mesh.Primitives)
                if (p.Material < 0)
                    p.Material = int(d.Materials.size() - 1);
    }
    std::set<uint64_t> matchedMaterials;
    for (size_t i = 0; i < d.Materials.size(); ++i)
    {
        auto source = materialSources[i];
        ImportMaterial value;
        if (reimport)
        {
            std::vector<const ImportMaterial *> matches;
            for (auto &candidate : old.Materials)
                if (candidate.Source == source)
                    matches.push_back(&candidate);
            Require(matches.size() == 1 && matchedMaterials.insert(matches[0]->ID).second,
                    "Reimport conflict: changed/ambiguous material semantics or duplicate source "
                    "materials. Existing authored materials retained; import into a new folder to "
                    "reconcile");
            value = *matches[0];
        }
        else
        {
            value.ID = NewID(ids);
            value.Source = source;
            value.Output = r.Destination / "materials" / (AssetKey(value.ID) + ".hmat");
        }
        value.SourceKey = d.Materials[i].SourceKey;
        m.Materials.push_back(value);
    }
    Require(!reimport || matchedMaterials.size() == old.Materials.size(),
            "Reimport conflict: source material removal; existing authored materials retained");
    std::vector<std::vector<std::string>> primitiveSources;
    for (size_t i = 0; i < d.Meshes.size(); ++i)
    {
        CheckCancelled(r.Cancelled);
        auto &mesh = d.Meshes[i];
        std::vector<uint64_t> slots;
        for (size_t j = 0; j < mesh.Primitives.size(); ++j)
            slots.push_back(j + 1);
        auto cooked = CookMesh(mesh, d.Materials, 1, slots, r.Cancelled);
        std::vector<std::string> sources;
        for (size_t j = 0; j < cooked.Primitives.size(); ++j)
            sources.push_back(
                PrimitiveSource(cooked, cooked.Primitives[j],
                                materialSources.at(size_t(mesh.Primitives[j].Material))));
        primitiveSources.push_back(sources);
        Put(stage.Path / ("candidate-" + std::to_string(i) + ".hgeom"), EncodeGeometry(cooked));
        ImportMesh value;
        value.SourceKey = mesh.SourceKey;
        value.Source = MeshSource(sources);
        m.Meshes.push_back(std::move(value));
    }
    std::set<uint64_t> matchedMeshes;
    for (size_t i = 0; i < d.Meshes.size(); ++i)
    {
        auto &value = m.Meshes[i];
        const ImportMesh *prior = nullptr;
        if (reimport)
        {
            std::vector<const ImportMesh *> exact;
            for (auto &candidate : old.Meshes)
                if (candidate.Source == value.Source)
                    exact.push_back(&candidate);
            Require(exact.size() <= 1, "Reimport conflict: ambiguous identical mesh content; "
                                       "source names/indices cannot select an identity");
            if (exact.size() == 1)
                prior = exact[0];
            else
            {
                std::multiset<std::filesystem::path> desired;
                for (auto &p : d.Meshes[i].Primitives)
                    desired.insert(m.Materials.at(size_t(p.Material)).Output);
                std::vector<const ImportMesh *> compatible;
                for (auto &candidate : old.Meshes)
                    if (!matchedMeshes.count(candidate.ID))
                    {
                        std::multiset<std::filesystem::path> current;
                        for (auto &slot : candidate.Slots)
                            current.insert(slot.Material);
                        if (current == desired)
                            compatible.push_back(&candidate);
                    }
                Require(compatible.size() == 1, "Reimport conflict: mesh reorder/change/removal is "
                                                "ambiguous; existing slot assignments retained");
                prior = compatible[0];
            }
            Require(matchedMeshes.insert(prior->ID).second,
                    "Reimport conflict: multiple meshes map to the same identity");
            value.ID = prior->ID;
            value.Output = prior->Output;
        }
        else
        {
            value.ID = NewID(ids);
            value.Output = r.Destination / "meshes" / (AssetKey(value.ID) + ".hmesh");
        }
        value.Geometry = r.Destination / "generations" / AssetKey(m.Generation) /
                         (AssetKey(value.ID) + ".hgeom");
        std::set<uint64_t> matchedSlots;
        for (size_t j = 0; j < d.Meshes[i].Primitives.size(); ++j)
        {
            auto &source = primitiveSources[i][j];
            auto material = m.Materials.at(size_t(d.Meshes[i].Primitives[j].Material)).Output;
            ImportSlot slot;
            if (prior)
            {
                std::vector<const ImportSlot *> matches;
                for (auto &candidate : prior->Slots)
                    if (candidate.Source == source)
                        matches.push_back(&candidate);
                Require(matches.size() <= 1,
                        "Reimport conflict: identical primitive slots are ambiguous");
                if (matches.empty())
                    for (auto &candidate : prior->Slots)
                        if (!matchedSlots.count(candidate.ID) && candidate.Material == material)
                            matches.push_back(&candidate);
                Require(matches.size() == 1 && matchedSlots.insert(matches[0]->ID).second,
                        "Reimport conflict: primitive reorder/change/removal is ambiguous; stable "
                        "slots retained");
                slot = *matches[0];
                slot.Source = source;
            }
            else
                slot = {NewID(ids), source, material, {}};
            slot.SourceKey = d.Meshes[i].Primitives[j].SourceKey;
            value.Slots.push_back(slot);
        }
        Require(!prior || matchedSlots.size() == prior->Slots.size(),
                "Reimport conflict: material-slot removal");
        auto geometry = ReadGeometry(stage.Path / ("candidate-" + std::to_string(i) + ".hgeom"));
        geometry.Mesh = value.ID;
        for (size_t j = 0; j < geometry.Primitives.size(); ++j)
            geometry.Primitives[j].Slot = value.Slots[j].ID;
        auto encoded = EncodeGeometry(geometry);
        value.GeometryHash = AssetHash(encoded);
        Put(stagedRoot / value.Geometry, encoded);
        MeshDefinition definition;
        if (reimport)
        {
            definition = ReadMeshText(authored.at(value.Output));
            Require(definition.ID == value.ID && definition.Import == manifestPath,
                    "Authored mesh changed import ownership/identity");
            MeshAsset candidate{definition, std::move(geometry), m.Generation};
            std::set<uint64_t> slots;
            for (auto &s : definition.Slots)
                slots.insert(s.ID);
            Require(slots.size() == value.Slots.size(), "Authored mesh slot contract changed");
            for (auto &slot : value.Slots)
                Require(slots.count(slot.ID), "Authored mesh stable slot mapping conflict");
            ValidateMeshAssignment(r.Assets, candidate, {});
        }
        else
        {
            definition.ID = value.ID;
            definition.Name = d.Meshes[i].Name;
            definition.Import = manifestPath;
            for (size_t j = 0; j < value.Slots.size(); ++j)
                definition.Slots.push_back({value.Slots[j].ID, "Surface " + std::to_string(j + 1),
                                            value.Slots[j].Material});
            Put(stagedRoot / value.Output, WriteMeshText(definition));
        }
    }
    Require(!reimport || matchedMeshes.size() == old.Meshes.size(),
            "Reimport conflict: mesh removal; accepted assets retained");
    std::vector<std::string> nodeSignatures;
    m.PrefabSource = NodeSource(d, m.Meshes, nodeSignatures);
    std::set<uint64_t> matchedNodes;
    for (size_t i = 0; i < d.Nodes.size(); ++i)
    {
        ImportNode node;
        if (reimport)
        {
            std::vector<const ImportNode *> matches;
            for (auto &candidate : old.Nodes)
                if (candidate.Source == nodeSignatures[i])
                    matches.push_back(&candidate);
            Require(matches.size() == 1 && matchedNodes.insert(matches[0]->ID).second,
                    "Reimport conflict: node identity is ambiguous or its transform/hierarchy/mesh "
                    "changed. Authored prefab retained");
            node = *matches[0];
        }
        else
            node = {NewID(ids), nodeSignatures[i], {}};
        node.SourceKey = d.Nodes[i].SourceKey;
        m.Nodes.push_back(std::move(node));
    }
    Require(!reimport || matchedNodes.size() == old.Nodes.size(),
            "Reimport conflict: source node removal");
    if (reimport)
    {
        Require(m.PrefabSource == old.PrefabSource,
                "Reimport conflict: node transforms/hierarchy/mesh assignments changed. Detached "
                "authored prefab retained; import into a new folder to reconcile");
        m.PrefabAcceptedHash = old.PrefabAcceptedHash;
    }
    else
    {
        for (size_t i = 0; i < d.Images.size(); ++i)
        {
            CheckCancelled(r.Cancelled);
            Put(stagedRoot / r.Destination / "images" / (std::to_string(i) + ".tga"),
                CookImage(d.Images[i]));
        }
        for (size_t i = 0; i < d.Materials.size(); ++i)
        {
            auto material = d.Materials[i].Values;
            for (size_t j = 0; j < 5; ++j)
                if (d.Materials[i].Images[j] >= 0)
                    material.Textures[j].Image =
                        r.Destination / "images" /
                        (std::to_string(d.Materials[i].Images[j]) + ".tga");
            auto bytes = WriteMaterialText(material);
            m.Materials[i].AcceptedHash = AssetHash(bytes);
            Put(stagedRoot / m.Materials[i].Output, bytes);
        }
        auto prefab = PrefabBytes(d, m, r.Assets);
        m.PrefabAcceptedHash = AssetHash(prefab);
        Put(stagedRoot / m.Prefab, prefab);
        Put(stagedRoot / m.Source, sourceBytes);
    }
    m.Validate();
    CheckMappings(reimport ? r.Assets : stagedRoot, manifestPath, m);
    auto accepted = WriteImportText(m);
    Put(stagedRoot / manifestPath, accepted);
    if (!reimport)
    {
        for (auto &mesh : m.Meshes)
            AuditMesh(stagedRoot, mesh.Output);
        Prefab::Load(stagedRoot, m.Prefab, false, ResourceLoading::MetadataOnly);
    }
    else
        for (auto &pair : authored)
            Require(ReadAssetBytes(Project::ResolveOwnedAsset(r.Assets, pair.first),
                                   64 * 1024 * 1024) == pair.second,
                    "Authored asset changed during reimport; retry. Accepted generation retained");
    OperationProgress::Report("Accepting validated import generation");
    CheckCancelled(r.Cancelled);
    CheckSource(r, sourceBytes);
    if (!reimport)
    {
        std::filesystem::create_directories(destination.parent_path());
        FileSystem::PublishDirectoryNew(stagedRoot / r.Destination, destination);
    }
    else
    {
        manifestFile.Check();
        auto generation = r.Destination / "generations" / AssetKey(m.Generation);
        auto published = Project::ResolveOwnedAsset(r.Assets, generation);
        std::filesystem::create_directories(published.parent_path());
        FileSystem::PublishDirectoryNew(stagedRoot / generation, published);
        // The single manifest replacement is the acceptance point. Immutable
        // files published before it are harmless unaccepted output after a kill.
        // Never delete a generation after an uncertain replacement failure.
        CheckCancelled(r.Cancelled);
        CheckSource(r, sourceBytes);
        manifestFile.Check();
        for (auto &pair : authored)
            Require(ReadAssetBytes(Project::ResolveOwnedAsset(r.Assets, pair.first),
                                   64 * 1024 * 1024) == pair.second,
                    "Authored asset changed before acceptance; generation retained for inspection");
        manifestFile.Save(accepted);
    }
    return {manifestPath, m.Prefab, m.Generation, false};
}
} // namespace Hazel::Import
