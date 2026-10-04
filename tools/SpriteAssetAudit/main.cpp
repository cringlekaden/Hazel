#include "Hazel/Assets/ProjectAssets.h"
#include <iostream>
#include <fstream>
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif
int main(int argc,char** argv) {
    try {
        std::vector<std::string> arguments;
#ifdef HZ_PLATFORM_WINDOWS
        arguments=Hazel::WindowsCommandLineUTF8();
#else
        for(int i=0;i<argc;++i)arguments.emplace_back(argv[i]);
#endif
        if(arguments.size()!=3)throw std::runtime_error("Usage: SpriteAssetAudit Assets-root inventory.txt");
        const auto root=std::filesystem::u8path(arguments[1]);
        std::ifstream input(std::filesystem::u8path(arguments[2]),std::ios::binary);if(!input)throw std::runtime_error("Cannot read asset inventory");
        std::vector<std::filesystem::path> files;std::string line;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())files.push_back(std::filesystem::u8path(line));}
        if(input.bad())throw std::runtime_error("Cannot read complete asset inventory");
        for(auto& dependency:Hazel::AuditSpriteAssets(root,files))std::cout<<"DEPENDENCY "<<dependency.generic_u8string()<<'\n';
        std::cout<<"PASS: native sprite/animation dependency closure\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Sprite asset audit: "<<error.what()<<'\n';return 1;}
}
