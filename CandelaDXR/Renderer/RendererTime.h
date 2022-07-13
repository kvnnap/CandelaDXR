#pragma once

#include <cstdint>

namespace candela::renderer
{
	class RendererTime
	{
	public:
		RendererTime();
		void start(bool resetElapsedTime);
		void stop();
		void setElapsedTime(std::uint32_t elapsedMs);

		bool isRunning() const;

		std::uint32_t getTimeMs() const;

	private:
		std::uint32_t elapsedMs;
		std::uint32_t startMs;
	};
}
