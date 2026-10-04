#include "Hazel.h"
#include "Hazel/Assets/SpriteSheetDocument.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <yaml-cpp/yaml.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>
using namespace Hazel;
static void Check(bool result,const char* message){if(!result)throw std::runtime_error(message);}
template<class F> static void Reject(F operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}Check(rejected,"Expected validation rejection");}
static std::string Read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
static void Image(const std::filesystem::path& path) {
    // Top-left TGA: upper row red/green, lower row blue/yellow, two-pixel cells.
    unsigned char header[18]{};header[2]=2;header[12]=4;header[14]=4;header[16]=32;header[17]=0x28;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(header),18);
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){unsigned char color[4]{static_cast<unsigned char>(y>=2 && x<2?255:0),static_cast<unsigned char>(x>=2?255:0),static_cast<unsigned char>(y<2 && x<2 || y>=2 && x>=2?255:0),255};out.write(reinterpret_cast<char*>(color),4);}
}
static void Logical(const std::filesystem::path& root) {
    TextureSpecification defaults;defaults.Validate();Check(defaults.MinFilter==TextureFilter::LinearMipmapLinear && defaults.MagFilter==TextureFilter::Nearest && defaults.WrapS==TextureWrap::Repeat && defaults.GenerateMips,"Whole texture defaults changed");
    defaults.GenerateMips=false;Reject([&]{defaults.Validate();});defaults.MinFilter=TextureFilter::Linear;defaults.Validate();defaults.MagFilter=TextureFilter::LinearMipmapLinear;Reject([&]{defaults.Validate();});
    auto image=Texture2D::ReadImage(root/"sheet.tga",ImageFormat::RGBA8);Check(image.Width==4 && image.Pixels[0]==255 && image.Pixels[2]==0 && image.Pixels[4*4*2+2]==255,"Top-left shared decoding orientation");
    auto uv=SpriteUV({0,0,2,2},4,4);Check(uv[3]==glm::vec2(.125,.875) && uv[1]==glm::vec2(.375,.625),"Rectangle origin / half-texel UV contract");Reject([]{SpriteUV({3,0,2,2},4,4);});Reject([]{SpriteUV({0,0,0,1},4,4);});
    auto corners=SpriteCorners({.5,1});Check(corners[0]==glm::vec2(-.5,0) && corners[2]==glm::vec2(.5,1),"Feet pivot world placement");
    Reject([]{SpriteCorners({-1,.5});});
    SpriteSheetDefinition s;s.Texture="sheet.tga";s.Sampling.Width=s.Sampling.Height=4;
    GridSliceOptions grid;grid.CellWidth=grid.CellHeight=2;auto generated=GenerateGridPreview(s,grid);Check(generated.size()==4,"Derived grid count");Check(AddGridRegions(s,generated)==4,"Grid append");auto stable=s.Regions[0].ID;Check(AddGridRegions(s,generated)==0 && s.Regions[0].ID==stable,"Grid regeneration changed identities");
    s.Regions[0].Name="renamed";s.Clips.push_back({s.NewID(true),"Blink",true,{{s.Regions[0].ID,.1},{s.Regions[1].ID,.2}}});s.Validate(true);
    SpritePlayback p;p.Reset(true);p.Advance(s.Clips[0],.15);Check(p.Frame==1,"Multiple frame durations");p.Advance(s.Clips[0],.2);Check(p.Frame==0 && std::abs(p.Time-.05)<1e-9,"Loop remainder");
    p.Playing=false;auto held=p.Time;p.Advance(s.Clips[0],10);Check(p.Time==held,"Paused playback advanced");p.Playing=true;p.Advance(s.Clips[0],10,0);Check(p.Time==held,"Zero speed advanced");
    p.Advance(s.Clips[0],std::numeric_limits<double>::max(),std::numeric_limits<double>::max());Check(std::isfinite(p.Time) && p.Time<.3,"Large timestep overflow");
    s.Clips[0].Loop=false;p.Reset(true);p.Advance(s.Clips[0],0,std::numeric_limits<double>::max());Check(p.Time==0&&!p.Finished,"Zero timestep completed a fast clip");p.Advance(s.Clips[0],999);Check(p.Finished&&!p.Playing&&p.Frame==1,"Play-once completion");Reject([&]{p.Advance(s.Clips[0],1,-1);});s.Clips[0].Loop=true;
    SaveSpriteSheet(root,"sheet.hsprites",s,WriteMode::CreateNew);auto text=Read(root/"sheet.hsprites");Reject([&]{SaveSpriteSheet(root,"sheet.hsprites",s,WriteMode::CreateNew);});Check(Read(root/"sheet.hsprites")==text,"Exclusive save clobbered file");
    auto invalid=s;invalid.Regions[0].Rect.Width=0;Reject([&]{SaveSpriteSheet(root,"sheet.hsprites",invalid);});Check(Read(root/"sheet.hsprites")==text,"Invalid save damaged original");
    FileSystem::WriteFileAtomically(root/"bad.hsprites",[](std::ostream& out){out<<"SpriteSheet: {Version: 999}";});Reject([&]{ReadSpriteSheet(root/"bad.hsprites");});
    auto loaded=ReadSpriteSheet(root/"sheet.hsprites");Check(loaded.Regions[0].ID==stable && loaded.Clips[0].Frames[0].Region==stable,"Rename broke identity");
    auto services=CreateRef<ProjectAssets>(root);SpriteSheetDocument document(services);document.Open("sheet.hsprites");document.Draft().Regions[0].Name="Unsaved change";document.Changed();
    FileSystem::WriteFileAtomically(root/"sheet.hsprites",[&](std::ostream& out){out<<text<<"\n# external edit\n";});Reject([&]{document.Save();});Check(document.Dirty()&&Read(root/"sheet.hsprites").find("external edit")!=std::string::npos,"Conflict overwrote disk/draft");
    Reject([&]{document.Open("bad.hsprites");});Check(document.Reference()=="sheet.hsprites"&&document.Dirty(),"Malformed open replaced recoverable document");document.Discard();
    services->ImportTexture(root/"sheet.tga","Imported/sheet.tga");Reject([&]{services->ImportTexture(root/"sheet.tga","Imported/sheet.tga");});Check(Read(root/"Imported/sheet.tga")==Read(root/"sheet.tga"),"Native import changed original bytes");
    Reject([&]{Project::ResolveOwnedAsset(root,"../escape.hsprites");});Reject([&]{Project::ResolveOwnedAsset(root,"C:\\external.png");});
#ifdef HZ_PLATFORM_LINUX
    std::filesystem::create_directory_symlink(root,root/"root-alias");
    Check(Project::MakeAssetReference(root/"root-alias",root/"sheet.tga")=="sheet.tga","Canonical root alias broke portable references");
    std::filesystem::remove(root/"root-alias");
#endif
    auto legacy=ReadSpriteSource(YAML::Load("TexturePath: sheet.tga\nTilingFactor: 2"));Check(std::get<TextureSpriteSource>(legacy).TilingFactor==2,"Legacy texture compatibility");
    Reject([]{ReadSpriteSource(YAML::Load("TexturePath: sheet.tga\nSource: {Type: None}"));});
    auto deps=AuditSpriteAssets(root,{"sheet.hsprites"});Check(deps.size()==2,"Unloaded export closure incomplete");
    auto relocated=root/"relocated";std::filesystem::create_directory(relocated);std::filesystem::copy_file(root/"sheet.tga",relocated/"sheet.tga");std::filesystem::copy_file(root/"sheet.hsprites",relocated/"sheet.hsprites");Check(AuditSpriteAssets(relocated,{"sheet.hsprites"}).size()==2,"Relocated closure failed");
}
static void Runtime(const std::filesystem::path& root,const std::filesystem::path& core,const std::filesystem::path& module) {
    auto assets=CreateRef<ProjectAssets>(root);auto sheet=assets->Sheet("sheet.hsprites");const auto region=sheet->Regions[0].ID;const auto clip=sheet->Clips[0].ID;
    auto resolved=assets->Resolve(SpriteReference{"sheet.hsprites",region});Check(assets->Resolve(SpriteReference{"sheet.hsprites",region})==resolved,"Region cache not reused");
    auto whole=assets->Texture("sheet.tga",TextureSpecification::FileDefaults());Check(whole!=resolved->Texture && whole->GetSpecification().GenerateMips,"Sampling identities collided");
    auto legacyScene=CreateRef<Scene>();
    YAML::Emitter legacyText;legacyText<<YAML::BeginMap<<YAML::Key<<"Scene"<<YAML::Value<<"Standalone legacy"<<YAML::Key<<"Entities"<<YAML::Value<<YAML::BeginSeq<<YAML::BeginMap<<YAML::Key<<"Entity"<<YAML::Value<<uint64_t(1)<<YAML::Key<<"SpriteRendererComponent"<<YAML::Value<<YAML::BeginMap<<YAML::Key<<"Color"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq<<1<<1<<1<<1<<YAML::EndSeq<<YAML::Key<<"TexturePath"<<YAML::Value<<(root/"sheet.tga").generic_u8string()<<YAML::Key<<"TilingFactor"<<YAML::Value<<2<<YAML::EndMap<<YAML::EndMap<<YAML::EndSeq<<YAML::EndMap;
    Check(SceneSerializer(legacyScene).DeserializeText(legacyText.c_str()),"Standalone whole-texture scene compatibility");
    Check(legacyScene->GetEntityByUUID(1).GetComponent<SpriteRendererComponent>().Resolved.Data->TilingFactor==2,"Standalone whole-texture tiling lost");
    FramebufferSpecification targetSpec;targetSpec.Width=targetSpec.Height=64;targetSpec.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER};
    auto target=Framebuffer::Create(targetSpec);target->Bind();glDisable(GL_DEPTH_TEST);target->ClearAttachment(1,-1);
    Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawSprite(glm::mat4(1),*resolved,glm::vec4(1),37);Renderer2D::EndScene();
    unsigned char pixel[4]{};glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);Check(pixel[0]>250 && pixel[2]<5 && target->ReadPixel(1,32,32)==37,"Atlas top-left UV render / picking");
    auto bottom=assets->Resolve(SpriteReference{"sheet.hsprites",sheet->Regions[2].ID});ResolvedSprite feet=*bottom;feet.Corners=SpriteCorners({.5,1});target->ClearAttachment(1,-1);
    Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawSprite(glm::mat4(1),feet,glm::vec4(1),38);Renderer2D::EndScene();
    glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(32,48,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);Check(pixel[2]>250 && pixel[0]<5 && target->ReadPixel(1,32,16)==-1 && target->ReadPixel(1,32,48)==38,"Atlas lower row orientation / feet pivot footprint");target->Unbind();glEnable(GL_DEPTH_TEST);
    resolved->Texture->Bind();GLint value=0;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&value);Check(value==GL_NEAREST,"Sheet sampling not applied");whole->Bind();glGetTexLevelParameteriv(GL_TEXTURE_2D,1,GL_TEXTURE_WIDTH,&value);Check(value==2,"File mipmaps missing");
    TextureSpecification generated;generated.Width=generated.Height=4;auto mip=Texture2D::Create(generated);unsigned char bytes[4*4*4]{};for(auto& b:bytes)b=255;mip->SetData(bytes,sizeof(bytes));mip->Bind();unsigned char small[4]{};glGetTexImage(GL_TEXTURE_2D,2,GL_RGBA,GL_UNSIGNED_BYTE,small);Check(small[0]==255,"SetData did not regenerate mips");
    auto scene=CreateRef<Scene>();scene->SetAssets(assets);auto entity=scene->CreateEntity("Animated");entity.AddComponent<SpriteRendererComponent>().Source=SpriteReference{"sheet.hsprites",region};auto& a=entity.AddComponent<SpriteAnimationComponent>();a.DefaultClip={"sheet.hsprites",clip};
    SceneSerializer(scene,root).Serialize((root/"scene.hazel").u8string());Prefab::Save(root,"sprite.hprefab",scene,entity);
    auto copy=Scene::Copy(scene);scene->OnRuntimeStart();scene->OnUpdateRuntime(.15f);Check(a.Playback.Frame==1,"Runtime animation timestep");
    scene->SetPaused(true);double time=a.Playback.Time;scene->OnUpdateRuntime(.15f);Check(time==a.Playback.Time,"Paused scene animation advanced");scene->Step();scene->OnUpdateRuntime(.1f);Check(time!=a.Playback.Time,"Frame step did not advance animation");
    auto duplicate=scene->DuplicateEntity(entity);Check(duplicate.GetComponent<SpriteAnimationComponent>().Playback.Time==0 && copy->GetEntityByUUID(entity.GetUUID()).GetComponent<SpriteAnimationComponent>().Playback.Time==0,"Copied entities share playback state");
    scene->OnRuntimeStop();copy->OnRuntimeStart();Check(copy->GetEntityByUUID(entity.GetUUID()).GetComponent<SpriteAnimationComponent>().Playback.Time==0,"New Play inherited time");copy->OnRuntimeStop();
    auto prefab=Prefab::Load(root,"sprite.hprefab");Check(Prefab::GetEntity(prefab).GetComponent<SpriteAnimationComponent>().DefaultClip.Clip==clip,"Prefab animation reference roundtrip");
    auto loaded=CreateRef<Scene>();loaded->SetAssets(assets);Check(SceneSerializer(loaded,root).Deserialize((root/"scene.hazel").u8string()),"Scene sprite roundtrip");
    auto previousResources=Resources::Get();auto resources=previousResources;resources.Root=root/"Resources";std::filesystem::create_directories(resources.Root/"Scripts");std::filesystem::copy_file(core,resources.Root/"Scripts/Hazel-ScriptCore.dll");Resources::Configure(resources);ScriptEngine::Init(module);
    SpriteSheetDocument clipEdit(assets);clipEdit.Open("sheet.hsprites");clipEdit.Draft().Clips[0].Loop=false;clipEdit.Draft().Clips[0].Name="Renamed clip";clipEdit.Changed();clipEdit.Save();
    auto managed=CreateRef<Scene>();managed->SetAssets(assets);auto probe=managed->CreateEntity("Sprite managed probe");probe.AddComponent<SpriteRendererComponent>();probe.AddComponent<SpriteAnimationComponent>();probe.AddComponent<ScriptComponent>().ClassName="Migration.SpriteProbe";
    auto& fields=ScriptEngine::GetScriptFieldMap(probe);fields["Icon"].Field={ScriptFieldType::Sprite,"Icon",nullptr};fields["Icon"].AssetReference="sheet.hsprites";fields["Icon"].AssetID=region;
    fields["Clip"].Field={ScriptFieldType::SpriteAnimation,"Clip",nullptr};fields["Clip"].AssetReference="sheet.hsprites";fields["Clip"].AssetID=clip;
    const auto managedText=SceneSerializer(managed,root).SerializeText();auto managedCopy=CreateRef<Scene>();Check(SceneSerializer(managedCopy,root).DeserializeText(managedText),"Typed managed references failed scene roundtrip");
    managedCopy->OnRuntimeStart();auto instance=ScriptEngine::GetEntityScriptInstance(probe.GetUUID());Check(instance && instance->GetFieldValue<bool>("Passed"),"Managed sprite/play/pause/resume/stop controls failed");managedCopy->OnUpdateRuntime(1);managedCopy->OnUpdateRuntime(.01f);Check(instance->GetFieldValue<bool>("Finished"),"Managed finished query / script-animation update order");managedCopy->OnRuntimeStop();ScriptEngine::Shutdown();Resources::Configure(previousResources);
    SpriteSheetDocument document(assets);document.Open("sheet.hsprites");document.Draft().Regions[0].Name="Second rename";document.Changed();document.Save();Check(assets->Resolve(SpriteReference{"sheet.hsprites",region})!=resolved && assets->Resolve(SpriteReference{"sheet.hsprites",region})->Texture!=resolved->Texture,"Cache not invalidated");
    Check(document.References(region).size()>=3,"Reference scan missed scenes/prefabs/clips");document.DeleteRegion(region);Reject([&]{document.PreviewClip(clip);});document.Save();Reject([&]{assets->Resolve(SpriteReference{"sheet.hsprites",region});});Check(document.Draft().RetiredRegionIDs.back()==region,"Deleted ID was not retired");
    Reject([&]{AuditSpriteAssets(root,{"sheet.hsprites","scene.hazel","sprite.hprefab"});});
    const auto before=SceneSerializer(loaded,root).SerializeText();Check(!SceneSerializer(loaded,root).Deserialize((root/"scene.hazel").u8string()),"Broken reference accepted strict load");Check(SceneSerializer(loaded,root).SerializeText()==before,"Broken load corrupted active scene");
    auto repair=CreateRef<Scene>();Check(SceneSerializer(repair,root,true).Deserialize((root/"scene.hazel").u8string()),"Repair load discarded references");Check(std::get<SpriteReference>(repair->GetEntityByUUID(entity.GetUUID()).GetComponent<SpriteRendererComponent>().Source).Region==region,"Repair substituted region");
    Check(glGetError()==GL_NO_ERROR,"Sprite GPU resource error");
}
int main(int argc,char** argv){
    std::filesystem::path root;
    try {Log::Init();Check(argc==3,"Usage: SpriteSmoke Core.dll Fixture.dll");root=std::filesystem::temp_directory_path()/("hazel-sprites-"+std::to_string(uint64_t(UUID())));std::filesystem::create_directory(root);Image(root/"sheet.tga");Logical(root);
        {ApplicationSpecification specification;specification.Name="Sprite services smoke";Application app(specification);glfwHideWindow(static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow()));Runtime(root,std::filesystem::u8path(argv[1]),std::filesystem::u8path(argv[2]));}
        std::filesystem::remove_all(root);std::cout<<"PASS: texture defaults/sampling/mips, origin/UV/pivot, stable identity/grid/save, scene/prefab/playback/pause/step/copy, cache/repair/export relocation\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';if(!root.empty())std::filesystem::remove_all(root);return 1;}
}
