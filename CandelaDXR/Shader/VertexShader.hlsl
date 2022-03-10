#include "Scene.hlsli"

StructuredBuffer<FaceAttributes> faceAttributes : register(t1); 
StructuredBuffer<float4x3> matrices : register(t7);

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
	float4x3 lToW = matrices[groupId];
	float3 worldPos = mul(float4(myInput.pos, 1.f), lToW);
	MyOutput myOutput;
	myOutput.Position = mul(float4(worldPos, 1.f), ViewPerspective);
	myOutput.Pos = worldPos;
	myOutput.Normal = mul(float4(myInput.normal, 0.f), lToW);
	myOutput.TexUV = myInput.texuv;
	return myOutput;
}
