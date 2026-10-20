#pragma once
#include "Hazel/Renderer/GPUTimer.h"
#include <array>
namespace Hazel
{
class OpenGLGPUTimer final : public GPUTimer
{
  public:
    OpenGLGPUTimer();
    ~OpenGLGPUTimer() override;
    void Begin(uint64_t submission) override;
    void End() override;
    Result Latest() const override
    {
        return m_Result;
    }

  private:
    struct Slot
    {
        uint32_t Start = 0, End = 0;
        uint64_t Submission = 0;
        bool Pending = false;
    };
    std::array<Slot, 16> m_Slots{};
    size_t m_Next = 0;
    bool m_Active = false;
    uint64_t m_CounterMask = ~uint64_t(0);
    Result m_Result;
};
} // namespace Hazel
