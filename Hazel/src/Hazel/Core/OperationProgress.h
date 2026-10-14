#pragma once
#include <functional>
#include <string>
#include <future>
#include <chrono>
namespace Hazel {
// Optional main-thread progress observer. CPU-only immutable parsing can run on
// one worker while its caller presents/polls. Registry, GPU and Mono work stays
// on the owning thread. No observer means the existing synchronous contract.
class OperationProgress {
public:
    using Observer=std::function<void(const std::string&,size_t,size_t)>;
    class Scope {
        Observer Previous;
    public:
        explicit Scope(Observer observer):Previous(std::move(Current)){Current=std::move(observer);}
        ~Scope(){Current=std::move(Previous);}
    };
    static void Report(const std::string& operation,size_t done=0,size_t total=0){if(Current)Current(operation,done,total);}
    template<class F> static auto CPU(const std::string& operation,F compute) {
        Report(operation);
        if(!Current)return compute();
        auto task=std::async(std::launch::async,std::move(compute));
        // Always join before propagating cancellation/errors; parser memory never
        // outlives its document operation. Rendering can continue while waiting.
        try {while(task.wait_for(std::chrono::milliseconds(8))!=std::future_status::ready)Report(operation);}
        catch(...){task.wait();throw;}
        return task.get();
    }
private:
    inline static thread_local Observer Current;
};
}
