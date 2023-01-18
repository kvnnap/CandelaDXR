#include "Utils.hlsli"
#include "Scene.hlsli"
#include "IrradianceItem.hlsli"
#include "LightTracingVars.hlsli"

#define UINT_MAX 0xFFFFFFFF

struct ConstBuff2
{
	// Light Camera
	float3 plane; // sensor dimensions (z contains distance to sensor plane)

	// Other
	uint2 lightCamDim;
	uint lightIndex;
	float lightCamPdf;
};

// CBVs
cbuffer CB2 : register(b0, space1)
{
	ConstBuff2 lBuff;
}

Texture2D<float> cdf[] : register(t0, space1);

struct RayPayload
{
	float2 bary;
	float t;
	uint faceIndex;
	uint instanceIndex;
};

struct ShadowPayload
{
	bool occluded;
};

uint2 getLightTexIndex(uint id)
{
	return uint2(id % lBuff.lightCamDim.x, id / lBuff.lightCamDim.x);
}

uint2 sampleImportanceMap(inout uint seed, out float pdf, uint cdfIndex)
{
	pdf = 0.f;

	if (cdf[cdfIndex][lBuff.lightCamDim - 1] != 1.f) // 0 means nothing in view
		return lBuff.lightCamDim;

	// Ensure r <= 1.f
	float r = rand_next(seed) * 0.999999940395355224609375f; // 0x3f7fffff 

	// Binary Search with Duplicates - https://jsfiddle.net/kvnnap/j7as6un3
	const uint size = lBuff.lightCamDim.x * lBuff.lightCamDim.y;
	uint min = 0;
	uint max = size;

	while (min < max)
	{
		uint mid = (min + max) >> 1;
		if (r >= cdf[cdfIndex][getLightTexIndex(mid)])
			min = mid + 1;
		else
			max = mid;
	}

	if (min == size) // Should be an impossible case now
		return lBuff.lightCamDim;

	// Min contains result
	uint2 uv = getLightTexIndex(min);
	pdf = cdf[cdfIndex][uv] - (min == 0 ? 0.f : cdf[cdfIndex][getLightTexIndex(min - 1)]);

	return uv;
}

bool sampleImpMapWithCosCDF(inout uint seed, inout RayDesc ray, inout float pdf, out float coeff, float3 unitLightNormal, uint cdfIndex)
{
	// Light Camera basis
	float3 w = unitLightNormal;
	float3 u = cross(w, createPerpendicularVector(w));
	float3 v = cross(u, w);

	// Check if ray intersects light camera raster square
		// compute denominator
	const float den = dot(w, ray.Direction);
	coeff = 1.f;

	// make this check or else risk of division by zero..
	if (den == 0.f)
		return false;

	const float3 posPlane = ray.Origin + w * lBuff.plane.z;
	const float t = dot(w, (posPlane - ray.Origin)) / den;
	if (t < ray.TMin || t > ray.TMax)
		return false;

	const float3 R = ray.Origin + ray.Direction * t - posPlane;
	float2 pt = float2(dot(R, u), dot(R, v));

	// Check if point is in bounds or not
	float2 halfPlane = lBuff.plane.xy * 0.5f;
	if (any(pt > halfPlane) || any(pt < -halfPlane))
		return false;

	// Ok in bounds, sample texel in texture
	float localPdf;
	uint2 texel = sampleImportanceMap(seed, localPdf, cdfIndex);
	if (localPdf == 0.f)
		return false;

	float2 ratio = lBuff.plane.xy / lBuff.lightCamDim;
	float texelArea = ratio.x * ratio.y;
	ratio.y = -ratio.y;

	// Convert texel boundary coordinates to world space 
	float2 planePt = float2(-halfPlane.x, halfPlane.y) + ratio * (texel + float2(rand_next(seed), rand_next(seed)));
	float3 worldPt = posPlane + planePt.x * u + planePt.y * v;
	ray.Direction = worldPt - ray.Origin;

	float invDistance = 1.f / length(ray.Direction);
	ray.Direction *= invDistance;
	coeff = dot(w, ray.Direction) * invDistance * invDistance;
	pdf = localPdf * lBuff.lightCamPdf / texelArea;
	return true;
}

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	//AddContribution(launchIndex.y * cBuffer.winDim.x + launchIndex.x, cdf[launchIndex]);
	//return;

	// Early-exit checks
	if (cBuffer.numLights == 0)
		return;

	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * (cBuffer.frameNumber + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * (cBuffer.frameNumber + 0) + launchIndex.y);

	// Choose light source
	const uint lightIndex = lBuff.lightIndex == UINT_MAX ? chooseInRange(seed, 0, cBuffer.numLights - 1) : lBuff.lightIndex;
	const uint lightIndexId = lights[lightIndex].PrimitiveId * 3;
	AreaLight areaLight = lights[lightIndex];
	Material lightMat = materials[areaLight.MaterialId];
	const bool lightDirectional = lightMat.EmissiveType == 1;

	// Compute light vertices
	float3 lv[3];
	getVertexWorldCoordinates(lv, lightIndexId, areaLight.InstanceIndex);

	// Generate a point on the light
	float2 lightBary;
	const float3 pointOnLightSource = samplePointOnTriangle(seed, lv, lightBary);

	// Compute MC Coefficients
	float3 localContribution = lightMat.Emissive;
	localContribution *= getTriangleArea(lv) * cBuffer.numLights;

	if (lightMat.EmissiveTextureId >= 0)
		localContribution *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(lightBary, lightIndexId), 0);

	// First check if light normal is the right way round wrt camera
	const float3 unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);

	// Construct ray from light source to camera origin
	RayDesc shadowRay;
	shadowRay.TMin = 0.001f;
	shadowRay.TMax = 1.f;
	shadowRay.Origin = pointOnLightSource;
	shadowRay.Direction = cBuffer.position - pointOnLightSource;
	float invShadowDistance = 1.f / length(shadowRay.Direction);
	float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;

	uint2 pixel = uint2(0, 0);
	float lightDot = dot(unitShadowRayDirection, unitLightNormal);
	float cameraDot = -dot(unitShadowRayDirection, cBuffer.w);

	ShadowPayload shadowPayload;

	uint i = 1;

	// Path filter
	PathInteraction prevStateFlags = Light;

	if (false && (prevStateFlags & cBuffer.pathFilter) != 0 && i >= cBuffer.minBounces && (i <= cBuffer.maxBounces || cBuffer.maxBounces == 0) && !lightDirectional && lightDot > 0.f && cameraDot > 0.f)
	{
		if (getPixel(shadowRay, cBuffer.winDim, pixel))
		{
			// Add direct light contribution
			shadowPayload.occluded = true;
			TraceRay(
				gRtScene,	// Acceleration Structure
				RAY_FLAG_FORCE_OPAQUE
				| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
				| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,			// Ray flags
				0xFF,		// Instance inclusion Mask (0xFF includes everything)
				1,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
				1,			// MultiplierForGeometryContributionToShaderIndex (We only have 1 hit group)
				1,			// Miss shader index (within the shader table) (calls shadowMiss)
				shadowRay,
				shadowPayload);

			if (!shadowPayload.occluded)
			{
				const uint pixLaunchIndex = pixel.y * cBuffer.winDim.x + pixel.x;
				float3 contrib = localContribution * lightDot * invShadowDistance * invShadowDistance * cameraDot;
				AddContribution(pixLaunchIndex, contrib);
			}
		}
	}

	// Construct ray from light source to random scene point
	float pdf;
	RayDesc ray;
	ray.TMin = 0.001f;
	ray.TMax = 3.402823e+38;
	ray.Origin = shadowRay.Origin;
	ray.Direction = unitLightNormal;
	if (!lightDirectional)
	{
		ray.Direction = randomRayLobe(seed, unitLightNormal, 1, pdf);

		float coeff = 1.f;

		// If this succeeds, ray.Direction, coeff and pdf will be updated
		sampleImpMapWithCosCDF(seed, ray, pdf, coeff, unitLightNormal, lBuff.lightIndex == UINT_MAX ? lightIndex : 0);

		localContribution *= dot(unitLightNormal, ray.Direction) * coeff / pdf;
	}

	// Number of entries in transmissive materials
	int numEntries = 0;

	// Traverse scene to another surface
	RayPayload rayPayload;
	while (TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		0,			// RayContributionToHitGroupIndex (calls chs)
		1,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table) (calls miss)
		ray,
		rayPayload), rayPayload.t != 0.f)
	{
		++i;
		if (cBuffer.maxBounces != 0 && i > cBuffer.maxBounces)
			break;

		float3 intersectionPoint = ray.Origin + rayPayload.t * ray.Direction;

		// Get Face attributes
		FaceAttributes fAttr = faceAttributes[rayPayload.faceIndex];

		const uint vertIndex = rayPayload.faceIndex * 3;

		// Get face unit normal
		const float3 unitFaceNormal = getUnitNormal(rayPayload.bary, vertIndex, rayPayload.instanceIndex);
		const float wiDot = dot(ray.Direction, unitFaceNormal);

		// Get appropriate BRDF - assuming Diffuse (Will handle Reflective and Transmissive later)
		const Material mat = materials[fAttr.MaterialId];

		const bool isInternal = wiDot > 0.f;

		// Setup Fresnel coeff
		float n1, n2, coeff;
		if (isInternal)
		{
			n1 = mat.RefractiveIndex;
			n2 = 1.f;
			coeff = -1.f;
		}
		else
		{
			n1 = 1.f;
			n2 = mat.RefractiveIndex;
			coeff = 1.f;
		}
		const float fr = fresnel(-coeff * wiDot, n1, n2); // Reflection

		// Beer's law
		if (isInternal)
		{
			if (numEntries <= 0)
				return;
			localContribution *= exp((-rayPayload.t) * mat.TransmissiveFilter);
		}
		
		if ((prevStateFlags & cBuffer.pathFilter) != 0 && i >= cBuffer.minBounces)
		{
			// Check contribution to eye
			shadowRay.Origin = intersectionPoint;
			shadowRay.Direction = cBuffer.position - intersectionPoint;
			invShadowDistance = 1.f / length(shadowRay.Direction);
			unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
			float surfaceDot = dot(unitShadowRayDirection, unitFaceNormal);
			cameraDot = -dot(unitShadowRayDirection, cBuffer.w);

			if (surfaceDot > 0.f && cameraDot > 0.f)
			{
				if (getPixel(shadowRay, cBuffer.winDim, pixel))
				{
					shadowPayload.occluded = true;
					TraceRay(
						gRtScene,	// Acceleration Structure
						RAY_FLAG_FORCE_OPAQUE
						| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
						| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,			// Ray flags
						0xFF,		// Instance inclusion Mask (0xFF includes everything)
						1,			// RayContributionToHitGroupIndex (calls shadowAnyHit)
						1,			// MultiplierForGeometryContributionToShaderIndex (We only have 1 hit group)
						1,			// Miss shader index (within the shader table) (calls shadowMiss)
						shadowRay,
						shadowPayload);

					if (!shadowPayload.occluded)
					{
						const uint pixLaunchIndex = pixel.y * cBuffer.winDim.x + pixel.x;
						float3 brdfDiff = mat.Diffuse * OneOverPI;
						if (mat.DiffuseTextureId >= 0)
							brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
						float3 contrib = (localContribution * brdfDiff) * ((1.f - fr) * mat.Dissolve * surfaceDot * invShadowDistance * invShadowDistance * cameraDot);
						if (!cBuffer.seperateCaustics || (prevStateFlags & (Reflect | Refract)) == 0)
							AddContribution(pixLaunchIndex, contrib, rayPayload.t);
						else
							AddCausticsContribution(pixLaunchIndex, contrib, rayPayload.t);
					}
				}
			}
		}

		// Russian roulette
		if (i >= 4)
		{
			const float probabilityOfContinuing = 0.5f;
			if (rand_next(seed) > probabilityOfContinuing)
				break;
			localContribution *= 1.f / probabilityOfContinuing;
		}

		ray.Origin = intersectionPoint;

		// Reflection
		if (rand_next(seed) <= fr)
		{
			ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
			prevStateFlags = Reflect;
			continue;
		}

		// Diffusion
		if (rand_next(seed) <= mat.Dissolve)
		{
			// Sample the brdf and generate a new ray
			ray.Direction = randomRayLobe(seed, unitFaceNormal, 1, pdf);
			float3 brdfDiff = mat.Diffuse * OneOverPI;
			if (mat.DiffuseTextureId >= 0)
				brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
			localContribution *= brdfDiff * dot(unitFaceNormal, ray.Direction) / pdf;
			prevStateFlags = Diffuse;
		}
		else
		{
			// Transmission
			float3 dir = refract(ray.Direction, coeff * unitFaceNormal, n1 / n2);
			if (any(dir))
			{
				ray.Direction = dir;
				numEntries += isInternal ? -1 : 1;
				prevStateFlags = Refract;
			}
			else
			{
				break; // Should never happen
			}
		}
	}
}

// Ray
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.bary = attribs.barycentrics;
	payload.t = RayTCurrent();
	payload.faceIndex = getFaceIndex();
	payload.instanceIndex = InstanceIndex();
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.t = 0.f;
}

// Shadow
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
	payload.occluded = false;
}
