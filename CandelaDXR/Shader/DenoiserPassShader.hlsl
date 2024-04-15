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
	uint denoiserSelected;
	uint denoiseCaustics;
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
RWTexture2D<float4> causRadAcc : register(u7);
RWTexture2D<float4> diffUnmerged : register(u8);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (mode == 0)
	{
		uint meshInstanceId = meshInfo[DTid.xy].y; // y is instanceId
		float3 pixWorldPos = position[DTid.xy].xyz;
		float3 prevPoint = mul(float4(pixWorldPos, 1.f), matrices[meshInstanceId]);
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

		float3 radAcc = denoiseCaustics == 0 ? diffRadAcc[DTid.xy].xyz : causRadAcc[DTid.xy].xyz;
		float3 rad = albedo[DTid.xy].xyz / PI;
		if (rad.x > 0)
			rad.x = radAcc.x / rad.x;
		if (rad.y > 0)
			rad.y = radAcc.y / rad.y;
		if (rad.z > 0)
			rad.z = radAcc.z / rad.z;
		float3 specRad = specRadAcc[DTid.xy].xyz;

		// Normalize hit distance..
		float diffHitDist = denoiseCaustics == 0 ? gRayHitT[DTid.xy].x : gRayHitT[DTid.xy].z;
		float specHitDist = gRayHitT[DTid.xy].y;
		if (denoiserSelected == 0)
		{
			float normHitDist = REBLUR_FrontEnd_GetNormHitDist(diffHitDist, vz, hitDistParams, mat.Dissolve);
			in_diff_radiance_hitdist[DTid.xy] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(rad, normHitDist);
			// Spec - no material additional stuff in our case?
			normHitDist = REBLUR_FrontEnd_GetNormHitDist(specHitDist, vz, hitDistParams, mat.Dissolve);
			in_spec_radiance_hitdist[DTid.xy] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(specRad, normHitDist);
		}
		else if (denoiserSelected == 1)
		{
			in_diff_radiance_hitdist[DTid.xy] = RELAX_FrontEnd_PackRadianceAndHitDist(rad, diffHitDist);
			in_spec_radiance_hitdist[DTid.xy] = RELAX_FrontEnd_PackRadianceAndHitDist(specRad, specHitDist);
		}
	}
	else if (mode == 1)
	{
		float4 diffResult = 0.f;
		float4 specResult = 0.f;
		if (denoiserSelected == 0)
		{
			diffResult = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(out_diff_radiance_hitdist[DTid.xy]);
			specResult = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(out_spec_radiance_hitdist[DTid.xy]);
		}
		else if (denoiserSelected == 1)
		{
			diffResult = RELAX_BackEnd_UnpackRadiance(out_diff_radiance_hitdist[DTid.xy]);
			specResult = RELAX_BackEnd_UnpackRadiance(out_spec_radiance_hitdist[DTid.xy]);
		}

		float3 decodedDiff = diffResult.xyz * (albedo[DTid.xy].xyz / PI);
        if (denoiseCaustics != 0)
        {
            causRadAcc[DTid.xy] = float4(decodedDiff.xyz, 1.f);
            diffUnmerged[DTid.xy] = float4(diffRadAcc[DTid.xy].xyz, 1.f);
            decodedDiff += diffRadAcc[DTid.xy].xyz;
        }
		else
        {
			// This output is only used for the paper
            diffUnmerged[DTid.xy] = float4(decodedDiff.xyz, 1.f);
            decodedDiff += causRadAcc[DTid.xy].xyz;
        }
		
		gOutputDiff[DTid.xy] = float4(decodedDiff, 1.f);
		gOutputSpec[DTid.xy] = float4(specResult.xyz, 1.f);
	}
}
