#pragma once
// Adapted from upstream platform detection; only actual implemented ports enabled.
#if defined(_WIN32) && defined(_WIN64)
#ifndef HZ_PLATFORM_WINDOWS
#define HZ_PLATFORM_WINDOWS
#endif
#elif defined(__linux__) && !defined(__ANDROID__)
#ifndef HZ_PLATFORM_LINUX
#define HZ_PLATFORM_LINUX
#endif
#else
#error "No Hazel native platform implementation configured for this target"
#endif
