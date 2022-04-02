#include "Utils.hlsli"
#include "Scene.hlsli"
#include "IrradianceItem.hlsli"
#include "LightTracingVars.hlsli"

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

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	gOutput[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);

	// Early-exit checks
	if (cBuffer.numLights == 0)
		return;

	// Initialise seed
	uint seed = rand_init(
		cBuffer.seeds.x + launchDim.x * (cBuffer.frameNumber + 0) + launchIndex.x,
		cBuffer.seeds.y + launchDim.y * (cBuffer.frameNumber + 0) + launchIndex.y);

	// Camera
	const float2 ratio = (launchIndex + float2(rand_next(seed), rand_next(seed))) / launchDim;
	const float2 filmPlanePosition = float2(cBuffer.plane.x * (ratio.x - 0.5f), cBuffer.plane.y * (0.5f - ratio.y));
	const float3 pointOnObjectPlane = cBuffer.position + cBuffer.w * cBuffer.plane.z + cBuffer.u * filmPlanePosition.x + cBuffer.v * filmPlanePosition.y;

	// Construct Ray
	RayDesc ray;
	ray.TMin = 0.001f;
	ray.TMax = 3.402823e+38;
	ray.Origin = cBuffer.position;
	ray.Direction = normalize(pointOnObjectPlane - ray.Origin);

	// Number of entries in transmissive materials
	int numEntries = 0;

	// Path segment index
	uint i = 0;

	float3 localCoefficient = 1.f;
	float3 radiance = 0.f;
	
	PathInteraction prevInteraction = Light;

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
		// Get Face attributes
		const FaceAttributes fAttr = faceAttributes[rayPayload.faceIndex];
		const Material mat = materials[fAttr.MaterialId];

		const uint vertIndex = rayPayload.faceIndex * 3;
		const float3 unitFaceNormal = getUnitNormal(rayPayload.bary, vertIndex, fAttr.InstanceIndex);
		const float wiDot = dot(ray.Direction, unitFaceNormal);
		const bool isInternal = wiDot > 0.f;
		const float3 intersectionPoint = ray.Origin + rayPayload.t * ray.Direction;

		// Fresnel vars
		float n1, n2, dissolve, coeff;

		// Beer's law
		if (isInternal)
		{
			if (numEntries <= 0)
				break;
			localCoefficient *= exp((-rayPayload.t) * mat.TransmissiveFilter);

			// Fresnel
			n1 = mat.RefractiveIndex;
			n2 = 1.f;
			dissolve = 0.f;
			coeff = -1.f;
		}
		else
		{
			// If material is emissive, add its radiance
			if (((prevInteraction & (Light | Reflect | Refract)) != 0) && any(mat.Emissive))
			{
				float3 albedo = mat.Emissive;
				if (mat.EmissiveTextureId >= 0)
					albedo *= gTextures[mat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
				radiance += localCoefficient * albedo;
			}

			// Fresnel
			n1 = 1.f;
			n2 = mat.RefractiveIndex;
			dissolve = mat.Dissolve;
			coeff = 1.f;
		}

		// Russian roulette
		if (++i >= 3)
		{
			const float probabilityOfContinuing = 0.5f;
			if (rand_next(seed) > probabilityOfContinuing)
				break;
			localCoefficient *= 1.f / probabilityOfContinuing;
		}

		// Next ray origin
		ray.Origin = intersectionPoint;

		// Compute Fresnel
		float fr = fresnel(-coeff * wiDot, n1, n2);

		// Should reflect?
		if (rand_next(seed) < fr)
		{
			ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
			prevInteraction = Reflect;
			continue;
		}

		localCoefficient *= 1.f / (1.f - fr);

		// Diffuse?
		if (rand_next(seed) < dissolve)
		{
			// NES - Cast a shadow ray and collect light
			const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);
			const uint lightIndexId = lights[lightIndex].PrimitiveId * 3;
			AreaLight areaLight = lights[lightIndex];

			// Compute light vertices
			float3 lv[3];
			getVertexWorldCoordinates(lv, lightIndexId, areaLight.InstanceIndex);

			// Generate a point on the light
			float2 lightBary;
			const float3 pointOnLightSource = samplePointOnTriangle(seed, lv, lightBary);
			const float3 unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);

			RayDesc shadowRay;
			shadowRay.TMin = 0.001f;
			shadowRay.TMax = 0.999f;
			shadowRay.Origin = intersectionPoint;
			shadowRay.Direction = pointOnLightSource - intersectionPoint;
			const float invShadowDistance = 1.f / length(shadowRay.Direction);
			const float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
			const float lightDot = -dot(unitShadowRayDirection, unitLightNormal);
			const float surfaceLightDot = dot(unitShadowRayDirection, unitFaceNormal);

			if (lightDot > 0.f && surfaceLightDot > 0.f)
			{
				// Test using shadow ray
				ShadowPayload shadowPayload;
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
					fr = fresnel(surfaceLightDot, n1, n2);
					Material lightMat = materials[areaLight.MaterialId];
					float3 lightRadiance = lightMat.Emissive;
					if (lightMat.EmissiveTextureId >= 0)
						lightRadiance *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(lightBary, lightIndexId), 0);
					lightRadiance *= getTriangleArea(lv) * cBuffer.numLights * surfaceLightDot * lightDot * invShadowDistance * invShadowDistance;
					float3 brdfDiff = mat.Diffuse * OneOverPI;
					if (mat.DiffuseTextureId >= 0)
						brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
					radiance += localCoefficient * lightRadiance * brdfDiff * (1.f - fr);
				}
			}

			// Proceed with normal diffuse hemispherical
			float pdf;
			ray.Direction = randomRayLobe(seed, unitFaceNormal, 1, pdf);
			fr = fresnel(dot(unitFaceNormal, ray.Direction), n1, n2);
			float3 brdfDiff = mat.Diffuse * OneOverPI;
			if (mat.DiffuseTextureId >= 0)
				brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
			localCoefficient *= brdfDiff * dot(unitFaceNormal, ray.Direction) * (1.f - fr) / pdf;
			prevInteraction = Diffuse;
		}
		else
		{
			// Transmission
			float3 dir = refract(ray.Direction, coeff * unitFaceNormal, n1 / n2);
			// fr = fresnel(dot(unitFaceNormal, ray.Direction), n1, n2); TODO!!
			if (any(dir))
			{
				ray.Direction = dir;
				numEntries += isInternal ? -1 : 1;
				prevInteraction = Refract;
				if (isInternal) // On Surface Exit, apply correct weights
				{ 
					fr = fresnel(dot(unitFaceNormal, ray.Direction), n2, n1);
					localCoefficient *= 1.f - fr;
				}
			}
			else
			{   // Total internal reflection - should occur at the start of the code, not here
				ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
				prevInteraction = Reflect;
			}
		}

		//ray.Direction = normalize(ray.Direction);
	}

	// Using this resource as a RADIANCE accumulator
	const uint flatLaunchIndex = launchIndex.y * launchDim.x + launchIndex.x;
	if (cBuffer.frameNumber == 1)
		gIrradianceDSFloat[flatLaunchIndex].value = 0.f;
	gIrradianceDSFloat[flatLaunchIndex].value += radiance;
	gOutput[launchIndex] = float4(linearToSrgb(toneMap(gIrradianceDSFloat[flatLaunchIndex].value / cBuffer.frameNumber)), 1.f);
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
