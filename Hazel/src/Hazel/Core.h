#pragma once

#ifdef HZ_PLATFORM_WINDOWS
    #ifdef HZ_BUILD_DLL
        #define HAZEL_API __declspec(dllexport)
    #else
        #define HAZEL_API __declspec(dllimport)
    #endif
#elif defined(HZ_PLATFORM_LINUX)
    #define HAZEL_API
#else
    #error Hazel only supports Windows and Linux!
#endif