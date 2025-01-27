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

#ifdef __cplusplus

namespace candela::scene
{
	struct alignas(16) FaceAttributes
	{
		std::uint32_t MaterialId;
		std::uint32_t MeshIndex;
	};

	// TODO: Change name to EmissivePrimitive
	struct alignas(16) AreaLight
	{
		std::uint32_t InstanceIndex;
		std::uint32_t PrimitiveId;
		std::uint32_t MaterialId;
	};

	// Same as ExternalLight
	struct alignas(16) Light
	{
		mathematics::Vector Position;
		mathematics::Vector Direction;
		mathematics::Vector Up; // matches V
		mathematics::Vector Right; // matches U = Direction CROSS Up - Custom
		mathematics::Vector3 Attenuation; // 1.f / ([0] + [1]*d + [2]*d^2) - d is distance
		std::uint32_t Type;
		mathematics::Vector3 Diffuse;
		float InnerConeAngle;
		mathematics::Vector3 Specular;
		float OuterConeAngle;
		mathematics::Vector2 AreaDimensions;
	};

	struct alignas(16) SpecularPrimitive
	{
		std::uint32_t InstanceIndex;
		std::uint32_t PrimitiveId;
		std::uint32_t MaterialId;
	};

	struct alignas(16) Material
	{
		mathematics::Vector3 Diffuse;
		std::int32_t DiffuseTextureId;
		mathematics::Vector3 Emissive;
		std::int32_t EmissiveTextureId;
		mathematics::Vector3 Specular;
		std::int32_t SpecularTextureId;
		mathematics::Vector3 TransmissiveFilter;
		float RefractiveIndex;
		float Dissolve;
		std::uint32_t EmissiveType; // This should move in the AreaLight struct but is easier here

		bool isEmissive() const;
		bool isSpecular() const;
	};
}

#else

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
	float4 Right;
	float3 Attenuation;
	uint Type;
	float3 Diffuse;
	float InnerConeAngle;
	float3 Specular;
	float OuterConeAngle;
	float2 AreaDimensions;
	uint2 padding;
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
#endif
