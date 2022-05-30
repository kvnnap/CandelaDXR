#include "Utils.h"

using candela::mathematics::Vector;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;

using candela::sampler::ISampler;

Vector2 candela::mathematics::SamplePointOnTriangle(ISampler& sampler)
{
	float r1 = sampler.nextSample();
	float r2 = sampler.nextSample();
	if (r1 + r2 > 1.f)
	{
		r1 = 1.f - r1;
		r2 = 1.f - r2;
	}

	return Vector2(r1, r2);
}
Vector candela::mathematics::InterpolateVertices(const Vector2& uv, const Vector& v0, const Vector& v1, const Vector& v2)
{
	auto Q1 = DirectX::XMVectorSubtract(v1, v0);
	auto Q2 = DirectX::XMVectorSubtract(v2, v0);
	Q1 = DirectX::XMVectorScale(Q1, uv.x);
	Q2 = DirectX::XMVectorScale(Q2, uv.y);
	auto res = DirectX::XMVectorAdd(Q1, Q2);
	return DirectX::XMVectorAdd(v0, res);
}

Vector candela::mathematics::GeneratePerpendicularVector(const Vector& vec)
{
	return vec.m128_f32[0] != 0.f || vec.m128_f32[1] != 0.f
		? Vector{ vec.m128_f32[1], -vec.m128_f32[0], 0.f, 0.f }
		: Vector{ vec.m128_f32[2], 0.f, 0.f, 0.f };
}
