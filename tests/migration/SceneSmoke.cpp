// Scene/entity ownership regressions; CPU-only until explicit renderer gates.
#include "Hazel/Core/Log.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/ScriptableEntity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/FileWatcher.h"
#include <mono/metadata/mono-gc.h>
#include <box2d/box2d.h>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>
#include <iostream>
#include <stdexcept>
#ifdef HZ_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif
using namespace Hazel;
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
    std::filesystem::path Previous=std::filesystem::current_path();
    std::filesystem::path Path=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel-scene-é-"+std::to_string(std::random_device{}()));
    Fixture() { Check(std::filesystem::create_directory(Path),"Scene fixture isolation failed"); }
    ~Fixture() {
        std::error_code error; std::filesystem::current_path(Previous,error);
        std::filesystem::remove_all(Path,error);
    }
};
static unsigned ResourceCount() {
#ifdef HZ_PLATFORM_WINDOWS
    DWORD count=0; Check(GetProcessHandleCount(GetCurrentProcess(),&count),"Cannot count watcher handles"); return count;
#else
    unsigned count=0; for (const auto& item:std::filesystem::directory_iterator("/proc/self/fd")) { (void)item; ++count; } return count;
#endif
}
static void WatcherChecks(const std::filesystem::path& directory) {
    const auto file=directory/std::filesystem::u8path("watched-é.dll"); std::ofstream(file)<<"initial";
    auto exercise=[&](bool atomic=false) {
        std::mutex mutex; std::condition_variable changed; bool observed=false;
        auto watcher=FileWatcher::Create(file,[&](const auto& path,FileWatchEvent event) {
            if (path.filename()==file.filename() && event==(atomic ? FileWatchEvent::renamed_new : FileWatchEvent::modified)) {
                std::lock_guard<std::mutex> lock(mutex); observed=true; changed.notify_one();
            }
        });
        if (atomic) {
            const auto replacement=directory/"replacement.tmp";
            std::ofstream(replacement)<<"atomic replacement";
            std::filesystem::rename(replacement,file);
        } else std::ofstream(file,std::ios::app)<<"modified";
        std::unique_lock<std::mutex> lock(mutex);
        Check(changed.wait_for(lock,std::chrono::seconds(5),[&] { return observed; }),atomic ? "Atomic replacement was not observed" : "UTF-8 file modification was not observed");
        lock.unlock(); watcher.reset(); // Must join callbacks before their captures disappear.
    };
    exercise(); // Allow one-time standard-library/thread initialization before resource baseline.
    const auto before=ResourceCount();
    for(int i=0;i<12;++i) exercise();
    exercise(true);
    Check(ResourceCount()==before,"Repeated watcher destruction leaked handles/descriptors");
    std::cout<<"PASS: UTF-8 file notifications/atomic replacement, joined callbacks and stable watcher resource count\n";
}
struct NativeProbe : ScriptableEntity {
    inline static int Created=0,Updated=0,Destroyed=0,Deleted=0;
    ~NativeProbe() override { ++Deleted; }
    void OnCreate() override { ++Created; }
    void OnUpdate(Timestep) override { ++Updated; }
    void OnDestroy() override { ++Destroyed; }
};
static void SerializerChecks(const std::filesystem::path& directory) {
    auto source=CreateRef<Scene>(); auto entity=source->CreateEntityWithUUID(200,u8"sérialisation-é");
    auto& transform=entity.GetComponent<TransformComponent>(); transform.Translation={1,2,3}; transform.Rotation={0.1f,0.2f,0.3f}; transform.Scale={2,3,4};
    auto& camera=entity.AddComponent<CameraComponent>(); camera.Primary=false; camera.FixedAspectRatio=true; camera.Camera.SetOrthographic(7,-2,5);
    auto& sprite=entity.AddComponent<SpriteRendererComponent>(); sprite.Color={0.1f,0.2f,0.3f,0.4f}; sprite.TilingFactor=3.5f;
    auto& circle=entity.AddComponent<CircleRendererComponent>(); circle.Thickness=0.25f; circle.Fade=0.02f; circle.Color={1,0,0,1};
    auto& rigid=entity.AddComponent<Rigidbody2DComponent>(); rigid.Type=Rigidbody2DComponent::BodyType::Kinematic; rigid.FixedRotation=true;
    auto& box=entity.AddComponent<BoxCollider2DComponent>(); box.Offset={0.1f,0.2f}; box.Size={0.3f,0.4f}; box.Density=2; box.Friction=0.7f; box.Restitution=0.8f; box.RestitutionThreshold=1.25f;
    auto& disk=entity.AddComponent<CircleCollider2DComponent>(); disk.Offset={0.2f,0.3f}; disk.Radius=0.6f; disk.Density=3; disk.Friction=0.4f; disk.Restitution=0.5f; disk.RestitutionThreshold=0.75f;
    entity.AddComponent<ScriptComponent>().ClassName="Migration.SceneProbe";
    const auto type=ScriptEngine::GetEntityClass("Migration.SceneProbe");
    auto& fields=ScriptEngine::GetScriptFieldMap(entity);
    auto store=[&](const char* name,auto value) { fields[name].Field=type->GetFields().at(name); fields[name].SetValue(value); };
    store("Speed",4.5f); store("Precise",7.125); store("Enabled",false); store("Character",uint16_t(0x03a9));
    store("SignedByte",int8_t(-12)); store("UnsignedByte",uint8_t(250)); store("SignedShort",int16_t(-1234)); store("UnsignedShort",uint16_t(60000));
    store("SignedInt",int32_t(-123456)); store("UnsignedInt",uint32_t(4000000000U)); store("SignedLong",int64_t(-1234567890123LL)); store("UnsignedLong",uint64_t(0xfedcba9876543210ULL));
    store("Pair",glm::vec2(2,3)); store("Position",glm::vec3(1,2,3)); store("Color",glm::vec4(1,2,3,4)); store("Target",UUID(200));
    fields["PreviouslyPublic"].Field={ScriptFieldType::Double,"PreviouslyPublic",nullptr}; fields["PreviouslyPublic"].SetValue<double>(6.25);
    auto missing=source->CreateEntityWithUUID(201,"Missing class"); missing.AddComponent<ScriptComponent>().ClassName="Unavailable.Script";
    auto& orphan=ScriptEngine::GetScriptFieldMap(missing)["Value"]; orphan.Field={ScriptFieldType::ULong,"Value",nullptr}; orphan.SetValue<uint64_t>(1234567890123ULL);
    auto duplicate=source->DuplicateEntity(entity); Check(ScriptEngine::GetScriptFieldMap(duplicate)["Character"].GetValue<uint16_t>()==0x03a9,"Entity duplication lost stored script fields"); source->DestroyEntity(duplicate);
    const auto expectedFields=fields;
    const auto file=directory/std::filesystem::u8path("scène-é.hazel"); SceneSerializer(source).Serialize(file.generic_u8string());
    auto restored=CreateRef<Scene>(); Check(SceneSerializer(restored).Deserialize(file.generic_u8string()),"Complete CPU scene round trip failed");
    auto loaded=restored->GetEntityByUUID(200); Check(loaded && loaded.GetName()==u8"sérialisation-é","UUID/UTF-8 tag serialization failed");
    const auto& expectedTransform=entity.GetComponent<TransformComponent>();
    const auto& tc=loaded.GetComponent<TransformComponent>(); Check(tc.Translation==expectedTransform.Translation && tc.Rotation==expectedTransform.Rotation && tc.Scale==expectedTransform.Scale,"Transform serialization failed");
    const auto& cc=loaded.GetComponent<CameraComponent>(); Check(!cc.Primary && cc.FixedAspectRatio && cc.Camera.GetOrthographicSize()==7 && cc.Camera.GetOrthographicNearClip()==-2 && cc.Camera.GetOrthographicFarClip()==5,"Camera serialization failed");
    Check(loaded.GetComponent<SpriteRendererComponent>().Color==entity.GetComponent<SpriteRendererComponent>().Color && loaded.GetComponent<SpriteRendererComponent>().TilingFactor==3.5f,"Sprite serialization failed");
    Check(loaded.GetComponent<CircleRendererComponent>().Thickness==entity.GetComponent<CircleRendererComponent>().Thickness && loaded.GetComponent<CircleRendererComponent>().Fade==entity.GetComponent<CircleRendererComponent>().Fade,"Circle serialization failed");
    Check(loaded.GetComponent<Rigidbody2DComponent>().Type==entity.GetComponent<Rigidbody2DComponent>().Type && loaded.GetComponent<Rigidbody2DComponent>().FixedRotation && !loaded.GetComponent<Rigidbody2DComponent>().RuntimeBody,"Rigid body serialization failed");
    const auto& bc=loaded.GetComponent<BoxCollider2DComponent>(); const auto& dc=loaded.GetComponent<CircleCollider2DComponent>();
    const auto& expectedBox=entity.GetComponent<BoxCollider2DComponent>(); const auto& expectedDisk=entity.GetComponent<CircleCollider2DComponent>();
    Check(bc.Offset==expectedBox.Offset && bc.Size==expectedBox.Size && bc.Density==expectedBox.Density && bc.Friction==expectedBox.Friction && bc.Restitution==expectedBox.Restitution && bc.RestitutionThreshold==expectedBox.RestitutionThreshold && !bc.RuntimeFixture,"Box collider serialization failed");
    Check(dc.Offset==expectedDisk.Offset && dc.Radius==expectedDisk.Radius && dc.Density==expectedDisk.Density && dc.Friction==expectedDisk.Friction && dc.Restitution==expectedDisk.Restitution && dc.RestitutionThreshold==expectedDisk.RestitutionThreshold && !dc.RuntimeFixture,"Circle collider serialization failed");
    const auto& saved=ScriptEngine::GetScriptFieldMap(loaded);
    for(auto [name,value]:expectedFields) {
        auto actual=saved.at(name); Check(actual.Field.Type==value.Field.Type,"Stored field type lost");
        Check((actual.GetValue<std::array<uint8_t,16>>()==value.GetValue<std::array<uint8_t,16>>()),"Stored field bytes changed on YAML round trip");
    }
    Check(ScriptEngine::GetScriptFieldMap(restored->GetEntityByUUID(201))["Value"].GetValue<uint64_t>()==1234567890123ULL,"Unavailable script class lost stored data");
    SceneSerializer(restored).Serialize((directory/"second.hazel").generic_u8string());
    const auto malformed=directory/"malformed.hazel";
    std::ofstream(malformed)<<"Scene: Broken\nEntities:\n  - Entity: 999\n    TransformComponent:\n      Translation: [1, 2]\n";
    Check(!SceneSerializer(restored).Deserialize(malformed.generic_u8string()) && restored->GetEntityByUUID(200) && !restored->GetEntityByUUID(999),"Malformed scene partially replaced live data");
    Check(ScriptEngine::GetScriptFieldMap(loaded).at("Character").GetValue<uint16_t>()==0x03a9,"Failed scene load modified saved field data");
    Check(!SceneSerializer(restored).Deserialize((directory/"missing.hazel").generic_u8string()),"Missing scene file reported success");
    bool rejected=false; try { SceneSerializer(restored).Serialize((directory/"missing-dir/file.hazel").generic_u8string()); } catch (const std::exception&) { rejected=true; }
    Check(rejected,"Scene write failure was not surfaced");
    std::cout<<"PASS: scene YAML camera/renderer/physics/managed fields, UTF-16/UTF-8, missing-class retention, duplication and transactional failure recovery\n";
}
static void ManagedChecks(const std::filesystem::path& core,const std::filesystem::path& app,Fixture& fixture) {
    std::filesystem::create_directories(fixture.Path/"Resources/Scripts");
    const auto target=fixture.Path/std::filesystem::u8path("Fixture-é.dll");
    std::filesystem::copy_file(core,fixture.Path/"Resources/Scripts/Hazel-ScriptCore.dll");
    std::filesystem::copy_file(app,target);
    std::filesystem::current_path(fixture.Path);
    auto project=Project::New(); project->GetConfig().AssetDirectory=".";
    project->GetConfig().ScriptModulePath=target.filename();
    Check(Project::SaveActive(fixture.Path/"Scene.hproj"),"Managed scene project setup failed");
    const auto validModule=project->GetConfig().ScriptModulePath;
    project->GetConfig().ScriptModulePath="missing.dll";
    bool refusedInit=false; try { ScriptEngine::Init(); } catch (const std::exception&) { refusedInit=true; }
    Check(refusedInit && !ScriptEngine::IsInitialized(),"Missing initial assembly silently initialized scripting");
    project->GetConfig().ScriptModulePath=validModule;
    std::ofstream(target,std::ios::binary|std::ios::trunc)<<"invalid initial assembly";
    refusedInit=false; try { ScriptEngine::Init(); } catch (const std::exception&) { refusedInit=true; }
    Check(refusedInit && !ScriptEngine::IsInitialized(),"Invalid initial assembly silently initialized scripting");
    std::filesystem::copy_file(app,target,std::filesystem::copy_options::overwrite_existing);
    ScriptEngine::Init();
    SerializerChecks(fixture.Path);
    auto type=ScriptEngine::GetEntityClass("Migration.SceneProbe"); Check(bool(type),"Full ScriptEngine failed to discover fixture class");
    const auto& fields=type->GetFields();
    Check(!fields.count("ProtectedField") && !fields.count("PrivateField") && !fields.count("StaticField"),"Non-public/static fields leaked into instance reflection");
    Check(fields.at("Character").Type==ScriptFieldType::Char && fields.at("SignedByte").Type==ScriptFieldType::Byte && fields.at("UnsignedByte").Type==ScriptFieldType::UByte,"Managed signed byte/char reflection mismatch");
    auto scene=CreateRef<Scene>(); auto script=scene->CreateEntityWithUUID(100,"Script");
    script.AddComponent<ScriptComponent>().ClassName="Migration.SceneProbe";
    auto body=scene->CreateEntityWithUUID(101,"Body"); body.AddComponent<ScriptComponent>().ClassName="Migration.SceneProbe";
    body.AddComponent<Rigidbody2DComponent>().Type=Rigidbody2DComponent::BodyType::Dynamic;
    body.AddComponent<BoxCollider2DComponent>(); body.AddComponent<CircleCollider2DComponent>();
    auto native=scene->CreateEntityWithUUID(102,"Native"); native.AddComponent<NativeScriptComponent>().Bind<NativeProbe>();
    auto& saved=ScriptEngine::GetScriptFieldMap(script)["Speed"]; saved.Field=fields.at("Speed"); saved.SetValue<float>(4.0f);
    scene->OnRuntimeStart(); auto instance=ScriptEngine::GetEntityScriptInstance(100);
    Check(instance && instance->GetFieldValue<float>("Speed")==4 && instance->GetFieldValue<int>("Creates")==1,"Runtime field override/OnCreate failed");
    Check(instance->GetFieldValue<uint16_t>("Character")==0x3bb && instance->GetFieldValue<uint64_t>("UnsignedLong")==0xfedcba9876543210ULL,"Managed UTF-16/uint64 layout failed");
    Check(instance->GetFieldValue<int8_t>("SignedByte")==-12 && instance->GetFieldValue<uint8_t>("UnsignedByte")==250,"Managed byte signedness failed");
    Check(instance->GetFieldValue<glm::vec4>("Color")==glm::vec4(1,2,3,4),"Managed vector layout failed");
    Check(instance->GetFieldValue<bool>("HasTransform") && !instance->GetFieldValue<bool>("HasBody") && !instance->GetFieldValue<bool>("HasText"),"Actual managed component registration/HasComponent failed");
    instance->SetFieldValue<uint64_t>("Target",101); Check(instance->GetFieldValue<uint64_t>("Target")==101,"Entity reference did not round trip through UUID");
    instance->SetFieldValue<uint64_t>("Target",0); Check(instance->GetFieldValue<uint64_t>("Target")==0,"Null Entity reference did not round trip");
    instance->SetFieldValue<uint64_t>("Target",102); Check(instance->GetFieldValue<uint64_t>("Target")==102,"Unscripted Entity reference failed");
    mono_gc_collect(mono_gc_max_generation()); Check(instance->GetManagedObject(),"Full ScriptEngine lost managed owner after GC");
    scene->OnUpdateRuntime(0.25f);
    Check(script.GetComponent<TransformComponent>().Translation.x==1 && instance->GetFieldValue<int>("Updates")==1,"Managed transform internal calls/update failed");
    Check(body.GetComponent<TransformComponent>().Translation.x>0 && body.GetComponent<TransformComponent>().Translation.y<0,"Managed impulse/scene physics update failed");
    scene->SetPaused(true); scene->OnUpdateRuntime(0.25f); Check(instance->GetFieldValue<int>("Updates")==1,"Paused scene updated scripts");
    scene->Step(2); for(int i=0;i<3;++i) scene->OnUpdateRuntime(0.25f);
    Check(instance->GetFieldValue<int>("Updates")==3 && NativeProbe::Updated==3,"Paused frame stepping failed");
    const auto nativeCopy=Scene::Copy(scene); auto duplicate=scene->DuplicateEntity(native);
    Check(!nativeCopy->GetEntityByUUID(102).GetComponent<NativeScriptComponent>().Instance && !duplicate.GetComponent<NativeScriptComponent>().Instance,"Copied native script retained an owner");
    native.RemoveComponent<NativeScriptComponent>(); Check(NativeProbe::Destroyed==1 && NativeProbe::Deleted==1,"Native removal did not destroy exactly once");
    auto* physics=static_cast<b2Body*>(body.GetComponent<Rigidbody2DComponent>().RuntimeBody);
    const int count=physics->GetWorld()->GetBodyCount(); body.RemoveComponent<CircleCollider2DComponent>();
    Check(physics->GetFixtureList() && !physics->GetFixtureList()->GetNext(),"Collider removal retained live fixture");
    auto bodyInstance=ScriptEngine::GetEntityScriptInstance(101); scene->DestroyEntity(body);
    Check(!bodyInstance->GetManagedObject() && !ScriptEngine::GetEntityScriptInstance(101),"Destroyed managed entity retained instance");
    // Another live body is needed to safely inspect the world's body count after deletion.
    Check(count==1,"Unexpected scene physics body count");
    instance->SetFieldValue<double>("Precise",7.125); instance->SetFieldValue<uint16_t>("Character",0x03a9);
    std::ofstream(target,std::ios::binary|std::ios::trunc)<<"broken assembly";
    bool refused=false; try { ScriptEngine::ReloadAssembly(); } catch (const std::exception&) { refused=true; }
    Check(refused && instance->GetManagedObject() && ScriptEngine::GetEntityScriptInstance(100)==instance,"Invalid assembly reload destroyed working domain");
    std::filesystem::copy_file(app,target,std::filesystem::copy_options::overwrite_existing);
    ScriptEngine::ReloadAssembly(); auto reloaded=ScriptEngine::GetEntityScriptInstance(100);
    Check(reloaded && reloaded!=instance && !instance->GetManagedObject() && !type->GetMethod("OnCreate",0),"Reload left stale external Mono observations");
    Check(reloaded->GetFieldValue<double>("Precise")==7.125 && reloaded->GetFieldValue<uint16_t>("Character")==0x03a9 && reloaded->GetFieldValue<int>("Updates")==3,"Reload lost live field values");
    scene->OnRuntimeStop(); Check(!reloaded->GetManagedObject() && !ScriptEngine::GetSceneContext(),"Runtime stop retained domain instances/context");
    Check(NativeProbe::Destroyed==1 && NativeProbe::Deleted==1,"Runtime stop destroyed a never-instantiated native script");
    scene->SetPaused(false); scene->OnRuntimeStart(); scene->OnUpdateRuntime(0.25f); scene->OnRuntimeStop();
    Check(NativeProbe::Destroyed==2 && NativeProbe::Deleted==2,"Restarted native script was not released exactly once");
    scene.reset();
    ScriptEngine::Shutdown(); Check(!ScriptEngine::GetSceneContext(),"ScriptEngine shutdown left scene context");
    std::cout<<"PASS: full managed reflection/GC/internal calls, native script ownership, scene pause/step/restart, invalid/valid reload and field preservation\n";
}
int main(int argc,char** argv) {
    try {
        Log::Init(); Fixture fixture; WatcherChecks(fixture.Path); auto scene=CreateRef<Scene>();
        auto stale=scene->CreateEntityWithUUID(42,u8"entité-é");
        Check(stale.GetUUID()==UUID(42) && stale.GetName()==u8"entité-é","Entity ID/tag initialization failed");
        scene->DestroyEntity(stale); Check(!stale,"Destroyed entity still reports valid");
        auto body=scene->CreateEntityWithUUID(43,"Body");
        auto& rigid=body.AddComponent<Rigidbody2DComponent>(); rigid.Type=Rigidbody2DComponent::BodyType::Dynamic;
        body.AddComponent<BoxCollider2DComponent>();
        scene->OnSimulationStart(); Check(rigid.RuntimeBody,"Scene simulation body missing");
        auto copy=Scene::Copy(scene); auto copied=copy->GetEntityByUUID(43);
        Check(!copied.GetComponent<Rigidbody2DComponent>().RuntimeBody,"Scene copy retained live physics body");
        auto duplicate=scene->DuplicateEntity(body);
        Check(duplicate.GetUUID()!=body.GetUUID() && !duplicate.GetComponent<Rigidbody2DComponent>().RuntimeBody,
              "Entity duplication retained runtime body or UUID");
        scene->OnSimulationStop(); Check(!body.GetComponent<Rigidbody2DComponent>().RuntimeBody,"Simulation stop left stale body observation");
        scene->OnSimulationStart(); Check(body.GetComponent<Rigidbody2DComponent>().RuntimeBody,"Simulation restart failed");
        scene->OnSimulationStop();
        {
            auto standalone=CreateRef<Scene>(); auto native=standalone->CreateEntity("Standalone native");
            native.AddComponent<NativeScriptComponent>().Bind<NativeProbe>();
            standalone->OnRuntimeStart(); standalone->OnUpdateRuntime(0); standalone->OnRuntimeStop();
            Check(NativeProbe::Created==1 && NativeProbe::Destroyed==1 && NativeProbe::Deleted==1,"Native scenes incorrectly require a Mono project or leak their owner");
            NativeProbe::Created=NativeProbe::Updated=NativeProbe::Destroyed=NativeProbe::Deleted=0;
            auto unavailable=standalone->CreateEntity("Unavailable managed"); unavailable.AddComponent<ScriptComponent>().ClassName="Migration.SceneProbe";
            bool rejected=false; try { standalone->OnRuntimeStart(); } catch (const std::exception&) { rejected=true; }
            Check(rejected && !standalone->IsRunning(),"Scene play with unavailable managed engine was not rejected cleanly");
        }
        Check(argc==3,"Usage: SceneSmoke Core.dll Fixture.dll");
        ManagedChecks(std::filesystem::absolute(std::filesystem::u8path(argv[1])),std::filesystem::absolute(std::filesystem::u8path(argv[2])),fixture);
        std::cout<<"PASS: entity validity, UUID/tag initialization, live scene copy/duplicate sanitation and simulation stop/restart\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; ScriptEngine::Shutdown(); return 1; }
}
