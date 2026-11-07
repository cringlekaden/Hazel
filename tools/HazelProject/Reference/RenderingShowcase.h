#pragma once
#include "IBLQualityReference.h"
#include "Hazel/Assets/ImportedAsset.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
namespace RenderingShowcase
{
inline void Create(const std::filesystem::path &referenceDescriptor,
                   const std::filesystem::path &destination,
                   const std::filesystem::path &inputs)
{
    using namespace Hazel;
    auto reference=Project::LoadCandidate(referenceDescriptor);
    if(!reference)throw std::runtime_error("Open Rendering Reference before creating the showcase");
    if(std::filesystem::exists(destination))throw std::runtime_error("Showcase destination exists; retain authored content");
    auto root=std::filesystem::absolute(destination)/"Assets";
    std::filesystem::create_directories(root/"Materials");
    auto copy=[&](const std::filesystem::path &asset){auto source=Project::ResolveOwnedAsset(reference->GetAssetRoot(),asset);auto target=root/asset;std::filesystem::create_directories(target.parent_path());if(!std::filesystem::exists(target))FileSystem::WriteNewFile(target,ReadAssetBytes(source,64*1024*1024));};
    for(auto mesh:{"Reference/Direct_v1/Sphere.hmesh","Native/Direct_v1/UnitCube.hmesh"})
        for(auto &dependency:AuditMesh(reference->GetAssetRoot(),mesh))copy(dependency);
    YAML::Node provenance;
    provenance["License"]="CC0-1.0";provenance["LicenseURL"]="https://polyhaven.com/license";
    for(auto name:{"wood_table_001","leather_red_02"})
    {
        std::filesystem::create_directories(root/"Materials"/name);
        MaterialDefinition material;
        material.Name=std::string(name)=="wood_table_001"?"Varnished oak / Poly Haven":"Pebbled leather / Poly Haven";
        material.Metallic=0;material.Roughness=1;material.NormalScale=.5f;
        const char *roles[]={std::string(name)=="wood_table_001"?"Diffuse":"coll1","arm","nor_gl"};
        for(unsigned i=0;i<3;++i)
        {
            auto original=inputs/(std::string(name)+"-"+roles[i]+".jpg");
            auto bytes=ReadAssetBytes(original,16*1024*1024);
            auto decoded=Texture2D::ReadImageBytes(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),ImageFormat::RGB8,4*1024*1024,true);
            if(decoded.Width!=1024||decoded.Height!=1024)throw std::runtime_error("Showcase requires the pinned original 1k JPEG material maps");
            auto asset=std::filesystem::path("Materials")/name/(std::string(roles[i])+".jpg");
            FileSystem::WriteNewFile(root/asset,bytes);
            material.Textures[i].Image=asset;
            if(i==1)material.Textures[3].Image=asset;
            auto file=provenance["Materials"][name][roles[i]];
            file["File"]=asset.generic_u8string();file["SHA256"]=AssetHash(bytes);
            file["Meaning"]=i==0?"sRGB base color":i==1?"Linear R occlusion / G roughness / B metallic":"Linear +Y tangent normal";
        }
        FileSystem::WriteNewFile(root/"Materials"/(std::string(name)+".hmat"),WriteMaterialText(material));
    }
    provenance["Materials"]["wood_table_001"]["Authors"]="Dimitrios Savva (photography), Rico Cilliers (processing)";
    provenance["Materials"]["leather_red_02"]["Authors"]="Rob Tuytel";
    FileSystem::WriteNewFile(root/"Materials/PolyHaven-original-downloads.json",FileDocument::Read(inputs/"provenance.json"));
    const std::filesystem::path environment="Environments/Studio.henv";
    std::filesystem::create_directories(root/environment.parent_path());
    EnvironmentDefinition lighting;lighting.Name="Studio Small 09 / high quality";lighting.Image="Environments/Studio.hdr";lighting.Profile=EnvironmentProfile::High();
    lighting.Notice="Studio Small 09 by Sergej Majboroda / Poly Haven, CC0-1.0. https://polyhaven.com/a/studio_small_09 ; https://polyhaven.com/license ; unchanged 1k HDR.";
    auto hdr=ReadAssetBytes(reference->GetAssetRoot()/"Reference/IBL_v1/Sources/Photo.hdr",64*1024*1024);
    if(AssetHash(hdr)!="e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45")throw std::runtime_error("Showcase HDR differs from admitted original source");
    FileSystem::WriteNewFile(root/lighting.Image,hdr);FileSystem::WriteNewFile(root/environment,WriteEnvironmentText(lighting));
    provenance["HDR"]["Notice"]=lighting.Notice;provenance["HDR"]["SHA256"]=AssetHash(hdr);
    YAML::Emitter notes;notes<<provenance;FileSystem::WriteNewFile(root/"NOTICE.yaml",std::string(notes.c_str())+"\n");
    auto scene=CreateRef<Scene>();SceneSerializer reader(scene,reference->GetAssetRoot(),true,reference->GetAssets());
    if(!reader.DeserializeText(FileDocument::Read(reference->GetAssetRoot()/"Scenes/IBLQuality_v2-Photo.hazel"),false,ResourceLoading::MetadataOnly))throw std::runtime_error(reader.Report().Error);
    std::vector<UUID> remove;
    for(auto h:scene->GetAllEntitiesWith<IDComponent>()){Entity e(h,scene.get());auto id=uint64_t(e.GetUUID());if(id!=1&&id!=70&&id!=71&&id!=72)remove.push_back(e.GetUUID());}
    for(auto id:remove)if(auto entity=scene->GetEntityByUUID(id))scene->DestroyEntity(entity);
    scene->SetName("Rendering Showcase / Studio materials");scene->Presentation().Environment={environment,.35f,1,true,true};scene->Presentation().Exposure=0;
    scene->GetEntityByUUID(71).GetComponent<TextComponent>().TextString="STUDIO / copper: sharp .05  .2  .45  .75  1 / wood + leather / direct shadows and local reflections deferred";
    auto add=[&](uint64_t id,const std::string &name,const std::filesystem::path &mesh,const std::filesystem::path &material,glm::vec3 position,glm::vec3 scale){
        auto e=scene->CreateEntityWithUUID(id,name);auto &t=e.GetComponent<TransformComponent>();t.Translation=position;t.Scale=scale;
        auto &binding=e.AddComponent<MeshRendererComponent>();binding.Mesh=mesh;
        for(auto &slot:ReadMeshAsset(root,mesh).Definition.Slots)binding.Materials[slot.ID]=material;
    };
    add(100,"Oak display board","Native/Direct_v1/UnitCube.hmesh","Materials/wood_table_001.hmat",{0,-.6f,0},{10,.3f,5});
    const float rough[]={.05f,.2f,.45f,.75f,1};
    for(unsigned i=0;i<5;++i)
    {
        MaterialDefinition metal;metal.Name="Copper / roughness "+std::to_string(rough[i]);metal.BaseColor={.955f,.638f,.538f,1};metal.Metallic=1;metal.Roughness=rough[i];
        auto asset=std::filesystem::path("Materials")/("Copper"+std::to_string(i)+".hmat");FileSystem::WriteNewFile(root/asset,WriteMaterialText(metal));
        add(110+i,metal.Name,"Reference/Direct_v1/Sphere.hmesh",asset,{-3.2f+1.6f*i,.25f,1},{1.4f,1.4f,1.4f});
    }
    add(120,"Leather study","Reference/Direct_v1/Sphere.hmesh","Materials/leather_red_02.hmat",{-2, .65f,-1.15f},{2.2f,2.2f,2.2f});
    add(121,"Varnished wood study","Reference/Direct_v1/Sphere.hmesh","Materials/wood_table_001.hmat",{1.0f,.65f,-1.15f},{2.2f,2.2f,2.2f});
    std::filesystem::create_directories(root/"Scenes");
    YAML::Node views;views["Exposure"]=0;views["Yaw"]=.35f;views["Gain"]=1;views["Environment"]=environment.generic_u8string();views["PreparationRecipe"]=EnvironmentPreparationRecipe;
    for(auto name:{"Gallery","RoughnessSweep","SurfaceDetail"})
    {
        auto camera=scene->GetEntityByUUID(1);auto &t=camera.GetComponent<TransformComponent>();
        glm::vec3 eye=std::string(name)=="Gallery"?glm::vec3(6,4,9):std::string(name)=="RoughnessSweep"?glm::vec3(0,2.3f,10):glm::vec3(3.8f,2.3f,5.5f);
        glm::vec3 look=std::string(name)=="SurfaceDetail"?glm::vec3(1,.4f,0):glm::vec3(0,.1f,0);
        t.Translation=eye;t.Rotation=glm::eulerAngles(glm::quat_cast(glm::inverse(glm::lookAt(eye,look,glm::vec3(0,1,0)))));
        auto path=std::filesystem::path("Scenes")/(std::string(name)+".hazel");auto bytes=SceneSerializer(scene,root).SerializeText();FileSystem::WriteNewFile(root/path,bytes);
        auto selector=CreateRef<Project>();selector->GetConfig()=reference->GetConfig();auto &config=selector->GetConfig();config.Name="Rendering Showcase";config.ScriptProject="RenderingShowcase";config.ScriptModulePath="Scripts/Binaries/RenderingShowcase.dll";config.SceneDefaults=SceneDefaults::DirectDefaults();config.SceneDefaults.Presentation.Environment=scene->Presentation().Environment;config.StartScene=path;
        FileSystem::WriteNewFile(std::filesystem::absolute(destination)/(std::string(name)=="Gallery"?"RenderingShowcase.hproj":std::string("RenderingShowcase-")+name+".hproj"),ProjectSerializer(selector).SerializeText());
        views["Views"][name]["Scene"]=path.generic_u8string();views["Views"][name]["SceneSHA256"]=AssetHash(bytes);views["Views"][name]["CameraPosition"]=std::vector<float>{eye.x,eye.y,eye.z};
    }
    YAML::Emitter output;output.SetFloatPrecision(9);output<<views;FileSystem::WriteNewFile(root/"views.yaml",std::string(output.c_str())+"\n");
}
}
