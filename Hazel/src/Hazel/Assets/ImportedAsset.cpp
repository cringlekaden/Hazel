#include "hzpch.h"
#include <set>
#include "ImportedAsset.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/Project.h"
#include <yaml-cpp/yaml.h>
namespace Hazel
{
namespace
{
void Check(bool yes, const char *why)
{
    if (!yes)
        throw std::runtime_error(why);
}
YAML::Node Root(const std::string &bytes, const char *key)
{
    Check(bytes.size() <= 4 * 1024 * 1024, "Asset document exceeds 4 MiB");
    auto root = YAML::Load(bytes);
    DocumentSchema::Structure(root);
    DocumentSchema::Keys(root, {key}, "root");
    auto n = root[key];
    Check(n["Version"].as<unsigned>() == 1, "Unsupported native asset document version");
    return n;
}
std::filesystem::path Path(const YAML::Node &n)
{
    return std::filesystem::u8path(n.as<std::string>());
}
void Hash(const std::string &s)
{
    ReadDigest(s);
}
void Key(uint64_t id)
{
    AssetKey(id);
}
YAML::Emitter &Field(YAML::Emitter &o, const char *name)
{
    return o << YAML::Key << name << YAML::Value;
}
void Sequence(const YAML::Node &n, size_t max)
{
    Check(n.IsSequence() && n.size() <= max, "Invalid/excessive native asset sequence");
}
} // namespace
void MeshDefinition::Validate() const
{
    Key(ID);
    AssetName(Name);
    AssetPath(Import, ".himport");
    Check(!Slots.empty() && Slots.size() <= 4096, "Mesh requires 1..4096 material slots");
    std::set<uint64_t> ids;
    for (auto &s : Slots)
    {
        Key(s.ID);
        AssetName(s.Name);
        AssetPath(s.Material, ".hmat");
        Check(ids.insert(s.ID).second, "Duplicate mesh slot ID");
    }
}
void ImportManifest::Validate() const
{
    AssetName(Producer);
    Key(ID);
    Key(Generation);
    Key(PrefabRoot);
    AssetPath(Source); // Producer provenance; runtime does not interpret a source format.
    AssetPath(Prefab, ".hprefab");
    Hash(SourceHash);
    Hash(PrefabSource);
    Hash(PrefabAcceptedHash);
    Check(Scene >= -1 && Scene < 10000, "Invalid import scene selection");
    Check(!Meshes.empty() && Meshes.size() <= 1024 && Materials.size() <= 4096,
          "Import output count limit");
    std::set<uint64_t> ids{ID, Generation, PrefabRoot};
    Check(ids.size() == 3, "Duplicate import/root/generation identities");
    Check(!Nodes.empty() && Nodes.size() <= 9999, "Import node count limit");
    for (auto &n : Nodes)
    {
        Key(n.ID);
        Hash(n.Source);
        AssetName(n.SourceKey);
        Check(ids.insert(n.ID).second, "Duplicate imported node identity");
    }
    std::set<std::filesystem::path> outputs{Source, Prefab};
    for (auto &m : Materials)
    {
        AssetName(m.SourceKey);
        Key(m.ID);
        Hash(m.Source);
        Hash(m.AcceptedHash);
        AssetPath(m.Output, ".hmat");
        Check(ids.insert(m.ID).second && outputs.insert(m.Output).second,
              "Duplicate import identity/output");
    }
    for (auto &m : Meshes)
    {
        AssetName(m.SourceKey);
        Key(m.ID);
        Hash(m.Source);
        Hash(m.GeometryHash);
        AssetPath(m.Output, ".hmesh");
        AssetPath(m.Geometry, ".hgeom");
        Check(ids.insert(m.ID).second && outputs.insert(m.Output).second &&
                  outputs.insert(m.Geometry).second,
              "Duplicate import identity/output");
        Check(!m.Slots.empty() && m.Slots.size() <= 4096, "Invalid imported slot count");
        for (auto &s : m.Slots)
        {
            AssetName(s.SourceKey);
            Key(s.ID);
            Hash(s.Source);
            AssetPath(s.Material, ".hmat");
            Check(ids.insert(s.ID).second, "Duplicate imported slot identity");
            bool found = false;
            for (auto &material : Materials)
                found |= material.Output == s.Material;
            Check(found, "Import slot references an unrecorded material");
        }
    }
}
MeshDefinition ReadMeshText(const std::string &bytes)
{
    auto n = Root(bytes, "Mesh");
    DocumentSchema::Keys(n, {"Version", "ID", "Name", "Import", "Slots"}, "Mesh");
    MeshDefinition d;
    d.ID = ReadAssetKey(n["ID"].as<std::string>());
    d.Name = n["Name"].as<std::string>();
    d.Import = Path(n["Import"]);
    Sequence(n["Slots"], 4096);
    for (auto s : n["Slots"])
    {
        DocumentSchema::Keys(s, {"ID", "Name", "Material"}, "mesh slot");
        d.Slots.push_back({ReadAssetKey(s["ID"].as<std::string>()), s["Name"].as<std::string>(),
                           Path(s["Material"])});
    }
    d.Validate();
    return d;
}
MeshDefinition ReadMesh(const std::filesystem::path &file)
{
    return ReadMeshText(ReadAssetBytes(file, 4 * 1024 * 1024));
}
std::string WriteMeshText(const MeshDefinition &d)
{
    d.Validate();
    YAML::Emitter o;
    o << YAML::BeginMap;
    Field(o, "Mesh") << YAML::BeginMap;
    Field(o, "Version") << 1;
    Field(o, "ID") << AssetKey(d.ID);
    Field(o, "Name") << d.Name;
    Field(o, "Import") << d.Import.generic_u8string();
    Field(o, "Slots") << YAML::BeginSeq;
    for (auto &s : d.Slots)
    {
        o << YAML::BeginMap;
        Field(o, "ID") << AssetKey(s.ID);
        Field(o, "Name") << s.Name;
        Field(o, "Material") << s.Material.generic_u8string();
        o << YAML::EndMap;
    }
    o << YAML::EndSeq << YAML::EndMap << YAML::EndMap;
    return o.c_str();
}
ImportManifest ReadImportText(const std::string &bytes)
{
    auto n = Root(bytes, "Import");
    DocumentSchema::Keys(n,
                         {"Version", "Recipe", "ID", "Generation", "Source", "SourceHash", "Scene",
                          "Prefab", "PrefabSource", "PrefabAcceptedHash", "Meshes", "Materials",
                          "PrefabRoot", "Nodes", "Producer"},
                         "Import");
    Check(n["Recipe"].as<std::string>() == "StaticMesh_v1", "Unsupported import cooking recipe");
    ImportManifest d;
    d.Producer = n["Producer"].as<std::string>();
    d.ID = ReadAssetKey(n["ID"].as<std::string>());
    d.PrefabRoot = ReadAssetKey(n["PrefabRoot"].as<std::string>());
    d.Generation = ReadAssetKey(n["Generation"].as<std::string>());
    d.Source = Path(n["Source"]);
    d.SourceHash = n["SourceHash"].as<std::string>();
    d.Scene = n["Scene"].as<int>();
    d.Prefab = Path(n["Prefab"]);
    d.PrefabSource = n["PrefabSource"].as<std::string>();
    d.PrefabAcceptedHash = n["PrefabAcceptedHash"].as<std::string>();
    Sequence(n["Materials"], 4096);
    Sequence(n["Meshes"], 1024);
    for (auto m : n["Materials"])
    {
        DocumentSchema::Keys(m, {"ID", "Source", "Output", "AcceptedHash", "SourceKey"},
                             "import material");
        d.Materials.push_back({ReadAssetKey(m["ID"].as<std::string>()),
                               m["Source"].as<std::string>(), Path(m["Output"]),
                               m["AcceptedHash"].as<std::string>(),
                               m["SourceKey"].as<std::string>()});
    }
    for (auto m : n["Meshes"])
    {
        DocumentSchema::Keys(
            m, {"ID", "Source", "Output", "Geometry", "GeometryHash", "Slots", "SourceKey"},
            "import mesh");
        ImportMesh v;
        v.ID = ReadAssetKey(m["ID"].as<std::string>());
        v.Source = m["Source"].as<std::string>();
        v.SourceKey = m["SourceKey"].as<std::string>();
        v.Output = Path(m["Output"]);
        v.Geometry = Path(m["Geometry"]);
        v.GeometryHash = m["GeometryHash"].as<std::string>();
        Sequence(m["Slots"], 4096);
        for (auto s : m["Slots"])
        {
            DocumentSchema::Keys(s, {"ID", "Source", "Material", "SourceKey"}, "import slot");
            v.Slots.push_back({ReadAssetKey(s["ID"].as<std::string>()),
                               s["Source"].as<std::string>(), Path(s["Material"]),
                               s["SourceKey"].as<std::string>()});
        }
        d.Meshes.push_back(std::move(v));
    }
    Sequence(n["Nodes"], 9999);
    for (auto v : n["Nodes"])
    {
        DocumentSchema::Keys(v, {"ID", "Source", "SourceKey"}, "import node");
        d.Nodes.push_back({ReadAssetKey(v["ID"].as<std::string>()), v["Source"].as<std::string>(),
                           v["SourceKey"].as<std::string>()});
    }
    d.Validate();
    return d;
}
ImportManifest ReadImport(const std::filesystem::path &file)
{
    return ReadImportText(ReadAssetBytes(file, 4 * 1024 * 1024));
}
std::string WriteImportText(const ImportManifest &d)
{
    d.Validate();
    YAML::Emitter o;
    o << YAML::BeginMap;
    Field(o, "Import") << YAML::BeginMap;
    Field(o, "Version") << 1;
    Field(o, "Recipe") << "StaticMesh_v1";
    Field(o, "Producer") << d.Producer;
    Field(o, "ID") << AssetKey(d.ID);
    Field(o, "Generation") << AssetKey(d.Generation);
    Field(o, "PrefabRoot") << AssetKey(d.PrefabRoot);
    Field(o, "Source") << d.Source.generic_u8string();
    Field(o, "SourceHash") << d.SourceHash;
    Field(o, "Scene") << d.Scene;
    Field(o, "Prefab") << d.Prefab.generic_u8string();
    Field(o, "PrefabSource") << d.PrefabSource;
    Field(o, "PrefabAcceptedHash") << d.PrefabAcceptedHash;
    Field(o, "Materials") << YAML::BeginSeq;
    for (auto &m : d.Materials)
    {
        o << YAML::BeginMap;
        Field(o, "ID") << AssetKey(m.ID);
        Field(o, "Source") << m.Source;
        Field(o, "Output") << m.Output.generic_u8string();
        Field(o, "AcceptedHash") << m.AcceptedHash;
        Field(o, "SourceKey") << m.SourceKey;
        o << YAML::EndMap;
    }
    o << YAML::EndSeq;
    Field(o, "Meshes") << YAML::BeginSeq;
    for (auto &m : d.Meshes)
    {
        o << YAML::BeginMap;
        Field(o, "ID") << AssetKey(m.ID);
        Field(o, "Source") << m.Source;
        Field(o, "Output") << m.Output.generic_u8string();
        Field(o, "Geometry") << m.Geometry.generic_u8string();
        Field(o, "GeometryHash") << m.GeometryHash;
        Field(o, "SourceKey") << m.SourceKey;
        Field(o, "Slots") << YAML::BeginSeq;
        for (auto &s : m.Slots)
        {
            o << YAML::BeginMap;
            Field(o, "ID") << AssetKey(s.ID);
            Field(o, "Source") << s.Source;
            Field(o, "SourceKey") << s.SourceKey;
            Field(o, "Material") << s.Material.generic_u8string();
            o << YAML::EndMap;
        }
        o << YAML::EndSeq << YAML::EndMap;
    }
    o << YAML::EndSeq;
    Field(o, "Nodes") << YAML::BeginSeq;
    for (auto &n : d.Nodes)
    {
        o << YAML::BeginMap;
        Field(o, "ID") << AssetKey(n.ID);
        Field(o, "Source") << n.Source;
        Field(o, "SourceKey") << n.SourceKey;
        o << YAML::EndMap;
    }
    o << YAML::EndSeq << YAML::EndMap << YAML::EndMap;
    return o.c_str();
}
void MeshBinding::Validate() const
{
    AssetPath(Mesh, ".hmesh", true);
    Check(Materials.size() <= 4096 && (!Mesh.empty() || Materials.empty()),
          "Unassigned mesh cannot retain material overrides");
    for (auto &m : Materials)
    {
        Key(m.first);
        AssetPath(m.second, ".hmat");
    }
}
MeshBinding ReadMeshBinding(const YAML::Node &n)
{
    DocumentSchema::Keys(n, {"Version", "Mesh", "Materials"}, "MeshRendererComponent");
    Check(n["Version"].as<unsigned>() == 1, "Unsupported MeshRendererComponent.Version");
    MeshBinding b;
    b.Mesh = Path(n["Mesh"]);
    Sequence(n["Materials"], 4096);
    for (auto m : n["Materials"])
    {
        DocumentSchema::Keys(m, {"Slot", "Material"}, "mesh material override");
        Check(b.Materials.emplace(ReadAssetKey(m["Slot"].as<std::string>()), Path(m["Material"]))
                  .second,
              "Duplicate mesh material override slot");
    }
    b.Validate();
    return b;
}
void WriteMeshBinding(YAML::Emitter &o, const MeshBinding &b)
{
    b.Validate();
    o << YAML::BeginMap;
    Field(o, "Version") << 1;
    Field(o, "Mesh") << b.Mesh.generic_u8string();
    Field(o, "Materials") << YAML::BeginSeq;
    for (auto &m : b.Materials)
    {
        o << YAML::BeginMap;
        Field(o, "Slot") << AssetKey(m.first);
        Field(o, "Material") << m.second.generic_u8string();
        o << YAML::EndMap;
    }
    o << YAML::EndSeq << YAML::EndMap;
}
std::vector<std::filesystem::path> AuditMeshBinding(const std::filesystem::path &root,
                                                    const MeshBinding &b)
{
    b.Validate();
    Check(!b.Mesh.empty(), "Mesh renderer is unassigned; select a .hmesh asset");
    auto a = ReadMeshAsset(root, b.Mesh);
    ValidateMeshAssignment(root, a, b.Materials);
    auto deps = AuditMesh(root, b.Mesh);
    std::set<std::filesystem::path> all(deps.begin(), deps.end());
    for (auto &m : b.Materials)
    {
        all.insert(m.second);
        for (auto &d :
             AuditMaterial(root, ReadMaterial(Project::ResolveOwnedAsset(root, m.second))))
            all.insert(d);
    }
    return {all.begin(), all.end()};
}
MeshAsset ReadMeshAsset(const std::filesystem::path &root, const std::filesystem::path &reference)
{
    AssetPath(reference, ".hmesh");
    MeshAsset a;
    a.Definition = ReadMesh(Project::ResolveOwnedAsset(root, reference));
    auto manifest = ReadImport(Project::ResolveOwnedAsset(root, a.Definition.Import));
    a.Generation = manifest.Generation;
    auto found = std::find_if(manifest.Meshes.begin(), manifest.Meshes.end(), [&](auto &m) {
        return m.ID == a.Definition.ID && m.Output == reference;
    });
    Check(found != manifest.Meshes.end(), "Mesh is absent from accepted import generation");
    auto bytes =
        ReadAssetBytes(Project::ResolveOwnedAsset(root, found->Geometry), GeometryByteLimit);
    Check(AssetHash(bytes) == found->GeometryHash, "Accepted geometry digest mismatch");
    a.Geometry = DecodeGeometry(bytes);
    Check(a.Geometry.Mesh == a.Definition.ID &&
              a.Geometry.Primitives.size() == a.Definition.Slots.size(),
          "Mesh geometry identity/slot count mismatch");
    std::set<uint64_t> ids;
    for (auto &s : a.Definition.Slots)
        ids.insert(s.ID);
    for (auto &p : a.Geometry.Primitives)
        Check(ids.count(p.Slot), "Mesh slot is missing from geometry");
    return a;
}
void ValidateMeshAssignment(const std::filesystem::path &root, const MeshAsset &a,
                            const std::map<uint64_t, std::filesystem::path> &overrides)
{
    for (auto &p : overrides)
    {
        AssetPath(p.second, ".hmat");
        Check(std::any_of(a.Definition.Slots.begin(), a.Definition.Slots.end(),
                          [&](auto &s) { return s.ID == p.first; }),
              "Material override refers to an unknown stable mesh slot");
    }
    for (auto &p : a.Geometry.Primitives)
    {
        auto s = std::find_if(a.Definition.Slots.begin(), a.Definition.Slots.end(),
                              [&](auto &s) { return s.ID == p.Slot; });
        auto it = overrides.find(p.Slot);
        auto path = it == overrides.end() ? s->Material : it->second;
        auto material = ReadMaterial(Project::ResolveOwnedAsset(root, path));
        AuditMaterial(root, material);
        if (!(p.Attributes & 1))
            for (auto &t : material.Textures)
                Check(t.Image.empty(),
                      "Textured material requires mesh UV0; this primitive has none");
    }
}
std::vector<std::filesystem::path> AuditImport(const std::filesystem::path &root,
                                               const std::filesystem::path &reference)
{
    AssetPath(reference, ".himport");
    auto m = ReadImport(Project::ResolveOwnedAsset(root, reference));
    std::set<std::filesystem::path> deps{reference, m.Prefab};
    for (auto &item : m.Meshes)
    {
        auto a = ReadMeshAsset(root, item.Output);
        Check(a.Definition.Import == reference && a.Definition.ID == item.ID,
              "Import/mesh ownership mismatch");
        ValidateMeshAssignment(root, a, {});
        deps.insert(item.Output);
        deps.insert(item.Geometry);
        for (auto &slot : a.Definition.Slots)
        {
            deps.insert(slot.Material);
            for (auto &dep :
                 AuditMaterial(root, ReadMaterial(Project::ResolveOwnedAsset(root, slot.Material))))
                deps.insert(dep);
        }
    }
    for (auto &item : m.Materials)
    {
        deps.insert(item.Output);
        for (auto &dep :
             AuditMaterial(root, ReadMaterial(Project::ResolveOwnedAsset(root, item.Output))))
            deps.insert(dep);
    }
    // Source is authoring provenance, deliberately absent from runtime closure.
    ReadAssetBytes(Project::ResolveOwnedAsset(root, m.Prefab), 64 * 1024 * 1024);
    return {deps.begin(), deps.end()};
}
std::vector<std::filesystem::path> AuditMesh(const std::filesystem::path &root,
                                             const std::filesystem::path &reference)
{
    auto a = ReadMeshAsset(root, reference);
    ValidateMeshAssignment(root, a, {});
    std::set<std::filesystem::path> deps{reference, a.Definition.Import};
    auto manifest = ReadImport(Project::ResolveOwnedAsset(root, a.Definition.Import));
    for (auto &m : manifest.Meshes)
    {
        auto bytes =
            ReadAssetBytes(Project::ResolveOwnedAsset(root, m.Geometry), GeometryByteLimit);
        Check(AssetHash(bytes) == m.GeometryHash, "Import geometry digest mismatch");
        Check(DecodeGeometry(bytes).Mesh == m.ID, "Import geometry identity mismatch");
        deps.insert(m.Geometry);
    }
    for (auto &s : a.Definition.Slots)
    {
        deps.insert(s.Material);
        for (auto &d :
             AuditMaterial(root, ReadMaterial(Project::ResolveOwnedAsset(root, s.Material))))
            deps.insert(d);
    }
    return {deps.begin(), deps.end()};
}
} // namespace Hazel
