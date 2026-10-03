// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once

#include <string>

namespace Hazel {

	class FileDialogs
	{
	public:
		// These return empty strings if cancelled
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
        static std::string SelectFolder();
        static bool OpenPath(const std::string& path);
	};

	class Time
	{
	public:
		static float GetTime();
	};

}
