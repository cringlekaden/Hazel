#pragma once
#include "Hazel/Assets/SpriteSheetDocument.h"
static void ImageFitSceneChecks(const std::filesystem::path &root)
{
    { // An atlas with a 4x2 green source region in an 8x8 red texture.
        unsigned char header[18]{};header[2]=2;header[12]=header[14]=8;header[16]=32;header[17]=0x28;
        std::ofstream out(root/"fit-atlas.tga",std::ios::binary);out.write(reinterpret_cast<char*>(header),18);
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){bool inside=x>=2&&x<6&&y>=2&&y<4;unsigned char color[4]{0,(unsigned char)(inside?255:0),(unsigned char)(inside?0:255),255};out.write(reinterpret_cast<char*>(color),4);}
    }
    SpriteSheetDefinition sheet;sheet.Texture="fit-atlas.tga";sheet.Sampling.Width=sheet.Sampling.Height=8;
    sheet.Regions={{1,"Wide region",{2,2,4,2}},{2,"Single pixel",{2,2,1,1}}};SaveSpriteSheet(root,"fit.hsprites",sheet);
    auto scene=CreateRef<Scene>();scene->SetAssets(CreateRef<ProjectAssets>(root));scene->OnViewportResize(160,120);
    auto canvas=scene->CreateEntity("Image canvas");canvas.AddComponent<UICanvasComponent>().ReferenceSize={160,120};canvas.GetComponent<UICanvasComponent>().SafeInsets={};
    auto image=scene->CreateEntity("Fitted atlas button");scene->Reparent(image,canvas,TransformPolicy::KeepLocal);
    auto &r=image.AddComponent<UIRectComponent>();r.Offset={10,10};r.FixedSize={100,80};r.Padding=glm::vec4(10);r.Fit=ImageFit::Contain;
    image.AddComponent<SpriteRendererComponent>().Source=SpriteReference{"fit.hsprites",1};image.AddComponent<UIButtonComponent>().Action="image";
    scene->PrepareSprites(true);
    FramebufferSpecification spec;spec.Width=160;spec.Height=120;spec.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);target->Bind();RenderCommand::Clear();target->ClearAttachment(1,-1);
    RuntimeUI runtime(*scene);runtime.Resolve({160,120});runtime.Draw();UIImageVisual painted;
    Check(runtime.ImageVisual(image.GetUUID(),painted)&&painted.SourceSize==glm::vec2(4,2)&&painted.Placement.Visual.Position==glm::vec2(20,30)&&painted.Placement.Visual.Size==glm::vec2(80,40),"Atlas fit used inset UV extent or whole texture dimensions");
    Check(target->ReadPixel(1,40,120-40)==int(uint32_t(image))&&target->ReadPixel(1,40,120-23)==-1,"Contain GPU picking ignored fitted visual/padding");
    glReadBuffer(GL_COLOR_ATTACHMENT0);unsigned char pixel[4]{};glReadPixels(40,120-40,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);Check(pixel[1]>240&&pixel[0]<10,"Atlas fit sampled outside selected region");
    auto generation=runtime.Snapshot().Generation;
    image.GetComponent<UIRectComponent>().Fit=ImageFit::Cover;runtime.Resolve({160,120});Check(runtime.Snapshot().Generation==generation,"Paint-only fit mutation rebuilt layout");
    RenderCommand::Clear();target->ClearAttachment(1,-1);runtime.Draw();runtime.ImageVisual(image.GetUUID(),painted);
    Check(painted.Placement.Visual.Position==glm::vec2(20,20)&&painted.Placement.Visual.Size==glm::vec2(80,60)&&std::abs(painted.Placement.UV.Size.x-2.f/3)<.001f,"Cover atlas crop failed");
    Check(target->ReadPixel(1,40,120-23)==int(uint32_t(image)),"Cover fitted visual failed actual GPU picking");
    image.GetComponent<UIRectComponent>().WidthMode=SizeMode::Content;
    image.GetComponent<UIButtonComponent>().Images[int(ButtonState::Disabled)]=TextureSpriteSource{"image.png",1,{}};image.GetComponent<UIButtonComponent>().Interactable=false;
    runtime.Resolve({160,120});runtime.Draw();runtime.ImageVisual(image.GetUUID(),painted);
    Check(runtime.Snapshot().Find(image.GetUUID())->Bounds.Size.x==24&&painted.SourceSize.x>4,"State image changed base intrinsic layout or failed independent fitting");
    image.GetComponent<UIRectComponent>().WidthMode=SizeMode::Fixed;image.GetComponent<UIButtonComponent>().Interactable=true;image.GetComponent<UIButtonComponent>().Images[int(ButtonState::Disabled)]=std::monostate{};runtime.Resolve({160,120});
    InputFrame input;input.Enabled=true;input.Mouse={12,12};runtime.UpdateInput(input);input.PointerPressed=input.PointerReleased=true;input.PressPosition=input.Mouse;runtime.UpdateInput(input);Action action;
    Check(runtime.Consume(image.GetUUID(),action),"Image padding/letterbox unexpectedly changed control-box hit region");
    scene->SetUIImageFit(image.GetUUID(),ImageFit::Native,{1,1});scene->RefreshUI();EditorCamera editorCamera(45,160.f/120,.1f,1000);scene->OnUpdateEditor(0,editorCamera);Check(scene->GetUIImageVisual(image.GetUUID(),painted)&&painted.Placement.Visual.Size==glm::vec2(4,2),"Native-size source presentation scaled atlas pixels");
    auto authored=SceneSerializer(scene,root).CaptureAuthoredState();auto copy=Scene::Copy(scene);Check(SceneSerializer(copy,root).CaptureAuthoredState()==authored,"Image fit clone lost fields");
    auto loaded=CreateRef<Scene>();Check(SceneSerializer(loaded,root).DeserializeText(SceneSerializer(scene,root).SerializeText())&&SceneSerializer(loaded,root).CaptureAuthoredState()==authored,"Image fit roundtrip lost state");
    Prefab::Save(root,"image-fit.hprefab",scene,image);auto instance=Prefab::Instantiate(root,"image-fit.hprefab",*loaded);Check(instance.GetComponent<UIRectComponent>().Fit==ImageFit::Native&&instance.GetComponent<UIRectComponent>().ImageAlignment==glm::vec2(1),"Image fit prefab lost values");
    scene->SetUIImageFit(image.GetUUID(),ImageFit::Stretch);image.AddComponent<UINineSliceComponent>();bool rejected=false;
    try{scene->SetUIImageFit(image.GetUUID(),ImageFit::Cover);}catch(const std::invalid_argument &){rejected=true;}
    Check(rejected&&image.GetComponent<UIRectComponent>().Fit==ImageFit::Stretch,"Nine-slice silently accepted incompatible image fitting");
    auto assets=scene->GetAssets();auto source=assets->Resolve(SpriteReference{"fit.hsprites",1});Check(source->PixelSize()==glm::vec2(4,2),"Resolved atlas intrinsic metadata lost authored extent");
    auto pixelRegion=assets->Resolve(SpriteReference{"fit.hsprites",2});Check(pixelRegion->PixelSize()==glm::vec2(1),"One-pixel atlas region collapsed to zero inset UV size");
    target->Unbind();
    std::cout<<"PASS: real atlas region fit/crop/color/picking, control hit policy, paint-only generation reuse, Native size, nine-slice validation and copy/prefab/serialization\n";
}
