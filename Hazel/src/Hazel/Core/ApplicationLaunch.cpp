#include "hzpch.h"
#include "ApplicationLaunch.h"
#include <stdexcept>
#include <string_view>

namespace Hazel {
    ApplicationLaunch ApplicationLaunch::Parse(int argc, char** argv) {
        if (argc < 1 || !argv || !argv[0]) throw std::invalid_argument("Missing application argv");
        ApplicationLaunch result;
        result.Arguments.push_back(argv[0]);
        for (int i = 1; i < argc; ++i) {
            if (!argv[i]) throw std::invalid_argument("Null application argument");
            if (std::string_view(argv[i]) == "--smoke-test") {
                if (result.SmokeTest) throw std::invalid_argument("Duplicate --smoke-test");
                result.SmokeTest = true;
            } else result.Arguments.push_back(argv[i]);
        }
        result.Arguments.push_back(nullptr);
        return result;
    }
}
