#pragma once

#include <cstdint>

namespace candela::animation
{
	class AnimationSequencer
	{
	public:
		AnimationSequencer(std::uint32_t framesPerAnimation, std::uint32_t timeDeltaMs);

		void tick();

		bool isNewFrame() const;
		bool isDeltaAnimationReady() const;
		bool isEnabled() const;

		std::uint32_t getTimeMs() const;
		void setEnabled(bool enabled);
	private:
		// Animation settings
		std::uint32_t framesPerAnimation;
		std::uint32_t timeDeltaMs;

		std::uint32_t currentFrameInAnimation;
		std::uint32_t currentTimeMs;

		bool enabled;
	};
}