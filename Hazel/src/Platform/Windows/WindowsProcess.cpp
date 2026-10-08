#include "hzpch.h"
#include "Hazel/Utils/Process.h"
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
ProcessResult Process::RunStreams(const std::filesystem::path &exe, const std::vector<std::string> &args,
                                  const std::filesystem::path &directory, std::chrono::milliseconds timeout,
                                  const ProcessCallbacks &callbacks)
{
    if (!exe.is_absolute())
        throw std::invalid_argument("Process executable must be absolute");
    std::wstring command = Quote(exe.wstring());
    for (const auto &arg : args)
        command += L" " + Quote(Wide(arg));
    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributes(attributeBytes);
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE reads[2]{}, writes[2]{}, input = nullptr;
    auto closePipes = [&] {
        for (auto handle : reads)
            if (handle)
                CloseHandle(handle);
        for (auto handle : writes)
            if (handle)
                CloseHandle(handle);
        if (input)
            CloseHandle(input);
    };
    for (int stream = 0; stream < 2; ++stream)
    {
        if (!CreatePipe(&reads[stream], &writes[stream], &security, 0) ||
            !SetHandleInformation(reads[stream], HANDLE_FLAG_INHERIT, 0))
        {
            closePipes();
            throw std::runtime_error("Cannot create process output pipes");
        }
    }
    input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE)
    {
        input = nullptr;
        closePipes();
        throw std::runtime_error("Cannot create process stdin");
    }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
    {
        if (job)
            CloseHandle(job);
        closePipes();
        throw std::runtime_error("Cannot create process job");
    }
    // Only stdin/stdout/stderr are inherited; debugger sockets and other editor handles stay private.
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    HANDLE handles[]{input, writes[0], writes[1]};
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes))
    {
        closePipes();
        CloseHandle(job);
        throw std::runtime_error("Cannot initialize child handle whitelist");
    }
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                   sizeof(handles), nullptr, nullptr))
    {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        closePipes();
        CloseHandle(job);
        throw std::runtime_error("Cannot restrict child handle inheritance");
    }
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = writes[0];
    startup.StartupInfo.hStdError = writes[1];
    startup.StartupInfo.hStdInput = input;
    PROCESS_INFORMATION process{};
    BOOL created =
        CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                       directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &process);
    auto launchError = GetLastError();
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    for (auto &handle : writes)
    {
        CloseHandle(handle);
        handle = nullptr;
    }
    CloseHandle(input);
    input = nullptr;
    if (!created)
    {
        closePipes();
        CloseHandle(job);
        return {-1, false, "Launch failed (Win32 " + std::to_string(launchError) + ")"};
    }
    if (!AssignProcessToJobObject(job, process.hProcess))
    {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        closePipes();
        CloseHandle(job);
        return {-1, false, "Cannot own process tree in a Windows job"};
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
    {
        TerminateJobObject(job, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        closePipes();
        CloseHandle(job);
        return {-1, false, "Cannot resume owned process"};
    }
    CloseHandle(process.hThread);
    ProcessResult result;
    auto start = std::chrono::steady_clock::now();
    auto drain = [&] {
        char data[4096];
        for (int pass = 0; pass < 8; ++pass)
            for (int stream = 0; stream < 2; ++stream)
            {
                DWORD available = 0, n = 0;
                if (!PeekNamedPipe(reads[stream], nullptr, 0, nullptr, &available, nullptr) || !available)
                    continue;
                if (!ReadFile(reads[stream], data, std::min<DWORD>(available, sizeof(data)), &n, nullptr) ||
                    !n)
                    continue;
                result.Output.append(data, n);
                if (result.Output.size() > 2 * 1024 * 1024)
                {
                    const auto dropped = result.Output.size() - 2 * 1024 * 1024;
                    result.DroppedBytes += dropped;
                    result.Output.erase(0, dropped);
                }
                if (callbacks.Output)
                    callbacks.Output(static_cast<ProcessStream>(stream), std::string(data, n));
            }
    };
    try
    {
        for (;;)
        {
            drain();
            if (WaitForSingleObject(process.hProcess, 15) == WAIT_OBJECT_0)
                break;
            const bool cancelled = callbacks.Cancel && callbacks.Cancel();
            if (cancelled || std::chrono::steady_clock::now() - start > timeout)
            {
                result.Cancelled = cancelled;
                result.TimedOut = !result.Cancelled;
                TerminateJobObject(job, 1);
                WaitForSingleObject(process.hProcess, INFINITE);
                break;
            }
        }
        TerminateJobObject(job, 1);
        for (int pass = 0; pass < 4; ++pass)
            drain();
    }
    catch (...)
    {
        TerminateJobObject(job, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hProcess);
        closePipes();
        CloseHandle(job);
        throw;
    }
    DWORD code;
    GetExitCodeProcess(process.hProcess, &code);
    result.ExitCode = static_cast<int>(code);
    CloseHandle(process.hProcess);
    closePipes();
    CloseHandle(job);
    return result;
}
} // namespace Hazel
