struct FaceAttributes
{
	uint MaterialId;
	uint AreaLightId;
	uint2 padding;
};

struct AreaLight {
	float4 Intensity;
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
};

// RT Specific
uint getFaceIndex()
{
	return InstanceID() / 3 + PrimitiveIndex();
}