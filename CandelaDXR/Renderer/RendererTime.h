#pragma once

#include <cstdint>
#include <chrono>

namespace candela::renderer
{
	class RendererTime
	{
	public:
		RendererTime();
		void reset(std::uint32_t offsetMs = 0u);
		std::uint32_t getTimeMs();
		std::uint32_t getTimeMs() const;

	private:
		std::chrono::milliseconds ms;
	};
}
