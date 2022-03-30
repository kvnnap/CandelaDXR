struct FaceAttributes
{
	uint MaterialId;
	uint AreaLightId;
	uint InstanceIndex;
	uint padding;
};

struct AreaLight
{
	uint InstanceIndex;
	uint PrimitiveId;
	uint MaterialId;
	uint padding;
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
