// Actual target project serialization and pinned Box2D CPU prerequisites.
#include "Hazel/Core/Log.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectSerializer.h"
#include <box2d/box2d.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace Hazel;
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
    std::filesystem::path Path=std::filesystem::temp_directory_path()/
        std::filesystem::u8path("hazel-project-é-"+std::to_string(std::random_device{}()));
    Fixture() { Check(std::filesystem::create_directory(Path),"Project fixture isolation failed"); }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(Path,error); }
};
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
    try { Log::Init(); Projects(); Physics();
        std::cout<<"PASS: native UTF-8 project/config round trips, asset resolution, transactional parse/save failures; exact target Box2D gravity, box/circle contacts, body types/materials/destruction and repeated worlds\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
