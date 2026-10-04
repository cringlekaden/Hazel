// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#include "hzpch.h"
#include "FileSystem.h"
#include <fstream>
#include "Hazel/Core/UUID.h"
#include <streambuf>
#include <stdexcept>

namespace Hazel {

	namespace {
		class FileWriteBuffer final : public std::streambuf {
		public:
			explicit FileWriteBuffer(std::FILE* file) : m_File(file) {}
		protected:
			std::streamsize xsputn(const char* data, std::streamsize count) override {
				return static_cast<std::streamsize>(std::fwrite(data, 1, static_cast<size_t>(count), m_File));
			}
			int_type overflow(int_type value) override {
				if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
				return std::fputc(traits_type::to_char_type(value), m_File) == EOF ? traits_type::eof() : value;
			}
			int sync() override { return std::fflush(m_File); }
		private:
			std::FILE* m_File;
		};
	}

	void FileSystem::WriteFileAtomically(const std::filesystem::path& path, const std::function<void(std::ostream&)>& writer, WriteMode mode)
	{
		std::filesystem::path temporary;
		std::FILE* file = nullptr;
		for (int attempt = 0; attempt < 32 && !file; ++attempt) {
			temporary = path;
			temporary += ".hazel-tmp-" + std::to_string(static_cast<uint64_t>(UUID()));
			file = OpenExclusiveOutput(temporary); // nullptr means a collision; never open someone else's file.
		}
		if (!file) throw std::runtime_error("Cannot reserve save temporary for " + path.generic_u8string());
		try {
			FileWriteBuffer buffer(file);
			std::ostream stream(&buffer);
			writer(stream);
			stream.flush();
			if (!stream || std::ferror(file)) throw std::runtime_error("Cannot write save temporary for " + path.generic_u8string());
			const int closed = std::fclose(file);
			file = nullptr;
			if (closed != 0) throw std::runtime_error("Cannot close save temporary for " + path.generic_u8string());
			ReplaceFile(temporary, path, mode);
		} catch (...) {
			if (file) std::fclose(file);
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored); // Only the file exclusively created by this attempt.
			throw;
		}
	}

    void FileSystem::WriteNewFile(const std::filesystem::path& path,const std::string& contents) {
        auto* file=OpenExclusiveOutput(path);
        if(!file) throw std::runtime_error("File already exists: "+path.generic_u8string());
        bool written=std::fwrite(contents.data(),1,contents.size(),file)==contents.size();
        if(std::fclose(file)!=0) written=false;
        if(!written) { std::filesystem::remove(path); throw std::runtime_error("Cannot create file: "+path.generic_u8string()); }
    }
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
