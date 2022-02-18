// Output texture
RWTexture2D<float4> gOutput : register(u0);

RaytracingAccelerationStructure gRtScene : register(t0);
Texture2D gTextures[]: register(t6);

struct ConstBuff {
	float3 u, v, w;
	float3 position;
	float3 direction;
	float3 plane;
};

cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

struct RayPayload
{
	float3 color;
};

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	// Setup Ray
	RayDesc ray;
	ray.TMin = 0.f;
	ray.TMax = 3.402823e+38;
	ray.Origin = cBuffer.position;
	const float2 ratio = (launchIndex + float2(0.5f, 0.5f)) / launchDim;
	const float2 filmPlanePosition = float2(cBuffer.plane.x * (ratio.x - 0.5f), cBuffer.plane.y * (0.5f - ratio.y));
	const float3 pointOnObjectPlane = ray.Origin + cBuffer.w * cBuffer.plane.z + cBuffer.u * filmPlanePosition.x + cBuffer.v * filmPlanePosition.y;
	ray.Direction = normalize(pointOnObjectPlane - ray.Origin);
	RayPayload payload;

	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		0,			// RayContributionToHitGroupIndex (calls chs)
		2,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table) (calls miss)
		ray,
		payload);

	//gOutput[launchIndex] = float4((float)launchIndex.x / launchDim.x, 0.f, 0.f, 1.f);
	gOutput[launchIndex] = float4(payload.color, 1.f);
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, RayTCurrent(), 1.f);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}
