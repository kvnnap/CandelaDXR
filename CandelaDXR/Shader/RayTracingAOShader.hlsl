struct ConstBuff
{
	float3 cameraPos;
};

struct ShadowPayload
{
	bool occluded;
};

// UAVs
RWTexture2D<float> gCdfMask : register(u0);

// SRVs
RaytracingAccelerationStructure gRtScene : register(t0);
Texture2D<float4> gPos : register(t1);
Texture2D<float4> gNorm : register(t2);
Texture2D<float4> gOutput : register(t3);

// CBVs
cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	gCdfMask[launchIndex] = 0.f;

	// Early-exit checks
	if (gOutput[launchIndex].w == 0.f || gNorm[launchIndex].w == 0.f)
		return;

	// Construct Ray
	RayDesc shadowRay;
	shadowRay.TMin = 0.001f;
	shadowRay.TMax = 0.999f;
	shadowRay.Origin = gPos[launchIndex].xyz;
	shadowRay.Direction = cBuffer.cameraPos - shadowRay.Origin;

	ShadowPayload shadowPayload;
	shadowPayload.occluded = true;
	TraceRay(
		gRtScene,	// Acceleration Structure
		RAY_FLAG_FORCE_OPAQUE
		| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
		| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		0,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
		0,			// MultiplierForGeometryContributionToShaderIndex (We only have 1 hit group)
		0,			// Miss shader index (within the shader table) (calls shadowMiss)
		shadowRay,
		shadowPayload);
	if (shadowPayload.occluded)
		return;

	gCdfMask[launchIndex] = 1.f;
}

// Shadow
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
	payload.occluded = false;
}
