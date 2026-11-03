#pragma once

#include <vector>

namespace Hazel {
    // Entry-point diagnostics are consumed once, before application-specific
    // parsing. Pointers borrow the original argv for the duration of main().
    struct ApplicationLaunch {
        bool SmokeTest = false;
        std::vector<char*> Arguments;
        int Count() const { return static_cast<int>(Arguments.size()) - 1; }
        static ApplicationLaunch Parse(int argc, char** argv);
    };
}
