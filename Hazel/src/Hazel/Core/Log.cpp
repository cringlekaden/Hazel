#include "hzpch.h"
#include "Log.h"
#include "Hazel/Core/Base.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include <mutex>
#include <atomic>
#include <algorithm>
#include <array>

namespace Hazel
{

struct LogObserver
{
    std::mutex Delivery;
    std::weak_ptr<LogReceiver> Receiver;
    const LogReceiver *Identity = nullptr;
    std::atomic<bool> Enabled{true};
    std::atomic<int> Level{spdlog::level::info};
};
struct LogDistribution
{
    std::mutex Mutex;
    std::vector<std::shared_ptr<LogObserver>> Observers;
    std::weak_ptr<spdlog::logger> Core, Client;
    int CoreLevel = spdlog::level::info, ClientLevel = spdlog::level::info;
    void Levels()
    { // Caller holds Mutex; logger level changes are atomic, not sink-vector edits.
        int level = spdlog::level::off;
        for (const auto &observer : Observers)
            if (observer->Enabled)
                level = std::min(level, observer->Level.load());
        if (auto core = Core.lock())
            core->set_level(static_cast<spdlog::level::level_enum>(std::min(CoreLevel, level)));
        if (auto client = Client.lock())
            client->set_level(static_cast<spdlog::level::level_enum>(std::min(ClientLevel, level)));
    }
};
static std::shared_ptr<LogDistribution> s_Distribution;
class DistributionSink : public spdlog::sinks::sink
{
  public:
    explicit DistributionSink(std::shared_ptr<LogDistribution> state) : m_State(std::move(state))
    {
    }
    void log(const spdlog::details::log_msg &msg) override
    {
        static thread_local bool delivering = false;
        if (delivering)
            return;
        struct Scope
        {
            bool &Flag;
            Scope(bool &flag) : Flag(flag)
            {
                Flag = true;
            }
            ~Scope()
            {
                Flag = false;
            }
        } scope(delivering);
        std::array<std::shared_ptr<LogObserver>, 8> observers;
        size_t count = 0;
        {
            std::lock_guard<std::mutex> lock(m_State->Mutex);
            count = m_State->Observers.size();
            std::copy(m_State->Observers.begin(), m_State->Observers.end(), observers.begin());
        }
        if (!count)
            return;
        // Bound producer copying even if a caller logs an enormous payload.
        const size_t cap = 64 * 1024;
        size_t cut = std::min<size_t>(msg.payload.size(), cap);
        if (msg.payload.size() > cap)
        {
            cut -= 32;
            while (cut && (static_cast<unsigned char>(msg.payload[cut]) & 0xc0) == 0x80)
                --cut;
        }
        size_t originSize = 0;
        if (msg.source.filename)
            while (originSize < 128 && msg.source.filename[originSize])
                ++originSize;
        LogRecord record{msg.level,
                         msg.time,
                         std::string(msg.logger_name.data(), std::min<size_t>(msg.logger_name.size(), 128)),
                         msg.source.filename ? std::string(msg.source.filename, originSize) : std::string{},
                         std::string(msg.payload.data(), cut),
                         msg.payload.size() > cap};
        if (msg.payload.size() > cap)
            record.Message += "\n[message truncated at 64 KiB]";
        for (size_t index = 0; index < count; ++index)
        {
            const auto &observer = observers[index];
            auto receiver =
                observer->Receiver.lock(); // Never destroy a receiver under the registry/delivery lock.
            {
                std::lock_guard<std::mutex> lock(observer->Delivery);
                if (receiver && observer->Enabled && static_cast<int>(record.Level) >= observer->Level)
                    try
                    {
                        receiver->Receive(record);
                    }
                    catch (...)
                    { /* Never log from a sink. */
                    }
            }
        }
    }
    void flush() override
    {
    }
    void set_pattern(const std::string &) override
    {
    }
    void set_formatter(std::unique_ptr<spdlog::formatter>) override
    {
    }

  private:
    std::shared_ptr<LogDistribution> m_State;
};
LogSubscription::LogSubscription(LogSubscription &&other) noexcept
    : m_State(std::move(other.m_State)), m_Observer(std::move(other.m_Observer))
{
}
LogSubscription &LogSubscription::operator=(LogSubscription &&other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_State = std::move(other.m_State);
        m_Observer = std::move(other.m_Observer);
    }
    return *this;
}
LogSubscription::~LogSubscription()
{
    Reset();
}
void LogSubscription::Reset()
{
    if (!m_Observer)
        return;
    {
        std::lock_guard<std::mutex> lock(m_Observer->Delivery);
        m_Observer->Enabled = false;
    }
    {
        std::lock_guard<std::mutex> lock(m_State->Mutex);
        auto &entries = m_State->Observers;
        entries.erase(std::remove(entries.begin(), entries.end(), m_Observer), entries.end());
        m_State->Levels();
    }
    m_Observer.reset();
    m_State.reset();
}
LogSubscription Log::Observe(const std::shared_ptr<LogReceiver> &receiver, spdlog::level::level_enum level)
{
    if (!s_Distribution || !receiver)
        throw std::logic_error("Initialize logging before attaching a receiver");
    LogSubscription token;
    token.m_State = s_Distribution;
    token.m_Observer = std::make_shared<LogObserver>();
    token.m_Observer->Receiver = receiver;
    token.m_Observer->Identity = receiver.get();
    token.m_Observer->Level = level;
    {
        std::lock_guard<std::mutex> lock(s_Distribution->Mutex);
        if (s_Distribution->Observers.size() >= 8)
            throw std::runtime_error("At most eight log observers are supported");
        if (s_Distribution->Observers.empty())
        {
            s_Distribution->CoreLevel = s_CoreLogger->level();
            s_Distribution->ClientLevel = s_ClientLogger->level();
        }
        s_Distribution->Observers.push_back(token.m_Observer);
        s_Distribution->Levels();
    }
    return token;
}
void Log::SetObserverLevel(const LogReceiver *receiver, spdlog::level::level_enum level)
{
    if (!s_Distribution)
        return;
    std::lock_guard<std::mutex> lock(s_Distribution->Mutex);
    for (const auto &observer : s_Distribution->Observers)
        if (observer->Identity == receiver)
            observer->Level = level;
    s_Distribution->Levels();
}

Ref<spdlog::logger> Log::s_CoreLogger;
Ref<spdlog::logger> Log::s_ClientLogger;

void Log::Init()
{
    spdlog::set_pattern("%^[%T] %n: %v%$");
    s_Distribution = std::make_shared<LogDistribution>();
    auto relay = std::make_shared<DistributionSink>(s_Distribution);
    auto stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    stdoutSink->set_level(spdlog::level::info);
    s_CoreLogger = std::make_shared<spdlog::logger>("HAZEL", spdlog::sinks_init_list{stdoutSink, relay});
    spdlog::register_logger(s_CoreLogger);
    s_CoreLogger->set_level(spdlog::level::info);

    s_ClientLogger = std::make_shared<spdlog::logger>("APP", spdlog::sinks_init_list{stdoutSink, relay});
    spdlog::register_logger(s_ClientLogger);
    s_ClientLogger->set_level(spdlog::level::info);
    s_CoreLogger->set_pattern("%^[%T] %n: %v%$");
    s_ClientLogger->set_pattern("%^[%T] %n: %v%$");
    s_Distribution->Core = s_CoreLogger;
    s_Distribution->Client = s_ClientLogger;
}

} // namespace Hazel