#include "Hazel.h"
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif
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
#include "WindowsCrashTrace.h"
using namespace Hazel;
static void Check(bool value,const char* message) {
    if(!value) { std::cerr<<"FAIL: "<<message<<std::endl;throw std::runtime_error(message); }
}
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
#ifdef HZ_PLATFORM_WINDOWS
    // Match production launchers: CRT narrow argv cannot represent Unicode paths.
    auto encoded=WindowsCommandLineUTF8();std::vector<char*> arguments;
    for(auto& argument:encoded)arguments.push_back(argument.data());
    argc=static_cast<int>(arguments.size());arguments.push_back(nullptr);argv=arguments.data();
#endif
    const auto temporary=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel games space-é-"+std::to_string(std::random_device{}()));
    try {
        Check(argc==4 || argc==5,"Usage: ExampleGamesSmoke MeadowRun.hproj Skybound.hproj captures [LastLightkeeper.hproj]");Log::Init();
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
                for(int i=0;i<240;i++)tick(session,1.0f/120);
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
        if(argc==5) {
#ifdef HZ_PLATFORM_WINDOWS
            WindowsCrashTrace trace;
            WindowsCrashTrace::Enabled=true;
#endif
            std::cout<<"Lightkeeper: project/domain"<<std::endl;
            auto project=Project::Load(std::filesystem::u8path(argv[4]));Check(bool(project),"Cannot open Lightkeeper project");
            ScriptEngine::Init(Project::GetAssetFileSystemPath(project->GetConfig().ScriptModulePath));
            Check(ScriptEngine::EntityClassExists("LastLightkeeper.Breakwater") && !ScriptEngine::EntityClassExists("Skybound.Game"),"Lightkeeper project domain isolation failed");
            RuntimeSession session;session.Resize(960,720);
            auto menu=project->LoadScene(project->GetConfig().StartScene);
            std::cout<<"Lightkeeper: title start"<<std::endl;session.Start(project,menu);tick(session);
            Check(!session.Storage().IsPersistent(),"Editor-style session enabled player persistence");
            Check(session.GetScene()->FindEntityByName("StartLabel").GetComponent<TextComponent>().TextString=="Enter   Begin","New journey menu did not initialize");
            session.Storage().Write("journey","LK1|31|5|2|1");
            Check(session.RequestSceneLoad("Scenes/Breakwater.hazel"),"Island transition request rejected");tick(session);
            auto runtime=session.GetScene();auto player=runtime->FindEntityByName("Iona");
            Check(bool(player) && session.GetError().empty(),"Authored island failed to start");
            Check(session.Storage().Read("journey").find("LK2|543|")==0 && !runtime->FindEntityByName("ServiceDoor"),"Service receiver/latching door or LK1 migration failed");
            Check(runtime->GetAllEntitiesWith<IDComponent>().size()>3500 && runtime->GetAllEntitiesWith<IDComponent>().size()<3700,"Island or bounded beam pool incomplete");
            auto controller=ScriptEngine::GetEntityScriptInstance(runtime->FindEntityByName("Breakwater / Journey controller").GetUUID());
            auto invoke=[&](const char* method,void** values,int count) {
                auto object=controller->GetManagedObject();MonoObject* exception=nullptr;
                auto target=mono_class_get_method_from_name(mono_object_get_class(object),method,count);
                Check(bool(target),"Missing Lightkeeper lifecycle/interaction method");mono_runtime_invoke(target,object,values,&exception);
                if(exception)mono_print_unhandled_exception(exception);
                Check(!exception,"Lightkeeper managed integration callback failed");
            };
            auto interactMirror=[&](int mirror){void* args[]={&mirror};invoke("Interact",args,1);};
            interactMirror(8);interactMirror(9);tick(session);
            Check(session.Storage().Read("journey").find("LK2|575|")==0,"Court did not power its stair receiver");
            Teleport(player,{4,12,.55f});for(int i=0;i<30;i++)tick(session);
            Capture(std::filesystem::u8path(argv[3])/"LastLightkeeper-court.ppm",960,720);
            bool announce=false;void* recovery[]={&announce};invoke("Checkpoint",recovery,1);
            auto checkpoint=runtime->FindEntityByName("Checkpoint2").GetComponent<TransformComponent>().Translation;
            Check(glm::length(player.GetComponent<TransformComponent>().Translation-checkpoint)<.01f,"Checkpoint didn't restore safe position/physics");
            interactMirror(10);Check(session.Storage().Read("journey").find("LK2|639|")==0,"Beacon wasn't saved");
            invoke("CloseDialogue",nullptr,0);tick(session);tick(session);
            Check(session.GetScene()->GetName()=="CliffCistern","Beacon did not transition to cistern");
            Check(!runtime->IsRunning() && !player.GetComponent<Rigidbody2DComponent>().RuntimeBody && !controller->GetManagedObject(),"Island retained retired owners");
            auto chapter=[&]{runtime=session.GetScene();player=runtime->FindEntityByName("Iona");controller=ScriptEngine::GetEntityScriptInstance(runtime->FindEntityByName(runtime->GetName()=="SeaCaves"?"Sea caves controller":"Cliff cistern controller").GetUUID());Check(bool(controller),"Chapter controller missing");};
            auto at=[&](const char* name){auto marker=runtime->FindEntityByName(name);Check(bool(marker),"Authored interaction marker missing");auto t=marker.GetComponent<TransformComponent>().Translation;t.z=.55f;Teleport(player,t);};
            bool focus=true;void* focused[]={&focus};
            auto shot=[&](const char* name,float x,float y){auto camera=runtime->FindEntityByName("Camera");camera.GetComponent<TransformComponent>().Translation={x,y,0};runtime->SetPaused(true);tick(session,0);Capture(std::filesystem::u8path(argv[3])/(std::string(name)+".ppm"),960,720);runtime->SetPaused(false);};
            chapter();at("LanternSleeve");invoke("Interact",nullptr,0);invoke("CloseDialogue",nullptr,0);
            Check(session.Storage().Read("journey").find("LK2|1663|")==0,"Focus upgrade wasn't saved");
            at("FocusPad0");invoke("Focus",focused,1);Check(bool(runtime->FindEntityByName("DrainGate")),"Beam passed closed shutter");shot("LastLightkeeper-cistern-blocked",-4,1);
            at("Handle0");invoke("Interact",nullptr,0);at("FocusPad0");invoke("Focus",focused,1);
            Check(!runtime->FindEntityByName("DrainGate"),"Drain latch did not release upper walk");
            at("Handle0");invoke("Interact",nullptr,0);at("FocusPad1");invoke("Focus",focused,1);
            Check(!runtime->FindEntityByName("CaveEntranceGate") && session.Storage().Read("journey").find("LK2|7807|")==0,"Raised mirror route/bridge release failed");shot("LastLightkeeper-cistern-solved",5,4);
            at("CaveExit");invoke("Interact",nullptr,0);tick(session);tick(session);Check(session.GetScene()->GetName()=="SeaCaves","Cave transition failed");chapter();invoke("CloseDialogue",nullptr,0);
            at("FocusPad2");invoke("Focus",focused,1);Check(bool(runtime->FindEntityByName("GalleryGate")),"Gallery ignored its shutter");
            at("Handle1");invoke("Interact",nullptr,0);invoke("Interact",nullptr,0);at("FocusPad2");invoke("Focus",focused,1);
            Check(!runtime->FindEntityByName("GalleryGate"),"Gallery focus receiver did not latch");shot("LastLightkeeper-caves-gallery",-7,3);
            // Meaningful antagonist contracts through real managed methods; no screen-click bot.
            float zero=0;auto object=controller->GetManagedObject();auto klass=mono_object_get_class(object);
            mono_field_set_value(object,mono_class_get_field_from_name(klass,"grace"),&zero);
            at("FocusPad3");float advance=2;void* elapsed[]={&advance};invoke("Wisp",elapsed,1);
            Check(runtime->FindEntityByName("RushMark").GetComponent<TransformComponent>().Scale.x>1,"Dive telegraph absent");shot("LastLightkeeper-caves-telegraph",0,1);
            at("Checkpoint2");advance=1;invoke("Wisp",elapsed,1);invoke("Wisp",elapsed,1);
            Check(glm::length(player.GetComponent<TransformComponent>().Translation-runtime->FindEntityByName("Checkpoint2").GetComponent<TransformComponent>().Translation)<.01f,"Dodging the fixed rush mark failed");
            int awake=0;mono_field_set_value(object,mono_class_get_field_from_name(klass,"wispState"),&awake);Teleport(player,{-2,0,.55f});runtime->FindEntityByName("StormWisp").GetComponent<TransformComponent>().Translation={0,0,.7f};invoke("Focus",focused,1);
            int state=0;mono_field_get_value(object,mono_class_get_field_from_name(klass,"wispState"),&state);Check(state==3,"Focused light did not repel wisp");
            bool hit=true;void* hitArgs[]={&hit};invoke("Recover",hitArgs,1);Check(glm::length(player.GetComponent<TransformComponent>().Translation-runtime->FindEntityByName("Checkpoint1").GetComponent<TransformComponent>().Translation)<.01f,"Cave checkpoint recovery failed");
            at("Handle2");invoke("Interact",nullptr,0);invoke("Interact",nullptr,0);tick(session,0);at("FocusPad3");invoke("Focus",focused,1);
            Check(!runtime->FindEntityByName("MaraDoor") && bool(runtime->FindEntityByName("ReturnGate")),"Rescue optics or reunion consequence fired prematurely");
            at("Mara");invoke("Interact",nullptr,0);Check(!runtime->FindEntityByName("ReturnGate"),"Mara didn't unlock dry passage");shot("LastLightkeeper-mara",12,-1);invoke("CloseDialogue",nullptr,0);
            at("DryExit");invoke("Interact",nullptr,0);invoke("CloseDialogue",nullptr,0);tick(session);tick(session);
            Check(session.GetScene()->FindEntityByName("Heading").GetComponent<TextComponent>().TextString=="THE DRY\nWAY HOME","Chapter two endpoint failed");
            Check(session.Storage().Read("journey").find("LK2|130687|")==0,"Second chapter progress wasn't committed");
            Capture(std::filesystem::u8path(argv[3])/"LastLightkeeper-dawn.ppm",960,720);
            // Completed Continue, corrupt/future retention and a fresh New Game.
            auto completed=session.Storage().Read("journey");Check(session.RequestSceneLoad("Scenes/MainMenu.hazel"),"Return to menu rejected");tick(session);tick(session);
            controller=ScriptEngine::GetEntityScriptInstance(session.GetScene()->FindEntityByName("Title / Journey menu").GetUUID());
            invoke("Start",nullptr,0);tick(session);tick(session);Check(session.GetScene()->GetName()=="CliffDawn","Completed Continue lost its chapter");
            for(const auto* bad:{"LK9|future","broken"}){session.Storage().Write("journey",bad);session.RequestSceneLoad("Scenes/MainMenu.hazel");tick(session);tick(session);controller=ScriptEngine::GetEntityScriptInstance(session.GetScene()->FindEntityByName("Title / Journey menu").GetUUID());invoke("Start",nullptr,0);tick(session);Check(session.GetScene()->GetName()=="MainMenu" && session.Storage().Read("journey")==bad,"Unreadable save was overwritten or continued");}
            session.Stop();session.Start(project,menu);tick(session);Check(session.Storage().Read("journey").empty(),"Editor Stop retained progress");
            controller=ScriptEngine::GetEntityScriptInstance(session.GetScene()->FindEntityByName("Title / Journey menu").GetUUID());invoke("Start",nullptr,0);tick(session);tick(session);Check(session.GetScene()->GetName()=="Breakwater","Fresh New Game failed");session.Stop();
            Check(menu->FindEntityByName("StartLabel").GetComponent<TextComponent>().TextString=="Enter   Begin","Runtime changed authored menu");
            std::cout<<"PASS: Lightkeeper LK1 migration, service latch, focus upgrade/shutter occlusion/two chapter transitions, wisp telegraph/dodge/repel/recovery, Mara consequence, completed Continue, corrupt/future retention, New Game and Stop isolation\n";
        }
        ScriptEngine::Shutdown();framebuffer->Unbind();std::filesystem::remove_all(temporary);
        std::cout<<"PASS: real project scripts, collection/completion/checkpoint, repeated lifecycle, authored field isolation, camera fit, bounded prefab spawn/destruction/reload and cross-project class isolation\n";
    }catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';std::error_code ignored;std::filesystem::remove_all(temporary,ignored);return 1;
    }
}
