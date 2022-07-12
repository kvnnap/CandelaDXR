#include "RendererTime.h"

using std::uint32_t;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using candela::renderer::RendererTime;

RendererTime::RendererTime()
	: ms{}
{
}

void RendererTime::reset(uint32_t offsetMs)
{
	ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	ms -= milliseconds(offsetMs);
}

uint32_t RendererTime::getTimeMs()
{
	if (ms == milliseconds::zero())
	{
		reset(0u);
		return 0u;
	}

	const RendererTime& rt = *this;
	return rt.getTimeMs();
}

uint32_t RendererTime::getTimeMs() const
{
	return static_cast<uint32_t>((duration_cast<milliseconds>(system_clock::now().time_since_epoch()) - ms).count());
}
