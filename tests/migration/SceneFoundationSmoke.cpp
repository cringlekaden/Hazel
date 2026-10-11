// CPU checks for exact target ECS/YAML dependencies and portable camera state.
#include "Hazel/Scene/SceneCamera.h"
#include "Hazel/Project/RuntimeStorage.h"
#include "Hazel/Core/FileSystem.h"
#include <fstream>
#include "Hazel/Core/Log.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Assets/SpriteSheetDocument.h"
#include <fstream>
#include <algorithm>
#include <entt.hpp>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
#include "EditorDocumentChecks.h"
#include "EditorStateChecks.h"
#include "ProjectCreationChecks.h"
#include "RendererPolicyChecks.h"
#include "HierarchyChecks.h"
static void Finite(const glm::mat4& projection)
{
    for (int column=0; column<4; ++column)
        for (int row=0; row<4; ++row)
            Check(std::isfinite(projection[column][row]), "Camera projection is non-finite");
}
static void RecoveryContracts()
{
    using namespace Hazel;
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel recovery space-é-"+std::to_string(uint64_t(UUID())));
    std::filesystem::create_directories(root);
    const auto file=root/std::filesystem::u8path(u8"source é.hazel");
    const std::string original=u8"Scene: Missing é\nEntities:\n  - Entity: 72\n    TagComponent: {Tag: Unresolved}\n    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TexturePath: Textures/missing é.png\n      TilingFactor: 1\n";
    FileSystem::WriteNewFile(file,original);
    FileDocument owner;owner.Open(file);
    auto scene=CreateRef<Scene>();
    SceneSerializer loader(scene,root,true,CreateRef<ProjectAssets>(root));
    Check(loader.DeserializeText(owner.Original()),"Missing texture was not editable");
    Check(loader.Report().Migration && loader.Report().State==DocumentLoadState::EditableWithProblems,
          "Missing resource/default changes were not identified");
    auto report=loader.Report();report.Saved();
    Check(!report.Migration && report.State==DocumentLoadState::EditableWithProblems && report.Problems.size()==1,
          "Saving migration incorrectly resolved a missing resource or retained obsolete encoding warnings");
    const auto saved=loader.SerializeText();
    Check(saved.find(u8"Textures/missing é.png")!=std::string::npos && saved.find("SceneVersion: 2")!=std::string::npos,
          "Unresolved texture reference or scene version lost");
    owner.PreserveOriginal();owner.Save(saved,root/"recovery");
    Check(FileDocument::Read(owner.Backup())==original,"First recovery Save lost original bytes");
    Check(loader.DeserializeText(saved),"Unresolved saved scene failed reopen");
    const auto draft=loader.SerializeText();
    for(const auto& text:{std::string("SceneVersion: 99\nScene: Future\nEntities: []\n"),
                        std::string("Scene: Unknown\nAlien: 4\nEntities: []\n"),
                        std::string("Scene: Duplicate\nScene: Second\n"),
                        std::string("Scene: X\nEntities: [{Entity: 72, TransformComponent: {Translation: [0, 0, 0], Rotation: [0, 0, 0], Scale: [1, 1, 1], Alien: true}}]\n")}) {
        Check(!loader.DeserializeText(text) && loader.SerializeText()==draft,"Rejected schema changed valid scene");
    }
    FileSystem::WriteFileAtomically(file,[](auto& out){out<<"External content";});
    bool conflict=false;try{owner.Save(saved,root/"recovery");}catch(const std::exception&){conflict=true;}
    Check(conflict && FileDocument::Read(file)=="External content" && loader.SerializeText()==draft,
          "Save conflict overwrote disk or draft");
    std::filesystem::remove(file);conflict=false;
    try{owner.Save(saved,root/"recovery");}catch(const std::exception&){conflict=true;}
    Check(conflict && !std::filesystem::exists(file),"Missing accepted file was silently recreated");
    auto config=CreateRef<Project>();config->GetConfig().Name="Retained";
    const auto descriptor=root/"future.hproj";
    FileSystem::WriteNewFile(descriptor,"Project: {Version: 99}\n");
    ProjectSerializer projectLoader(config);
    Check(!projectLoader.Deserialize(descriptor) && config->GetConfig().Name=="Retained" &&
          projectLoader.Report().Error.find("Unsupported Project Version")!=std::string::npos,
          "Future project changed accepted configuration or lacked version diagnostic");
    // An exclusive prefab write must preserve an existing destination.
    auto entity=scene->GetEntityByUUID(72);
    Prefab::Save(root,"retained.hprefab",scene,entity,true,WriteMode::CreateNew);
    const auto prefab=FileDocument::Read(root/"retained.hprefab");
    const auto beforePrefab=loader.SerializeText();
    Check(!loader.DeserializeText(prefab) && loader.SerializeText()==beforePrefab,"Scene reader silently stripped prefab metadata");
    auto prefabCandidate=CreateRef<Scene>();
    Check(SceneSerializer(prefabCandidate,root,true).DeserializeText(prefab,true),"Explicit prefab reader rejected known prefab content");
    bool exclusive=false;try{Prefab::Save(root,"retained.hprefab",scene,entity,true,WriteMode::CreateNew);}catch(const std::exception&){exclusive=true;}
    Check(exclusive && FileDocument::Read(root/"retained.hprefab")==prefab,"Prefab creation replaced another file");
    unsigned char image[18+16]{};image[2]=2;image[12]=image[14]=2;image[16]=32;image[17]=0x20;
    {std::ofstream out(root/"texture.tga",std::ios::binary);out.write(reinterpret_cast<const char*>(image),sizeof(image));}
    // Audit a real whole-texture source through the same UTF-8 file service used
    // by scene Open; narrow YAML::LoadFile paths fail on Windows in this root.
    FileSystem::WriteNewFile(root/"textured.hazel", "Scene: Closure\nEntities:\n  - Entity: 91\n    TagComponent: {Tag: Textured}\n    SpriteRendererComponent: {Color: [1, 1, 1, 1], TexturePath: texture.tga, TilingFactor: 1}\n");
    const auto closure=AuditSpriteAssets(root,{"textured.hazel"});
    Check(std::find(closure.begin(),closure.end(),std::filesystem::path("texture.tga"))!=closure.end(),"Unicode scene closure omitted whole-texture dependency");
    const std::string sheetText="SpriteSheet: {Version: 1, Texture: texture.tga, TextureSize: [2, 2], Filter: Nearest, Regions: [{ID: '0000000000000001', Name: First, Rect: [0, 0, 2, 2]}]}\n";
    FileSystem::WriteNewFile(root/"legacy.hsprites",sheetText);
    auto retainedAssets=CreateRef<ProjectAssets>(root);
    try{retainedAssets->Sheet("repaired.hsprites");}catch(const std::exception&){}
    FileSystem::WriteNewFile(root/"repaired.hsprites",sheetText);
    auto candidateAssets=CreateRef<ProjectAssets>(root);
    Check(candidateAssets->Sheet("repaired.hsprites")->Region(1).Name=="First","Fresh Open candidate reused a stale missing-sheet failure");
    bool oldFailure=false;try{retainedAssets->Sheet("repaired.hsprites");}catch(const std::exception&){oldFailure=true;}
    Check(oldFailure,"Candidate validation altered the retained asset cache");
    retainedAssets->Refresh();
    Check(retainedAssets->Sheet("repaired.hsprites")->Region(1).Name=="First","Accepted refresh did not resolve an externally repaired sheet");
    SpriteSheetDocument sheet(CreateRef<ProjectAssets>(root));sheet.Open("legacy.hsprites");
    Check(sheet.File().NeedsBackup(),"Known sheet defaults/Filter migration was hidden");
    sheet.Draft().Regions.front().Name="Edited";sheet.Changed();
    bool failedSave=false;try{sheet.Save(root/"texture.tga");}catch(const std::exception&){failedSave=true;}
    Check(failedSave && sheet.Dirty() && FileDocument::Read(root/"legacy.hsprites")==sheetText,"Failed original backup overwrote sheet or lost draft");
    sheet.Save(root/"recovery");
    Check(!sheet.Dirty() && FileDocument::Read(sheet.File().Backup())==sheetText,"Sheet migration lost original or dirty semantics");
    for(unsigned i=0;i<22;++i) {
        const auto item=root/("bounded-"+std::to_string(i));FileSystem::WriteNewFile(item,"Original");
        FileDocument original;original.Open(item,true);original.Save("Saved",root/"recovery");
    }
    unsigned retained=0;for(const auto& item:std::filesystem::directory_iterator(root/"recovery"))if(item.path().extension()==".original")++retained;
    Check(retained==20,"Original recovery history was not bounded");
    std::filesystem::remove_all(root);
    std::cout<<"PASS: CPU production recovery load/round-trip, unknown/future/duplicate schema rejection, original backups, missing/external save conflicts and exclusive prefab publication\n";
}
static void StorageChecks() {
    using namespace Hazel;
    auto root=std::filesystem::temp_directory_path()/("hazel-save-"+std::to_string(uint64_t(UUID())));
    struct Cleanup{std::filesystem::path P;~Cleanup(){std::error_code ec;std::filesystem::remove_all(P,ec);}} cleanup{root};
    RuntimeStorage player;player.Configure(root,"Keeper",true);
    Check(player.Read("progress").empty(),"Missing save isn't empty");
    player.Write("progress","v1:harbor é");player.Write("progress","v1:relay é");
    RuntimeStorage reopened;reopened.Configure(root,"Keeper",true);
    Check(reopened.Read("progress")=="v1:relay é","Atomic replace/UTF-8 reopen failed");
    RuntimeStorage editor;editor.Configure(root,"Keeper",false);
    Check(editor.Read("progress").empty(),"Editor loaded player save");editor.Write("progress","editor-only");
    Check(editor.Read("progress")=="editor-only" && reopened.Read("progress")=="v1:relay é","Editor changed player save");
    editor.Clear();editor.Configure(root,"Keeper",false);Check(editor.Read("progress").empty(),"Stop retained editor progress");
    RuntimeStorage other;other.Configure(root,"Other",true);Check(other.Read("progress").empty(),"Projects share progress");
    for(auto slot:{"../progress","/absolute","C:drive","..","CON/path","NUL","com1","trail."}) {
        bool rejected=false;try{reopened.Write(slot,"data");}catch(const std::exception&){rejected=true;}Check(rejected,"Unsafe slot accepted");
    }
    bool rejected=false;try{reopened.Write("progress",std::string(RuntimeStorage::MaximumPayload+1,'x'));}catch(const std::exception&){rejected=true;}
    Check(rejected && reopened.Read("progress")=="v1:relay é","Oversize write damaged progress");
    auto file=root/"Keeper/slot-progress.save";
    FileSystem::WriteFileAtomically(file,[](auto& out){out<<"Version: 99\nOwner: Keeper\nSlot: progress\nPayload: newer\n";});
    rejected=false;try{reopened.Read("progress");}catch(const std::exception&){rejected=true;}Check(rejected,"Future save accepted");
    std::ifstream in(file);std::string bytes((std::istreambuf_iterator<char>(in)),{});Check(bytes.find("99")!=std::string::npos,"Future file erased");
    FileSystem::WriteFileAtomically(file,[](auto& out){out<<"[malformed";});
    rejected=false;try{reopened.Read("progress");}catch(const std::exception&){rejected=true;}Check(rejected,"Corrupt save accepted");
    std::cout<<"PASS: project save isolation, editor overlay, UTF-8 atomic replace, traversal/size/corrupt/future rejection\n";
}

int main()
{
    try {
        StorageChecks();
        Hazel::Log::Init();
        Hazel::EditorDocumentChecks();
        Hazel::EditorStateChecks();
        Hazel::ProjectCreationChecks();Hazel::RendererPolicyChecks(); Hazel::HierarchyChecks(); Hazel::SubtreePrefabChecks();
        RecoveryContracts();
        Hazel::SceneCamera camera;
        Finite(camera.GetProjection());
        camera.SetViewportSize(1600,900);
        const auto projection=camera.GetProjection();
        camera.SetViewportSize(0,0);
        Check(camera.GetProjection()==projection, "Minimized viewport changed valid projection");
        camera.SetPerspective(glm::radians(60.0f),0.1f,500.0f);
        Finite(camera.GetProjection());
        Check(camera.GetProjectionType()==Hazel::SceneCamera::ProjectionType::Perspective,
              "Perspective camera state lost");
        camera.SetOrthographic(20,-2,3);
        Finite(camera.GetProjection());
        Check(camera.GetProjectionType()==Hazel::SceneCamera::ProjectionType::Orthographic,
              "Orthographic camera state lost");

        struct ID { std::uint64_t Value; };
        struct Tag { std::string Name; };
        struct Position { float X,Y; };
        entt::registry registry;
        const auto first=registry.create();
        const auto second=registry.create();
        registry.emplace<ID>(first,ID{static_cast<std::uint64_t>(Hazel::UUID())});
        registry.emplace<Tag>(first,Tag{u8"camera-é-\U0001f680"});
        registry.emplace<Position>(first,Position{2,3});
        registry.emplace<Tag>(second,Tag{"other"});
        std::size_t count=0;
        for (const auto entity:registry.view<ID,Tag,Position>()) {
            Check(entity==first, "ECS intersection selected wrong entity");
            ++count;
        }
        Check(count==1, "ECS intersection size differs");
        YAML::Emitter output;
        output << YAML::BeginMap << YAML::Key << "Entity" << YAML::Value << registry.get<ID>(first).Value
               << YAML::Key << "Tag" << YAML::Value << registry.get<Tag>(first).Name
               << YAML::Key << "Position" << YAML::Value << YAML::Flow << YAML::BeginSeq
               << registry.get<Position>(first).X << registry.get<Position>(first).Y << YAML::EndSeq
               << YAML::Key << "ProjectionType" << YAML::Value << static_cast<int>(camera.GetProjectionType())
               << YAML::Key << "Size" << YAML::Value << camera.GetOrthographicSize() << YAML::EndMap;
        Check(output.good(), "YAML emit failed");
        const auto saved=YAML::Load(output.c_str());
        Check(saved["Entity"].as<std::uint64_t>()==registry.get<ID>(first).Value, "YAML UUID round trip failed");
        Check(saved["Tag"].as<std::string>()==registry.get<Tag>(first).Name, "YAML UTF-8 tag lost");
        Check(saved["Position"].size()==2 && saved["Position"][1].as<float>()==3, "YAML component values lost");
        Check(saved["Size"].as<float>()==20 && saved["ProjectionType"].as<int>()==1, "YAML camera values lost");
        registry.destroy(first);
        Check(!registry.valid(first) && registry.valid(second), "ECS destruction damaged another entity");
        const auto replacement=registry.create();
        Check(replacement!=first && !registry.valid(first), "ECS recycled stale handle without generation");
        bool rejected=false;
        try { YAML::Load("Entities: [unterminated"); } catch (const YAML::Exception&) { rejected=true; }
        Check(rejected, "Invalid YAML accepted");
        std::cout << "PASS: finite default/perspective/orthographic/minimized cameras, exact target ECS views/generations/lifetimes and YAML UUID/UTF-8/component/camera round trips\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
