#pragma once
#include "Hazel/Core/Base.h"
#include <filesystem>
#include <string>

namespace Hazel {
// Deliberately small color-pass ABI: texture(0) is read-only RGBA32F,
// texture(1) is write-only RGBA32F; buffer(0) is an immutable <=256-byte
// parameter block. One thread per destination pixel; kernels must bounds-check.
// The owning framebuffer packet supplies residency, ordering and retirement.
class ComputePipeline {
public:
    virtual ~ComputePipeline() = default;
    static Ref<ComputePipeline> Create(const std::filesystem::path& library, const std::string& entry);
};
}
