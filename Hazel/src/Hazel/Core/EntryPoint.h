#pragma once

#include "Hazel/Debug/Instrumentor.h"

#if defined(HZ_PLATFORM_WINDOWS) || defined(HZ_PLATFORM_LINUX)

extern Hazel::Scope<Hazel::Application> Hazel::CreateApplication();

int main()
{
    Hazel::Log::Init();
    HZ_CORE_WARN("Initialized Log...");
    HZ_PROFILE_BEGIN_SESSION("Startup", "HazelProfile-Startup.json");
    auto app = Hazel::CreateApplication();
    HZ_PROFILE_END_SESSION();
    HZ_PROFILE_BEGIN_SESSION("Runtime", "HazelProfile-Runtime.json");
    app->Run();
    HZ_PROFILE_END_SESSION();
    HZ_PROFILE_BEGIN_SESSION("Shutdown", "HazelProfile-Shutdown.json");
    app.reset();
    HZ_PROFILE_END_SESSION();
}
#endif