#include "hzpch.h"
#include "WindowsCommandLine.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <stdexcept>
namespace Hazel {
std::vector<std::string> WindowsCommandLineUTF8()
{
    int count = 0;
    auto* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) throw std::runtime_error("Cannot read Windows command line");
    const auto release = [](wchar_t** value) { LocalFree(value); };
    std::unique_ptr<wchar_t*, decltype(release)> owner(arguments, release);
    std::vector<std::string> result;
    for (int i = 0; i < count; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, arguments[i], -1, nullptr, 0, nullptr, nullptr);
        if (!size) throw std::runtime_error("Invalid UTF-16 command line argument");
        std::string value(size, '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, arguments[i], -1, value.data(), size, nullptr, nullptr))
            throw std::runtime_error("Cannot encode command line argument as UTF-8");
        value.pop_back();
        result.push_back(std::move(value));
    }
    return result;
}
}
