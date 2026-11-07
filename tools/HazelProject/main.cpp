#include "DirectRenderingReference.h"
#include "Hazel/Assets/NativeStarter.h"
#include "Hazel/Assets/SpriteSheet.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Core/OperationProgress.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectCreation.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Import.h"
#include "Environment/EnvironmentBake.h"
#include "EnvironmentRenderingReference.h"
#include "Reference/IBLQualityReference.h"
#include "Reference/RenderingShowcase.h"
#include "RenderingReference.h"
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif
namespace {
constexpr const char* Usage =
    "Usage: HazelProject COMMAND [arguments]\n"
    "  create --name NAME --identifier ID --destination FOLDER --templates FOLDER\n"
    "  create-rendering-reference FOLDER TEMPLATES  (maintainer Core_v1 recipe)\n"
    "  import-glb PROJECT.hproj SOURCE.glb ASSET_FOLDER [--scene INDEX] [--cancel-file ABSOLUTE]\n"
    "  reimport-glb PROJECT.hproj ASSET_FOLDER/Import.himport [--cancel-file ABSOLUTE]\n"
    "  prepare-environment PROJECT.hproj ASSET.henv [--cancel-file ABSOLUTE]\n"
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
        if(args.size()==2 && args[1]=="--environment-contract"){std::cout<<"HAZEL_ENVIRONMENT_PREPARE_CONTRACT=1\n";return 0;}
        if(args.size()==2 && args[1]=="--import-contract"){std::cout<<"HAZEL_ASSET_IMPORT_CONTRACT=1\n";return 0;}
        if ((args.size()==4 || args.size()==6) && args[1]=="prepare-environment") {
            std::filesystem::path cancel;
            if(args.size()==6){if(args[4]!="--cancel-file")throw std::runtime_error(Usage);cancel=std::filesystem::u8path(args[5]);if(!cancel.is_absolute())throw std::runtime_error("Cancellation marker requires an absolute owned path");}
            Hazel::Log::Init();auto project=Hazel::Project::LoadCandidate(std::filesystem::u8path(args[2]));if(!project)throw std::runtime_error("Cannot read project");
            // Throttle progress at the source: useful native console feedback,
            // bounded even for large face/sample counts.
            std::string previous;size_t last=SIZE_MAX;
            Hazel::OperationProgress::Scope progress([&](const std::string &label,size_t done,size_t total){auto bucket=total?done*20/total:0;if(label!=previous||bucket!=last){std::cout<<"[prepare] "<<label<<' '<<done<<'/'<<total<<std::endl;previous=label;last=bucket;}});
            Hazel::EnvironmentBake::PrepareImage(project->GetAssetRoot(),std::filesystem::u8path(args[3]),[&]{return !cancel.empty()&&std::filesystem::exists(cancel);});return 0;
        }
        if(args.size()>=4 && (args[1]=="import-glb"||args[1]=="reimport-glb")) {
            const bool reimport=args[1]=="reimport-glb";const size_t required=reimport?4:5;
            if(args.size()<required||(args.size()-required)%2)throw std::runtime_error(Usage);
            int scene=-1;std::filesystem::path cancelFile;
            std::set<std::string> options;
            for(size_t i=required;i<args.size();i+=2){if(!options.insert(args[i]).second)throw std::runtime_error("Duplicate import option");if(args[i]=="--scene"&&!reimport){size_t used=0;scene=std::stoi(args[i+1],&used);if(used!=args[i+1].size()||scene< -1||scene>=10000)throw std::runtime_error("Scene index must be -1 (default) or 0..9999");}else if(args[i]=="--cancel-file"){cancelFile=std::filesystem::u8path(args[i+1]);if(!cancelFile.is_absolute())throw std::runtime_error("Cancellation marker requires an absolute owned path");}else throw std::runtime_error("Unknown import option: "+args[i]);}
            Hazel::Log::Init();Hazel::Resources::Configure(Hazel::Resources::Defaults("HazelProject"));auto project=Hazel::Project::LoadCandidate(std::filesystem::u8path(args[2]));if(!project)throw std::runtime_error("Cannot read project descriptor");
            Hazel::OperationProgress::Scope progress([](const std::string& label,size_t done,size_t total){std::cout<<"[import] "<<label;if(total)std::cout<<" "<<done<<"/"<<total;std::cout<<std::endl;});
            Hazel::Import::Cancel cancelled=[&]{return !cancelFile.empty()&&std::filesystem::exists(cancelFile);};Hazel::Import::Result result;
            if(reimport)result=Hazel::Import::ReimportGLB(project->GetAssetRoot(),std::filesystem::u8path(args[3]),cancelled);
            else result=Hazel::Import::ImportGLB({project->GetAssetRoot(),std::filesystem::absolute(std::filesystem::u8path(args[3])),std::filesystem::u8path(args[4]),scene,cancelled});
            std::cout << "IMPORTED " << result.Manifest.generic_u8string() << "\nPREFAB "
                      << result.Prefab.generic_u8string() << "\nGeneration "
                      << Hazel::AssetKey(result.Generation)
                      << (result.Unchanged ? " (unchanged)" : " (accepted)")
                      << "\nNative authoring ready; lit meshes require scene-referred presentation "
                         "and direct or prepared image lighting\n";
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
        if (args.size() == 3 && args[1] == "add-direct-surfaces")
        {
            Hazel::Log::Init();
            AddDirectSurfaceReference(std::filesystem::u8path(args[2]));
            return 0;
        }
        if (args.size() == 4 && args[1] == "add-direct-reference")
        {
            Hazel::Log::Init();
            AddDirectRenderingReference(std::filesystem::u8path(args[2]),
                                        std::filesystem::u8path(args[3]));
            return 0;
        }
        if(args.size()==3 && args[1]=="finish-environment-reference-labels"){Hazel::Log::Init();FinishEnvironmentReferenceLabels(std::filesystem::u8path(args[2]));return 0;}
        if(args.size()==3 && args[1]=="generate-environment-starter"){Hazel::Log::Init();GenerateEnvironmentStarter(std::filesystem::u8path(args[2]));return 0;}
        if(args.size()==5 && args[1]=="create-rendering-showcase") {Hazel::Log::Init();RenderingShowcase::Create(std::filesystem::u8path(args[2]),std::filesystem::u8path(args[3]),std::filesystem::u8path(args[4]));return 0;}
        if(args.size()==3 && args[1]=="create-ibl-quality-reference") {Hazel::Log::Init();IBLQualityReference::Create(std::filesystem::u8path(args[2]));return 0;}
        if(args.size()==3 && args[1]=="freeze-ibl-quality-reference") {Hazel::Log::Init();IBLQualityReference::Freeze(std::filesystem::u8path(args[2]));return 0;}
        if(args.size()==4 && args[1]=="add-environment-reference"){Hazel::Log::Init();AddEnvironmentRenderingReference(std::filesystem::u8path(args[2]),std::filesystem::u8path(args[3]));return 0;}
        if (args.size() == 3 && args[1] == "generate-native-starter")
        {
            Hazel::Log::Init();
            Hazel::NativeStarter::Generate(std::filesystem::u8path(args[2]));
            return 0;
        }
        if(args.size()==4 && args[1]=="create-rendering-reference") {
            Hazel::Log::Init();const auto templates=std::filesystem::u8path(args[3]);
            Hazel::Resources::Configure(Hazel::Resources::Defaults("HazelProject",{templates.parent_path(),{}, {}}));
            CreateRenderingReference(std::filesystem::u8path(args[2]),templates);return 0;
        }
        if (args.size() == 3 && args[1] == "validate") {
            Hazel::Log::Init();
            Hazel::DocumentLoadReport report;
            if (!Hazel::Project::LoadCandidate(std::filesystem::u8path(args[2]), &report))
                throw std::runtime_error(report.Error);
            std::cout << "Validated native project schema\n";
            return 0;
        }
        bool direct = false;
        if (args.size() == 12 && args[10] == "--preset" && args[11] == "direct-pbr")
        {
            direct = true;
            args.resize(10);
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
        request.DirectPBR = direct;
        const auto result = Hazel::ProjectCreation::Create(request);
        std::cout << "Created project: " << result.Descriptor.generic_u8string()
                  << "\nEditing ready; scripts intentionally not compiled\n";
        return 0;
    } catch (const std::filesystem::filesystem_error &e) {
        // filesystem_error::what() uses a native code page on MSVC. Preserve
        // paths as UTF-8 and the actual error code instead of emitting ANSI.
        std::cerr << "HazelProject: native filesystem operation failed (" << e.code().category().name()
                  << " error " << e.code().value() << "): " << e.path1().generic_u8string();
        if (!e.path2().empty()) std::cerr << " -> " << e.path2().generic_u8string();
        std::cerr << '\n';
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "HazelProject: " << e.what() << '\n';
        return 1;
    }
}
