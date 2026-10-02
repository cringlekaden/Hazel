#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#ifdef HZ_PLATFORM_WINDOWS
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

#include "Hazel/Core/Base.h"
#include "Hazel/Debug/Instrumentor.h"