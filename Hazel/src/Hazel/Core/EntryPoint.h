#pragma once

#include "Hazel/Debug/Instrumentor.h"
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif

#if defined(HZ_PLATFORM_WINDOWS) || defined(HZ_PLATFORM_LINUX)

extern Hazel::Scope<Hazel::Application> Hazel::CreateApplication(Hazel::ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
#ifdef HZ_PLATFORM_WINDOWS
    auto encodedArguments = Hazel::WindowsCommandLineUTF8();
    std::vector<char*> argumentPointers;
    for (auto& value : encodedArguments) argumentPointers.push_back(value.data());
    argc = static_cast<int>(argumentPointers.size());
    argumentPointers.push_back(nullptr);
    argv = argumentPointers.data();
#endif
    Hazel::Log::Init();
    try {
        auto app = Hazel::CreateApplication({argc, argv});
        if (app) app->Run();
        return 0;
    } catch (const std::exception& error) {
        HZ_CORE_ERROR("{}", error.what());
        return 1;
    }

}
#endif
