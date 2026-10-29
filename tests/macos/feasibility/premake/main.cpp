#include <iostream>

extern const char* HazelMacOSFoundationVersion();

int main() {
    const char* version = HazelMacOSFoundationVersion();
    if (!version || !*version) return 1;
    std::cout << "PASS: native Premake C++/Objective-C++/Foundation probe " << version << '\n';
    return 0;
}
