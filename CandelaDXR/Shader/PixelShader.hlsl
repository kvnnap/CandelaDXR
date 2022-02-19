#include "Utils.hlsli"
#include "Scene.hlsli"

struct Camera {
	float4 position;
};

struct ConstBuff {
	matrix MVP;
	Camera camera;
};

cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

StructuredBuffer<Material> materials : register(t0);
StructuredBuffer<FaceAttributes> faceAttributes : register(t1);
StructuredBuffer<AreaLight> lights : register(t2);

StructuredBuffer<float3> verts : register(t3);
StructuredBuffer<float2> texVerts : register(t4);
StructuredBuffer<float3> normals : register(t5);
StructuredBuffer<uint3> indices : register(t6);

struct MyInput
{
	uint id : SV_PrimitiveID;
	float4 position : VS_POSITION;
	float4 normal : VS_NORMAL;
};

float4 main(MyInput myInput) : SV_TARGET
{
	if (dot(cBuffer.camera.position - myInput.position, normalize(myInput.normal)) < 0.f)
		return float4(0.f, 0.f, 0.f, 1.f);
	uint matId = faceAttributes[myInput.id].MaterialId;
	Material mat = materials[matId];
	
	float3 total = float3(0.f, 0.f, 0.f);

	// Calculate lights
	for (uint i = 0; i < 2; ++i)
	{
		AreaLight light = lights[i];
		// Get area light primitive
		uint3 index = indices[light.PrimitiveId];
		// Use just one point
		float3 lightPos = verts[index.x];
		float3 lightNorm = normals[index.x];
		Material lightMat = materials[light.MaterialId];

		// Shadow ray
		float3 shadowRay = lightPos - myInput.position.xyz;
		float primDot = dot(myInput.normal.xyz, shadowRay);
		float lightDot = -dot(lightNorm, shadowRay);
		
		if (primDot < 0 || lightDot < 0)
			continue;

		float len = length(shadowRay);
		total += lightMat.Emissive * mat.Diffuse * primDot * lightDot / (len * len * 3.141f);
	}

	return float4(linearToSrgb(toneMap(mat.Emissive + total)), 1.0f);
}
