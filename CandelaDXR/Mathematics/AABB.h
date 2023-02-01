#pragma once

#include <cstddef>
#include "Types.h"

namespace candela::mathematics
{
	class AABB
	{
	public:
		AABB();

		void contain(const Vector& point);
		void contain(const AABB& aabb);

		AABB transform(const mathematics::Matrix& trans) const;

		// pointIndex cannot be larger than 7
		Vector getCornerPoint(std::size_t pointIndex) const;
		Vector getClosestToDirection(const Vector& dir) const;
		Vector getDimensions() const;

		// Data
		Vector Min;
		Vector Max;
	};
}