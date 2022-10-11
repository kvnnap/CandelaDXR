#ifndef IRRADIANCE_ITEM
#define IRRADIANCE_ITEM

struct IrradianceItem
{
	uint3 value;
	uint padding;
};

struct IrradianceItemFloat
{
	float3 value;
	float padding;
};

#endif