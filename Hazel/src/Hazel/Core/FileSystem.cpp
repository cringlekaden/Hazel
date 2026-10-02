// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#include "hzpch.h"
#include "FileSystem.h"
#include <fstream>

namespace Hazel {

	Buffer FileSystem::ReadFileBinary(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

		if (!stream)
		{
			// Failed to open the file
			return {};
		}


		std::streampos end = stream.tellg();
        if (end < 0) return {};
		stream.seekg(0, std::ios::beg);
		uint64_t size = end - stream.tellg();

		if (size == 0)
		{
			// File is empty
			return {};
		}

		Buffer buffer(size);
		if (!stream.read(buffer.As<char>(), static_cast<std::streamsize>(size))) return {};
		stream.close();
		return buffer;
	}

}
