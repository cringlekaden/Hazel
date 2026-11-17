#pragma once
#include <cstdint>
#include <memory>

namespace Hazel {
// A renderer-owned immutable texture version. The UI adapter retains Owner through
// GPU completion; Handle is full-width and is never a portable renderer ID.
struct EditorTexture
{
    uintptr_t Handle = 0;
    std::shared_ptr<const void> Owner;
    bool FlipY = false;
    // Optional backend sampler from the same immutable resource generation.
    // Zero selects the UI backend's default (encoded targets and font atlas).
    uintptr_t SamplerHandle = 0;
};
}
