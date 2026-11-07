#include "ProducerMetrics.h"
#include <stdexcept>
#ifdef HZ_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
namespace Hazel::EnvironmentBake
{
uint64_t PeakResidentBytes()
{
#ifdef HZ_PLATFORM_WINDOWS
    using Query = BOOL(WINAPI *)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    auto query = reinterpret_cast<Query>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo"));
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (!query || !query(GetCurrentProcess(), &counters, sizeof(counters)))
        throw std::runtime_error("Cannot measure producer peak resident bytes");
    return uint64_t(counters.PeakWorkingSetSize);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage))
        throw std::runtime_error("Cannot measure producer peak resident bytes");
#ifdef HZ_PLATFORM_MACOS
    return uint64_t(usage.ru_maxrss);
#else
    return uint64_t(usage.ru_maxrss) * 1024;
#endif
#endif
}
} // namespace Hazel::EnvironmentBake
