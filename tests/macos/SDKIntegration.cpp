// Editor SDK/tool services exercised with private preferences and disposable content.
#include "Authoring/HazelSDK.h"
#include "Authoring/ProjectTools.h"
#include "Authoring/EditorPreferences.h"
#include "Hazel/Core/BuildIdentity.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Utils/Process.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
using namespace Hazel;
namespace fs=std::filesystem;
static void Check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static void Write(const fs::path& p,const std::string& text="fixture") {fs::create_directories(p.parent_path());FileSystem::WriteFileAtomically(p,[&](auto& out){out<<text;});}
static std::string Read(const fs::path& p){std::ifstream in(p);return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char** argv) try {
    Log::Init();Check(argc==4,"Usage: SDKIntegration CHECKOUT PRIVATE_ROOT initial|restart");
    const fs::path checkout=fs::canonical(argv[1]),working=fs::absolute(fs::u8path(argv[2]));
    fs::create_directories(working);
    const auto identity=BuildIdentity::Current();const auto config=identity.ToolConfiguration();
    Resources::Configure({checkout/"Hazel/Resources",checkout/"build/dependencies/mono/macos-arm64/sdk",working/"profile"});
    std::string diagnostic;
    if(std::string(argv[3])=="restart") {
        auto preferences=EditorPreferences::Load(diagnostic);
        Check(diagnostic.empty()&&preferences.SDK==checkout.generic_u8string(),"Selected SDK preference did not persist across process restart");
        Check(bool(HazelSDK::Discover(preferences.SDK)),"Persisted SDK did not validate");
        std::cout<<"PASS: private preferences and selected SDK survive process restart\n";return 0;
    }
    Check(bool(HazelSDK::Discover({},identity.NativeTool(checkout,config,"Hazelnut"))),"Actual Xcode product layout not discovered");
    const auto fake=working/fs::u8path(u8"SDK space é");
    for(const char* p:{"premake5.lua","scripts/hazel.py","scripts/internal/authoring.py","scripts/internal/packaging.py","scripts/internal/child_tools.py","Hazel/Resources/Templates/project.lua","Hazel/Resources/Templates/Entity.cs"})Write(fake/p);
    Write(fake/"scripts/internal/toolchain.json",Read(checkout/"scripts/internal/toolchain.json"));
    Write(fake/"Hazel/Resources/Templates/contract.json",Read(checkout/"Hazel/Resources/Templates/contract.json"));
    Check(HazelSDK::Validate(fake).State==SDKState::PreparationRequired,"Compatible unprepared checkout misclassified");
    const auto products=identity.Products(fake,config);
    for(const auto& p:{identity.Premake(fake),identity.NativeTool(fake,config,"HazelProject"),products/"Hazel-ScriptCore/Hazel-ScriptCore.dll",products/"PackageAudit/PackageAudit.exe"})Write(p);
    const auto executable=identity.NativeTool(fake,config,"Hazelnut");Write(executable);
    Check(bool(HazelSDK::Discover({},executable)),"Unicode development layout failed");
    fs::remove(identity.NativeTool(fake,config,"HazelProject"));
    Check(HazelSDK::Validate(fake).State==SDKState::PreparationRequired,"Missing native tool accepted");
    Write(identity.NativeTool(fake,config,"HazelProject"));
    Check(HazelSDK::Validate(fake,"test",config=="Debug"?"Release":"Debug").State==SDKState::PreparationRequired,"Wrong product configuration accepted");
    Check(HazelSDK::Discover(fake/"invalid",executable).State==SDKState::Missing,"Explicit invalid SDK silently fell back");
    Check(HazelSDK::Discover("relative",executable).State==SDKState::Incompatible,"Relative explicit SDK used cwd");
    Check(HazelSDK::Discover({},working/"unrelated/Hazelnut").State==SDKState::NotConfigured,"Unbounded SDK search");
    auto metadata=YAML::Load(Read(fake/"scripts/internal/toolchain.json"));metadata["targets"][identity.System]["architecture"]="wrong";
    YAML::Emitter out;out<<metadata;Write(fake/"scripts/internal/toolchain.json",out.c_str());
    Check(HazelSDK::Validate(fake).State==SDKState::Incompatible,"Foreign target metadata accepted");
    auto oldMetadata=YAML::Load(Read(checkout/"scripts/internal/toolchain.json"));oldMetadata.remove("targets");
    YAML::Emitter legacy;legacy<<oldMetadata;Write(fake/"scripts/internal/toolchain.json",legacy.c_str());
    Check(HazelSDK::Validate(fake).Diagnostic.find("lacks the shared target")!=std::string::npos,"Missing target metadata lacks an actionable diagnostic");
    for(const auto& item:std::vector<std::pair<BuildIdentity,std::string>>{{{"windows","x86_64","Debug",".exe",true},"Debug-windows-x86_64/HazelProject/HazelProject.exe"},{{"linux","x86_64","Release","",true},"Release-linux-x86_64/HazelProject/HazelProject"},{{"macos","arm64","Debug","",false},"Debug-macos-arm64/HazelProject/HazelProject"}}) {
        Check(item.first.NativeTool(fake,item.first.Configuration,"HazelProject")==fake/"bin"/item.second,"Native product contract changed");
    }
    Check(!Toolchain::DiscoverPython(working/"missing-python",checkout),"Invalid explicit Python fell back");
    auto python=Toolchain::DiscoverPython({},checkout);Check(bool(python),python.Error);
    std::cout<<"Python: "<<python.Executable<<" ("<<python.Source<<")\n";
    auto preferences=EditorPreferences::Load(diagnostic);Check(diagnostic.empty(),diagnostic);
    preferences.SDK=checkout.generic_u8string();preferences.Python.clear();preferences.Save();
    const auto project=working/fs::u8path(u8"Project space ü");
    auto execute=[&](std::vector<std::string> args){auto report=ProjectTools::Execute({{},checkout,{},std::move(args),"SDK integration"});Check(report.Success,report.Output);return report;};
    execute({"new-project","--name","SDK Integration","--identifier","SDKIntegration","--destination",project.generic_u8string()});
    execute({"authoring-preflight","--config",config,"--operation","scripts"});
    execute({"script-build",(project/"SDKIntegration.hproj").generic_u8string(),"--config",config});
    Check(fs::is_regular_file(project/"Assets/Scripts/Binaries/SDKIntegration.dll"),"No managed artifact published");
    const auto incomplete=working/"Incomplete Mono SDK é";
    const auto sdk=checkout/"build/dependencies/mono/macos-arm64/sdk";
    for(const char* file:{"bin/mono","include/mono-2.0/mono/jit/jit.h","lib/libmonosgen-2.0.dylib","lib/mono/4.5/mscorlib.dll","lib/mono/4.7.2-api/mscorlib.dll","etc/mono/config"}) {
        fs::create_directories((incomplete/file).parent_path());fs::create_symlink(sdk/file,incomplete/file);
    }
    const char* previous=std::getenv("HAZEL_MONO_SDK");const bool hadOverride=previous;
    const std::string savedOverride=previous?previous:"";
    setenv("HAZEL_MONO_SDK",incomplete.c_str(),1);
    auto missingCompiler=ProjectTools::Execute({{},checkout,{}, {"authoring-preflight","--config",config,"--operation","scripts"},"Missing compiler"});
    fs::create_symlink(sdk/"lib/mono/4.5/mcs.exe",incomplete/"lib/mono/4.5/mcs.exe");
    fs::remove(incomplete/"lib/mono/4.7.2-api/mscorlib.dll");
    auto missingProfile=ProjectTools::Execute({{},checkout,{}, {"authoring-preflight","--config",config,"--operation","scripts"},"Missing profile"});
    if(hadOverride)setenv("HAZEL_MONO_SDK",savedOverride.c_str(),1);else unsetenv("HAZEL_MONO_SDK");
    Check(!missingCompiler.Success&&missingCompiler.Output.find("mcs.exe")!=std::string::npos,"Missing explicit Mono compiler silently fell back");
    Check(!missingProfile.Success&&missingProfile.Output.find("4.7.2-api")!=std::string::npos,"Missing explicit managed profile accepted");
    auto exporting=ProjectTools::Execute({{},checkout,{}, {"authoring-preflight","--config",config,"--operation","export"},"Export readiness"});
    Check(!exporting.Success&&exporting.Output.find("not yet implemented")!=std::string::npos,"Export limitation not explicit");
    auto restart=Process::Run(FileSystem::GetExecutablePath(),{checkout.string(),working.string(),"restart"},working,std::chrono::seconds(15));
    Check(restart.ExitCode==0,restart.Output);std::cout<<restart.Output;
    std::cout<<"PASS: SDK discovery/classification/precedence/configuration/target metadata, restricted-PATH Python, editor tool-service readiness, real disposable script compilation and private-profile restart\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
