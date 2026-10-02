// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once
#include <cstdint>
#include <functional>

namespace Hazel {

	class UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t() const { return m_UUID; }
	private:
		uint64_t m_UUID;
	};

}

namespace std {
	template <typename T> struct hash;

	template<>
	struct hash<Hazel::UUID>
	{
		std::size_t operator()(const Hazel::UUID& uuid) const
		{
			return (uint64_t)uuid;
		}
	};

}
