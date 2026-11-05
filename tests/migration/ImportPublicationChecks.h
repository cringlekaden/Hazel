#pragma once
#include "Import.h"
#include "Hazel/Assets/MaterialDocument.h"
#include "Hazel/Renderer/ImageDecodeMemory.h"
inline void ImportPublicationChecks()
{
    using namespace Hazel;
    using namespace Hazel::Import;
    auto root = std::filesystem::temp_directory_path() /
                std::filesystem::u8path("hazel-import-café-" + AssetKey(UUID()));
    std::filesystem::create_directories(root / "Assets");
    struct Cleanup
    {
        std::filesystem::path Root;
        ~Cleanup()
        {
            std::error_code ec;
            std::filesystem::remove_all(Root, ec);
        }
    } cleanup{root};
    auto [json, bin] = ImportFixture();
    auto source = root / "model.glb";
    auto write = [&](const auto &path, const auto &bytes) {
        FileSystem::WriteFileAtomically(path,
                                        [&](auto &out) { out.write(bytes.data(), bytes.size()); });
    };
    write(source, ImportGLB(json, bin));
    auto assets = root / "Assets";
    auto imported = Hazel::Import::ImportGLB({assets, source, "Imported/Diagnostic", -1, {}});
    auto manifest = ReadImport(assets / imported.Manifest);
    Check(manifest.Meshes.size() == 1 && manifest.Materials.size() == 2,
          "Native publication omitted outputs");
    auto original = ReadAssetBytes(assets / imported.Manifest, 4 * 1024 * 1024);
    auto mesh = ReadMeshAsset(assets, manifest.Meshes[0].Output);
    auto oldGeometry = EncodeGeometry(mesh.Geometry);
    auto oldGeneration = manifest.Generation;
    auto prefab = Prefab::Load(assets, imported.Prefab, false, ResourceLoading::MetadataOnly);
    Check(prefab->GetAllEntitiesWith<MeshRendererComponent>().size() == 2,
          "Detached imported prefab lost repeated mesh nodes");
    auto authored = ReadAssetBytes(assets / manifest.Prefab, 64 * 1024 * 1024);
    auto edited = ReadMaterial(assets / manifest.Materials[0].Output);
    edited.Roughness = .37f;
    auto materialBytes = WriteMaterialText(edited);
    write(assets / manifest.Materials[0].Output, materialBytes);
    auto rejects = [](auto f) {
        bool rejected = false;
        try
        {
            f();
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        Check(rejected, "Failed import operation was accepted");
    };
    rejects([&] { Hazel::Import::ImportGLB({assets, source, "Imported/Diagnostic", -1, {}}); });
    Check(ReadAssetBytes(assets / imported.Manifest, 4 * 1024 * 1024) == original,
          "Failed initial import replaced accepted manifest");
    auto sourceOwned = assets / manifest.Source;
    auto changed = bin;
    float two = 2;
    uint32_t bits;
    std::memcpy(&bits, &two, 4);
    for (int i = 0; i < 4; ++i)
        changed[12 + i] = char(bits >> (8 * i));
    write(sourceOwned, ImportGLB(json, changed));
    rejects([&] { ReimportGLB(assets, imported.Manifest, [] { return true; }); });
    Check(ReadAssetBytes(assets / imported.Manifest, 4 * 1024 * 1024) == original,
          "Cancelled import accepted partial generation");
    auto accepted = ReimportGLB(assets, imported.Manifest);
    auto next = ReadImport(assets / imported.Manifest);
    Check(next.Generation != oldGeneration && next.Meshes[0].ID == manifest.Meshes[0].ID &&
              next.Meshes[0].Slots[0].ID == manifest.Meshes[0].Slots[0].ID,
          "Geometry reimport changed persistent mesh/slot identities");
    Check(ReadAssetBytes(assets / manifest.Materials[0].Output, 65536) == materialBytes &&
              ReadAssetBytes(assets / manifest.Prefab, 64 * 1024 * 1024) == authored,
          "Reimport overwrote authored material/prefab");
    Check(ReadAssetBytes(assets / manifest.Meshes[0].Geometry, GeometryByteLimit) == oldGeometry,
          "Reimport removed old immutable geometry");
    const std::string first=R"({"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0})";
    const std::string second=R"({"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":1})";
    auto reordered=json;auto primitiveAt=reordered.find(first+","+second);Check(primitiveAt!=std::string::npos,"Reorder fixture missing");reordered.replace(primitiveAt,first.size()+second.size()+1,second+","+first);write(sourceOwned,ImportGLB(reordered,changed));ReimportGLB(assets,imported.Manifest);auto reorderedManifest=ReadImport(assets/imported.Manifest);
    Check(reorderedManifest.Meshes[0].Slots[0].ID==next.Meshes[0].Slots[1].ID&&reorderedManifest.Meshes[0].Slots[1].ID==next.Meshes[0].Slots[0].ID,"Primitive reorder redirected stable material slots by array index");
    Check(reorderedManifest.Nodes[0].ID==next.Nodes[0].ID&&reorderedManifest.Nodes[1].ID==next.Nodes[1].ID,"Geometry-only reorder changed recorded source-node IDs");
    json=std::move(reordered);next=std::move(reorderedManifest);
    auto acceptedBytes = ReadAssetBytes(assets / imported.Manifest, 4 * 1024 * 1024);
    auto conflicting = json;
    auto where = conflicting.find("\"metallicFactor\":0");
    conflicting.replace(where, std::strlen("\"metallicFactor\":0"), "\"metallicFactor\":0.5");
    write(sourceOwned, ImportGLB(conflicting, changed));
    rejects([&] { ReimportGLB(assets, imported.Manifest); });
    Check(ReadAssetBytes(assets / imported.Manifest, 4 * 1024 * 1024) == acceptedBytes,
          "Conflicting material semantics changed accepted generation");
    write(sourceOwned, ImportGLB(json, changed));
    bool changedDuring = false;
    {
        OperationProgress::Scope progress([&](const std::string &operation, size_t, size_t) {
            if (operation == "Accepting validated import generation")
            {
                changedDuring = true;
                write(sourceOwned, std::string("Changed during import"));
            }
        }); // Force a new, valid source generation, then change it at acceptance.
        auto extra = json;
        extra.insert(extra.size() - 1, ",\"extras\":{\"revision\":2}");
        write(sourceOwned, ImportGLB(extra, changed));
        rejects([&] { ReimportGLB(assets, imported.Manifest); });
    }
    Check(changedDuring &&
              ReadAssetBytes(assets / imported.Manifest, 4 * 1024 * 1024) == acceptedBytes,
          "Changing source at acceptance published a partial generation");
    auto cache = CreateRef<ProjectAssets>(assets);
    MaterialDocument materialDocument(cache);
    materialDocument.Open(next.Materials[0].Output);
    auto beforeMaterial = materialDocument.Draft(), afterMaterial = beforeMaterial;
    afterMaterial.Name = "Edited natively";
    materialDocument.Edit(afterMaterial);
    Check(materialDocument.Dirty() && materialDocument.CanUndo(),
          "Material authored edit/history missing");
    materialDocument.Save();
    materialDocument.Undo();
    Check(materialDocument.Dirty() && materialDocument.Draft() == beforeMaterial,
          "Material undo after save lost exact dirty state");
    materialDocument.Redo();
    Check(!materialDocument.Dirty(), "Material redo did not restore saved content");
    afterMaterial.Roughness = .6f;
    materialDocument.Begin();
    materialDocument.Preview(afterMaterial);
    materialDocument.Finish(true);
    Check(!materialDocument.Dirty(), "Material gesture cancellation changed saved content");
    auto retainedDraft = materialDocument.Draft();
    write(assets / next.Materials[0].Output, materialBytes);
    rejects([&] { materialDocument.Save(); });
    Check(materialDocument.Draft() == retainedDraft &&
              ReadAssetBytes(assets / next.Materials[0].Output, 65536) == materialBytes,
          "Material save conflict overwrote draft/disk");
    {
        ImageDecodeBudget budget(64);
        auto p = ImageDecodeAllocate(1024);
        Check(!p && budget.Failed(), "Shared decoder allocation hook bypassed its budget");
    }
    auto pinned = cache->Mesh(next.Meshes[0].Output);
    Check(pinned.Accepted && pinned.Error.empty(), "CPU mesh accepted cache failed");
    auto geometryFile = assets / next.Meshes[0].Geometry;
    auto geometryBytes = ReadAssetBytes(geometryFile, GeometryByteLimit);
    write(geometryFile, std::string("corrupt"));
    cache->Reload(next.Meshes[0].Output);
    auto retained = cache->Mesh(next.Meshes[0].Output);
    Check(retained.Accepted == pinned.Accepted && !retained.Error.empty(),
          "Corrupt replacement retired accepted CPU mesh");
    write(geometryFile, geometryBytes);
    std::filesystem::remove(sourceOwned);
    Check(ReadMeshAsset(assets, next.Meshes[0].Output).Generation == next.Generation,
          "Runtime mesh decoding required original GLB/source access");
    AuditMesh(assets, next.Meshes[0].Output);
    prefab->SetAssets(cache);
    RuntimeSession::Validate(prefab, false, false);
    rejects([&] { RuntimeSession::Validate(prefab, false); });
    auto copy = Scene::Copy(prefab);
    Check(copy->GetAllEntitiesWith<MeshRendererComponent>().size() == 2,
          "Scene copy omitted authored mesh references");
    std::cout
        << "PASS: coherent native import; stable geometry reimport; material/prefab preservation; "
           "cancellation/source-change/conflict/failed-destination atomicity; accepted CPU "
           "versions retained; parser/source-free decoding and explicit unsupported Play\n";
}
