#include "FpsCounter.h"

using std::uint64_t;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using candela::renderer::FpsCounter;

FpsCounter::FpsCounter()
	: frames{}, framesPrev{}, fps{}, ms{}, prevMs{}, currentMs{}
{
}

bool FpsCounter::hitFrame()
{
	++frames;
	prevMs = currentMs;
	currentMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	if (prevMs == milliseconds::zero())
		prevMs = currentMs;

	auto range = (currentMs - ms).count();
	auto expired = range >= 1000;
	if (expired)
	{
		ms = currentMs;
		fps = 1000.f * (frames - framesPrev) / range;
		framesPrev = frames;
	}

	return expired;
}

uint64_t FpsCounter::getTotalFrames() const
{
	return frames;
}

float FpsCounter::getFramesPerSecond() const
{
	return fps;
}

uint64_t FpsCounter::getLastFrameTime() const
{
	return (currentMs - prevMs).count();
}
