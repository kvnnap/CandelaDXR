#include "Plane.h"

using candela::mathematics::Plane;
using candela::mathematics::Vector2;
using candela::mathematics::Vector3;
using candela::mathematics::Vector;

Plane::Plane(const Vector& pos, const Vector& normal)
    : Position(pos), UnitNormal(DirectX::XMVector3Normalize(normal)), Basis(UnitNormal)
{
}

Vector Plane::projectPointInWorldSpace(const Vector& point)
{
    // Generate direction
    using namespace DirectX;
    auto t = XMVector3Dot(UnitNormal, point - Position);
    return XMVectorMultiplyAdd(-UnitNormal, t, point);
}

Vector Plane::projectPointInUVSpace(const Vector& point)
{
    using namespace DirectX;
    auto ret = Basis.getUVW(point - Position);
    ret.m128_f32[2] = 0.f; // If we set w to zero, point will reside on the plane
    return ret;
}

Vector Plane::pointFromUV(const Vector& uv)
{
    using namespace DirectX;
    auto tempUv = uv;
    tempUv.m128_f32[2] = tempUv.m128_f32[3] = 0.f;
    return Position + Basis.getPoint(tempUv);
}
