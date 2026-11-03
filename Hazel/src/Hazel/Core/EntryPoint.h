#pragma once

#include "Hazel/Debug/Instrumentor.h"
#include "Hazel/Core/ApplicationLaunch.h"
#include "Hazel/Core/FileSystem.h"
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif

#if defined(HZ_PLATFORM_WINDOWS) || defined(HZ_PLATFORM_LINUX) || defined(HZ_PLATFORM_MACOS)

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
        auto launch = Hazel::ApplicationLaunch::Parse(argc, argv);
        if (launch.SmokeTest &&
            (!Hazel::FileSystem::GetEnvironmentPath("HAZEL_DATA").is_absolute() ||
             !Hazel::FileSystem::GetEnvironmentPath("HAZEL_SAVE_ROOT").is_absolute()))
            throw std::invalid_argument("--smoke-test requires absolute HAZEL_DATA and HAZEL_SAVE_ROOT pointing to private test storage");
        auto app = Hazel::CreateApplication({launch.Count(), launch.Arguments.data()});
        if (launch.SmokeTest) {
            if (!app) throw std::invalid_argument("--smoke-test requires a graphical application launch");
            app->RunForSmokeTest();
            app.reset();
            HZ_CORE_INFO("SMOKE PASS: application teardown completed");
        } else if (app) app->Run();
        return 0;
    } catch (const std::exception& error) {
        HZ_CORE_ERROR("{}", error.what());
        return 1;
    }

}
#endif
