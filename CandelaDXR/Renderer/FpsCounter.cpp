#include "FpsCounter.h"

using std::uint64_t;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using candela::renderer::FpsCounter;

FpsCounter::FpsCounter()
	: frames{}, framesPrev{}, fps{}, ms{}
{
}

bool FpsCounter::hitFrame()
{
	++frames;
	auto localMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	auto range = (localMs - ms).count();
	auto expired = range >= 1000;
	if (expired)
	{
		ms = localMs;
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
