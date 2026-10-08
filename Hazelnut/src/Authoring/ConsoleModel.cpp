#include "ConsoleModel.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cctype>
namespace Hazel
{
namespace
{
std::string Bounded(std::string value, size_t maximum, bool *truncated = nullptr)
{
    if (value.size() > maximum)
    {
        size_t cut = maximum;
        while (cut > 0 && (static_cast<unsigned char>(value[cut]) & 0xc0) == 0x80)
            --cut;
        value.resize(cut);
        value += "\n[message truncated]";
        if (truncated)
            *truncated = true;
    }
    std::replace(value.begin(), value.end(), '\0', '?');
    return value;
}
std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool Failed(ToolOutcome state)
{
    return state == ToolOutcome::Failed || state == ToolOutcome::TimedOut || state == ToolOutcome::Cancelled;
}
} // namespace
bool ConsoleFilter::Matches(const ConsoleEntry &entry) const
{
    return (Severities & (1u << entry.Level)) && (Sources & (1u << static_cast<unsigned>(entry.Source))) &&
           (!Operation || entry.Operation == Operation) &&
           (Search.empty() || Lower(entry.Message).find(Lower(Search)) != std::string::npos);
}
ConsoleModel::ConsoleModel() : ConsoleModel(Limits{})
{
}
ConsoleModel::ConsoleModel(Limits limits) : m_Limits(limits)
{
    if (!limits.InboxEntries || !limits.RetainedEntries || !limits.Operations || limits.MessageBytes < 32 ||
        limits.InboxBytes < limits.MessageBytes + 128 || limits.RetainedBytes < limits.MessageBytes + 128)
        throw std::invalid_argument("Console limits must hold at least one bounded record");
}
void ConsoleModel::Receive(const LogRecord &record)
{
    if (static_cast<int>(record.Level) < CaptureLevel.load())
        return;
    auto source = record.Logger == "HAZEL" ? ConsoleSource::Core : ConsoleSource::App;
    if (record.Origin == "Managed")
        source = ConsoleSource::Managed;
    if (record.Origin == "Authoring")
        source = ConsoleSource::Authoring;
    Append(source, record.Level, record.Message, 0, record.Time, record.Truncated);
}
void ConsoleModel::Append(ConsoleSource source, spdlog::level::level_enum level, std::string text,
                          uint64_t operation, std::chrono::system_clock::time_point time, bool truncated)
{
    text = Bounded(std::move(text), m_Limits.MessageBytes, &truncated);
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Accepting)
        return;
    ConsoleEntry entry{
        m_NextSequence++,
        operation,
        time,
        level,
        source,
        std::move(text),
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_Epoch)};
    if (truncated)
        ++m_Truncated;
    if (level >= spdlog::level::err)
        ++m_Errors;
    const auto bytes = Size(entry);
    while (!m_Inbox.empty() &&
           (m_Inbox.size() >= m_Limits.InboxEntries || m_InboxBytes + bytes > m_Limits.InboxBytes))
    {
        m_InboxBytes -= Size(m_Inbox.front());
        m_Inbox.pop_front();
        ++m_Dropped;
    }
    m_InboxBytes += bytes;
    m_Inbox.push_back(std::move(entry));
}
void ConsoleModel::Pump(size_t count, size_t bytes)
{
    std::deque<ConsoleEntry> batch;
    size_t size = 0;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        while (!m_Inbox.empty() && batch.size() < count &&
               (batch.empty() || size + Size(m_Inbox.front()) <= bytes))
        {
            size += Size(m_Inbox.front());
            m_InboxBytes -= Size(m_Inbox.front());
            batch.push_back(std::move(m_Inbox.front()));
            m_Inbox.pop_front();
        }
    }
    uint64_t dropped = 0;
    for (auto &entry : batch)
    {
        const auto added = Size(entry);
        while (!m_Retained.empty() && (m_Retained.size() >= m_Limits.RetainedEntries ||
                                       m_RetainedBytes + added > m_Limits.RetainedBytes))
        {
            m_RetainedBytes -= Size(m_Retained.front());
            m_Retained.pop_front();
            ++dropped;
        }
        m_RetainedBytes += added;
        m_Retained.push_back(std::move(entry));
    }
    if (dropped)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Dropped += dropped;
    }
}
void ConsoleModel::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Inbox.clear();
    m_Retained.clear();
    m_InboxBytes = m_RetainedBytes = 0;
    m_Dropped = m_Truncated = m_Errors = 0;
}
void ConsoleModel::Stop()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Accepting = false;
}
std::vector<const ConsoleEntry *> ConsoleModel::Filtered(const ConsoleFilter &filter) const
{
    std::vector<const ConsoleEntry *> result;
    for (const auto &entry : m_Retained)
        if (filter.Matches(entry))
            result.push_back(&entry);
    return result;
}
std::string ConsoleModel::Copy(const ConsoleFilter &filter, const std::vector<uint64_t> &selected) const
{
    std::string text;
    for (const auto &entry : m_Retained)
        if (filter.Matches(entry) && (selected.empty() || std::find(selected.begin(), selected.end(),
                                                                    entry.Sequence) != selected.end()))
        {
            text += "[" + Timestamp(entry.Time, true) + "] [" + SourceName(entry.Source) + "] [" +
                    std::string(spdlog::level::to_string_view(entry.Level).data()) + "]";
            if (entry.Operation)
                text += " [operation " + std::to_string(entry.Operation) + "]";
            text += " [+" + std::to_string(entry.Elapsed.count()) + " ms] " + entry.Message + "\n";
        }
    return text;
}
uint64_t ConsoleModel::Dropped() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Dropped;
}
uint64_t ConsoleModel::Truncated() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Truncated;
}
uint64_t ConsoleModel::Errors() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Errors;
}
void ConsoleModel::Begin(ConsoleOperation operation)
{
    operation.Label = Bounded(std::move(operation.Label), 256);
    operation.Project = Bounded(std::move(operation.Project), 4096);
    operation.SDK = Bounded(std::move(operation.SDK), 4096);
    operation.Python = Bounded(std::move(operation.Python), 4096);
    operation.Command = Bounded(std::move(operation.Command), 8192);
    operation.Artifact = Bounded(std::move(operation.Artifact), 4096);
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Accepting)
        return;
    if (m_Operations.size() >= m_Limits.Operations)
        m_Operations.pop_front();
    m_Operations.push_back(std::move(operation));
    ++m_Revision;
}
void ConsoleModel::Stage(uint64_t id, std::string stage)
{
    stage = Bounded(std::move(stage), 512);
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto &op : m_Operations)
        if (op.ID == id && op.Outcome == ToolOutcome::Running)
        {
            if (op.Stage != stage)
            {
                op.Stage = std::move(stage);
                ++m_Revision;
            }
            return;
        }
}
void ConsoleModel::SetPython(uint64_t id, std::string executable, std::string source)
{
    executable = Bounded(std::move(executable), 32768);
    source = Bounded(std::move(source), 512);
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto &op : m_Operations)
        if (op.ID == id)
        {
            op.EffectivePython = std::move(executable);
            op.PythonSource = std::move(source);
            ++m_Revision;
            return;
        }
}
void ConsoleModel::Finish(uint64_t id, ToolOutcome outcome, int code, const std::string &diagnostic,
                          std::string artifact)
{
    // Compiler failures commonly arrive at the end. Copy only the bounded tail,
    // not the complete process report, on the main thread.
    std::string bounded;
    if (diagnostic.size() > 64 * 1024)
    {
        size_t cut = diagnostic.size() - 64 * 1024;
        while (cut < diagnostic.size() && (static_cast<unsigned char>(diagnostic[cut]) & 0xc0) == 0x80)
            ++cut;
        bounded = "[earlier result output omitted]\n" + diagnostic.substr(cut);
    }
    else
        bounded = diagnostic;
    std::replace(bounded.begin(), bounded.end(), '\0', '?');
    artifact = Bounded(std::move(artifact), 4096);
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto &op : m_Operations)
        if (op.ID == id)
        {
            op.Outcome = outcome;
            op.ExitCode = code;
            op.End = std::chrono::system_clock::now();
            op.Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - op.ClockStart);
            op.Diagnostic = std::move(bounded);
            if (!artifact.empty())
                op.Artifact = std::move(artifact);
            op.Stage = OutcomeName(outcome);
            if (Failed(outcome))
                m_LatestFailure = op;
            ++m_Revision;
            return;
        }
}
std::vector<ConsoleOperation> ConsoleModel::Operations() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return {m_Operations.begin(), m_Operations.end()};
}
std::optional<ConsoleOperation> ConsoleModel::LatestFailure() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_LatestFailure;
}
void ConsoleModel::DismissFailure()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_LatestFailure.reset();
    ++m_Revision;
}
bool ConsoleModel::HasFailure() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return bool(m_LatestFailure);
}
std::string ConsoleModel::Status() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Operations.empty())
        return {};
    const auto &op = m_Operations.back();
    return op.Label + ": " + (op.Outcome == ToolOutcome::Running ? op.Stage : OutcomeName(op.Outcome));
}
const char *ConsoleModel::SourceName(ConsoleSource source)
{
    const char *names[]{"Core", "App", "Managed", "Authoring", "stdout", "stderr", "Tool"};
    return names[static_cast<unsigned>(source)];
}
const char *ConsoleModel::OutcomeName(ToolOutcome outcome)
{
    const char *names[]{"Running", "Succeeded", "Failed", "Cancelled", "Timed out", "Previous project"};
    return names[static_cast<unsigned>(outcome)];
}
std::string ConsoleModel::Timestamp(std::chrono::system_clock::time_point time, bool full)
{
    const auto value = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
#ifdef HZ_PLATFORM_WINDOWS
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count() % 1000;
    std::ostringstream out;
    out << std::put_time(&local, full ? "%Y-%m-%d %H:%M:%S" : "%H:%M:%S") << '.' << std::setw(3)
        << std::setfill('0') << milliseconds;
    if (full)
        out << std::put_time(&local, " %z");
    return out.str();
}
ConsoleSession::ConsoleSession() : Model(std::make_shared<ConsoleModel>()), m_Logging(Log::Observe(Model))
{
}
ConsoleSession::~ConsoleSession()
{
    m_Logging.Reset();
    Model->Stop();
}
} // namespace Hazel
