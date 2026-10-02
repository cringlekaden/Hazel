#pragma once
#include <string>
#include <vector>
namespace Hazel {
// Windows supplies UTF-16; common Application arguments use UTF-8 on every OS.
std::vector<std::string> WindowsCommandLineUTF8();
}
