#include "ProjectTools.h"
#include "HazelSDK.h"
#include "Hazel/Utils/Process.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace Hazel
{
namespace
{
std::atomic<uint64_t> NextOperation{1};
spdlog::level::level_enum Severity(const std::string &text)
{
    // stderr is a source, not a severity. Recognize common compiler diagnostics only.
    if (text.find("error:") != std::string::npos || text.find("error C") != std::string::npos ||
        text.find("fatal error") != std::string::npos || text.find("error MSB") != std::string::npos ||
        text.rfind("ERROR:", 0) == 0)
        return spdlog::level::err;
    if (text.find("warning:") != std::string::npos || text.find("warning C") != std::string::npos)
        return spdlog::level::warn;
    return spdlog::level::info;
}
std::filesystem::path Artifact(const ToolRequest &request)
{
    if (!request.Artifact.empty())
        return request.Artifact;
    for (size_t index = 0; index + 1 < request.Arguments.size(); ++index)
        if (request.Arguments[index] == "--output")
            return std::filesystem::u8path(request.Arguments[index + 1]);
    return {};
}
} // namespace
ProjectTools::~ProjectTools()
{
    Shutdown();
}
bool ProjectTools::Start(ToolRequest request)
{
    if (Busy() || m_ShuttingDown)
        return false;
    m_Request = std::move(request);
    m_State = std::make_shared<JobState>();
    m_State->Console = m_Console;
    m_State->ID = NextOperation++;
    if (m_Console)
    {
        ConsoleOperation op;
        op.ID = m_State->ID;
        op.Generation = m_Request.Generation;
        op.Label = m_Request.Label;
        op.Project = m_Request.Project.generic_u8string();
        op.SDK = m_Request.SDK.generic_u8string();
        op.Python = m_Request.Python.generic_u8string();
        std::ostringstream arguments;
        for (const auto &arg : m_Request.Arguments)
            arguments << std::quoted(arg) << ' ';
        op.Command = arguments.str(); // Display argv boundaries; execution never parses this string.
        op.Stage = "Locating Python";
        op.Start = std::chrono::system_clock::now();
        op.ClockStart = std::chrono::steady_clock::now();
        m_Console->Begin(std::move(op));
    }
    try
    {
        m_Job = std::async(std::launch::async,
                           [request = m_Request, state = m_State] { return Work(request, state); });
    }
    catch (const std::exception &error)
    {
        if (m_Console)
            m_Console->Finish(m_State->ID, ToolOutcome::Failed, -1, error.what());
        return false;
    }
    return true;
}
void ProjectTools::Cancel()
{
    if (m_State)
        m_State->Cancelled = true;
}
void ProjectTools::Finish(ToolReport &report)
{
    if (!m_Console)
        return;
    m_Console->Finish(report.Operation, report.Outcome, report.ExitCode, report.Output,
                      report.Success ? Artifact(report.Request).generic_u8string() : std::string{});
    m_Console->Append(ConsoleSource::Tool,
                      report.Success                              ? spdlog::level::info
                      : report.Outcome == ToolOutcome::Superseded ? spdlog::level::warn
                                                                  : spdlog::level::err,
                      report.Request.Label + " process: " + ConsoleModel::OutcomeName(report.Outcome),
                      report.Operation);
}
bool ProjectTools::Poll(ToolReport &report, const ToolOwner *owner)
{
    if (!m_Job.valid() || m_Job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return false;
    report = m_Job.get();
    const bool projectBound =
        !report.Request.Arguments.empty() &&
        (report.Request.Arguments[0] == "script-build" || report.Request.Arguments[0] == "editor-export" ||
         report.Request.Arguments[0] == "new-project");
    if (owner && projectBound &&
        (owner->Generation != report.Request.Generation || owner->Project != report.Request.Project))
    {
        report.Success = false;
        report.Outcome = ToolOutcome::Superseded;
        report.Output += "\nThe project/session changed. No editor follow-up was applied; inspect the "
                         "original project snapshot.";
    }
    Finish(report);
    return true;
}
void ProjectTools::Shutdown()
{
    m_ShuttingDown = true;
    if (!m_Job.valid())
        return;
    Cancel();
    auto report = m_Job.get();
    // Teardown consumes completion data without executing any editor actions.
    Finish(report);
    m_State.reset();
}
ToolReport ProjectTools::Execute(const ToolRequest &request,
                                 const std::function<void(const std::string &)> &output)
{
    auto state = std::make_shared<JobState>();
    return Work(request, state, output);
}
ToolReport ProjectTools::Work(const ToolRequest &request, const std::shared_ptr<JobState> &state,
                              const std::function<void(const std::string &)> &output)
{
    ToolReport report;
    report.Request = request;
    report.Operation = state->ID;
    const auto publish = [&](ConsoleSource source, spdlog::level::level_enum level, const std::string &text) {
        if (state->Console)
            state->Console->Append(source, level, text, state->ID);
    };
    const auto stage = [&](const std::string &text) {
        if (state->Console)
            state->Console->Stage(state->ID, text);
    };
    const auto cancelled = [&] { return state->Cancelled.load(); };
    std::string pending[2];
    const auto line = [&](int stream, std::string text) {
        if (!text.empty() && text.back() == '\r')
            text.pop_back();
        if (text.empty())
            return;
        if (text.rfind("+ ", 0) == 0)
            stage("Command: " +
                  text.substr(2, 480)); // Real canonical CLI child command, never guessed percentages.
        publish(stream ? ConsoleSource::Stderr : ConsoleSource::Stdout,
                stream && text.rfind("Hazel: ", 0) == 0 ? spdlog::level::err : Severity(text), text);
    };
    const auto chunks = [&](ProcessStream stream, const std::string &chunk) {
        if (output)
            output(chunk);
        auto &buffer = pending[static_cast<int>(stream)];
        buffer += chunk;
        size_t newline;
        while ((newline = buffer.find('\n')) != std::string::npos)
        {
            line(static_cast<int>(stream), buffer.substr(0, newline));
            buffer.erase(0, newline + 1);
        }
        // Bound an unterminated line, preserving UTF-8 boundaries across chunks.
        if (buffer.size() > 64 * 1024)
        {
            size_t cut = 64 * 1024;
            while (cut && (static_cast<unsigned char>(buffer[cut]) & 0xc0) == 0x80)
                --cut;
            line(static_cast<int>(stream), buffer.substr(0, cut) + " [continued long line]");
            buffer.erase(0, cut);
        }
    };
    try
    {
        if (cancelled())
        {
            report.Outcome = ToolOutcome::Cancelled;
            report.Output = "Cancelled before launch";
            return report;
        }
        stage("Validating SDK");
        const auto sdk = HazelSDK::Validate(request.SDK);
        stage("Locating Python");
        report.Python = Toolchain::DiscoverPython(
            request.Python, request.SDK.is_absolute() ? request.SDK : std::filesystem::path{});
        if (state->Console)
            state->Console->SetPython(state->ID, report.Python.Executable.generic_u8string(),
                                      report.Python.Source);
        report.Output = "Python: " + report.Python.Executable.generic_u8string() + " (" +
                        report.Python.Source + ", " + report.Python.Version + ")\n";
        publish(ConsoleSource::Tool, spdlog::level::info, report.Output);
        if (cancelled())
        {
            report.Outcome = ToolOutcome::Cancelled;
            report.Output += "Cancelled before tool launch";
            return report;
        }
        if (!report.Python)
        {
            report.Output += report.Python.Error;
            if (!sdk)
                report.Output += "\nSDK also unavailable: " + sdk.Diagnostic;
            publish(ConsoleSource::Tool, spdlog::level::err, report.Output);
            return report;
        }
        if (request.Arguments.empty())
        {
            report.Success = true;
            report.Outcome = ToolOutcome::Succeeded;
            report.ExitCode = 0;
            return report;
        }
        if (!sdk)
        {
            report.Output += std::string("SDK ") + sdk.Status() + ": " + sdk.Diagnostic + "\n" +
                             HazelSDK::SetupInstructions();
            publish(ConsoleSource::Tool, spdlog::level::err, report.Output);
            return report;
        }
        std::vector<std::string> args{"-X","utf8","-u", (request.SDK / "scripts/hazel.py").generic_u8string()};
        args.insert(args.end(), request.Arguments.begin(), request.Arguments.end());
        stage("Running " + request.Arguments.front());
        const auto result = Process::RunStreams(report.Python.Executable, args, request.SDK,
                                                std::chrono::minutes(60), {chunks, cancelled});
        for (int stream = 0; stream < 2; ++stream)
            if (!pending[stream].empty())
                line(stream, std::move(pending[stream]));
        report.ExitCode = result.ExitCode;
        report.Success = result.ExitCode == 0 && !result.TimedOut && !result.Cancelled;
        report.Outcome = result.Cancelled  ? ToolOutcome::Cancelled
                         : result.TimedOut ? ToolOutcome::TimedOut
                         : report.Success  ? ToolOutcome::Succeeded
                                           : ToolOutcome::Failed;
        report.Output += result.Output;
        if (result.DroppedBytes)
            report.Output += "\n[Process report dropped " + std::to_string(result.DroppedBytes) +
                             " earlier bytes; Console retention is separately bounded.]";
        if (result.TimedOut)
            report.Output += "\nTool exceeded its 60 minute limit; owned process tree terminated.";
        if (result.Cancelled)
            report.Output += "\nTool cancelled during editor teardown; owned process tree terminated.";
        if (!report.Success)
            publish(ConsoleSource::Tool, spdlog::level::err,
                    request.Label + ": " + ConsoleModel::OutcomeName(report.Outcome) + " (exit " +
                        std::to_string(report.ExitCode) + ")");
    }
    catch (const std::exception &error)
    {
        report.Output += error.what();
        publish(ConsoleSource::Tool, spdlog::level::err, error.what());
    }
    return report;
}
} // namespace Hazel
