#include "Hazel.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <box2d/box2d.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <set>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace Hazel;
static void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
static void Teleport(Entity entity,glm::vec3 position) {
    entity.GetComponent<TransformComponent>().Translation=position;
    auto* body=static_cast<b2Body*>(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
    body->SetTransform({position.x,position.y},0);body->SetLinearVelocity({0,0});
}
static void Capture(const std::filesystem::path& path,unsigned width,unsigned height) {
    std::vector<unsigned char> pixels(width*height*3);
    glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path,[&](std::ostream& out) {
        out<<"P6\n"<<width<<' '<<height<<"\n255\n";
        for(unsigned row=height;row>0;--row)out.write(reinterpret_cast<char*>(pixels.data()+(row-1)*width*3),width*3);
    });
    glPixelStorei(GL_PACK_ALIGNMENT,4);
}
int main(int argc,char** argv) {
    const auto temporary=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel games space-é-"+std::to_string(std::random_device{}()));
    try {
        Check(argc==4,"Usage: ExampleGamesSmoke MeadowRun.hproj Skybound.hproj captures");Log::Init();
        ApplicationSpecification spec;spec.Name="Example regression";spec.EnableImGui=false;Application app(spec);
        glfwHideWindow(static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow()));
        FramebufferSpecification frame;frame.Width=960;frame.Height=720;
        frame.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH24STENCIL8};
        auto framebuffer=Framebuffer::Create(frame);framebuffer->Bind();
        auto tick=[&](RuntimeSession& session,float dt=.016f) { RenderCommand::SetClearColor({.1,.1,.1,1});RenderCommand::Clear();session.Update(dt); };
        std::filesystem::create_directories(temporary);
        for(int pass=0;pass<3;pass++) {
            const bool meadow=pass!=1;const auto source=std::filesystem::u8path(argv[meadow?1:2]);
            const auto copied=temporary/(meadow?"MeadowRun":"Skybound");
            if(!std::filesystem::exists(copied))std::filesystem::copy(source.parent_path(),copied,std::filesystem::copy_options::recursive);
            auto project=Project::Load(copied/source.filename());Check(bool(project),"Cannot open independent project");
            ScriptEngine::Init(Project::GetAssetFileSystemPath(project->GetConfig().ScriptModulePath));
            Check(ScriptEngine::EntityClassExists(meadow?"MeadowRun.Explorer":"Skybound.Game")&&
                  !ScriptEngine::EntityClassExists(meadow?"Skybound.Game":"MeadowRun.Explorer"),"Project managed classes leaked across switch");
            RuntimeSession session;session.Resize(960,720);
            auto menu=project->LoadScene(project->GetConfig().StartScene);session.Start(project,menu);tick(session);
            auto scene=project->LoadScene(meadow?"Scenes/Meadow.hazel":"Scenes/Flight.hazel");
            const auto count=scene->GetAllEntitiesWith<IDComponent>().size();
            session.Start(project,scene);
            if(meadow) {
                auto authored=scene->FindEntityByName("Explorer");
                const float speed=ScriptEngine::GetScriptFieldMap(authored)["Speed"].GetValue<float>();
                for(int repeat=0;repeat<4;repeat++) {
                    if(repeat)session.Start(project,scene);
                    Ref<Scene> runtime; Entity player;Ref<ScriptInstance> instance;
                    const char* levelNames[]={"Meadow","Orchard","LanternGrove"};
                    for(int level=0;level<3;level++) {
                        runtime=session.GetScene();player=runtime->FindEntityByName("Explorer");
                        instance=ScriptEngine::GetEntityScriptInstance(player.GetUUID());instance->SetFieldValue<float>("Speed",7);
                        tick(session);
                        if(repeat==0)Capture(std::filesystem::u8path(argv[3])/(std::string("MeadowRun-")+levelNames[level]+".ppm"),960,720);
                        for(int i=0;i<5;i++) {Teleport(player,runtime->FindEntityByName("Seed"+std::to_string(i)).GetComponent<TransformComponent>().Translation);tick(session);}
                        Check(runtime->FindEntityByName("Progress").GetComponent<TextComponent>().TextString=="Lantern seeds  5 / 5","Collectible progress or once-only pickup failed");
                        Teleport(player,runtime->FindEntityByName("Pond").GetComponent<TransformComponent>().Translation);tick(session);
                        Check(std::abs(player.GetComponent<TransformComponent>().Translation.x+6)<.01f && runtime->FindEntityByName("Progress").GetComponent<TextComponent>().TextString=="Lantern seeds  5 / 5","Checkpoint lost progress or failed physics teleport");
                        Teleport(player,runtime->FindEntityByName("Exit").GetComponent<TransformComponent>().Translation);tick(session);tick(session);
                        if(level<2) Check(session.GetScene()->FindEntityByName("Explorer") ,"Next meadow level missing");
                    }
                    Check(session.GetScene()->FindEntityByName("Title").GetComponent<TextComponent>().TextString=="TRAIL RESTORED","Final completion transition failed");
                    if(repeat==0)Capture(std::filesystem::u8path(argv[3])/"MeadowRun-complete.ppm",960,720);
                    Check(!runtime->IsRunning()&&!player.GetComponent<Rigidbody2DComponent>().RuntimeBody&&!instance->GetManagedObject(),"Retired game retained physics or managed observations");
                    Check(ScriptEngine::GetScriptFieldMap(authored)["Speed"].GetValue<float>()==speed&&
                          authored.GetComponent<TransformComponent>().Translation.x==-6&&
                          scene->FindEntityByName("Seed0").GetComponent<TransformComponent>().Translation.x==-5,
                          "Play/Stop changed authored fields or entities");
                    Check(scene->GetAllEntitiesWith<IDComponent>().size()==count,"Authored entity count changed");
                }
            }else {
                for(int i=0;i<1000;i++)tick(session,1.0f/120);
                Check(session.GetScene()->GetAllEntitiesWith<IDComponent>().size()==count+8&&
                      session.GetScene()->FindEntityByName("Score").GetComponent<TextComponent>().TextString=="SCORE  0",
                      "Ready run advanced scoring or grew authored pool");
            }
            if(!meadow) {
                auto runtime=session.GetScene();
                auto controller=runtime->FindEntityByName("Game rules");
                Check(bool(controller),"Missing flight controller");
                auto game=ScriptEngine::GetEntityScriptInstance(controller.GetUUID());
                std::set<uint64_t> oldPipes;
                for(auto e:runtime->GetAllEntitiesWith<TagComponent,IDComponent>()) {
                    Entity entity{e,runtime.get()};if(entity.GetName()=="Lower0"||entity.GetName()=="Upper0")oldPipes.insert(entity.GetUUID());
                }
                for(int cycle=0;cycle<100;cycle++) {
                    // Advance the real game's spawn-generation trigger through reflection, without
                    // production test hooks or depending on a desktop's key timing. Sync uses the
                    // same managed Instantiate/Destroy callbacks as gameplay.
                    auto object=game->GetManagedObject();MonoObject* model=nullptr;
                    mono_field_get_value(object,mono_class_get_field_from_name(mono_object_get_class(object),"flight"),&model);
                    MonoArray* gates=nullptr;mono_field_get_value(model,mono_class_get_field_from_name(mono_object_get_class(model),"Gates"),&gates);
                    auto gate=mono_array_get(gates,MonoObject*,cycle%4);
                    auto field=mono_class_get_field_from_name(mono_object_get_class(gate),"<Generation>k__BackingField");
                    int generation=0;mono_field_get_value(gate,field,&generation);generation++;mono_field_set_value(gate,field,&generation);
                    MonoObject* exception=nullptr;
                    mono_runtime_invoke(mono_class_get_method_from_name(mono_object_get_class(object),"Sync",0),object,nullptr,&exception);
                    Check(!exception,"Managed spawn-cycle callback failed");tick(session);
                    Check(runtime->GetAllEntitiesWith<IDComponent>().size()==count+8,"Repeated pipe spawn grew entities/resources");
                }
                for(auto id:oldPipes)Check(!runtime->GetEntityByUUID(id),"Obsolete pipe identity was recycled");
                ScriptEngine::ReloadAssembly();tick(session);
                Check(runtime->GetAllEntitiesWith<IDComponent>().size()==count+8,"Assembly reload retained gameplay-owned prefab instances");
            }
            if(!meadow) for(auto dimensions:std::vector<glm::uvec2>{{1280,720},{960,720},{600,1000},{640,480}}) {
                framebuffer->Unbind();framebuffer->Resize(dimensions.x,dimensions.y);framebuffer->Bind();session.Resize(dimensions.x,dimensions.y);tick(session);
                auto runtime=session.GetScene();auto ground=runtime->FindEntityByName("Ground").GetComponent<TransformComponent>();
                auto camera=runtime->GetPrimaryCameraEntity().GetComponent<CameraComponent>().Camera;
                float floor=ground.Translation.y+ground.Scale.y/2;
                Check(std::abs(ground.Translation.y-ground.Scale.y/2+camera.GetOrthographicSize()/2)<.001f,"Ground banner is not flush with viewport bottom");
                for(auto id:runtime->GetAllEntitiesWith<TagComponent,TransformComponent>()) {Entity entity{id,runtime.get()};
                    auto transform=entity.GetComponent<TransformComponent>();
                    if(entity.GetName()=="Lower0") Check(std::abs(transform.Translation.y-transform.Scale.y/2-floor)<.001f,"Pipe extends below collision floor");
                    if(entity.GetName()=="Upper0") Check(std::abs(transform.Translation.y+transform.Scale.y/2-camera.GetOrthographicSize()/2)<.001f,"Upper pipe misses viewport top");
                }
                Check(std::abs(runtime->FindEntityByName("Score").GetComponent<TransformComponent>().Translation.y-(camera.GetOrthographicSize()/2-1.2f))<.001f,"HUD does not follow viewport top");
                Capture(std::filesystem::u8path(argv[3])/("Skybound-"+std::to_string(dimensions.x)+"x"+std::to_string(dimensions.y)+".ppm"),dimensions.x,dimensions.y);
            }
            framebuffer->Unbind();framebuffer->Resize(960,720);framebuffer->Bind();
            session.Resize(600,1000);tick(session);
            const auto& camera=session.GetScene()->GetPrimaryCameraEntity().GetComponent<CameraComponent>().Camera;
            Check(camera.GetOrthographicSize()>=12&&camera.GetOrthographicSize()*camera.GetAspectRatio()>=15.99f,"Resize cropped the authored game area");
            session.Stop();Check(!scene->IsRunning(),"Authored scene entered runtime");
        }
        ScriptEngine::Shutdown();framebuffer->Unbind();std::filesystem::remove_all(temporary);
        std::cout<<"PASS: real project scripts, collection/completion/checkpoint, repeated lifecycle, authored field isolation, camera fit, bounded prefab spawn/destruction/reload and cross-project class isolation\n";
    }catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';std::error_code ignored;std::filesystem::remove_all(temporary,ignored);return 1;
    }
}
