#include "OrthonormalBasis.h"

#include "Utils.h"

using candela::mathematics::OrthonormalBasis;
using candela::mathematics::Vector;
using candela::mathematics::GeneratePerpendicularVector;

OrthonormalBasis::OrthonormalBasis()
	:	U(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f)), 
		V(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f)), 
		W(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f))
{
}

OrthonormalBasis::OrthonormalBasis(Vector w)
{
	using namespace DirectX;
	W = XMVector3Normalize(w);
	U = XMVector3Cross(W, GeneratePerpendicularVector(W));
	V = XMVector3Cross(U, W);
}

Vector OrthonormalBasis::getPoint(Vector uvw)
{
	using namespace DirectX;
	return	XMVectorScale(U, uvw.m128_f32[0]) +
			XMVectorScale(V, uvw.m128_f32[1]) +
			XMVectorScale(W, uvw.m128_f32[2]);
}

Vector OrthonormalBasis::getPoint(float u, float v, float w)
{
	return getPoint(DirectX::XMVectorSet(u, v, w, 0.f));
}

Vector OrthonormalBasis::getUVW(Vector point)
{
	using namespace DirectX;

	return Vector{
		XMVector3Dot(point, U).m128_f32[0],
		XMVector3Dot(point, V).m128_f32[0],
		XMVector3Dot(point, W).m128_f32[0],
		0.f
	};
}
