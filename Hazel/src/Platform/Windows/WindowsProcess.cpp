#include "Hazel/Utils/Process.h"
#include "hzpch.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
namespace Hazel
{
static std::wstring Wide(const std::string &s)
{
	return std::filesystem::u8path(s).wstring();
}
// CommandLineToArgvW / CRT compatible quoting, including trailing backslashes.
static std::wstring Quote(const std::wstring &s)
{
	std::wstring out = L"\"";
	size_t slashes = 0;
	for (auto c : s)
	{
		if (c == L'\\')
		{
			++slashes;
			continue;
		}
		out.append(slashes * (c == L'"' ? 2 : 1), L'\\');
		slashes = 0;
		if (c == L'"')
			out += L'\\';
		out += c;
	}
	out.append(slashes * 2, L'\\');
	return out + L'"';
}
bool Process::Launch(const std::filesystem::path &exe, const std::vector<std::string> &args)
{
	if (!exe.is_absolute())
		return false;
	std::wstring command = Quote(exe.wstring());
	for (auto &arg : args)
		command += L" " + Quote(Wide(arg));
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
						&process))
		return false;
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}
ProcessResult Process::Run(const std::filesystem::path &exe, const std::vector<std::string> &args,
						   const std::filesystem::path &directory, std::chrono::milliseconds timeout,
						   const std::function<void(const std::string &)> &onOutput)
{
	if (!exe.is_absolute())
		throw std::invalid_argument("Process executable must be absolute");
	SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
	HANDLE read = nullptr, write = nullptr;
	if (!CreatePipe(&read, &write, &security, 0))
		throw std::runtime_error("Cannot create process output pipe");
	SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
	HANDLE job = CreateJobObjectW(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
	{
		if (job)
			CloseHandle(job);
		CloseHandle(read);
		CloseHandle(write);
		throw std::runtime_error("Cannot create process job");
	}
	std::wstring command = Quote(exe.wstring());
	for (const auto &arg : args)
		command += L" " + Quote(Wide(arg));
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdOutput = startup.hStdError = write;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	PROCESS_INFORMATION process{};
	BOOL created = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
								  CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
								  directory.empty() ? nullptr : directory.c_str(), &startup, &process);
	auto launchError = GetLastError();
	CloseHandle(write);
	if (!created)
	{
		CloseHandle(read);
		CloseHandle(job);
		return {-1, false, "Launch failed (Win32 " + std::to_string(launchError) + ")"};
	}
	if (!AssignProcessToJobObject(job, process.hProcess))
	{
		TerminateProcess(process.hProcess, 1);
		WaitForSingleObject(process.hProcess, INFINITE);
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		CloseHandle(read);
		CloseHandle(job);
		return {-1, false, "Cannot own process tree in a Windows job"};
	}
	ResumeThread(process.hThread);
	CloseHandle(process.hThread);
	ProcessResult result;
	auto start = std::chrono::steady_clock::now();
	auto drain = [&] {
		DWORD available = 0, n = 0;
		char data[4096];
		while (PeekNamedPipe(read, nullptr, 0, nullptr, &available, nullptr) && available)
		{
			if (!ReadFile(read, data, std::min<DWORD>(available, sizeof(data)), &n, nullptr) || !n)
				break;
			result.Output.append(data, n);
			if (result.Output.size() > 2 * 1024 * 1024)
				result.Output.erase(0, result.Output.size() - 2 * 1024 * 1024);
			if (onOutput)
				onOutput(std::string(data, n));
		}
	};
	try
	{
		for (;;)
		{
			drain();
			if (WaitForSingleObject(process.hProcess, 15) == WAIT_OBJECT_0)
				break;
			if (std::chrono::steady_clock::now() - start > timeout)
			{
				result.TimedOut = true;
				TerminateJobObject(job, 1);
				WaitForSingleObject(process.hProcess, INFINITE);
				break;
			}
		}
		drain();
	}
	catch (...)
	{
		TerminateJobObject(job, 1);
		WaitForSingleObject(process.hProcess, INFINITE);
		CloseHandle(process.hProcess);
		CloseHandle(read);
		CloseHandle(job);
		throw;
	}
	DWORD code;
	GetExitCodeProcess(process.hProcess, &code);
	result.ExitCode = static_cast<int>(code);
	CloseHandle(process.hProcess);
	CloseHandle(read);
	CloseHandle(job);
	return result;
}
} // namespace Hazel
