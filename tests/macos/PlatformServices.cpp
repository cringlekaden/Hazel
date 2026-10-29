// Focused regression fixture for Darwin process/atomic-publication/lease adapters.
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/FileLease.h"
#include "Hazel/Utils/Process.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

static void Check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) try {
    if(argc>1 && std::string(argv[1])=="child") {
        Check(argc==4 && std::string(argv[2])==u8"argument space é","Structured argv corrupted");
        Check(fcntl(std::stoi(argv[3]),F_GETFD)==-1,"Inherited parent descriptor");
        std::cout<<std::filesystem::current_path().string();std::cerr<<"stderr-marker";
        return 0;
    }
    if(argc>1 && std::string(argv[1])=="flood") {for(;;)std::cout<<std::string(4096,'x')<<std::flush;}
    char pattern[]="/tmp/Hazel services XXXXXXXX";
    auto* created=mkdtemp(pattern);Check(created,"mkdtemp failed");
    const std::filesystem::path root=created;
    struct Cleanup {std::filesystem::path Root;~Cleanup(){std::error_code e;std::filesystem::remove_all(Root,e);}} cleanup{root};
    auto lease=Hazel::FileLease::Try(root/"lock");Check(bool(lease),"First lease failed");
    Check(!Hazel::FileLease::Try(root/"lock"),"Concurrent lease accepted");
    lease.reset();Check(bool(Hazel::FileLease::Try(root/"lock")),"Lease not released");
    std::filesystem::create_directory(root/"source");std::filesystem::create_directory(root/"occupied");
    bool rejected=false;try{Hazel::FileSystem::PublishDirectoryNew(root/"source",root/"occupied");}catch(...){rejected=true;}
    Check(rejected && std::filesystem::exists(root/"source"),"Exclusive directory publication replaced existing target");
    Hazel::FileSystem::PublishDirectoryNew(root/"source",root/"published");
    Check(std::filesystem::is_directory(root/"published"),"Directory publication failed");
    int descriptor=open("/dev/null",O_RDONLY);Check(descriptor>=0,"Descriptor fixture failed");
    auto child=Hazel::Process::RunStreams(Hazel::FileSystem::GetExecutablePath(),{"child",u8"argument space é",std::to_string(descriptor)},root,std::chrono::seconds(5));
    close(descriptor);Check(child.ExitCode==0 && child.Output.find(root.string())!=std::string::npos && child.Output.find("stderr-marker")!=std::string::npos,"Process argv/cwd/streams/inheritance failed");
    auto timeout=Hazel::Process::RunStreams(Hazel::FileSystem::GetExecutablePath(),{"flood"},root,std::chrono::milliseconds(150));
    Check(timeout.TimedOut && timeout.Output.size()<=2*1024*1024,"Bounded capture/timeout failed");
    Hazel::ProcessCallbacks callbacks;callbacks.Cancel=[] {return true;};
    auto cancel=Hazel::Process::RunStreams(Hazel::FileSystem::GetExecutablePath(),{"flood"},root,std::chrono::seconds(5),callbacks);
    Check(cancel.Cancelled && !cancel.TimedOut,"Cancellation failed");
    std::cout<<"PASS: Darwin argv/cwd/stdout/stderr/descriptor closure, bounded flood/timeout/cancellation, exclusive publication and leases\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
