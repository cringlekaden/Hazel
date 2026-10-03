#pragma once
#include "Hazel/Utils/Toolchain.h"
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
namespace Hazel
{
struct ToolRequest
{
	std::filesystem::path Python, SDK, Project;
	std::vector<std::string> Arguments;
	std::string Label;
};
struct ToolReport
{
	bool Success = false;
	PythonSelection Python;
	std::string Output;
};
// Main-thread owner; workers receive only immutable paths/argv, never scenes or ImGui.
class ProjectTools
{
  public:
	~ProjectTools();
	bool Start(ToolRequest request);
	bool Poll(ToolReport &report);
	bool Busy() const
	{
		return m_Job.valid();
	}
	const ToolRequest &Request() const
	{
		return m_Request;
	}
	static ToolReport Execute(const ToolRequest &request,
							  const std::function<void(const std::string &)> &onOutput = {});
	bool ReadProgress(std::string &output);

  private:
	struct Progress
	{
		std::mutex Mutex;
		std::string Output;
		bool Changed = false;
	};
	std::shared_ptr<Progress> m_Progress;
	ToolRequest m_Request;
	std::future<ToolReport> m_Job;
};
} // namespace Hazel
