#pragma once
#include "Hazel/Core/Log.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>
#include <filesystem>
namespace Hazel
{
enum class ConsoleSource
{
    Core,
    App,
    Managed,
    Authoring,
    Stdout,
    Stderr,
    Tool,
    Count
};
struct ConsoleEntry
{
    uint64_t Sequence = 0, Operation = 0;
    std::chrono::system_clock::time_point Time;
    spdlog::level::level_enum Level = spdlog::level::info;
    ConsoleSource Source = ConsoleSource::App;
    std::string Message;
    std::chrono::milliseconds Elapsed{0};
};
struct ConsoleFilter
{
    unsigned Severities = 0x3f, Sources = 0x7f;
    std::string Search;
    uint64_t Operation = 0;
    bool Matches(const ConsoleEntry &entry) const;
    bool Active() const
    {
        return Severities != 0x3f || Sources != 0x7f || !Search.empty() || Operation;
    }
};
enum class ToolOutcome
{
    Running,
    Succeeded,
    Failed,
    Cancelled,
    TimedOut,
    Superseded
};
struct ConsoleOperation
{
    uint64_t ID = 0, Generation = 0;
    std::string Label, Project, SDK, Python, Command, Stage, Diagnostic, Artifact, EffectivePython,
        PythonSource;
    ToolOutcome Outcome = ToolOutcome::Running;
    int ExitCode = -1;
    std::chrono::system_clock::time_point Start, End;
    std::chrono::steady_clock::time_point ClockStart;
    std::chrono::milliseconds Elapsed{0};
};
// Producers only touch the bounded inbox/operation snapshots. Retained entries and
// filtering are main-thread owned; Pump never copies the entire producer backlog.
class ConsoleModel : public LogReceiver
{
  public:
    struct Limits
    {
        size_t InboxBytes = 1024 * 1024, InboxEntries = 1024, RetainedBytes = 8 * 1024 * 1024,
               RetainedEntries = 10000, MessageBytes = 64 * 1024, Operations = 20;
    };
    ConsoleModel();
    explicit ConsoleModel(Limits limits);
    void Receive(const LogRecord &record) override;
    void Append(ConsoleSource source, spdlog::level::level_enum level, std::string text,
                uint64_t operation = 0,
                std::chrono::system_clock::time_point time = std::chrono::system_clock::now(),
                bool truncated = false);
    void Pump(size_t entries = 256, size_t bytes = 256 * 1024);
    void Clear(); // Includes queued messages and log-error badge; never resets operation results or sequence
                  // IDs.
    void Stop();
    const std::deque<ConsoleEntry> &Entries() const
    {
        return m_Retained;
    }
    std::vector<const ConsoleEntry *> Filtered(const ConsoleFilter &filter) const;
    std::string Copy(const ConsoleFilter &filter, const std::vector<uint64_t> &selected = {}) const;
    uint64_t Dropped() const;
    uint64_t Truncated() const;
    uint64_t Errors() const;
    void Begin(ConsoleOperation operation);
    void Stage(uint64_t id, std::string stage);
    void SetPython(uint64_t id, std::string executable, std::string source);
    void Finish(uint64_t id, ToolOutcome outcome, int exitCode, const std::string &diagnostic = {},
                std::string artifact = {});
    std::vector<ConsoleOperation> Operations() const;
    std::optional<ConsoleOperation> LatestFailure() const;
    bool HasFailure() const;
    std::string Status() const;
    uint64_t Revision() const
    {
        return m_Revision.load();
    }
    void DismissFailure();
    std::atomic<int> CaptureLevel{spdlog::level::info};
    static const char *SourceName(ConsoleSource source);
    static const char *OutcomeName(ToolOutcome outcome);
    static std::string Timestamp(std::chrono::system_clock::time_point time, bool full = false);

  private:
    static size_t Size(const ConsoleEntry &entry)
    {
        return entry.Message.size() + sizeof(ConsoleEntry);
    }
    Limits m_Limits;
    const std::chrono::steady_clock::time_point m_Epoch = std::chrono::steady_clock::now();
    mutable std::mutex m_Mutex;
    std::deque<ConsoleEntry> m_Inbox;
    size_t m_InboxBytes = 0, m_RetainedBytes = 0;
    uint64_t m_NextSequence = 1, m_Dropped = 0, m_Truncated = 0, m_Errors = 0;
    bool m_Accepting = true;
    std::deque<ConsoleEntry> m_Retained;
    std::deque<ConsoleOperation> m_Operations;
    std::optional<ConsoleOperation> m_LatestFailure;
    std::atomic<uint64_t> m_Revision{0};
};
// Construct before Application, destroy after Application's base shutdown. No UI
// references in the observer; Reset completes before releasing the data model.
class ConsoleSession
{
  public:
    ConsoleSession();
    ~ConsoleSession();
    std::shared_ptr<ConsoleModel> Model;

  private:
    LogSubscription m_Logging;
};
} // namespace Hazel
