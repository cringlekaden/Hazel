#pragma once
#include "Hazel/Assets/Environment.h"
#include "Hazel/Assets/Material.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <cmath>
// Maintainer-only native authoring, additive to the immutable earlier cases.
// Preparation and freezing are separate explicit operations; no runtime bake.
namespace IBLQualityReference
{
inline std::string TinyEmitterHDR()
{
    std::string bytes="#?RADIANCE\n# Hazel IBLQuality_v2 diagnostic; Apache-2.0\nFORMAT=32-bit_rle_rgbe\n\n-Y 128 +X 256\n";
    for(unsigned y=0;y<128;++y)
        for(unsigned x=0;x<256;++x)
        {
            double value=x==139&&y==50?10000.:.01;
            int exponent=0;
            auto scale=std::frexp(value,&exponent)*256/value;
            bytes.append(3,char(uint8_t(value*scale)));
            bytes+=char(exponent+128);
        }
    return bytes;
}
inline void Create(const std::filesystem::path &descriptor)
{
    using namespace Hazel;
    auto project=Project::LoadCandidate(descriptor);
    if(!project)throw std::runtime_error("Open the native Rendering Reference project");
    const auto root=project->GetAssetRoot();
    const std::filesystem::path folder="Reference/IBLQuality_v2";
    if(std::filesystem::exists(root/folder))
        throw std::runtime_error("IBLQuality_v2 already exists; preserve the diagnostic generation");
    std::filesystem::create_directories(root/folder/"Materials");
    FileSystem::WriteNewFile(root/folder/"TinyEmitter.hdr",TinyEmitterHDR());
    FileSystem::WriteNewFile(root/folder/"NOTICE.txt",
        "Diagnostic IBLQuality_v2, original Hazel Apache-2.0. Tiny HDR: 256x128, RGBE .01 background, "
        "one RGBE 10000 pixel at (139,50), no clipping or editing of highlights.\n"
        "Photo input: unchanged Studio Small 09 1k by Sergej Majboroda / Poly Haven, CC0-1.0. "
        "https://polyhaven.com/a/studio_small_09 ; https://polyhaven.com/license\n"
        "Original SHA256 e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45.\n");
    const float roughness[]={0,.1f,.25f,.5f,.75f,1};
    for(auto name:{"Photo","TinyEmitter","Constant"})
    {
        EnvironmentDefinition environment;
        environment.Name=std::string("IBLQuality_v2 ")+name;
        environment.Notice=FileDocument::Read(root/folder/"NOTICE.txt");
        environment.Image=std::string(name)=="TinyEmitter"?folder/"TinyEmitter.hdr"
            :std::filesystem::path("Reference/IBL_v1/Sources")/(std::string(name)+".hdr");
        if(std::string(name)=="Constant")environment.Profile={16,32,16,1024};
        auto environmentPath=folder/(std::string(name)+".henv");
        FileSystem::WriteNewFile(root/environmentPath,WriteEnvironmentText(environment));
        auto scene=CreateRef<Scene>();
        SceneSerializer codec(scene,root,true,project->GetAssets());
        if(!codec.DeserializeText(FileDocument::Read(root/"Scenes/IBL_v1-Photo.hazel"),false,ResourceLoading::MetadataOnly))
            throw std::runtime_error(codec.Report().Error);
        scene->SetName(std::string("IBLQuality_v2 ")+name);
        scene->Presentation().Environment={environmentPath,0,1,true,true};
        for(auto h:scene->GetAllEntitiesWith<DirectionalLightComponent>())
            Entity(h,scene.get()).GetComponent<DirectionalLightComponent>().Enabled=false;
        for(unsigned row=0;row<3;++row)
            for(unsigned column=0;column<6;++column)
            {
                auto entity=scene->GetEntityByUUID(10+row*6+column);
                auto &binding=entity.GetComponent<MeshRendererComponent>();
                auto mesh=project->GetAssets()->Mesh(binding.Mesh);
                if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
                MaterialDefinition material;
                material.Name="Controlled roughness "+std::to_string(column)+" metallic "+std::to_string(row);
                material.BaseColor={.18f,.4f,.7f,1};
                material.Metallic=.5f*row;
                material.Roughness=roughness[column];
                auto path=folder/"Materials"/("M"+std::to_string(row)+"R"+std::to_string(column)+".hmat");
                // These shared controlled materials are authored once, independently of the environment.
                if(!std::filesystem::exists(root/path))FileSystem::WriteNewFile(root/path,WriteMaterialText(material));
                for(const auto &slot:mesh.Accepted->Definition.Slots)binding.Materials[slot.ID]=path;
            }
        for(auto h:scene->GetAllEntitiesWith<TextComponent>())
        {
            auto &text=Entity(h,scene.get()).GetComponent<TextComponent>().TextString;
            if(text.find("IBL_v1")!=std::string::npos)
                text=Entity(h,scene.get()).HasComponent<UIRectComponent>()
                    ?"IBLQuality_v2 / same base, normals and AO / metallic 0 .5 1 / roughness 0 .1 .25 .5 .75 1"
                    :std::string("IBLQuality_v2 / ")+name+" / fixed EV 0 / no direct light";
        }
        auto scenePath=std::filesystem::path("Scenes")/(std::string("IBLQuality_v2-")+name+".hazel");
        FileSystem::WriteNewFile(root/scenePath,codec.SerializeText());
        auto selector=CreateRef<Project>();selector->GetConfig()=project->GetConfig();selector->GetConfig().StartScene=scenePath;
        FileSystem::WriteNewFile(std::filesystem::absolute(descriptor).parent_path()/(std::string("RenderingReference-IBLQuality-")+name+".hproj"),ProjectSerializer(selector).SerializeText());
    }
}
inline void Freeze(const std::filesystem::path &descriptor)
{
    using namespace Hazel;
    auto project=Project::LoadCandidate(descriptor);if(!project)throw std::runtime_error("Open Rendering Reference");
    const auto root=project->GetAssetRoot();const std::filesystem::path folder="Reference/IBLQuality_v2";
    if(std::filesystem::exists(root/folder/"conditions.yaml"))throw std::runtime_error("IBLQuality_v2 conditions are already frozen");
    YAML::Node conditions;conditions["Version"]=2;conditions["BRDFRecipe"]=IBLRecipe;conditions["PreparationRecipe"]=EnvironmentPreparationRecipe;
    conditions["Exposure"]=0;conditions["Yaw"]=0;conditions["Gain"]=1;conditions["CameraID"]=1;
    conditions["Output"]=std::vector<int>{1280,720};conditions["BaseLinearRGB"]=std::vector<float>{.18f,.4f,.7f};
    conditions["Metallic"]=std::vector<float>{0,.5f,1};conditions["Roughness"]=std::vector<float>{0,.1f,.25f,.5f,.75f,1};
    conditions["Normals"]="geometric, no normal map";conditions["AO"]=1;conditions["DirectLight"]=false;
    for(auto name:{"Photo","TinyEmitter","Constant"})
    {
        auto environment=folder/(std::string(name)+".henv");auto prepared=ReadPreparedEnvironment(root,environment);
        if(prepared.PreparationRecipe!=EnvironmentPreparationRecipe)throw std::runtime_error("Prepare current quality recipe before freezing");
        auto scene=std::filesystem::path("Scenes")/(std::string("IBLQuality_v2-")+name+".hazel");
        auto entry=conditions["Cases"][name];entry["Scene"]=scene.generic_u8string();entry["Environment"]=environment.generic_u8string();
        entry["SourceHash"]=prepared.SourceHash;entry["Generation"]=AssetKey(prepared.Generation);entry["LUTIdentity"]=prepared.LUTIdentity;
        for(auto &dependency:prepared.Dependencies)conditions["Files"][dependency.generic_u8string()]=AssetHash(ReadAssetBytes(root/dependency,LightingArtifactByteLimit));
        conditions["Files"][scene.generic_u8string()]=AssetHash(FileDocument::Read(root/scene));
    }
    for(auto &file:std::filesystem::directory_iterator(root/folder/"Materials"))
        conditions["Files"][file.path().lexically_relative(root).generic_u8string()]=AssetHash(FileDocument::Read(file.path()));
    YAML::Emitter out;out.SetFloatPrecision(9);out<<conditions;
    FileSystem::WriteNewFile(root/folder/"conditions.yaml",std::string(out.c_str())+"\n");
}
}
