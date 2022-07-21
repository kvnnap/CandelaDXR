#include "AnimationSequencer.h"

using std::uint32_t;

using candela::animation::AnimationSequencer;

AnimationSequencer::AnimationSequencer()
	: framesPerAnimation(), timeDeltaMs(), maxTimeMs(),
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

	if (currentTimeMs > maxTimeMs)
	{
		enabled = false;
		completed = true;
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

bool AnimationSequencer::isCompleted() const
{
	return completed;
}

uint32_t AnimationSequencer::getTimeMs() const
{
	return currentTimeMs;
}

void AnimationSequencer::setEnabled(bool enabled)
{
	this->enabled = enabled;
}

void AnimationSequencer::setFramesPerAnimation(uint32_t framesPerAnimation)
{
	this->framesPerAnimation = framesPerAnimation;
}

void AnimationSequencer::setTimeDeltaMs(uint32_t timeDeltaMs)
{
	this->timeDeltaMs = timeDeltaMs;
}

void AnimationSequencer::setMaxTimeMs(uint32_t maxTimeMs)
{
	this->maxTimeMs = maxTimeMs;
}

uint32_t AnimationSequencer::getFramesPerAnimation() const
{
	return framesPerAnimation;
}

uint32_t AnimationSequencer::getTimeDeltaMs() const
{
	return timeDeltaMs;
}

uint32_t AnimationSequencer::getMaxTimeMs() const
{
	return maxTimeMs;
}
