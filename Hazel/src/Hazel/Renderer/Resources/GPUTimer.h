#pragma once
#include "Hazel/Core/Base.h"
#include <cstdint>
namespace Hazel
{
// Bounded, asynchronous command-interval timing. Results may lag submissions;
// unavailable/saturated timing never delays rendering. Context-thread lifetime.
class GPUTimer
{
  public:
    struct Result
    {
        double Milliseconds = 0;
        uint64_t Submission = 0;
        bool Available = false;
    };
    virtual ~GPUTimer() = default;
    virtual void Begin(uint64_t submission) = 0;
    virtual void End() = 0;
    virtual Result Latest() const = 0;
    static Scope<GPUTimer> Create();
};
} // namespace Hazel
