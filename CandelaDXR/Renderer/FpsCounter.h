#pragma once

#include <cstdint>
#include <chrono>

namespace candela::renderer
{
	class FpsCounter
	{
	public:
		FpsCounter();

		bool hitFrame();
		void resetFrameCount();

		std::uint64_t getTotalFrames() const;
		float getFramesPerSecond() const;
		std::uint64_t getLastFrameTime() const;
	private:
		std::uint64_t frames, framesPrev;
		float fps;
		std::chrono::milliseconds ms;
		std::chrono::milliseconds prevMs, currentMs;
	};
}