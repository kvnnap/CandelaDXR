#include "Scene.hlsli"

StructuredBuffer<FaceAttributes> faceAttributes : register(t1); 
StructuredBuffer<float4x3> matrices : register(t7);

cbuffer CB1 : register(b0)
{
	matrix MVP;
}

cbuffer CB2 : register(b1)
{
	uint groupId;
	uint instanceId;
}

struct MyInput
{
	float4 pos : POSITION;
	float4 normal : NORMAL;
};

struct MyOutput
{
	float4 Position : SV_POSITION;
	float4 Pos : VS_POSITION;
	float4 Normal : VS_NORMAL;
};

MyOutput main(MyInput myInput)
{
	float4x3 lToW = matrices[groupId];
	float3 tPos = mul(myInput.pos, lToW);
	MyOutput myOutput;
	myOutput.Pos = float4(tPos, 1.f);
	myOutput.Position = mul(myOutput.Pos, MVP);
	myOutput.Normal = myInput.normal;
	return myOutput;
}
