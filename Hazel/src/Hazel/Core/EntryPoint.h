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
    HZ_CORE_WARN("Initialized Log...");
    HZ_PROFILE_BEGIN_SESSION("Startup", "HazelProfile-Startup.json");
    auto app = Hazel::CreateApplication({argc, argv});
    HZ_PROFILE_END_SESSION();
    HZ_PROFILE_BEGIN_SESSION("Runtime", "HazelProfile-Runtime.json");
    app->Run();
    HZ_PROFILE_END_SESSION();
    HZ_PROFILE_BEGIN_SESSION("Shutdown", "HazelProfile-Shutdown.json");
    app.reset();
    HZ_PROFILE_END_SESSION();
}
#endif
