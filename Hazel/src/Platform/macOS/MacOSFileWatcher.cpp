#include "hzpch.h"
#include "Hazel/Utils/FileWatcher.h"
#include <sys/event.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <map>
#include <system_error>

namespace Hazel {
namespace {
class MacOSFileWatcher final : public FileWatcher {
    using Stamp=std::pair<std::filesystem::file_time_type,uintmax_t>;
    using Snapshot=std::map<std::filesystem::path,Stamp>;
    int m_Queue=-1,m_Directory=-1;
    std::filesystem::path m_Path,m_Name;
    Callback m_Callback;
    std::thread m_Thread;
    std::atomic<bool> m_Stop{false};
    Snapshot Scan() {
        Snapshot result;
        for(const auto& entry:std::filesystem::directory_iterator(m_Path)) {
            if(!m_Name.empty() && entry.path().filename()!=m_Name) continue;
            result.emplace(entry.path().filename(),Stamp{entry.last_write_time(),entry.is_regular_file()?entry.file_size():0});
        }
        return result;
    }
public:
    MacOSFileWatcher(const std::filesystem::path& path,Callback callback):m_Callback(std::move(callback)) {
        auto absolute=std::filesystem::absolute(path);
        if(std::filesystem::is_directory(absolute)) m_Path=absolute;
        else { m_Path=absolute.parent_path(); m_Name=absolute.filename(); }
        m_Directory=open(m_Path.c_str(),O_EVTONLY|O_CLOEXEC);
        m_Queue=kqueue();
        if(m_Directory<0 || m_Queue<0) { if(m_Directory>=0)close(m_Directory);if(m_Queue>=0)close(m_Queue);throw std::system_error(errno,std::generic_category(),"Create macOS watcher"); }
        struct kevent event;
        EV_SET(&event,m_Directory,EVFILT_VNODE,EV_ADD|EV_CLEAR,NOTE_WRITE|NOTE_RENAME|NOTE_DELETE,0,nullptr);
        if(kevent(m_Queue,&event,1,nullptr,0,nullptr)<0) {close(m_Directory);close(m_Queue);throw std::runtime_error("Register macOS watcher");}
        auto snapshot=Scan();
        m_Thread=std::thread([this,previous=std::move(snapshot)]() mutable {
            try { while(!m_Stop) {
                struct kevent event; timespec timeout{0,100000000};
                const int count=kevent(m_Queue,nullptr,0,&event,1,&timeout);
                if(count<0) {if(errno==EINTR)continue;throw std::runtime_error("Wait for macOS file event");}
                // Directory notifications miss in-place child writes; bounded scans
                // cover those as well as atomic replacement and rename publication.
                auto current=Scan();
                for(const auto& [name,stamp]:current) {
                    auto old=previous.find(name);
                    if(old==previous.end())m_Callback(name,FileWatchEvent::added);
                    else if(old->second!=stamp)m_Callback(name,FileWatchEvent::modified);
                }
                for(const auto& [name,stamp]:previous)if(!current.count(name))m_Callback(name,FileWatchEvent::removed);
                previous=std::move(current);
            }} catch(const std::exception& error) { HZ_CORE_ERROR("File watcher: {}",error.what()); }
        });
    }
    ~MacOSFileWatcher() override { m_Stop=true;if(m_Thread.joinable())m_Thread.join();close(m_Directory);close(m_Queue); }
};
}
Scope<FileWatcher> CreateMacOSFileWatcher(const std::filesystem::path& path,FileWatcher::Callback callback) {return CreateScope<MacOSFileWatcher>(path,std::move(callback));}
}
