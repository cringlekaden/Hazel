// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once

#include "Hazel/Core/Buffer.h"
#include <filesystem>

namespace Hazel {

	class FileSystem
	{
	public:
		// TODO: move to FileSystem class
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
	};

}
