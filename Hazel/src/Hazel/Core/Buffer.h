// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once

#include <stdint.h>
#include "Hazel/Core/Base.h"
#include <cstring>

namespace Hazel {

	// Move-only owning storage; Data is a view of the Scope-owned allocation.
	struct Buffer
	{
		uint8_t* Data = nullptr;
		uint64_t Size = 0;

		Buffer() = default;

		Buffer(uint64_t size)
		{
			Allocate(size);
		}

		Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&& other) noexcept { *this = std::move(other); }
        Buffer& operator=(Buffer&& other) noexcept
        {
            if (this != &other) {
                m_Storage = std::move(other.m_Storage);
                Data = m_Storage.get(); Size = std::exchange(other.Size, 0);
                other.Data = nullptr;
            }
            return *this;
        }

		static Buffer Copy(const Buffer& other)
		{
			Buffer result(other.Size);
			if (other.Size) memcpy(result.Data, other.Data, other.Size);
			return result;
		}

		void Allocate(uint64_t size)
		{
			Release();

			m_Storage = size ? CreateScope<uint8_t[]>(size) : nullptr;
            Data = m_Storage.get();
			Size = size;
		}

		void Release()
		{
			m_Storage.reset();
			Data = nullptr;
			Size = 0;
		}

		template<typename T>
		T* As()
		{
			return (T*)Data;
		}

		operator bool() const
		{
			return (bool)Data;
		}

    private:
        Scope<uint8_t[]> m_Storage;
	};

	struct ScopedBuffer
	{
		ScopedBuffer(Buffer buffer)
			: m_Buffer(std::move(buffer))
		{
		}

		ScopedBuffer(uint64_t size)
			: m_Buffer(size)
		{
		}

		~ScopedBuffer()
		{
			m_Buffer.Release();
		}

		uint8_t* Data() { return m_Buffer.Data; }
		uint64_t Size() { return m_Buffer.Size; }

		template<typename T>
		T* As()
		{
			return m_Buffer.As<T>();
		}

		operator bool() const { return m_Buffer; }
	private:
		Buffer m_Buffer;
	};


}
