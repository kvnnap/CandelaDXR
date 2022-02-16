// Output texture
RWTexture2D<float4> gOutput : register(u0);


struct RayPayload
{
	float3 color;
};

//struct IndirectPayload
//{
//	uint instanceIndex;
//	uint primitiveId;
//	float tHit;
//	float2 bary;
//};

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	gOutput[launchIndex] = float4((float)launchIndex.x / launchDim.x, 0.f, 0.f, 1.f);
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, RayTCurrent(), 1.f);
}

//[shader("closesthit")]
//void indirectChs(inout IndirectPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
//{
//	payload.instanceIndex = InstanceIndex();
//	payload.primitiveId = getPrimitiveIndex();
//	payload.tHit = RayTCurrent();
//	payload.bary = attribs.barycentrics;
//}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}

//[shader("miss")]
//void indirectMiss(inout IndirectPayload payload)
//{
//	payload.tHit = -1.f;
//}
