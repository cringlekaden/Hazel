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
#include <sys/inotify.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>
#include <array>
#include <cerrno>
#include <system_error>
#include <thread>
// Foundation: pinned MIT FileWatch.h's directory inotify watch, filename filter
// and event decoding. Local adaptation adds atomic renames, close/overflow
// notifications, nonblocking reads and a stop descriptor joined before closing.
namespace Hazel {
namespace {
    struct Descriptor {
        int Value=-1;
        ~Descriptor() { if(Value>=0) close(Value); }
        Descriptor()=default;
        Descriptor(const Descriptor&)=delete;
        Descriptor& operator=(const Descriptor&)=delete;
    };
    class LinuxFileWatcher final : public FileWatcher {
    public:
        LinuxFileWatcher(const std::filesystem::path& path,Callback callback)
            : m_Callback(std::move(callback)) {
            const auto absolute=std::filesystem::absolute(path);
            const bool file=std::filesystem::is_regular_file(absolute);
            const auto directory=file ? absolute.parent_path() : absolute;
            if(file) m_Filename=absolute.filename();
            m_Notifications.Value=inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
            if(m_Notifications.Value<0) Throw("Create file notifications");
            m_Watch=inotify_add_watch(m_Notifications.Value,directory.c_str(),
                IN_MODIFY|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO|IN_DELETE_SELF|IN_MOVE_SELF);
            if(m_Watch<0) Throw("Watch directory");
            m_Stop.Value=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);
            if(m_Stop.Value<0) Throw("Create watcher stop descriptor");
            m_Thread=std::thread([this] { Monitor(); });
        }
        ~LinuxFileWatcher() override {
            const uint64_t stop=1;
            while(write(m_Stop.Value,&stop,sizeof(stop))<0 && errno==EINTR) {}
            if(m_Thread.joinable()) m_Thread.join();
            inotify_rm_watch(m_Notifications.Value,m_Watch);
        }
    private:
        static void Throw(const char* operation) { throw std::system_error(errno,std::system_category(),operation); }
        void Monitor() {
            alignas(inotify_event) std::array<unsigned char,64*1024> buffer{};
            try {
                pollfd descriptors[]{{m_Stop.Value,POLLIN,0},{m_Notifications.Value,POLLIN,0}};
                for(;;) {
                    const int result=poll(descriptors,2,-1);
                    if(result<0) { if(errno==EINTR) continue; Throw("Wait for file notification"); }
                    if(descriptors[0].revents) return;
                    if(descriptors[1].revents&(POLLERR|POLLHUP|POLLNVAL)) throw std::runtime_error("File notification descriptor failed");
                    if(!(descriptors[1].revents&POLLIN)) continue;
                    const auto length=read(m_Notifications.Value,buffer.data(),buffer.size());
                    if(length<0) { if(errno==EINTR || errno==EAGAIN) continue; Throw("Read file notification"); }
                    for(size_t offset=0;offset<static_cast<size_t>(length);) {
                        const auto* event=reinterpret_cast<const inotify_event*>(buffer.data()+offset);
                        if(event->mask&IN_Q_OVERFLOW) {
                            if(!m_Filename.empty()) m_Callback(m_Filename,FileWatchEvent::modified);
                        } else if(event->mask&(IN_DELETE_SELF|IN_MOVE_SELF|IN_IGNORED)) {
                            throw std::runtime_error("Watched directory moved or removed");
                        } else if(event->len) {
                            const auto changed=std::filesystem::u8path(event->name);
                            if(m_Filename.empty() || changed==m_Filename) {
                                if(event->mask&IN_MOVED_FROM) m_Callback(changed,FileWatchEvent::renamed_old);
                                else if(event->mask&IN_MOVED_TO) m_Callback(changed,FileWatchEvent::renamed_new);
                                else if(event->mask&IN_CREATE) m_Callback(changed,FileWatchEvent::added);
                                else if(event->mask&IN_DELETE) m_Callback(changed,FileWatchEvent::removed);
                                else if(event->mask&(IN_MODIFY|IN_CLOSE_WRITE)) m_Callback(changed,FileWatchEvent::modified);
                            }
                        }
                        offset+=sizeof(inotify_event)+event->len;
                    }
                }
            } catch(const std::exception& error) { HZ_CORE_ERROR("File watcher: {}",error.what()); }
        }
        Callback m_Callback;
        std::filesystem::path m_Filename;
        Descriptor m_Notifications,m_Stop;
        int m_Watch=-1;
        std::thread m_Thread;
    };
}
    Scope<FileWatcher> CreateLinuxFileWatcher(const std::filesystem::path& path,FileWatcher::Callback callback) {
        return CreateScope<LinuxFileWatcher>(path,std::move(callback));
    }
}
