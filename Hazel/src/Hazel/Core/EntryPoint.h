#pragma once

#if defined(HZ_PLATFORM_WINDOWS) || defined(HZ_PLATFORM_LINUX)

extern Hazel::Application* Hazel::CreateApplication();

int main()
{
    Hazel::Log::Init();
    HZ_CORE_WARN("Initialized Log!");
    auto app = Hazel::CreateApplication();
    app->Run();
    delete app;
}
#endif