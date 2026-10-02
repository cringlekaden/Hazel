//	MIT License
//
//	Copyright(c) 2017 Thomas Monkman
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files(the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions :
//
//	The above copyright notice and this permission notice shall be included in all
//	copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//	SOFTWARE.
#include "hzpch.h"
#include "Hazel/Utils/FileWatcher.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <array>
#include <cwchar>
#include <future>
#include <system_error>
#include <thread>
// The pinned MIT FileWatch.h's ReadDirectoryChangesW/overlapped/event algorithm
// is the foundation. This project-owned OS adapter closes BOTH event handles,
// joins before closing the directory, handles worker errors and uses native UTF-16.
namespace Hazel {
namespace {
    struct Handle {
        HANDLE Value = nullptr;
        ~Handle() { if (Value && Value!=INVALID_HANDLE_VALUE) CloseHandle(Value); }
        Handle() = default;
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
    };
    class WindowsFileWatcher final : public FileWatcher {
    public:
        WindowsFileWatcher(const std::filesystem::path& path, Callback callback)
            : m_Callback(std::move(callback)) {
            const auto absolute=std::filesystem::absolute(path);
            const bool file=std::filesystem::is_regular_file(absolute);
            const auto directory=file ? absolute.parent_path() : absolute;
            if (file) m_Filename=absolute.filename().native();
            m_Directory.Value=CreateFileW(directory.c_str(),FILE_LIST_DIRECTORY,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);
            if (m_Directory.Value==INVALID_HANDLE_VALUE) Throw("Open watched directory");
            m_Stop.Value=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if (!m_Stop.Value) Throw("Create watcher stop event");
            m_Changed.Value=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if (!m_Changed.Value) Throw("Create watcher change event");
            auto started=m_Started.get_future();
            m_Thread=std::thread([this] { Monitor(); });
            try { started.get(); }
            catch (...) { SetEvent(m_Stop.Value); m_Thread.join(); throw; }
        }
        ~WindowsFileWatcher() override {
            SetEvent(m_Stop.Value);
            if (m_Thread.joinable()) m_Thread.join();
        }
    private:
        static void Throw(const char* operation) { throw std::system_error(GetLastError(),std::system_category(),operation); }
        void Monitor() {
            // FILE_NOTIFY_INFORMATION requires aligned storage.
            alignas(FILE_NOTIFY_INFORMATION) std::array<unsigned char,64*1024> buffer{};
            OVERLAPPED operation{}; operation.hEvent=m_Changed.Value;
            bool pending=false, started=false;
            try {
                for (;;) {
                    ResetEvent(m_Changed.Value); DWORD bytes=0;
                    if (!ReadDirectoryChangesW(m_Directory.Value,buffer.data(),static_cast<DWORD>(buffer.size()),TRUE,
                        FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE,
                        &bytes,&operation,nullptr)) Throw("Read watched directory");
                    pending=true;
                    if (!started) { m_Started.set_value(); started=true; }
                    HANDLE events[]{m_Stop.Value,m_Changed.Value};
                    const DWORD result=WaitForMultipleObjects(2,events,FALSE,INFINITE);
                    if (result==WAIT_OBJECT_0) break;
                    if (result!=WAIT_OBJECT_0+1) Throw("Wait for watched directory");
                    if (!GetOverlappedResult(m_Directory.Value,&operation,&bytes,TRUE)) Throw("Read file notification");
                    pending=false;
                    if (!bytes) { // Buffer overflow: tell a file consumer to re-check its target.
                        if (!m_Filename.empty()) m_Callback(std::filesystem::path(m_Filename),FileWatchEvent::modified);
                        continue;
                    }
                    auto* notification=reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
                    for (;;) {
                        std::wstring name(notification->FileName,notification->FileNameLength/sizeof(wchar_t));
                        if (m_Filename.empty() || _wcsicmp(name.c_str(),m_Filename.c_str())==0) {
                            FileWatchEvent event;
                            switch(notification->Action) {
                                case FILE_ACTION_ADDED: event=FileWatchEvent::added; break;
                                case FILE_ACTION_REMOVED: event=FileWatchEvent::removed; break;
                                case FILE_ACTION_MODIFIED: event=FileWatchEvent::modified; break;
                                case FILE_ACTION_RENAMED_OLD_NAME: event=FileWatchEvent::renamed_old; break;
                                case FILE_ACTION_RENAMED_NEW_NAME: event=FileWatchEvent::renamed_new; break;
                                default: goto next_notification;
                            }
                            m_Callback(std::filesystem::path(name),event);
                        }
next_notification:
                        if (!notification->NextEntryOffset) break;
                        notification=reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                            reinterpret_cast<unsigned char*>(notification)+notification->NextEntryOffset);
                    }
                }
            } catch (const std::exception& error) {
                if (!started) m_Started.set_exception(std::current_exception());
                else HZ_CORE_ERROR("File watcher: {}",error.what());
            }
            if (pending) {
                CancelIoEx(m_Directory.Value,&operation);
                DWORD ignored=0; GetOverlappedResult(m_Directory.Value,&operation,&ignored,TRUE);
            }
        }
        Callback m_Callback;
        std::wstring m_Filename;
        Handle m_Directory, m_Stop, m_Changed;
        std::promise<void> m_Started;
        std::thread m_Thread;
    };
}
    Scope<FileWatcher> CreateWindowsFileWatcher(const std::filesystem::path& path, FileWatcher::Callback callback) {
        return CreateScope<WindowsFileWatcher>(path,std::move(callback));
    }
}
