#pragma once

#include <cstdint>

namespace candela::animation
{
	class AnimationSequencer
	{
	public:
		AnimationSequencer();

		void tick();

		bool isNewFrame() const;
		bool isDeltaAnimationReady() const;
		bool isEnabled() const;
		bool isCompleted() const;

		std::uint32_t getTimeMs() const;
		void setEnabled(bool enabled);
		void setFramesPerAnimation(std::uint32_t framesPerAnimation);
		void setTimeDeltaMs(std::uint32_t timeDeltaMs);
		void setMaxTimeMs(std::uint32_t maxTimeMs);

		std::uint32_t getFramesPerAnimation() const;
		std::uint32_t getTimeDeltaMs() const;
		std::uint32_t getMaxTimeMs() const;
	private:
		// Animation settings
		std::uint32_t framesPerAnimation;
		std::uint32_t timeDeltaMs;
		std::uint32_t maxTimeMs;

		std::uint32_t currentFrameInAnimation;
		std::uint32_t currentTimeMs;

		bool enabled;
		bool completed;
	};
}