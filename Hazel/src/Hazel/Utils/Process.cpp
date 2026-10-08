#include "hzpch.h"
#include "Process.h"
namespace Hazel
{
ProcessResult Process::Run(const std::filesystem::path &exe, const std::vector<std::string> &args,
                           const std::filesystem::path &directory, std::chrono::milliseconds timeout,
                           const std::function<void(const std::string &)> &output)
{
    ProcessCallbacks callbacks;
    if (output)
        callbacks.Output = [output](ProcessStream, const std::string &chunk) { output(chunk); };
    return RunStreams(exe, args, directory, timeout, callbacks);
}
} // namespace Hazel
