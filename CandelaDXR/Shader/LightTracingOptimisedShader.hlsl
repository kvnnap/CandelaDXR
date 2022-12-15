#include "Utils.hlsli"
#include "Scene.hlsli"
#include "IrradianceItem.hlsli"
#include "LightTracingVars.hlsli"

struct OptimisedConstBuff
{
	uint numSpeculars;
	float causticsRatio;
};

cbuffer CB2 : register(b1)
{
	OptimisedConstBuff cOptBuffer;
}

struct RayPayload
{
	float2 bary;
	float t;
	uint faceIndex;
};

struct ShadowPayload
{
	bool occluded;
};

// Functions
bool sampleDiffuse(inout uint seed, inout RayDesc ray, inout float3 localContribution, inout bool causticsPath, inout uint specularPrimitiveId, float3 unitNormal)
{
	float localCR;
	if (cOptBuffer.numSpeculars == 0)
	{
		localCR = 0.f;
		causticsPath = false;
	}
	else 
	{
		localCR = cOptBuffer.causticsRatio;
		causticsPath = rand_next(seed) <= localCR;
	}
	
	if (causticsPath)
	{
		// Choose a primitive
		const uint specularIndex = chooseInRange(seed, 0, cOptBuffer.numSpeculars - 1);
		SpecularPrimitive specularPrimitive = speculars[specularIndex];
		const uint specularIndexId = specularPrimitive.PrimitiveId * 3;
		specularPrimitiveId = specularPrimitive.PrimitiveId;

		// Compute specular primitive vertices
		float3 lv[3];
		getVertexWorldCoordinates(lv, specularIndexId, specularPrimitive.InstanceIndex);

		// Generate a point on the specular primitive
		float2 specularBary;
		const float3 pointOnSpecular = samplePointOnTriangle(seed, lv, specularBary);

		ray.Direction = pointOnSpecular - ray.Origin;
		float invDistance = 1.f / length(ray.Direction);
		ray.Direction *= invDistance; // Get Unit Direction

		float3 specularUnitNormal = getUnitNormal(specularBary, specularIndexId, specularPrimitive.InstanceIndex);
		float surfaceDot = dot(unitNormal, ray.Direction);
		float causticsDot = -dot(specularUnitNormal, ray.Direction);

		if (surfaceDot < 0.f || causticsDot < 0.f)
			return false;
		localContribution *= getTriangleArea(lv) * cOptBuffer.numSpeculars * surfaceDot * causticsDot * invDistance * invDistance / localCR;
	}
	else
	{
		// Sample the brdf and generate a new ray
		float pdf;
		ray.Direction = randomRayLobe(seed, unitNormal, 1, pdf);
		localContribution *= dot(unitNormal, ray.Direction) / (pdf * (1.f - localCR));
	}

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

	// Early-exit checks
	if (cBuffer.numLights == 0)
		return;

	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * (cBuffer.frameNumber + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * (cBuffer.frameNumber + 0) + launchIndex.y);

	// Choose light source
	const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);
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
	RayDesc ray;
	ray.TMin = 0.001f;
	ray.TMax = 3.402823e+38;
	ray.Origin = shadowRay.Origin;
	ray.Direction = unitLightNormal;

	bool performChecks = !lightDirectional; // true for diffuse lighting
	bool causticsPath;
	uint specularPrimitiveId;
	if(!lightDirectional)
		if(!sampleDiffuse(seed, ray, localContribution, causticsPath, specularPrimitiveId, unitLightNormal))
			return;

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
		rayPayload), rayPayload.t != 0.f && (!performChecks || !causticsPath || specularPrimitiveId == rayPayload.faceIndex))
	{
		++i;
		if (cBuffer.maxBounces != 0 && i > cBuffer.maxBounces)
			break;

		// Get Face attributes
		FaceAttributes fAttr = faceAttributes[rayPayload.faceIndex];

		// Get appropriate BRDF - assuming Diffuse (Will handle Reflective and Transmissive later)
		Material mat = materials[fAttr.MaterialId];

		// If we hit spec object when we're not sampling them, return [Rejection Sampling pt2])
		if (performChecks && !causticsPath && mat.Dissolve != 1.f)
			return;

		performChecks = false;

		const uint vertIndex = rayPayload.faceIndex * 3;

		// Get face unit normal
		const float3 unitFaceNormal = getUnitNormal(rayPayload.bary, vertIndex, fAttr.InstanceIndex);
		const float wiDot = dot(ray.Direction, unitFaceNormal);
		const bool isInternal = wiDot > 0.f;
		
		float3 intersectionPoint = ray.Origin + rayPayload.t * ray.Direction;

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

		// Compute Fresnel
		if (rand_next(seed) <= fr)
		{
			ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
			prevStateFlags = Reflect;
			continue;
		}

		// Diffuse?
		if (rand_next(seed) <= mat.Dissolve)
		{
			// Sample the brdf and generate a new ray
			if (!sampleDiffuse(seed, ray, localContribution, causticsPath, specularPrimitiveId, unitFaceNormal))
				return;
			float3 brdfDiff = mat.Diffuse * OneOverPI;
			if (mat.DiffuseTextureId >= 0)
				brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
			localContribution *= brdfDiff;
			performChecks = true;
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
