// Actual target project serialization and pinned Box2D CPU prerequisites.
#include "Hazel/Core/Log.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectSerializer.h"
#include <box2d/box2d.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef HZ_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/resource.h>
#include <csignal>
#endif
using namespace Hazel;
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
static std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary); return {std::istreambuf_iterator<char>(input),{}};
}
struct Fixture {
    std::filesystem::path Path=std::filesystem::temp_directory_path()/
        std::filesystem::u8path("hazel-project-é-"+std::to_string(std::random_device{}()));
    Fixture() { Check(std::filesystem::create_directory(Path),"Project fixture isolation failed"); }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(Path,error); }
};
static void SafeSaves() {
    Fixture fixture;
    const auto file=fixture.Path/std::filesystem::u8path(u8"save é 🚀.hazel");
    std::ofstream(file)<<"previous scene bytes";
    const auto foreign=fixture.Path/"unrelated.hazel-tmp-reserved";
    std::ofstream(foreign)<<"another attempt";
    auto noOwnedTemps=[&]() {
        for(const auto& entry:std::filesystem::directory_iterator(fixture.Path))
            Check(entry.path()==foreign || entry.path().filename().generic_u8string().find(".hazel-tmp-")==std::string::npos,
                  "Failed save leaked its sibling temporary");
        Check(Read(foreign)=="another attempt","Save removed another attempt's temporary");
    };
    for(bool failStream:{false,true}) {
        bool rejected=false;
        try { FileSystem::WriteFileAtomically(file,[&](std::ostream& output) {
            output<<"partial replacement";
            if(failStream) output.setstate(std::ios::badbit);
            else throw std::runtime_error("Deterministic writer failure");
        }); } catch(const std::runtime_error&) { rejected=true; }
        Check(rejected && Read(file)=="previous scene bytes","Failed write damaged previous destination");
        noOwnedTemps();
    }
    FileSystem::WriteFileAtomically(file,[](std::ostream& output) { output<<"complete replacement"; });
    Check(Read(file)=="complete replacement","Atomic replacement did not replace destination");
#ifdef HZ_PLATFORM_WINDOWS
    auto locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(locked!=INVALID_HANDLE_VALUE,"Cannot create deterministic replacement lock");
    bool lockRejected=false;
    try { FileSystem::WriteFileAtomically(file,[](std::ostream& output) { output<<"cannot replace locked file"; }); }
    catch(const std::runtime_error&) { lockRejected=true; }
    CloseHandle(locked);
    Check(lockRejected && Read(file)=="complete replacement","Locked replacement damaged previous file");
#else
    rlimit previous{}, limited{}; Check(getrlimit(RLIMIT_FSIZE,&previous)==0,"Cannot read process file-size limit");
    limited=previous; limited.rlim_cur=128;
    const auto previousSignal=std::signal(SIGXFSZ,SIG_IGN);
    Check(setrlimit(RLIMIT_FSIZE,&limited)==0,"Cannot set process-only deterministic write limit");
    bool writeRejected=false;
    try { FileSystem::WriteFileAtomically(file,[](std::ostream& output) { output<<std::string(4096,'x'); }); }
    catch(const std::runtime_error&) { writeRejected=true; }
    const int restored=setrlimit(RLIMIT_FSIZE,&previous); std::signal(SIGXFSZ,previousSignal);
    Check(restored==0 && writeRejected && Read(file)=="complete replacement","Kernel write failure damaged previous file");
#endif
    noOwnedTemps();
    const auto blocked=fixture.Path/"blocked.hazel";
    std::filesystem::create_directory(blocked); std::ofstream(blocked/"previous")<<"retained";
    bool rejected=false;
    try { FileSystem::WriteFileAtomically(blocked,[](std::ostream& output) { output<<"complete"; }); }
    catch(const std::runtime_error&) { rejected=true; }
    Check(rejected && Read(blocked/"previous")=="retained","Failed replacement damaged previous destination");
    noOwnedTemps();
    std::cout<<"PASS: sibling exclusive saves, successful replacement, partial/failed writes, replacement failure and owned-only cleanup\n";
}
static void Projects() {
    Fixture fixture; const auto file=fixture.Path/std::filesystem::u8path(u8"Projet-é-\U0001f680.hproj");
    Check(!Project::SaveActive(file),"Save without active project accepted");
    bool rejected=false; try { Project::GetAssetDirectory(); } catch(const std::logic_error&) { rejected=true; }
    Check(rejected,"Missing active project was not diagnosed");
    auto original=Project::New(); auto& config=original->GetConfig();
    config.Name=u8"Projet-é-\U0001f680";
    config.StartScene=std::filesystem::u8path(u8"Scenes/scène-\U0001f680.hazel");
    config.AssetDirectory=std::filesystem::u8path(u8"Assets-é");
    config.ScriptModulePath=std::filesystem::u8path(u8"Scripts/Binaries/Jeu-é.dll");
    const ProjectConfig expected=config;
    Check(Project::SaveActive(file),"Native UTF-8 project save failed");
    Check(Project::GetProjectDirectory()==fixture.Path,"Project directory not updated after save");
    Project::New(); auto loaded=Project::Load(file); Check(bool(loaded),"Native UTF-8 project load failed");
    auto& actual=loaded->GetConfig();
    Check(actual.Name==expected.Name && actual.StartScene==expected.StartScene &&
          actual.AssetDirectory==expected.AssetDirectory && actual.ScriptModulePath==expected.ScriptModulePath,
          "Project fields did not round trip");
    Check(Project::GetAssetFileSystemPath(actual.StartScene)==fixture.Path/expected.AssetDirectory/expected.StartScene,
          "Project-relative asset resolution failed");
    const auto bad=fixture.Path/"bad.hproj";
    { std::ofstream output(bad); output<<"Project: [unterminated"; }
    Check(!Project::Load(bad) && Project::GetActive()==loaded,"Invalid project replaced active project");
    { std::ofstream output(bad); output<<"Project: {Name: partial}"; }
    ProjectSerializer serializer(loaded);
    Check(!serializer.Deserialize(bad) && actual.Name==expected.Name && actual.StartScene==expected.StartScene,
          "Partial parse mutated live project config");
    Check(!Project::Load(fixture.Path/"missing.hproj"),"Missing project accepted");
    Check(!Project::SaveActive(fixture.Path/"missing-directory"/"out.hproj") &&
          Project::GetProjectDirectory()==fixture.Path,"Failed save changed active project directory");
}
struct Contacts : b2ContactListener {
    unsigned Began=0;
    void BeginContact(b2Contact*) override { ++Began; }
};
static void Physics() {
    for(int restart=0;restart<2;++restart) {
        Contacts contacts; b2World world({0,-9.8f}); world.SetContactListener(&contacts);
        b2BodyDef floorDefinition; floorDefinition.position={0,-.5f};
        auto floor=world.CreateBody(&floorDefinition); b2PolygonShape floorShape; floorShape.SetAsBox(10,.5f);
        floor->CreateFixture(&floorShape,0);
        b2BodyDef definition; definition.type=b2_dynamicBody; definition.position={-2,3}; definition.fixedRotation=true;
        auto box=world.CreateBody(&definition); b2PolygonShape boxShape; boxShape.SetAsBox(.5f,.5f);
        b2FixtureDef fixture; fixture.shape=&boxShape; fixture.density=2; fixture.friction=.7f;
        fixture.restitution=.1f; fixture.restitutionThreshold=.5f; auto boxFixture=box->CreateFixture(&fixture);
        definition.position={2,6}; definition.fixedRotation=false;
        auto circle=world.CreateBody(&definition); b2CircleShape circleShape; circleShape.m_radius=.5f;
        fixture.shape=&circleShape; fixture.density=1; fixture.restitution=.4f; circle->CreateFixture(&fixture);
        b2BodyDef moving; moving.type=b2_kinematicBody; moving.position={20,2}; moving.linearVelocity={1,0};
        auto kinematic=world.CreateBody(&moving);
        for(int step=0;step<360;++step) world.Step(1.0f/60,6,2);
        Check(contacts.Began>=2 && std::abs(box->GetPosition().y-.5f)<.06f &&
              std::abs(circle->GetPosition().y-.5f)<.08f,"Rigid body/collider contact or gravity integration failed");
        Check(box->IsFixedRotation() && box->GetAngle()==0 && std::abs(box->GetMass()-2)<.001f &&
              boxFixture->GetFriction()==.7f && boxFixture->GetRestitutionThreshold()==.5f,
              "Target fixed rotation/material/density APIs changed");
        Check(std::abs(kinematic->GetPosition().x-26)<.01f && kinematic->GetPosition().y==2,
              "Kinematic body affected by gravity or lost velocity");
        world.DestroyBody(box); world.DestroyBody(circle); Check(world.GetBodyCount()==2,"Body destruction failed");
    }
}
int main() {
    try { Log::Init(); SafeSaves(); Projects(); Physics();
        std::cout<<"PASS: native UTF-8 project/config round trips, asset resolution, transactional parse/save failures; exact target Box2D gravity, box/circle contacts, body types/materials/destruction and repeated worlds\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
