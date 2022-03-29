// Used to filter path components for analysis
enum PathInteraction : uint
{
	Light = 1,
	Reflect = 2,
	Refract = 4,
	Diffuse = 8
};

struct ConstBuff
{
	float3 u, v, w;
	float3 position;
	float3 direction;
	float3 plane; // sensor dimensions (z contains distance to sensor plane)
	uint2 seeds;
	uint2 winDim;
	uint numLights;
	uint numSpeculars;
	uint frameNumber;
	float causticsRatio;
	PathInteraction pathFilter;
	uint3 padding;
};

// UAVs

// Output texture
RWTexture2D<float4> gOutput : register(u0);
RWStructuredBuffer<IrradianceItem> gIrradianceDS : register(u1);

// SRVs
StructuredBuffer<float3> verts : register(t0);
StructuredBuffer<float2> texVerts : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<uint> indices : register(t3);
StructuredBuffer<float4x3> matrices : register(t4);
StructuredBuffer<float3x3> normalMatrices : register(t5);
StructuredBuffer<FaceAttributes> faceAttributes : register(t6);
StructuredBuffer<Material> materials : register(t7);
StructuredBuffer<AreaLight> lights : register(t8);
StructuredBuffer<SpecularPrimitive> speculars : register(t9);

RaytracingAccelerationStructure gRtScene : register(t10);

Texture2D<float> gIrrToRad : register(t11);
Texture2D<float3> gTextures[]: register(t12);

// Sampler
SamplerState gSampler : register(s0);

// CBVs
cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

// Functions
uint getFaceIndex()
{
	return InstanceID() / 3 + PrimitiveIndex();
}

bool getPixel(RayDesc ray, uint2 screenDimensions, inout uint2 pixel)
{
	//compute denominator
	const float den = dot(cBuffer.w, ray.Direction);

	// make this check or else risk of division by zero..
	if (den == 0.f)
		return false;

	const float3 posPlane = cBuffer.position + cBuffer.w * cBuffer.plane.z;
	const float t = -dot(cBuffer.w, (ray.Origin - posPlane)) / den;
	if (t < ray.TMin || t > ray.TMax)
		return false;

	const float3 R = ray.Origin + ray.Direction * t - posPlane;
	float2 pt = float2(dot(R, cBuffer.u), dot(R, cBuffer.v));
	pt /= cBuffer.plane.xy;
	pt += float2(0.5f, 0.5f);
	pt *= float2(screenDimensions);
	pt.y = float(screenDimensions.y) - pt.y;
	if (pt.x < 0.f || pt.y < 0.f || pt.x >= float(screenDimensions.x) || pt.y >= float(screenDimensions.y))
		return false;

	pixel = uint2(pt);
	return true;
}

float3 getUnitNormal(float2 bary, uint vertBaseId, uint matrixId)
{
	float3 ln[3];
	ln[0] = mul(normals[indices[vertBaseId + 0]], normalMatrices[matrixId]);
	ln[1] = mul(normals[indices[vertBaseId + 1]], normalMatrices[matrixId]);
	ln[2] = mul(normals[indices[vertBaseId + 2]], normalMatrices[matrixId]);
	return normalize(interpolateVertices(bary, ln));
}

float2 getTextureLocation(float2 bary, uint vertBaseId)
{
	float2 lt[3];
	lt[0] = texVerts[indices[vertBaseId + 0]];
	lt[1] = texVerts[indices[vertBaseId + 1]];
	lt[2] = texVerts[indices[vertBaseId + 2]];
	return pointOnTriangle(bary, lt);
}

void getVertexWorldCoordinates(inout float3 lv[3], uint vertBaseId, uint matrixId)
{
	lv[0] = mul(float4(verts[indices[vertBaseId + 0]], 1.f), matrices[matrixId]);
	lv[1] = mul(float4(verts[indices[vertBaseId + 1]], 1.f), matrices[matrixId]);
	lv[2] = mul(float4(verts[indices[vertBaseId + 2]], 1.f), matrices[matrixId]);
}

void AddContribution(uint pixLaunchIndex, float3 contrib)
{
	uint3 uContrib = floatToFixed(contrib, ConvRangeBits);
	InterlockedAdd(gIrradianceDS[pixLaunchIndex].value.x, uContrib.x);
	InterlockedAdd(gIrradianceDS[pixLaunchIndex].value.y, uContrib.y);
	InterlockedAdd(gIrradianceDS[pixLaunchIndex].value.z, uContrib.z);
}