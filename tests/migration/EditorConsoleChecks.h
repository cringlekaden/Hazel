// Logical checks use the production model, observer, process runner and tool worker.
// No pixel driver and no special production branches for tests.
#include "Authoring/ConsoleModel.h"
#include <atomic>
namespace Hazel
{
static void EditorConsoleChecks(const std::filesystem::path &directory)
{
    ConsoleModel::Limits limits;
    limits.MessageBytes = 256;
    limits.InboxBytes = 4096;
    limits.InboxEntries = 8;
    limits.RetainedBytes = 4096;
    limits.RetainedEntries = 4;
    limits.Operations = 2;
    auto model = std::make_shared<ConsoleModel>(limits);
    model->Receive({spdlog::level::info, std::chrono::system_clock::now(), "HAZEL", "", "core é"});
    model->Receive({spdlog::level::warn, std::chrono::system_clock::now(), "APP", "", "application"});
    model->Receive({spdlog::level::err, std::chrono::system_clock::now(), "APP", "Managed",
                    "managed\nsecond line %n ##literal"});
    model->Receive({spdlog::level::info, std::chrono::system_clock::now(), "APP", "Authoring", "authoring"});
    model->Pump();
    Check(model->Entries().size() == 4 && model->Entries()[0].Source == ConsoleSource::Core &&
              model->Entries()[1].Source == ConsoleSource::App &&
              model->Entries()[2].Source == ConsoleSource::Managed &&
              model->Entries()[3].Source == ConsoleSource::Authoring,
          "Mixed native sources were misclassified");
    for (size_t i = 1; i < model->Entries().size(); ++i)
        Check(model->Entries()[i].Sequence > model->Entries()[i - 1].Sequence,
              "Console ingestion reordered sequences");
    ConsoleFilter filter;
    filter.Sources = 1u << static_cast<unsigned>(ConsoleSource::Managed);
    filter.Search = "SECOND";
    const auto text = model->Copy(filter);
    Check(model->Filtered(filter).size() == 1 && text.find("[Managed] [error]") != std::string::npos &&
              text.find("second line %n ##literal") != std::string::npos &&
              text.find("application") == std::string::npos,
          "Filter/copy lost multiline or included hidden records");
    filter.Severities = 1u << spdlog::level::info;
    Check(model->Filtered(filter).empty(), "Severity filter ignored level");
    Check(model->Copy({}, {model->Entries()[1].Sequence}).find("core é") == std::string::npos,
          "Selected copy included other rows");
    const auto sequence = model->Entries().back().Sequence;
    ConsoleOperation op;
    op.ID = 1;
    op.Label = "Failed build";
    op.Start = std::chrono::system_clock::now();
    op.ClockStart = std::chrono::steady_clock::now();
    model->Begin(op);
    model->Stage(1, "Compiler command");
    model->Append(ConsoleSource::Stderr, spdlog::level::err, "error: failure", 1);
    for (int i = 0; i < 30; ++i)
        model->Append(ConsoleSource::Stdout, spdlog::level::info, std::string(400, 'x'), 1);
    Check(model->Dropped() == 23 && model->Truncated() == 30,
          "Inbox retention/truncation accounting was not bounded");
    model->Pump(2);
    Check(model->Entries().size() == 4 && model->Dropped() == 25, "Pump did not obey batch/retention limits");
    model->Finish(1, ToolOutcome::Failed, 7, "Failure survives messages");
    model->Finish(1,ToolOutcome::Failed,7,std::string(90*1024,'n')+"\nLast compiler failure");
    Check(model->LatestFailure()->Diagnostic.find("Last compiler failure")!=std::string::npos&&
          model->LatestFailure()->Diagnostic.size()<65*1024,"Bounded operation detail lost the final compiler diagnostic");
    model->Clear();
    Check(model->Entries().empty() && model->Dropped() == 0 && model->Errors() == 0 &&
              model->LatestFailure() && model->Operations()[0].ExitCode == 7,
          "Clear lost structured failure or retained queued records");
    model->Append(ConsoleSource::Stdout, spdlog::level::info, "after clear", 3);
    model->Pump();
    Check(model->Entries()[0].Sequence > sequence, "Clear reused message identities");
    for (uint64_t id = 2; id <= 4; ++id)
    {
        op.ID = id;
        op.Label = "Later success";
        model->Begin(op);
        model->Finish(id, ToolOutcome::Succeeded, 0);
    }
    Check(model->Operations().size() == 2 && model->LatestFailure()->ID == 1,
          "History eviction/new success hid pinned failed result");
    model->DismissFailure();
    Check(!model->HasFailure(), "Failure dismissal did not apply");
    model->Stop();
    model->Append(ConsoleSource::Tool, spdlog::level::err, "after teardown");
    model->Pump();
    Check(model->Entries().size() == 1, "Stopped receiver accepted records");

    {
        auto bridge = std::make_shared<ConsoleModel>();
        auto subscription = Log::Observe(bridge);
        HZ_CORE_INFO("Console core fixture");
        HZ_INFO("Console application fixture");
        Log::GetClientLogger()->log(spdlog::source_loc{"Managed", 0, ""}, spdlog::level::info,
                                    "managed bridge");
        Log::GetClientLogger()->log(spdlog::source_loc{"Authoring", 0, ""}, spdlog::level::info,
                                    "authoring bridge");
        bridge->Pump();
        Check(bridge->Entries().size() == 4 && bridge->Entries()[0].Source == ConsoleSource::Core &&
                  bridge->Entries()[1].Source == ConsoleSource::App &&
                  bridge->Entries()[2].Source == ConsoleSource::Managed &&
                  bridge->Entries()[3].Source == ConsoleSource::Authoring,
              "Actual spdlog relay lost source/order metadata");
        bridge->CaptureLevel = spdlog::level::debug;
        Log::SetObserverLevel(bridge.get(), spdlog::level::debug);
        const auto payload = std::string(64 * 1024 - 33, 'x') + u8"🚀" + std::string(100, 'y');
        Log::GetCoreLogger()->debug("{}", payload);
        bridge->Pump();
        Check(bridge->Truncated() == 1 &&
                  bridge->Entries().back().Message.find("truncated") != std::string::npos &&
                  std::all_of(bridge->Entries().back().Message.begin(),
                              bridge->Entries().back().Message.end(),
                              [](unsigned char c) { return c < 128; }),
              "Native log relay failed UTF-8-safe truncation/counting or capture admission");
        subscription.Reset();
    }

    struct Receiver final : LogReceiver
    {
        std::atomic<unsigned> Count{0};
        void Receive(const LogRecord &) override
        {
            ++Count;
            HZ_CORE_TRACE("nested observer record");
        }
    };
    const auto coreLevel = Log::GetCoreLogger()->level();
    auto receiver = std::make_shared<Receiver>();
    auto subscription = Log::Observe(receiver, spdlog::level::trace);
    std::atomic<bool> running{true};
    std::thread producer([&] {
        while (running)
        {
            HZ_CORE_TRACE("producer");
            std::this_thread::yield();
        }
    });
    for (int i = 0; i < 1000 && !receiver->Count; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    subscription.Reset();
    const auto delivered = receiver->Count.load();
    HZ_CORE_TRACE("after removal");
    running = false;
    producer.join();
    Check(delivered > 0 && receiver->Count == delivered && Log::GetCoreLogger()->level() == coreLevel,
          "Observer teardown raced callback, recursed or changed normal logging admission");
    auto weak = Log::Observe(receiver);
    receiver.reset();
    HZ_CORE_INFO("Expired observer is safe");
    weak.Reset();

    const auto python = Toolchain::DiscoverPython();
    Check(bool(python), "Console fixture needs the established Python interpreter");
    const auto executable = FileSystem::GetExecutablePath();
    std::string streams[2];
    auto result = Process::RunStreams(
        executable, {"--console-stream-probe"}, directory, std::chrono::seconds(5),
        {[&](ProcessStream stream, const auto &chunk) { streams[static_cast<int>(stream)] += chunk; }, {}});
    Check(result.ExitCode == 0 && streams[0].find("stdout é") != std::string::npos &&
              streams[1].find("stderr warning") != std::string::npos,
          "Process runner merged/lost stdout and stderr source identity");
    result = Process::RunStreams(
        python.Executable,
        {"-c", "import sys,time; sys.stdout.write('begin\\n'); sys.stdout.flush(); time.sleep(3)"}, directory,
        std::chrono::milliseconds(80));
    Check(result.TimedOut && !result.Cancelled && result.ExitCode != 0,
          "Timed-out process reported success or was not reaped");
    result = Process::RunStreams(
        python.Executable, {"-c", "import sys;\nwhile True: sys.stdout.write('x'*8192); sys.stdout.flush()"},
        directory, std::chrono::milliseconds(80));
    Check(result.TimedOut && result.Output.size() <= 2 * 1024 * 1024,
          "Noisy pipe starved process deadline or exceeded output bound");
    bool callbackThrew = false;
    try
    {
        Process::RunStreams(
            python.Executable, {"-c", "import sys,time; print('ready',flush=True); time.sleep(3)"}, directory,
            std::chrono::seconds(5),
            {[](ProcessStream, const auto &) { throw std::runtime_error("controlled consumer exception"); },
             {}});
    }
    catch (const std::runtime_error &)
    {
        callbackThrew = true;
    }
    Check(callbackThrew, "Process callback exception was lost instead of terminating owned children");

    // SDKChecks prepares the canonical directory/pins contract. Only its fixture CLI changes.
    const auto sdk = directory / std::filesystem::u8path(u8"SDK space é");
    FileSystem::WriteFileAtomically(sdk / "scripts/hazel.py", [](auto &out) {
        out << R"PY(import sys, pathlib, time, subprocess
mode=sys.argv[2]
print('+ fixture compiler',flush=True)
print('fixture stdout é',flush=True)
print('warning: fixture stderr',file=sys.stderr,flush=True)
if mode=='fail':
    print('error: deterministic failure',file=sys.stderr,flush=True)
    sys.exit(7)
if mode=='wait':
    marker=sys.argv[3]
    subprocess.Popen([sys.executable,'-c','import time,pathlib,sys; time.sleep(0.7); pathlib.Path(sys.argv[1]).write_text("orphan")',marker])
    pathlib.Path(marker+'.ready').write_text('ready')
    time.sleep(3)
for i in range(80): print('noise '+str(i),flush=True)
)PY";
    });
    auto console = std::make_shared<ConsoleModel>(limits);
    ProjectTools tools;
    tools.SetConsole(console);
    ToolRequest request{python.Executable,
                        sdk,
                        directory / "AuthoringProject/Authoring.hproj",
                        {"script-build", "success"},
                        "Fixture build"};
    request.Generation = 23;
    request.Completion = ToolCompletion::ReloadScripts;
    request.Artifact = directory / "artifact é";
    const ToolOwner owner{request.Generation, request.Project};
    auto poll = [&](const ToolOwner &target) {
        ToolReport report;
        bool complete = false;
        for (int i = 0; i < 1000 && !complete; ++i)
        {
            console->Pump();
            complete = tools.Poll(report, &target);
            if (!complete)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        Check(complete, "Tool fixture did not complete in bounded time");
        return report;
    };
    Check(tools.Start(request), "Tool fixture failed to start");
    Check(!tools.Start(request), "Concurrent tool request bypassed ownership");
    auto report = poll(owner);
    console->Pump();
    Check(report.Success && report.Request.Completion == ToolCompletion::ReloadScripts &&
              console->Dropped() > 0 && console->Operations().back().Outcome == ToolOutcome::Succeeded &&
              console->Operations().back().Artifact == request.Artifact.generic_u8string(),
          "Log truncation lost completion or request/artifact snapshot");
    Check(console->Operations().back().Diagnostic.find(u8"fixture stdout é")!=std::string::npos,
          "Canonical Python stdout did not preserve UTF-8 tool diagnostics");
    const auto output=directory/std::filesystem::u8path(u8"export output é");
    request.Artifact.clear();request.Arguments={"editor-export","success","--output",output.generic_u8string(),"--name","PackageNameIsNotTheOutput"};
    Check(tools.Start(request),"Export snapshot fixture did not start");report=poll(owner);
    Check(report.Success&&console->Operations().back().Artifact==output.generic_u8string(),
          "Export artifact used the last argument instead of the captured output destination");
    request.Arguments={"script-build","fail"};
    Check(tools.Start(request), "Failed-tool fixture did not start");
    report = poll(owner);
    Check(!report.Success && report.ExitCode == 7 && console->LatestFailure() &&
              console->LatestFailure()->Diagnostic.find("deterministic failure") != std::string::npos,
          "Failed tool lost result/exit diagnostics");
    console->Pump();
    ConsoleFilter stderrFilter;
    stderrFilter.Sources = 1u << static_cast<unsigned>(ConsoleSource::Stderr);
    stderrFilter.Severities = 1u << spdlog::level::err;
    Check(!console->Filtered(stderrFilter).empty(),
          "Production tool stream lost compiler stderr severity/source");
    request.Arguments[1] = "success";
    Check(tools.Start(request), "Replacement fixture failed to start");
    auto replacement = owner;
    ++replacement.Generation;
    report = poll(replacement);
    Check(!report.Success && report.Outcome == ToolOutcome::Superseded &&
              report.Request.Project == owner.Project,
          "Project replacement accepted stale successful completion");
    Check(tools.Start(request), "Foreign-project fixture failed to start");
    replacement = owner;
    replacement.Project = directory / "Other.hproj";
    report = poll(replacement);
    Check(report.Outcome == ToolOutcome::Superseded,
          "A different project accepted a completion with the same generation");
    const auto marker = directory / "console orphan marker";
    auto ready = marker;
    ready += ".ready";
    request.Arguments = {"script-build", "wait", marker.generic_u8string()};
    Check(tools.Start(request), "Cancellation fixture failed to start");
    for (int i = 0; i < 1000 && !std::filesystem::exists(ready); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Check(std::filesystem::exists(ready), "Owned child fixture did not launch");
    tools.Cancel();
    report = poll(owner);
    Check(!report.Success && report.Outcome == ToolOutcome::Cancelled, "Cancelled tool reported success");
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    Check(!std::filesystem::exists(marker), "Cancellation left an owned descendant running");
    std::filesystem::remove(ready);
    Check(tools.Start(request), "Shutdown fixture did not start");
    for (int i = 0; i < 1000 && !std::filesystem::exists(ready); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Check(std::filesystem::exists(ready), "Shutdown child fixture did not launch");
    tools.Shutdown();
    console->Clear();
    Check(!tools.Busy() && !tools.Start(request) &&
              console->LatestFailure()->Outcome == ToolOutcome::Cancelled,
          "Shutdown lost completion, admitted a job or left a worker running");
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    Check(!std::filesystem::exists(marker), "Shutdown left an owned descendant running");
    std::cout << "PASS: Console mixed sources/order/filter/copy/bounds/Clear, failure retention, observer "
                 "recursion/teardown, dual process streams/timeouts, tool completion/truncation/project "
                 "lifetime/cancel/shutdown\n";
}
} // namespace Hazel
