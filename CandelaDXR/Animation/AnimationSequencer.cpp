#include "AnimationSequencer.h"

using std::uint32_t;

using candela::animation::AnimationSequencer;

AnimationSequencer::AnimationSequencer(uint32_t framesPerAnimation, uint32_t timeDeltaMs)
	: framesPerAnimation(framesPerAnimation), timeDeltaMs(timeDeltaMs),
	  currentFrameInAnimation(), currentTimeMs(), enabled()
{
}

void AnimationSequencer::tick()
{
	if (!enabled)
		return;

	++currentFrameInAnimation;
	if (currentFrameInAnimation == framesPerAnimation)
	{
		currentTimeMs += timeDeltaMs;
		currentFrameInAnimation = 0;
	}
}

bool AnimationSequencer::isNewFrame() const
{
	return currentFrameInAnimation == 0;
}

bool AnimationSequencer::isDeltaAnimationReady() const
{
	return currentFrameInAnimation + 1 == framesPerAnimation;
}

bool AnimationSequencer::isEnabled() const
{
	return enabled;
}

uint32_t AnimationSequencer::getTimeMs() const
{
	return currentTimeMs;
}

void AnimationSequencer::setEnabled(bool enabled)
{
	this->enabled = enabled;
}
