#pragma once
#include "Hazel/Project/ProjectCreation.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Renderer/Color.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Assets/SpriteSheet.h"
// Maintainer-only CPU authoring recipe. Refuses existing destinations. The checked-in
// Core_v1 documents/assets are the reference authority, never regenerated at runtime.
inline void CreateRenderingReference(const std::filesystem::path& destination,const std::filesystem::path& templates) {
    using namespace Hazel;
    const auto created=ProjectCreation::Create({"Rendering Reference","RenderingReference",destination,templates});
    auto project=Project::LoadCandidate(created.Descriptor);const auto assets=destination/"Assets";
    auto write=[&](const auto& path,const std::string& text){FileSystem::WriteFileAtomically(path,[&](auto& out){out<<text;});};
    // Exact diagnostic top-left RGBA pixels in a small uncompressed TGA. stb's
    // production image decoder normalizes its declared top-left origin.
    std::string pixels(18,'\0');pixels[2]=2;pixels[12]=8;pixels[14]=8;pixels[16]=32;pixels[17]=0x28;
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        const unsigned char rgba[4]={static_cast<unsigned char>(x<4?(y<4?255:0):(y<4?0:255)),static_cast<unsigned char>(x<4?0:128),static_cast<unsigned char>(y<4?0:255),static_cast<unsigned char>(x==7&&y==7?128:255)};
        pixels+=char(rgba[2]);pixels+=char(rgba[1]);pixels+=char(rgba[0]);pixels+=char(rgba[3]);
    }
    FileSystem::WriteNewFile(assets/"Textures/Asymmetric.tga",pixels);
    for(bool data:{false,true}){
        SpriteSheetDefinition sheet;sheet.Texture="Textures/Asymmetric.tga";sheet.Sampling.Width=sheet.Sampling.Height=8;
        sheet.Sampling.Encoding=data?TextureEncoding::Linear:TextureEncoding::SRGB;
        sheet.Regions.push_back({1,"Asymmetric top-left image",{0,0,8,8},{.5f,.5f}});
        FileSystem::WriteNewFile(assets/(data?"Textures/AsymmetricData.hsprites":"Textures/AsymmetricColor.hsprites"),WriteSpriteSheetText(sheet));
    }
    for(bool perspective:{false,true}) {
        auto scene=CreateRef<Scene>();scene->SetName(perspective?"Core_v1 Perspective":"Core_v1 Orthographic");
        scene->Presentation().Background={.018f,.018f,.018f,1};
        scene->OnViewportResize(1280,720);
        auto camera=scene->CreateEntityWithUUID(1,perspective?"Perspective":"Orthographic");
        camera.GetComponent<TransformComponent>().Translation={0,0,10};
        auto& c=camera.AddComponent<CameraComponent>().Camera;
        if(perspective)c.SetPerspective(glm::radians(45.f),.1f,100);else c.SetOrthographic(10,.1f,100);
        auto sprite=[&](uint64_t id,const std::string& name,glm::vec3 position,glm::vec2 size,glm::vec4 color){
            auto e=scene->CreateEntityWithUUID(id,name);auto& t=e.GetComponent<TransformComponent>();t.Translation=position;t.Scale={size,1};e.AddComponent<SpriteRendererComponent>().Color=color;return e;
        };
        auto label=[&](uint64_t id,const std::string& name,glm::vec3 position,float size,const std::string& text){
            auto e=scene->CreateEntityWithUUID(id,name);auto& t=e.GetComponent<TransformComponent>();t.Translation=position;t.Scale={size,size,1};e.AddComponent<TextComponent>().TextString=text;return e;
        };
        const float levels[]{0,.21404114f,.5f,1};
        for(int i=0;i<4;++i)sprite(10+i,"Known linear swatch "+std::to_string(i),{-5.5f+1.6f*i,1.8f,0},{1.3f,1.3f},{levels[i],levels[i],levels[i],1});
        label(20,"Known swatch labels",{-6.2f,2.8f,.1f},.27f,"sRGB: 0         0.5          0.735       1");
        auto color=sprite(30,"Color texture: sRGB decode",{2.2f,1.8f,0},{1.4f,1.4f},glm::vec4(1));
        color.GetComponent<SpriteRendererComponent>().Source=SpriteReference{"Textures/AsymmetricColor.hsprites",1};
        auto data=sprite(31,"Data texture: unchanged UNORM",{4.3f,1.8f,0},{1.4f,1.4f},glm::vec4(1));
        data.GetComponent<SpriteRendererComponent>().Source=SpriteReference{"Textures/AsymmetricData.hsprites",1};
        label(32,"Texture labels",{1.4f,2.8f,.1f},.27f,"Color           Data");
        sprite(40,"White backing",{-3,-1,0},{3,2},glm::vec4(1));
        sprite(41,"Half red",{-3.5f,-1,.1f},{2,1.5f},{1,0,0,.5f});
        sprite(42,"Half blue overlapping red",{-2.5f,-1,.2f},{2,1.5f},{0,0,1,.5f});
        auto circle=scene->CreateEntityWithUUID(50,"Circle ring");circle.GetComponent<TransformComponent>().Translation={.3f,-1,.3f};
        auto& ring=circle.AddComponent<CircleRendererComponent>();ring.Color=ColorSpace::FromSRGB({.3f,.85f,.6f,1});ring.Thickness=.18f;
        auto text=label(51,"World typography",{1.4f,-.7f,.3f},.45f,"World text\nLinear RGB");
        label(52,"Title",{-6.2f,3.6f,.1f},.42f,"Rendering Reference / Core_v1");
        auto canvas=scene->CreateEntityWithUUID(100,"UI");canvas.AddComponent<UICanvasComponent>();
        auto footer=scene->CreateEntityWithUUID(101,"UI footer");scene->Reparent(footer,canvas,TransformPolicy::KeepLocal);
        auto& rect=footer.AddComponent<UIRectComponent>();rect.Offset={24,650};rect.FixedSize={1200,52};
        footer.AddComponent<TextComponent>().TextString="Screen UI / select shapes for entity picking / Planar and Orbit affect only the editor";
        footer.AddComponent<UITextStyleComponent>().Size=22;
        auto button=scene->CreateEntityWithUUID(102,"UI state control");scene->Reparent(button,canvas,TransformPolicy::KeepLocal);
        auto& br=button.AddComponent<UIRectComponent>();br.Offset={1020,520};br.FixedSize={200,56};
        button.AddComponent<SpriteRendererComponent>().Color=ColorSpace::FromSRGB({.2f,.4f,.6f,1});button.AddComponent<UIButtonComponent>();button.AddComponent<UIAppearanceComponent>().Styled=true;
        const auto name=perspective?"Core_v1-Perspective.hazel":"Core_v1-Orthographic.hazel";
        write(assets/"Scenes"/name,SceneSerializer(scene,assets).SerializeText());
    }
    project->GetConfig().StartScene="Scenes/Core_v1-Orthographic.hazel";
    write(created.Descriptor,ProjectSerializer(project).SerializeText());
    project->GetConfig().StartScene="Scenes/Core_v1-Perspective.hazel";
    project->GetConfig().SceneDefaults.Perspective=true;project->GetConfig().SceneDefaults.Navigation=EditorNavigation::Orbit;
    write(destination/"RenderingReference-Perspective.hproj",ProjectSerializer(project).SerializeText());
    std::filesystem::remove(assets/"Scenes/Start.hazel");
}
