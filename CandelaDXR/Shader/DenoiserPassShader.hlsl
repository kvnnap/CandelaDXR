#include "Utils.hlsli"

#include "NRDEncoding.hlsli"
#define NRD_HEADER_ONLY
#include "NRD.hlsli"

cbuffer CB1 : register(b0)
{
	float near;
	float far;
	float camZ;
	uint mode;
}

Texture2D<float4> radAccumulator : register(t0);
Texture2D<float4> albedo : register(t1);
Texture2D<float4> normal : register(t2);
Texture2D<float> depth : register(t3);
Texture2D<float4> pt_rad_hitt : register(t4);
Texture2D<float4> out_diff_radiance_hitdist : register(t5);
Texture2D<float4> position : register(t6);
Texture2D<uint2> meshInfo : register(t7);
StructuredBuffer<float4x3> matrices : register(t8); // WorldToLocalToPrevWorld

RWTexture2D<float4> in_mv : register(u0);
RWTexture2D<float4> in_normal_roughness : register(u1);
RWTexture2D<float> in_view_z : register(u2);
RWTexture2D<float4> in_diff_radiance_hitdist : register(u3);
RWTexture2D<float4> gOutput : register(u4);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (mode == 0)
	{
		uint meshGroupId = meshInfo[DTid.xy].y; // y is groupId
		float3 pixWorldPos = position[DTid.xy].xyz;
		float3 prevPoint = mul(float4(pixWorldPos, 1.f), matrices[meshGroupId]);
		float3 motion = prevPoint - pixWorldPos;

		// Check PrimaryRays.cd.hlsl:106 - motion * STL::Math::LinearStep( 0.0, 0.0000005, abs( motion ) );
		// has the effect of a quadratic curve for value components less than 0.0000005 ? Smoothing maybe? 
		in_mv[DTid.xy] = float4(motion * saturate(abs(motion) / 0.0000005), 0.f);

		// TODO: Roughness is always one here, alter with material!
		in_normal_roughness[DTid.xy] = NRD_FrontEnd_PackNormalAndRoughness(normal[DTid.xy].xyz, 1.f);

		// Can skip some denoising steps here by providing
		// providing viewZ > CommonSettings::denoisingRange (default 500000.0f) (black image)
		in_view_z[DTid.xy] = position[DTid.xy].z - camZ;

		float3 radAcc = radAccumulator[DTid.xy].xyz;
		float3 rad = albedo[DTid.xy].xyz;
		if (rad.x > 0)
			rad.x = radAcc.x / rad.x;
		if (rad.y > 0)
			rad.y = radAcc.y / rad.y;
		if (rad.z > 0)
			rad.z = radAcc.z / rad.z;

		in_diff_radiance_hitdist[DTid.xy] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(rad, pt_rad_hitt[DTid.xy].w);
	}
	else if (mode == 1)
	{
		float4 result = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(out_diff_radiance_hitdist[DTid.xy]);
		gOutput[DTid.xy] = float4(result.xyz * albedo[DTid.xy].xyz, 1.f);
	}
}
