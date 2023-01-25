#ifndef SCENE_HLSLI
#define SCENE_HLSLI

#define LT_UNDEFINED 0
#define LT_DIRECTIONAL 1
#define LT_POINT 2
#define LT_SPOT 3
#define LT_AMBIENT 4
#define LT_AREA 5

//enum LightType : uint
//{
//	Undefined = 0,
//	Directional = 1,
//	Point = 2,
//	Spot = 3,
//	Ambient = 4,
//	Area = 5
//};

struct FaceAttributes
{
	uint MaterialId;
	uint MeshIndex;
	uint padding[2];
};

struct AreaLight
{
	uint InstanceIndex;
	uint PrimitiveId;
	uint MaterialId;
	uint padding;
};

struct ExternalLight
{
	float4 Position;
	float4 Direction;
	float4 Up;
	float3 Attenuation;
	uint Type;
	float3 Diffuse;
	float InnerConeAngle;
	float3 Specular;
	float OuterConeAngle;
	float2 AreaDimensions;
};

struct SpecularPrimitive
{
	uint InstanceIndex;
	uint PrimitiveId;
	uint MaterialId;
	uint padding;
};

struct Material
{
	float3 Diffuse;
	int DiffuseTextureId;
	float3 Emissive;
	int EmissiveTextureId;
	float3 Specular;
	int SpecularTextureId;
	float3 TransmissiveFilter;
	float RefractiveIndex;
	float Dissolve;
	uint EmissiveType;
	uint2 padding;
};

#endif