#pragma once
// Adapted from upstream Assert.h for portable C++17 optional/variadic messages.
#include "Hazel/Core/Base.h"
#include "Hazel/Core/Log.h"
#include <filesystem>
namespace Hazel::detail {
inline void AssertionMessage(const Ref<spdlog::logger>& logger, const char* expression, const char* file, int line)
{
    logger->error("Assertion '{}' failed at {}:{}", expression, std::filesystem::path(file).filename().string(), line);
}
template<typename... Args>
void AssertionMessage(const Ref<spdlog::logger>& logger, const char*, const char*, int, const char* message, Args&&... args)
{
    logger->error("Assertion failed: {}", fmt::format(fmt::runtime(message), std::forward<Args>(args)...));
}
template<typename Check, typename... Args>
bool CheckAssertion(const Ref<spdlog::logger>& logger, const char* expression, const char* file, int line, Check&& check, Args&&... args)
{
    if (static_cast<bool>(check)) return true;
    AssertionMessage(logger, expression, file, line, std::forward<Args>(args)...);
    return false;
}
}
#ifdef HZ_ENABLE_ASSERTS
#define HZ_INTERNAL_ASSERT(logger, ...) do { if (!::Hazel::detail::CheckAssertion(logger, #__VA_ARGS__, __FILE__, __LINE__, __VA_ARGS__)) { HZ_DEBUGBREAK(); } } while (false)
#define HZ_ASSERT(...) HZ_INTERNAL_ASSERT(::Hazel::Log::GetClientLogger(), __VA_ARGS__)
#define HZ_CORE_ASSERT(...) HZ_INTERNAL_ASSERT(::Hazel::Log::GetCoreLogger(), __VA_ARGS__)
#else
#define HZ_ASSERT(...) ((void)0)
#define HZ_CORE_ASSERT(...) ((void)0)
#endif
