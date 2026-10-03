// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once

#include "Hazel/Core/Buffer.h"
#include <filesystem>
#include <cstdio>
#include <functional>
#include <ostream>

namespace Hazel {

	class FileSystem
	{
	public:
		static std::filesystem::path GetExecutablePath();
        static std::filesystem::path GetUserDataDirectory();
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
		// Checked sibling-temporary write followed by replacement; no power-loss durability claim.
		static void WriteFileAtomically(const std::filesystem::path& filepath, const std::function<void(std::ostream&)>& writer);
	private:
		static std::FILE* OpenExclusiveOutput(const std::filesystem::path& path);
		static void ReplaceFile(const std::filesystem::path& source, const std::filesystem::path& destination);
	};

}
