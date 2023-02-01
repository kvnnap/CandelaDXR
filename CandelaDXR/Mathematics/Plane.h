#pragma once

#include "Types.h"
#include "OrthonormalBasis.h"

namespace candela::mathematics
{
	class Plane
	{
	public:
		Plane(const Vector& pos, const Vector& normal);

		Vector projectPointInWorldSpace(const Vector& point);
		Vector projectPointInUVSpace(const Vector& point);
		Vector pointFromUV(const Vector& uv);

		Vector Position;
		Vector UnitNormal;

		// Basis
		OrthonormalBasis Basis;

	};
}
