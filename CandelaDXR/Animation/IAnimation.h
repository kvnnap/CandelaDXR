#pragma once

#include <cstdint>
#include "Mathematics/Types.h"

namespace candela::animation
{
	class IAnimation
	{
	public:
		virtual ~IAnimation() = default;
		virtual mathematics::Matrix animate(std::uint32_t timeMs, const mathematics::Vector& centreTranslation = {}) const = 0;
	};
}