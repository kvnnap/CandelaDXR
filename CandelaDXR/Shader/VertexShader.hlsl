#include "Scene.hlsli"

StructuredBuffer<FaceAttributes> faceAttributes : register(t1); 
StructuredBuffer<float4x3> matrices : register(t7);
StructuredBuffer<float3x3> normalMatrices : register(t8);

cbuffer CB1 : register(b0)
{
	matrix ViewPerspective;
}

cbuffer CB2 : register(b1)
{
	uint groupId;
	uint instanceId;
}

struct MyInput
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 texuv : TEXUV;
};

struct MyOutput
{
	float4 Position : SV_POSITION;
	float3 Pos : VS_POSITION;
	float3 Normal : VS_NORMAL;
	float2 TexUV : VS_TEXUV;
};

MyOutput main(MyInput myInput)
{
	float3 worldPos = mul(float4(myInput.pos, 1.f), matrices[groupId]);
	MyOutput myOutput;
	myOutput.Position = mul(float4(worldPos, 1.f), ViewPerspective);
	myOutput.Pos = worldPos;
	myOutput.Normal = normalize(mul(myInput.normal, normalMatrices[groupId]));
	myOutput.TexUV = myInput.texuv;
	return myOutput;
}
