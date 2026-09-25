#pragma once

#if defined(HZ_PLATFORM_WINDOWS)
    #define HZ_DEBUGBREAK() __debugbreak()
#elif defined(HZ_PLATFORM_LINUX)
    #include <signal.h>
    #define HAZEL_API
    #define HZ_DEBUGBREAK() raise(SIGTRAP)
#else
    #error Hazel only supports Windows and Linux...
#endif

#define BIT(x) (1 << x)
#define HZ_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

#ifdef HZ_DEBUG
    #define HZ_ENABLE_ASSERTS
#endif

#ifdef HZ_ENABLE_ASSERTS
    #define HZ_ASSERT(x, ...) { if(!(x)) { HZ_ERROR("Assertion Failed: {0}", __VA_ARGS__); HZ_DEBUGBREAK(); } }
    #define HZ_CORE_ASSERT(x, ...) { if(!(x)) { HZ_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); HZ_DEBUGBREAK(); } }
#else
    #define HZ_ASSERT(x, ...)
    #define HZ_CORE_ASSERT(x, ...)
#endif