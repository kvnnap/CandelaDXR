#ifndef UTILS_HLSLI
#define UTILS_HLSLI

/*******************************************************************
	Random numbers based on Mersenne Twister
*******************************************************************/
uint rand_init(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0;
	uint v1 = val1;
	uint s0 = 0;

	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

// PRNG PCG - https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
float rand_next(inout uint s)
{
	//uint LCG_A = 1664525u;
	//uint LCG_C = 1013904223u;
	//s = (LCG_A * s + LCG_C);
	//return float(s & 0x00FFFFFF) / float(0x01000000);
	//return s / 4294967295.f;
	uint state = s;
	s = s * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	uint r = (word >> 22u) ^ word;
	return r / 4294967295.f;
	//return float(r & 0x00FFFFFF) / float(0x01000000); // <-- This works too but only 16Mil states?
}

// Range is [a-b] (inclusive), a <= b
// Gens a num from 0 to 1, scales it, and returns uint
uint chooseInRange(inout uint s, uint a, uint b) {
	return a + uint(rand_next(s) * (b - a + 1));
}

static const float PI = 3.14159265f;
static const float OneOverPI = 1.f / PI;

float2 pointOnTriangle(float2 uv, float2 verts[3])
{
	const float2 Q1 = verts[1] - verts[0];
	const float2 Q2 = verts[2] - verts[0];
	return verts[0] + uv.x * Q1 + uv.y * Q2;
}

float3 interpolateVertices(float2 uv, float3 verts[3])
{
	const float3 Q1 = verts[1] - verts[0];
	const float3 Q2 = verts[2] - verts[0];
	return verts[0] + uv.x * Q1 + uv.y * Q2;
}

float3 samplePointOnTriangle(inout uint s, float3 verts[3], out float2 uv) {
	float r1 = rand_next(s);
	float r2 = rand_next(s);

	// Avoiding conditional - not needed for statements without else?
	/*bool cond = (r1 + r2 > 1.f);
	r1 = r1 * !cond + (1.f - r1) * cond;
	r2 = r2 * !cond + (1.f - r2) * cond;*/

	/*const float condVal = step(1.f, r1 + r2);
	r1 += condVal * (1.f - 2.f * r1);
	r2 += condVal * (1.f - 2.f * r2);*/

	if (r1 + r2 > 1.f)
	{
		r1 = 1.f - r1;
		r2 = 1.f - r2;
	}
	
	uv.x = r1;
	uv.y = r2;

	const float3 Q1 = verts[1] - verts[0];
	const float3 Q2 = verts[2] - verts[0];

	return verts[0] + r1 * Q1 + r2 * Q2;
}

float getTriangleArea(float3 verts[3]) {
	const float3 Q1 = verts[1] - verts[0];
	const float3 Q2 = verts[2] - verts[0];
	const float Q1Q2 = dot(Q1, Q2);

	return 0.5f * length(Q1) * length(Q2) * sqrt(1.f - (Q1Q2 * Q1Q2 / (dot(Q1, Q1) * dot(Q2, Q2))));
}

float3 getUnitNormal(float3 a0, float3 a1, float3 a2) {
	return normalize(cross(a1 - a0, a2 - a0));
}

float3 getUnitNormal(float3 a[3]) {
	return getUnitNormal(a[0], a[1], a[2]);
}

float3 getCentroid(float3 a[3]) {
	return (a[0] + a[1] + a[2]) / 3.f;
}

float3 exposure(float3 x, float c)
{
    return x * pow(2, c);
}

float3 toneMap(float3 c)
{
	return c / (c + 1.f);
}

float3 toneMapAces(float3 x)
{
    x *= 0.6f;
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 linearToSrgb(float3 c)
{
	// Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	float3 sq1 = sqrt(c);
	float3 sq2 = sqrt(sq1);
	float3 sq3 = sqrt(sq2);
	float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
	return srgb;
}

float3 linearToSrgbGamma(float3 c)
{
	const float kInvGamma = 1.0f / 2.2f;
	//const float kInvGamma = 1.0f / 1.8f;
	return float3(pow(c.x, kInvGamma), pow(c.y, kInvGamma), pow(c.z, kInvGamma));
}

float3 createPerpendicularVector(float3 unitVec)
{
	return unitVec.y != 0.f || unitVec.x != 0.f ?
		normalize(float3(unitVec.y, -unitVec.x, 0.f)) :
		float3(unitVec.z, 0.f, 0.f);
}

float3 transformPointToBasis(float3 unitNormal, float3 pt) {
	// Create first vector perpendicular to the normal
	const float3 u = createPerpendicularVector(unitNormal);

	// Create second vector perpendicular to the normal
	const float3 w = normalize(cross(u, unitNormal));
	const float3 v = unitNormal;

	return pt.x * u + pt.y * v + pt.z * w;
}

float3 randomRayLobe(inout uint s, float3 unitNormal, float n, inout float p) {
	// The pdf is (n + 1) cos^n(phi) / (2*pi)
	const float nPlusOne = n + 1.f;
	const float cosPhiToTheNPlusOne = rand_next(s);
	// TODO: uncomment if used
	p = nPlusOne / (2.f * PI) * pow(cosPhiToTheNPlusOne, n / nPlusOne);
	const float cosPhi = pow(cosPhiToTheNPlusOne, 1.f / nPlusOne);
	const float sinPhi = sqrt(1.f - cosPhi * cosPhi);
	const float theta = 2.f * PI * rand_next(s);

	float sinTheta;
	float cosTheta;
	sincos(theta, sinTheta, cosTheta);

	return transformPointToBasis(unitNormal, float3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta));
}

float3 randomRayHemisphere(inout uint s, float3 unitNormal) {
	float p;
	return randomRayLobe(s, unitNormal, 0, p);
}

// Phi_Constant ranges from 0 (top of sphere) to PI (bottom of sphere)
float3 randomRaySphericalCapBase(inout uint s, inout float p, float oneMinusCosPhiConstant)
{
    const float cosPhi = 1.f - oneMinusCosPhiConstant * rand_next(s);
	const float sinPhi = sqrt(1.f - cosPhi * cosPhi);
	const float theta = 2.f * PI * rand_next(s);

	float sinTheta;
	float cosTheta;
	sincos(theta, sinTheta, cosTheta);

    p = 1.f / (2.f * oneMinusCosPhiConstant * PI);
	return float3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
}

float3 randomRaySphere(inout uint s, inout float p)
{
	// 1.f - cos(pi) = 2.f
    return randomRaySphericalCapBase(s, p, 2.f);
}

float3 randomRaySphericalCap(inout uint s, inout float p, float oneMinusCosPhiConstant, float3 unitNormal)
{
    return transformPointToBasis(unitNormal, randomRaySphericalCapBase(s, p, oneMinusCosPhiConstant));
}

static const uint ConvRangeBits = 24;

uint4 floatToFixed(float4 value, uint rangeBits)
{
	return round(value * pow(2.f, rangeBits));
}

float4 fixedToFloat(uint4 value, uint rangeBits)
{
	return value * pow(2.f, -(float)rangeBits);
}

float fresnel(const float cosx, const float n1, const float n2)
{
	float fr = 1.f;
	const float ior = n1 / n2;
	const float t = 1.f - ior * ior * (1.f - cosx * cosx);

	if (t > 0.f)
	{
		const float s = sqrt(t);
		const float n1Cosx = n1 * cosx;
		const float n2S = n2 * s;
		const float ra = (n1Cosx - n2S) / (n1Cosx + n2S);
		const float n2Cosx = n2 * cosx;
		const float n1S = n1 * s;
		const float rb = (n1S - n2Cosx) / (n1S + n2Cosx);
		fr = 0.5f * (ra * ra + rb * rb);
	}
	
	return fr;
}

#endif