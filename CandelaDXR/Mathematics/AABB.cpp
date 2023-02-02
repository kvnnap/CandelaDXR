#include "AABB.h"
#include "Constants.h"

using candela::mathematics::AABB;
using candela::mathematics::Vector;

AABB::AABB()
	:	Min{ constants::Infinity, constants::Infinity, constants::Infinity, constants::Infinity },
		Max{ constants::NegativeInfinity, constants::NegativeInfinity, constants::NegativeInfinity, constants::NegativeInfinity }
{
}

void AABB::contain(const Vector& point)
{
	Min = DirectX::XMVectorMin(Min, point);
	Max = DirectX::XMVectorMax(Max, point);
}

void AABB::contain(const AABB& aabb)
{
	contain(aabb.Min);
	contain(aabb.Max);
}

AABB AABB::transform(const mathematics::Matrix& trans) const
{
	AABB aabb;
	for (size_t i = 0u; i < 8u; ++i)
		aabb.contain(DirectX::XMVector3Transform(getCornerPoint(i), trans));

	return aabb;
}

Vector AABB::getCornerPoint(std::size_t i) const
{
	return Vector{
		i & (1 << 0) ? Max.m128_f32[0] : Min.m128_f32[0],
		i & (1 << 1) ? Max.m128_f32[1] : Min.m128_f32[1],
		i & (1 << 2) ? Max.m128_f32[2] : Min.m128_f32[2],
		1.f
	};
}

Vector AABB::getClosestToDirection(const Vector& dir) const
{
	return Vector{
		(dir.m128_f32[0] < 0.f ? Min.m128_f32[0] : Max.m128_f32[0]),
		(dir.m128_f32[1] < 0.f ? Min.m128_f32[1] : Max.m128_f32[1]),
		(dir.m128_f32[2] < 0.f ? Min.m128_f32[2] : Max.m128_f32[2]),
		1.f
	};
}

Vector AABB::getCentre() const
{
	using namespace DirectX;
	return 0.5f * (Min + Max);
}

Vector AABB::getDimensions() const
{
	return DirectX::XMVectorSubtract(Max, Min);
}
