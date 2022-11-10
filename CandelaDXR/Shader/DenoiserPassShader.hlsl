#include "Utils.hlsli"
#include "Scene.hlsli"

#include "NRDEncoding.hlsli"
#define NRD_HEADER_ONLY
#include "NRD.hlsli"

cbuffer CB1 : register(b0)
{
	float4 hitDistParams;
	float camZ;
	uint mode;
}

Texture2D<float4> diffRadAcc : register(t0);
Texture2D<float4> specRadAcc : register(t1);
Texture2D<float4> albedo : register(t2);
Texture2D<float4> normal : register(t3);
Texture2D<float> depth : register(t4);
Texture2D<float4> gRayHitT : register(t5);
Texture2D<float4> out_diff_radiance_hitdist : register(t6);
Texture2D<float4> out_spec_radiance_hitdist : register(t7);
Texture2D<float4> position : register(t8);
Texture2D<uint2> meshInfo : register(t9);
StructuredBuffer<float4x3> matrices : register(t10); // WorldToLocalToPrevWorld
StructuredBuffer<Material> materials : register(t11);

RWTexture2D<float4> in_mv : register(u0);
RWTexture2D<float4> in_normal_roughness : register(u1);
RWTexture2D<float> in_view_z : register(u2);
RWTexture2D<float4> in_diff_radiance_hitdist : register(u3);
RWTexture2D<float4> in_spec_radiance_hitdist : register(u4);
RWTexture2D<float4> gOutputDiff : register(u5);
RWTexture2D<float4> gOutputSpec : register(u6);

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

		// TODO: Roughness is equal to dissolve value, is this correct? Should be a very crude approx.
		// Not sure. If sample was diffuse, we should 1.f and if sample was specular, we should use 0.f; I guess.
		const Material mat = materials[meshInfo[DTid.xy].x];
		in_normal_roughness[DTid.xy] = NRD_FrontEnd_PackNormalAndRoughness(normal[DTid.xy].xyz, mat.Dissolve);

		// Can skip some denoising steps here by providing
		// providing viewZ > CommonSettings::denoisingRange (default 500000.0f) (black image)
		float vz = position[DTid.xy].z - camZ;
		in_view_z[DTid.xy] = vz;

		float3 radAcc = diffRadAcc[DTid.xy].xyz;
		float3 rad = albedo[DTid.xy].xyz / PI;
		if (rad.x > 0)
			rad.x = radAcc.x / rad.x;
		if (rad.y > 0)
			rad.y = radAcc.y / rad.y;
		if (rad.z > 0)
			rad.z = radAcc.z / rad.z;

		// Normalize hit distance..
		float normHitDist = REBLUR_FrontEnd_GetNormHitDist(gRayHitT[DTid.xy].x, vz, hitDistParams, 1.0f);
		in_diff_radiance_hitdist[DTid.xy] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(rad, normHitDist);

		// Spec - no material additional stuff in our case?
		normHitDist = REBLUR_FrontEnd_GetNormHitDist(gRayHitT[DTid.xy].y, vz, hitDistParams, 1.0f);
		in_spec_radiance_hitdist[DTid.xy] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(specRadAcc[DTid.xy].xyz, normHitDist);
	}
	else if (mode == 1)
	{
		float4 result = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(out_diff_radiance_hitdist[DTid.xy]);
		gOutputDiff[DTid.xy] = float4(result.xyz * (albedo[DTid.xy].xyz / PI), 1.f);

		result = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(out_spec_radiance_hitdist[DTid.xy]);
		gOutputSpec[DTid.xy] = float4(result.xyz, 1.f);
	}
}
