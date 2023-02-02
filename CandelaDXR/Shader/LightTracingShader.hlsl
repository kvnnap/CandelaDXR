#include "Utils.hlsli"
#include "Scene.hlsli"
#include "IrradianceItem.hlsli"
#include "LightTracingVars.hlsli"

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

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	// Early-exit checks
	if (cBuffer.numTotalLights == 0)
		return;
	
	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * (cBuffer.frameNumber + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * (cBuffer.frameNumber + 0) + launchIndex.y);

	// Choose light source
	uint lightIndex = chooseInRange(seed, 0, cBuffer.numTotalLights - 1);
	const bool isExternalLight = lightIndex >= cBuffer.numLights;
	PathInteraction prevStateFlags = Light;
	uint2 pixel = uint2(0, 0);
	uint i = 1;
	float pdf;
	float3 localContribution = cBuffer.numTotalLights; // Chose a light!
	ShadowPayload shadowPayload;
	RayDesc shadowRay;
	RayDesc ray;
	ray.TMin = 0.001f;
	ray.TMax = 3.402823e+38;

	if (isExternalLight)
	{
		lightIndex -= cBuffer.numLights;
		ExternalLight eLight = eLights[lightIndex];

		localContribution *= eLight.Diffuse;

		if (eLight.Type == LT_POINT)
		{
			ray.Origin = eLight.Position.xyz;
			ray.Direction = randomRaySphere(seed, pdf);
			localContribution *= 1.f / (eLight.Attenuation[2] * pdf);
		}
		else if (eLight.Type == LT_DIRECTIONAL)
		{
			// Sample point on light source (rectangle on a plane)
			const float2 uvPoint = eLight.AreaDimensions * float2(rand_next(seed), rand_next(seed));
			ray.Origin = eLight.Position.xyz + uvPoint.x * eLight.Right.xyz + uvPoint.y * eLight.Up.xyz;
			ray.Direction = eLight.Direction.xyz;
			localContribution *= (eLight.AreaDimensions.x * eLight.AreaDimensions.y) / eLight.Attenuation[0];
		}
	}
	else
	{
		#include "LightTracingLightCodeSection.hlsli"

		if (!lightDirectional)
		{
			ray.Direction = randomRayLobe(seed, unitLightNormal, 1, pdf);
			localContribution *= dot(unitLightNormal, ray.Direction) / pdf;
		}
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
			float invShadowDistance = 1.f / length(shadowRay.Direction);
			float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
			float surfaceDot = dot(unitShadowRayDirection, unitFaceNormal);
			float cameraDot = -dot(unitShadowRayDirection, cBuffer.w);

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
