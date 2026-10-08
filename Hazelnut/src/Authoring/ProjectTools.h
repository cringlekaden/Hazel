#pragma once
#include "Hazel/Utils/Toolchain.h"
#include "ConsoleModel.h"
#include <future>
#include <functional>
namespace Hazel
{
enum class ToolCompletion
{
    None,
    ReloadScripts,
    OpenCreatedProject
};
struct ToolRequest
{
    std::filesystem::path Python, SDK, Project;
    std::vector<std::string> Arguments;
    std::string Label;
    uint64_t Generation = 0;
    ToolCompletion Completion = ToolCompletion::None;
    std::filesystem::path Target{}, Artifact{};
};
struct ToolOwner
{
    uint64_t Generation = 0;
    std::filesystem::path Project;
};
struct ToolReport
{
    bool Success = false;
    PythonSelection Python;
    std::string Output;
    uint64_t Operation = 0;
    ToolOutcome Outcome = ToolOutcome::Failed;
    int ExitCode = -1;
    ToolRequest Request;
};
// Owner polls immutable reports. Workers capture only request/data, never editor closures.
class ProjectTools
{
  public:
    ~ProjectTools();
    void SetConsole(std::shared_ptr<ConsoleModel> console)
    {
        m_Console = std::move(console);
    }
    bool Start(ToolRequest request);
    bool Poll(ToolReport &report, const ToolOwner *owner = nullptr);
    void Shutdown(); // Out-of-band teardown cancels owned children and joins; no follow-up application.
    void Cancel();   // Internal lifecycle API; interactive compiler cancellation is deliberately not exposed.
    bool Busy() const
    {
        return m_Job.valid();
    }
    const ToolRequest &Request() const
    {
        return m_Request;
    }
    static ToolReport Execute(const ToolRequest &request,
                              const std::function<void(const std::string &)> &output = {});

  private:
    struct JobState
    {
        std::atomic<bool> Cancelled{false};
        std::shared_ptr<ConsoleModel> Console;
        uint64_t ID = 0;
    };
    static ToolReport Work(const ToolRequest &request, const std::shared_ptr<JobState> &state,
                           const std::function<void(const std::string &)> &output = {});
    void Finish(ToolReport &report);
    std::shared_ptr<ConsoleModel> m_Console;
    std::shared_ptr<JobState> m_State;
    ToolRequest m_Request;
    std::future<ToolReport> m_Job;
    bool m_ShuttingDown = false;
};
} // namespace Hazel
