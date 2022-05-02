#include "Utils.hlsli"
#include "Scene.hlsli"

struct Camera {
	float4 position;
};

struct ConstBuff {
	matrix ViewPerspective;
	Camera camera;
	uint numLights;
};

cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

cbuffer CB2 : register(b1)
{
	uint groupId;
	uint instanceId;
}

StructuredBuffer<Material> materials : register(t0);
StructuredBuffer<FaceAttributes> faceAttributes : register(t1);
StructuredBuffer<AreaLight> lights : register(t2);
StructuredBuffer<float3> verts : register(t3);
StructuredBuffer<float2> texVerts : register(t4);
StructuredBuffer<float3> normals : register(t5);
StructuredBuffer<uint> indices : register(t6);
StructuredBuffer<float4x3> matrices : register(t7);
StructuredBuffer<float3x3> normalMatrices : register(t8);
Texture2D<float3> gTextures[]: register(t9);

// Sampler
SamplerState gSampler : register(s0);

// Functions
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

struct MyInput
{
	uint id : SV_PrimitiveID;
	float3 position : VS_POSITION;
	float3 normal : VS_NORMAL;
	float2 texUV : VS_TEXUV;
};

struct MyOutput
{
	float4 radiance : SV_Target0;
	float4 position : SV_Target1;
	float4 normal : SV_Target2;
	float4 albedo : SV_Target3;
};

MyOutput main(MyInput myInput)
{
	MyOutput output;
	output.radiance = float4(0.f, 0.f, 0.f, 1.f);
	output.position = float4(myInput.position, 1.f);
	output.normal = float4(normalize(myInput.normal), 1.f);
	output.albedo = 0;

	if (dot(cBuffer.camera.position.xyz - myInput.position, normalize(myInput.normal)) < 0.f)
		return output;
	const uint matId = faceAttributes[instanceId / 3 + myInput.id].MaterialId;
	const Material mat = materials[matId];

	const float2 midpointBary = 1.f / 3.f;
	float3 total = 0.f;

	float3 diffTex = mat.Diffuse;
	if (mat.DiffuseTextureId >= 0)
		diffTex *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, myInput.texUV, 0);
	output.albedo = float4(diffTex * mat.Dissolve, mat.RefractiveIndex);

	// Calculate lights
	for (uint i = 0; i < cBuffer.numLights; ++i)
	{
		const AreaLight light = lights[i];
		const uint lightIndexId = light.PrimitiveId * 3;

		float3 lv[3];
		getVertexWorldCoordinates(lv, lightIndexId, light.InstanceIndex);
		const float3 lightPos = interpolateVertices(midpointBary, lv); // = (lv[0] + lv[1] + lv[2]) / 3.f;
		const float3 unitLightNorm = getUnitNormal(midpointBary, lightIndexId, light.InstanceIndex);
		const Material lightMat = materials[light.MaterialId];

		// Shadow ray
		const float3 shadowRay = lightPos - myInput.position.xyz;
		const float invShadLen = 1.f / length(shadowRay);
		const float3 unitShadowRay = shadowRay * invShadLen;
		const float primDot = dot(normalize(myInput.normal.xyz), unitShadowRay);
		const float lightDot = -dot(unitLightNorm, unitShadowRay);

		if (primDot <= 0.f || lightDot <= 0.f)
			continue;

		// Also apply any emission from texture
		float3 lightEmissive = lightMat.Emissive;
		if (lightMat.EmissiveTextureId >= 0)
			lightEmissive *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(midpointBary, lightIndexId), 0);
		
		// Fresnel and Dissolve (contributing towards Diffuse reflection)
		const float frAndDissolve = (1.f - fresnel(primDot, 1.f, mat.RefractiveIndex)) * mat.Dissolve;

		const float triArea = getTriangleArea(lv);
		total += lightEmissive * diffTex * (frAndDissolve * triArea * OneOverPI * primDot * lightDot * invShadLen * invShadLen);
	}
	
	output.radiance = float4(mat.Emissive + total, 1.f);
	return output;
}
