#pragma once

#include "Hazel/Core/Memory.h"
#include "Hazel/Core/PlatformDetection.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include <chrono>
#include <string>

namespace Hazel {

    struct LogRecord {
        spdlog::level::level_enum Level;
        std::chrono::system_clock::time_point Time;
        std::string Logger, Origin, Message;
        bool Truncated = false;
    };
    // Receivers only ingest data. Never call ImGui, log recursively or perform I/O here.
    class LogReceiver {
    public:
        virtual ~LogReceiver() = default;
        virtual void Receive(const LogRecord& record) = 0;
    };
    struct LogDistribution;
    struct LogObserver;
    class LogSubscription {
    public:
        LogSubscription() = default;
        LogSubscription(LogSubscription&& other) noexcept;
        LogSubscription& operator=(LogSubscription&& other) noexcept;
        ~LogSubscription();
        void Reset(); // Waits for bounded in-flight delivery; no callback after Reset returns.
    private:
        friend class Log;
        std::shared_ptr<LogDistribution> m_State;
        std::shared_ptr<LogObserver> m_Observer;
    };

    class Log
    {
    public:
        static void Init();
        static LogSubscription Observe(const std::shared_ptr<LogReceiver>& receiver,
                                       spdlog::level::level_enum level = spdlog::level::info);
        static void SetObserverLevel(const LogReceiver* receiver, spdlog::level::level_enum level);

        inline static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
    private:
        static Ref<spdlog::logger> s_CoreLogger;
        static Ref<spdlog::logger> s_ClientLogger;
    };
}

// Core log macros
#define HZ_CORE_TRACE(...)    ::Hazel::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define HZ_CORE_INFO(...)     ::Hazel::Log::GetCoreLogger()->info(__VA_ARGS__)
#define HZ_CORE_WARN(...)     ::Hazel::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define HZ_CORE_ERROR(...)    ::Hazel::Log::GetCoreLogger()->error(__VA_ARGS__)
#define HZ_CORE_FATAL(...)    ::Hazel::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define HZ_TRACE(...)         ::Hazel::Log::GetClientLogger()->trace(__VA_ARGS__)
#define HZ_INFO(...)          ::Hazel::Log::GetClientLogger()->info(__VA_ARGS__)
#define HZ_WARN(...)          ::Hazel::Log::GetClientLogger()->warn(__VA_ARGS__)
#define HZ_ERROR(...)         ::Hazel::Log::GetClientLogger()->error(__VA_ARGS__)
#define HZ_FATAL(...)         ::Hazel::Log::GetClientLogger()->critical(__VA_ARGS__)
#define HZ_CORE_CRITICAL(...) ::Hazel::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define HZ_CRITICAL(...) ::Hazel::Log::GetClientLogger()->critical(__VA_ARGS__)
