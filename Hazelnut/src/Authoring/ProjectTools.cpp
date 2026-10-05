#include "ProjectTools.h"
#include "HazelSDK.h"
#include "Hazel/Utils/Process.h"
namespace Hazel
{
ProjectTools::~ProjectTools()
{
	if (m_Job.valid())
		m_Job.wait();
}
bool ProjectTools::Start(ToolRequest request)
{
	if (Busy())
		return false;
	m_Request = std::move(request);
	m_Progress = std::make_shared<Progress>();
	m_Job = std::async(std::launch::async, [request = m_Request, progress = m_Progress] {
		return Execute(request, [progress](const std::string &chunk) {
			std::lock_guard<std::mutex> lock(progress->Mutex);
			progress->Output += chunk;
			if (progress->Output.size() > 256 * 1024)
				progress->Output.erase(0, progress->Output.size() - 256 * 1024);
			progress->Changed = true;
		});
	});
	return true;
}
bool ProjectTools::Poll(ToolReport &report)
{
	if (!m_Job.valid() || m_Job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return false;
	report = m_Job.get();
	return true;
}
bool ProjectTools::ReadProgress(std::string &output)
{
	if (!m_Progress)
		return false;
	std::lock_guard<std::mutex> lock(m_Progress->Mutex);
	if (!m_Progress->Changed)
		return false;
	output = m_Progress->Output;
	m_Progress->Changed = false;
	return true;
}
ToolReport ProjectTools::Execute(const ToolRequest &request,
								 const std::function<void(const std::string &)> &onOutput)
{
	ToolReport report;
	try
	{
		const auto sdk = HazelSDK::Validate(request.SDK);
		report.Python = Toolchain::DiscoverPython(request.Python,
			request.SDK.is_absolute() ? request.SDK : std::filesystem::path{});
		report.Output = "Python: " + report.Python.Executable.generic_u8string() + " (" +
						report.Python.Source + ", " + report.Python.Version + ")\n";
		if (!report.Python)
		{
			report.Output += report.Python.Error;
			if (!sdk)
				report.Output += "\nSDK also unavailable: " + sdk.Diagnostic +
					" Configure its location separately in Editor Preferences.";
			return report;
		}
		if (request.Arguments.empty())
		{
			report.Success = true;
			return report;
		}
		if (!sdk)
		{
			report.Output += std::string("SDK ") + sdk.Status() + ": " + sdk.Diagnostic + "\n" +
				HazelSDK::SetupInstructions();
			return report;
		}
		std::vector<std::string> args = {"-u", (request.SDK / "scripts/hazel.py").generic_u8string()};
		args.insert(args.end(), request.Arguments.begin(), request.Arguments.end());
		auto result =
			Process::Run(report.Python.Executable, args, request.SDK, std::chrono::minutes(60), onOutput);
		report.Success = result.ExitCode == 0 && !result.TimedOut;
		report.Output += result.Output;
		if (result.TimedOut)
			report.Output += "\nTool operation exceeded its 60 minute limit.";
	}
	catch (const std::exception &error)
	{
		report.Output += error.what();
	}
	return report;
}
} // namespace Hazel
