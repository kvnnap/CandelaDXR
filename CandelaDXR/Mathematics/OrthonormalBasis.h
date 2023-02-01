#pragma once

#include "Types.h"

namespace candela::mathematics
{
	class OrthonormalBasis
	{
	public:

		OrthonormalBasis();

		OrthonormalBasis(Vector w);
		Vector getPoint(Vector uvw);
		Vector getPoint(float u, float v, float w);
		Vector getUVW(Vector point);

		Vector U;
		Vector V;
		Vector W;
	};
}