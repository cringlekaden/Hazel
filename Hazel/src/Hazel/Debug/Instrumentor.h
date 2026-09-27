#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <string>
#include <thread>

namespace Hazel {

    using ProfileClock = std::chrono::steady_clock;

    struct ProfileResult
    {
        std::string Name;
        ProfileClock::time_point Start;
        ProfileClock::duration Elapsed;
        std::thread::id ThreadID;
    };

    class Instrumentor
    {
    public:
        void BeginSession(
            const std::string& name,
            const std::string& filepath = "results.json")
        {
            std::lock_guard<std::mutex> lock(m_Mutex);

            if (m_SessionActive)
                EndSessionInternal();

            m_OutputStream.open(
                filepath,
                std::ios::out | std::ios::trunc);

            if (!m_OutputStream)
                return;

            m_SessionActive = true;
            m_FirstEvent = true;

            m_OutputStream
                << "{\"otherData\":{\"session\":\""
                << Escape(name)
                << "\"},\"traceEvents\":[";
        }

        void EndSession()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            EndSessionInternal();
        }

        void WriteProfile(const ProfileResult& result)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);

            if (!m_SessionActive)
                return;

            const double start =
                std::chrono::duration<double, std::micro>(
                    result.Start.time_since_epoch()).count();

            const double duration =
                std::chrono::duration<double, std::micro>(
                    result.Elapsed).count();

            const std::uint64_t threadID =
                static_cast<std::uint64_t>(
                    std::hash<std::thread::id>{}(result.ThreadID));

            if (!m_FirstEvent)
                m_OutputStream << ',';

            m_FirstEvent = false;

            m_OutputStream
                << std::fixed << std::setprecision(3)
                << "{\"cat\":\"function\",\"dur\":" << duration
                << ",\"name\":\"" << Escape(result.Name)
                << "\",\"ph\":\"X\",\"pid\":0,\"tid\":" << threadID
                << ",\"ts\":" << start << '}';
        }

        static Instrumentor& Get()
        {
            static Instrumentor instance;
            return instance;
        }

    private:
        static std::string Escape(const std::string& input)
        {
            std::string output;

            for (unsigned char character : input)
            {
                switch (character)
                {
                    case '"':
                        output += "\\\"";
                        break;
                    case '\\':
                        output += "\\\\";
                        break;
                    case '\n':
                        output += "\\n";
                        break;
                    case '\r':
                        output += "\\r";
                        break;
                    case '\t':
                        output += "\\t";
                        break;
                    default:
                        if (character < 0x20)
                        {
                            constexpr char hex[] =
                                "0123456789abcdef";

                            output += "\\u00";
                            output += hex[character >> 4];
                            output += hex[character & 0x0f];
                        }
                        else
                        {
                            output += static_cast<char>(character);
                        }
                }
            }

            return output;
        }

        void EndSessionInternal()
        {
            if (!m_SessionActive)
                return;

            m_OutputStream << "]}";
            m_OutputStream.close();
            m_SessionActive = false;
        }

        std::mutex m_Mutex;
        std::ofstream m_OutputStream;
        bool m_SessionActive = false;
        bool m_FirstEvent = true;
    };

    class InstrumentationTimer
    {
    public:
        explicit InstrumentationTimer(const char* name)
            : m_Name(name),
              m_Start(ProfileClock::now())
        {
        }

        ~InstrumentationTimer()
        {
            Stop();
        }

        InstrumentationTimer(const InstrumentationTimer&) = delete;
        InstrumentationTimer& operator=(const InstrumentationTimer&) = delete;

        void Stop()
        {
            if (m_Stopped)
                return;

            const auto end = ProfileClock::now();
            m_Stopped = true;

            Instrumentor::Get().WriteProfile({
                m_Name,
                m_Start,
                end - m_Start,
                std::this_thread::get_id()
            });
        }

    private:
        const char* m_Name;
        ProfileClock::time_point m_Start;
        bool m_Stopped = false;
    };
}

#ifndef HZ_PROFILE
    #define HZ_PROFILE 1
#endif

#define HZ_PROFILE_CONCAT_IMPL(a, b) a##b
#define HZ_PROFILE_CONCAT(a, b) HZ_PROFILE_CONCAT_IMPL(a, b)

#if defined(_MSC_VER)
    #define HZ_PROFILE_FUNCTION_NAME __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
    #define HZ_PROFILE_FUNCTION_NAME __PRETTY_FUNCTION__
#else
    #define HZ_PROFILE_FUNCTION_NAME __func__
#endif

#if HZ_PROFILE
    #define HZ_PROFILE_BEGIN_SESSION(name, filepath) \
        ::Hazel::Instrumentor::Get().BeginSession(name, filepath)

    #define HZ_PROFILE_END_SESSION() \
        ::Hazel::Instrumentor::Get().EndSession()

    #define HZ_PROFILE_SCOPE(name) \
        ::Hazel::InstrumentationTimer \
            HZ_PROFILE_CONCAT(_hzProfileTimer_, __LINE__)(name)

    #define HZ_PROFILE_FUNCTION() \
        HZ_PROFILE_SCOPE(HZ_PROFILE_FUNCTION_NAME)
#else
    #define HZ_PROFILE_BEGIN_SESSION(name, filepath) ((void)0)
    #define HZ_PROFILE_END_SESSION() ((void)0)
    #define HZ_PROFILE_SCOPE(name) ((void)0)
    #define HZ_PROFILE_FUNCTION() ((void)0)
#endif