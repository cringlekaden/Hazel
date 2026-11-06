#include "hzpch.h"
#include "NativeStarter.h"
#include "ImportedAsset.h"
#include "ProjectAssets.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Project/Project.h"
namespace Hazel::NativeStarter {
static const std::filesystem::path Folder="Native/Direct_v1";
void Generate(const std::filesystem::path& root){
    auto folder=root/Folder;if(std::filesystem::exists(folder))throw std::runtime_error("Native starter destination already exists");std::filesystem::create_directories(folder/"generations/0000000000000002");
    GeometryData g;g.Mesh=4;
    const glm::vec3 normals[]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    for(auto n:normals){glm::vec3 t=std::abs(n.y)<.9f?glm::normalize(glm::cross(glm::vec3(0,1,0),n)):glm::vec3(1,0,0);auto b=glm::cross(n,t);uint32_t first=uint32_t(g.Vertices.size());for(auto uv:{glm::vec2(0,0),glm::vec2(1,0),glm::vec2(1,1),glm::vec2(0,1)}){MeshVertex v;v.Position=n*.5f+t*(uv.x-.5f)+b*(.5f-uv.y);v.Normal=n;v.Tangent={t,-1};v.UV=uv;g.Vertices.push_back(v);}for(auto i:{0,2,1,0,3,2})g.Indices.push_back(first+i);}
    g.Primitives.push_back({7,0,36,1,{}});g.UpdateBounds();auto geometry=EncodeGeometry(g);
    MaterialDefinition mat;mat.Name="Neutral dielectric";mat.BaseColor={.21404114f,.21404114f,.21404114f,1};auto material=WriteMaterialText(mat);
    MeshDefinition mesh{4,"One metre cube",Folder/"Import.himport",{{7,"Surface",Folder/"Surface.hmat"}}};
    auto scene=CreateRef<Scene>();auto base=scene->CreateEntityWithUUID(5,"Native cube");auto node=scene->CreateEntityWithUUID(6,"One metre cube");scene->Reparent(node,base,TransformPolicy::KeepLocal);node.AddComponent<MeshRendererComponent>().Mesh=Folder/"UnitCube.hmesh";
    auto prefab=Prefab::Serialize(root,scene,base,true);
    std::string source="Hazel native unit cube Direct_v1; right handed +Y metres; 24 corners, 36 indices";
    ImportManifest m;m.ID=1;m.Generation=2;m.PrefabRoot=5;m.Source=Folder/"Source.generated";m.SourceHash=AssetHash(source);m.Producer="Hazel.Native.Direct_v1";m.SourceNotice="Original engine-authored diagnostic geometry; Apache-2.0 (Hazel LICENSE)";m.Prefab=Folder/"UnitCube.hprefab";m.PrefabSource=AssetHash("Native root + cube");m.PrefabAcceptedHash=AssetHash(prefab);m.Nodes={{6,AssetHash("Native cube node"),"native/cube"}};m.Materials={{3,AssetHash(material),Folder/"Surface.hmat",AssetHash(material),"native/surface"}};ImportMesh output;output.ID=4;output.Source=AssetHash(geometry);output.SourceKey="native/cube";output.Output=Folder/"UnitCube.hmesh";output.Geometry=Folder/"generations/0000000000000002/UnitCube.hgeom";output.GeometryHash=AssetHash(geometry);output.Slots={{7,AssetHash("Native cube surface"),Folder/"Surface.hmat","native/surface"}};m.Meshes={output};
    auto put=[&](auto path,const std::string& bytes){FileSystem::WriteNewFile(root/path,bytes);};
    put(output.Geometry,geometry);put(Folder/"Surface.hmat",material);put(Folder/"UnitCube.hmesh",WriteMeshText(mesh));put(m.Prefab,prefab);put(m.Source,source);put(Folder/"Import.himport",WriteImportText(m));
    put(Folder/"NOTICE.txt",m.SourceNotice+"\nNo external model, importer or lighting data.\n");AuditImport(root,Folder/"Import.himport");
}
void Ensure(const std::filesystem::path& templates,const std::filesystem::path& root){
    auto destination=Project::ResolveOwnedAsset(root,Folder);
    if(std::filesystem::exists(destination)){AuditMesh(root,Folder/"UnitCube.hmesh");return;}
    auto source=templates/Folder;AuditImport(templates,Folder/"Import.himport");
    auto temporary=root/(".native-starter-"+std::to_string(uint64_t(UUID())));std::filesystem::create_directory(temporary);
    struct Cleanup{std::filesystem::path Path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(Path,ec);}} cleanup{temporary};
    std::filesystem::copy(source,temporary/"content",std::filesystem::copy_options::recursive);std::filesystem::create_directories(destination.parent_path());FileSystem::PublishDirectoryNew(temporary/"content",destination);
}
}
