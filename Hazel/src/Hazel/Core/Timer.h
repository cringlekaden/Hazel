// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once

#include <chrono>

namespace Hazel {

	class Timer
	{
	public:
		Timer()
		{
			Reset();
		}

		void Reset()
		{
			m_Start = std::chrono::steady_clock::now();
		}

		float Elapsed()
		{
			return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - m_Start).count() * 0.001f * 0.001f * 0.001f;
		}

		float ElapsedMillis()
		{
			return Elapsed() * 1000.0f;
		}

	private:
		std::chrono::time_point<std::chrono::steady_clock> m_Start;
	};

}
