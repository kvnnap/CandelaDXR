struct FaceAttributes
{
	uint MaterialId;
	uint AreaLightId;
	uint2 padding;
};

//struct alignas(16) AreaLight {
//	DirectX::XMVECTOR Intensity;
//	std::uint32_t InstanceIndex;
//	std::uint32_t PrimitiveId;
//	std::uint32_t MaterialId;
//};

struct Material
{
	float3 Diffuse;
	int DiffuseTextureId;
	float3 Emissive;
	int EmissiveTextureId;
};

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
	return float4(mat.Diffuse, 1.0f);
}
