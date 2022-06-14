#pragma once

#include "Types.h"

#include "Sampler/ISampler.h"

namespace candela::mathematics
{
    Vector2 SamplePointOnTriangle(sampler::ISampler& sampler);
    Vector InterpolateVertices(const Vector2& uv, const Vector& v0, const Vector& v1, const Vector& v2);
    Vector GeneratePerpendicularVector(const Vector& vec);

	float Gauss(float x, float stdDev);
	float GaussIntegral(float a, float b, float stdDev);

    // Integrals
	
	// Integral cos(theta) * cos(theta_area)/r^2 dA - for special case
	float f1(float x, float y, float z);
	float f1Definite(float x0, float x1, float y0, float y1, float z);

	// Integral cos(theta_area)/r^2 dA - for special case
	float f2(float x, float y, float z);
	float f2Definite(float x0, float x1, float y0, float y1, float z);
}
