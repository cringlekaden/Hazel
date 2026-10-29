#pragma once
#include <cstddef>
#include <string>
extern "C" { typedef struct _MonoString MonoString; }

namespace Hazel::ManagedStrings {
    enum class NulPolicy { Reject, Preserve };
    // Full Windows extended paths fit this boundary; native asset ownership,
    // format and OS limits still apply. Never infer a shorter path from NUL.
    constexpr size_t PathUnits = 32768, PathBytes = 4 * PathUnits;

    // Null retains the scripting API's empty-string convention. Validate/count
    // the complete UTF-16 before allocating UTF-8; no Mono conversion buffer or
    // lossy replacement is used. Callers translate failures at their API boundary.
    std::string Read(MonoString* value, size_t maxUnits, size_t maxBytes,
                     const char* subject, NulPolicy nul = NulPolicy::Reject);
}
