#include <chrono>

#include "RendererTime.h"

using std::uint32_t;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using candela::renderer::RendererTime;

RendererTime::RendererTime()
	: elapsedMs{}, startMs {}
{
}

void RendererTime::start(bool resetElapsedTime)
{
	if (resetElapsedTime)
		elapsedMs = 0;
	startMs = static_cast<uint32_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

void RendererTime::stop()
{
	elapsedMs = getTimeMs();
	startMs = 0;
}

void RendererTime::setElapsedTime(std::uint32_t elapsedMs)
{
	if (isRunning())
	{
		startMs = 0;
		this->elapsedMs = elapsedMs;
		start(false);
	}
	else
	{
		this->elapsedMs = elapsedMs;
	}
}

bool RendererTime::isRunning() const
{
	return startMs != 0;
}

uint32_t RendererTime::getTimeMs() const
{
	auto ret = elapsedMs;
	if (isRunning())
		ret += static_cast<uint32_t>((duration_cast<milliseconds>(system_clock::now().time_since_epoch()) - milliseconds(startMs)).count());
	return ret;
}
