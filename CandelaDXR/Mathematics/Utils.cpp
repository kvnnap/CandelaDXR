#include "Utils.h"

#include "Constants.h"

#include <cmath>

using std::sqrt;
using std::atan;
using std::exp;
using std::erff;

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

float candela::mathematics::Gauss(float x, float stdDev)
{
	const float invStdDev = 1.f / stdDev;
	const float ind = x * invStdDev;
	return invStdDev * constants::OneOverSqrtTwoPi * exp(-0.5f * ind * ind);
}

float candela::mathematics::GaussIntegral(float a, float b, float stdDev)
{
	const float den = 1.f / (stdDev * constants::SqrtTwo);
	return 0.5f * (erff(b * den) - erff(a * den));
}

// Integral cos(theta) * cos(theta_area)/r^2 dA - for special case
float candela::mathematics::f1(float x, float y, float z)
{
	float zSq = z * z;
	float r1 = 1.f / sqrt(y * y + zSq);
	float r2 = 1.f / sqrt(x * x + zSq);

	return 0.5f * (
		y * atan(x * r1) * r1 +
		x * atan(y * r2) * r2);
}

float candela::mathematics::f1Definite(float x0, float x1, float y0, float y1, float z)
{
	return f1(x1, y1, z)
		 + f1(x0, y0, z)
		 - f1(x1, y0, z)
		 - f1(x0, y1, z);
}

// Integral cos(theta_area)/r^2 dA - for special case
float candela::mathematics::f2(float x, float y, float z)
{
	return atan((x * y) / (z * sqrt(x * x + y * y + z * z)));
}

float candela::mathematics::f2Definite(float x0, float x1, float y0, float y1, float z)
{
	return f2(x1, y1, z)
		 + f2(x0, y0, z)
		 - f2(x1, y0, z)
		 - f2(x0, y1, z);
}

Vector3 candela::mathematics::QuaternionToRotationXYZ(const Vector& rot)
{
	Vector3 rotation{};
	float a = 2.f * (rot.m128_f32[3] * rot.m128_f32[0] + rot.m128_f32[1] * rot.m128_f32[2]);
	float b = 1.f - 2.f * (rot.m128_f32[0] * rot.m128_f32[0] + rot.m128_f32[1] * rot.m128_f32[1]);
	rotation.x = atan2f(a, b); //roll

	a = sqrtf(1.f + 2.f * (rot.m128_f32[3] * rot.m128_f32[1] - rot.m128_f32[0] * rot.m128_f32[2]));
	b = sqrtf(1.f - 2.f * (rot.m128_f32[3] * rot.m128_f32[1] - rot.m128_f32[0] * rot.m128_f32[2]));
	rotation.y = 2.f * atan2f(a, b) - mathematics::constants::PiOver2; // pitch

	a = 2.f * (rot.m128_f32[3] * rot.m128_f32[2] + rot.m128_f32[0] * rot.m128_f32[1]);
	b = 1.f - 2.f * (rot.m128_f32[1] * rot.m128_f32[1] + rot.m128_f32[2] * rot.m128_f32[2]);
	rotation.z = atan2(a, b); // yaw
	return rotation;
}
