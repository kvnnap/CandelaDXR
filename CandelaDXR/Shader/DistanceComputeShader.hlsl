#include "Scene.hlsli"

cbuffer CB1 : register(b0)
{
	uint2 ScreenDim;
	uint InputIndex;
	uint OutputIndex;
}

cbuffer CB1 : register(b0, space1)
{
	float3 LightPosition;
	float3 LightPlane;
	float3 LightPlaneU;
	float3 LightPlaneV;
	float3 CamPosition;
	float3 CamUnitDir;
	uint padding;
	uint Mode;
	uint Orthographic;
	uint SinglePointSource;
}

Texture2D<float4> input[] : register(t0);
RWTexture2D<float> output[] : register(u0);

Texture2D<uint2> meshInfo : register(t0, space1);
Texture2D<float4> gNormals : register(t1, space1);
Texture2D<float> cdfMask : register(t2, space1);
StructuredBuffer<Material> materials : register(t3, space1);

// 64 threads per group should be optimal on both NVIDIA and AMD
// i.e. 2 warps per thread group or 1 depending on NVIDIA/AMD
[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid.x >= ScreenDim.x || DTid.y >= ScreenDim.y)
		return;

	// Generate coefficient - can generate on the host one-time (more precise and less computation on the gpu)
	float result = 1.f;
	float3 lightPos = LightPosition;

	float2 ratio = LightPlane.xy / ScreenDim;
	ratio.y = -ratio.y;
	float2 halfPlane = LightPlane.xy * 0.5f;
	float2 planePt = float2(-halfPlane.x, halfPlane.y) + ratio * DTid.xy; // UVs relative to the centre of the plane

	if (Orthographic == 0)
	{
		float3 worldPt = float3(planePt, LightPlane.z); // Origin is (0,0,0) and normal (0,0,1)
		float3 vecDir = worldPt;
		float invDistance = 1.f / length(vecDir);
		vecDir *= invDistance;
		float coeff = vecDir.z * vecDir.z * invDistance * invDistance; // cos weighted solid angle approx
		result = coeff;
	}
	else
	{
		lightPos += planePt.x * LightPlaneU + planePt.y * LightPlaneV;
	}

	// Load gBuffer values
	const float4 inData = input[InputIndex][DTid.xy];
	const float3 gPos = inData.xyz;
	const float3 gNorm = gNormals[DTid.xy].xyz;
	const Material gMat = materials[meshInfo[DTid.xy].x];
	const float3 dir = gPos - lightPos;
	const float lenDir = length(dir);
	const float noValue = SinglePointSource == 0 ? 0.015625f : 0.f;

	//
	if (Mode == 0)
	{
		result *= inData.w == 1.f ? lenDir * lenDir : noValue;
	}
	else if (Mode > 0)
	{
		// Uncomment for view-direction-dependency
		float cameraDot = 1.f; // dot(CamUnitDir, normalize(gPos - CamPosition));
		float areaDot = dot(gNorm, normalize(CamPosition - gPos));
		float camAreaLenInv = 1.f / length(CamPosition - gPos);
		if (cameraDot < 0.f || (gMat.Dissolve >= 1.f && areaDot < 0.f) || (Mode == 2 && cdfMask[DTid.xy] == 0.f))
			result *= noValue;
		else
			result *= max(noValue, cameraDot * abs(areaDot) * camAreaLenInv * camAreaLenInv * lenDir * lenDir);
	}

	output[OutputIndex][DTid.xy] = result;
}
