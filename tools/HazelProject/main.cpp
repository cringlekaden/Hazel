#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectCreation.h"
#include "Hazel/Assets/SpriteSheet.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif
namespace {
constexpr const char* Usage =
    "Usage: HazelProject COMMAND [arguments]\n"
    "  create --name NAME --identifier ID --destination FOLDER --templates FOLDER\n"
    "  validate PROJECT.hproj\n"
    "  slice-sheet ASSETS_ROOT SHEET IMAGE TILE_SIZE\n"
    "  add-clip ASSETS_ROOT SHEET NAME DURATION ORDINALS_CSV\n"
    "  set-region ASSETS_ROOT SHEET REGION_ID X Y WIDTH HEIGHT NAME\n"
    "  measure-scene PROJECT.hproj SCENE_ASSET  (maintainer CPU diagnostic)\n"
    "Sheet/image/scene arguments are project asset-relative paths.\n";
}
int main(int argc, char **argv) {
    try {
        std::vector<std::string> args;
#ifdef HZ_PLATFORM_WINDOWS
        args = Hazel::WindowsCommandLineUTF8();
#else
        for (int i = 0; i < argc; ++i)
            args.emplace_back(argv[i]);
#endif
        if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
            std::cout << Usage;
            return 0;
        }
        if (args.size() == 2 && args[1] == "--contract") {
            std::cout << "HAZEL_AUTHORING_CONTRACT=1\n";
            return 0;
        }
        if(args.size()==4 && args[1]=="measure-scene") {
            Hazel::Log::Init();const auto project=Hazel::Project::LoadCandidate(std::filesystem::u8path(args[2]));
            if(!project)throw std::runtime_error("Cannot read project");
            auto scene=Hazel::CreateRef<Hazel::Scene>();Hazel::SceneSerializer serializer(scene,project->GetAssetRoot(),true);
            auto measured=[&](const char* operation,auto work){auto start=std::chrono::steady_clock::now();work();std::cout<<operation<<": "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" s\n";};
            measured("Parse/schema/components/hierarchy",[&]{if(!serializer.DeserializeText(Hazel::FileDocument::Read(Hazel::Project::ResolveOwnedAsset(project->GetAssetRoot(),std::filesystem::u8path(args[3]))),false,Hazel::ResourceLoading::MetadataOnly))throw std::runtime_error(serializer.Report().Error);});
            measured("10 YAML dirty snapshots",[&]{for(int i=0;i<10;i++)serializer.SerializeAuthoredSnapshot();});
            measured("100 exact authored comparisons",[&]{for(int i=0;i<100;i++)serializer.CaptureAuthoredState();});
            return 0;
        }
        // Same grid/validation/persistence services as Hazelnut's Sprite Sheet panel.
        // Creates a new sheet only; established region identities are never regenerated.
        if(args.size()==10 && args[1]=="set-region") {
            Hazel::Log::Init();
            const auto root=std::filesystem::u8path(args[2]),reference=std::filesystem::u8path(args[3]);
            auto sheet=Hazel::ReadSpriteSheet(Hazel::Project::ResolveOwnedAsset(root,reference));
            Hazel::SpriteRegion* region=nullptr;const auto id=Hazel::ParseSpriteID(args[4]);
            for(auto& candidate:sheet.Regions)if(candidate.ID==id)region=&candidate;
            if(!region)throw std::runtime_error("Unknown region identity");
            region->Rect={static_cast<uint32_t>(std::stoul(args[5])),static_cast<uint32_t>(std::stoul(args[6])),static_cast<uint32_t>(std::stoul(args[7])),static_cast<uint32_t>(std::stoul(args[8]))};
            region->Name=args[9];Hazel::SaveSpriteSheet(root,reference,sheet);
            std::cout<<"Updated native editable region: "<<region->Name<<"\n";return 0;
        }
        if(args.size()==7 && args[1]=="add-clip") {
            Hazel::Log::Init();
            const auto root=std::filesystem::u8path(args[2]),reference=std::filesystem::u8path(args[3]);
            auto sheet=Hazel::ReadSpriteSheet(Hazel::Project::ResolveOwnedAsset(root,reference));
            for(const auto& c:sheet.Clips)if(c.Name==args[4])throw std::runtime_error("Clip already exists; edit it in Hazelnut");
            Hazel::SpriteClip clip;clip.ID=sheet.NewID(true);clip.Name=args[4];
            const double duration=std::stod(args[5]);
            std::stringstream frames(args[6]);std::string index;
            while(std::getline(frames,index,','))clip.Frames.push_back({sheet.Regions.at(std::stoul(index)).ID,duration});
            sheet.Clips.push_back(clip);Hazel::SaveSpriteSheet(root,reference,sheet);
            std::cout<<"Added native editable clip: "<<clip.Name<<" ("<<clip.Frames.size()<<" frames)\n";return 0;
        }
        if(args.size()==6 && args[1]=="slice-sheet") {
            Hazel::Log::Init();
            const auto root=std::filesystem::u8path(args[2]);
            const auto reference=std::filesystem::u8path(args[3]);
            Hazel::SpriteSheetDefinition sheet;sheet.Texture=std::filesystem::u8path(args[4]);
            const auto image=Hazel::Texture2D::ReadImage(Hazel::Project::ResolveOwnedAsset(root,sheet.Texture),Hazel::ImageFormat::RGBA8);
            sheet.Sampling.Width=image.Width;sheet.Sampling.Height=image.Height;
            Hazel::GridSliceOptions grid;grid.CellWidth=grid.CellHeight=static_cast<uint32_t>(std::stoul(args[5]));
            grid.Prefix="tile";
            Hazel::AddGridRegions(sheet,Hazel::GenerateGridPreview(sheet,grid));
            Hazel::SaveSpriteSheet(root,reference,sheet,Hazel::WriteMode::CreateNew);
            std::cout<<"Created native editable sheet: "<<reference.generic_u8string()<<" ("<<sheet.Regions.size()<<" regions)\n";return 0;
        }
        if (args.size() == 3 && args[1] == "validate") {
            Hazel::Log::Init();
            Hazel::DocumentLoadReport report;
            if (!Hazel::Project::LoadCandidate(std::filesystem::u8path(args[2]), &report))
                throw std::runtime_error(report.Error);
            std::cout << "Validated native project schema\n";
            return 0;
        }
        if (args.size() != 10 || args[1] != "create")
            throw std::runtime_error(Usage);
        std::map<std::string, std::string> values;
        for (size_t i = 2; i < args.size(); i += 2)
            if (!values.emplace(args[i], args[i + 1]).second)
                throw std::runtime_error("Duplicate generator argument");
        for (const auto *key : {"--name", "--identifier", "--destination", "--templates"})
            if (!values.count(key))
                throw std::runtime_error("Missing generator argument");
        Hazel::Log::Init();
        Hazel::Resources::Configure(Hazel::Resources::Defaults("HazelProject"));
        Hazel::ProjectCreateRequest request{values["--name"], values["--identifier"],
                                            std::filesystem::u8path(values["--destination"]),
                                            std::filesystem::u8path(values["--templates"])};
        const auto result = Hazel::ProjectCreation::Create(request);
        std::cout << "Created project: " << result.Descriptor.generic_u8string()
                  << "\nEditing ready; scripts intentionally not compiled\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "HazelProject: " << e.what() << '\n';
        return 1;
    }
}
