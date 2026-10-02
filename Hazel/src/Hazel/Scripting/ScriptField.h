#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
extern "C" { typedef struct _MonoClassField MonoClassField; }
namespace Hazel {
	enum class ScriptFieldType
	{
		None = 0,
		Float, Double,
		Bool, Char, Byte, Short, Int, Long,
		UByte, UShort, UInt, ULong,
		Vector2, Vector3, Vector4,
		Entity
	};

	struct ScriptField
	{
		ScriptFieldType Type = ScriptFieldType::None;
		std::string Name;

		MonoClassField* ClassField = nullptr;
	};

	// ScriptField + data storage
	struct ScriptFieldInstance
	{
		ScriptField Field;

		ScriptFieldInstance()
		{
			memset(m_Buffer, 0, sizeof(m_Buffer));
		}
		ScriptFieldInstance(const ScriptFieldInstance& other) : Field(other.Field) {
			Field.ClassField = nullptr; // Stored values never own domain metadata.
			std::memcpy(m_Buffer, other.m_Buffer, sizeof(m_Buffer));
		}
		ScriptFieldInstance& operator=(const ScriptFieldInstance& other) {
			if (this == &other) return *this;
			Field = other.Field; Field.ClassField = nullptr;
			std::memcpy(m_Buffer, other.m_Buffer, sizeof(m_Buffer));
			return *this;
		}

		template<typename T>
		T GetValue()
		{
			static_assert(sizeof(T) <= 16, "Type too large!");
			T value{};
			std::memcpy(&value, m_Buffer, sizeof(T));
			return value;
		}

		template<typename T>
		void SetValue(T value)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");
			Field.ClassField = nullptr; // Persistent values keep names/types, never Mono pointers.
			std::memset(m_Buffer, 0, sizeof(m_Buffer));
			std::memcpy(m_Buffer, &value, sizeof(T));
		}
	private:
		uint8_t m_Buffer[16];

		friend class ScriptEngine;
		friend class ScriptInstance;
	};

	using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldInstance>;

}
