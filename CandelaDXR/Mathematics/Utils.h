#pragma once

#include "Types.h"

#include "Sampler/ISampler.h"

namespace candela::mathematics
{
    Vector2 SamplePointOnTriangle(sampler::ISampler& sampler);
    Vector InterpolateVertices(const Vector2& uv, const Vector& v0, const Vector& v1, const Vector& v2);
    Vector GeneratePerpendicularVector(const Vector& vec);
}
