#include "Utils.hlsli"
#include "Scene.hlsli"

// Used to filter path components for analysis
enum PathInteraction : uint
{
	Light = 1,
	Reflect = 2,
	Refract = 4,
	Diffuse = 8
};

struct ConstBuff
{
	float3 u, v, w;
	float3 position;
	float3 direction;
	float3 plane; // sensor dimensions (z contains distance to sensor plane)
	uint2 seeds;
	uint2 winDim;
	uint numLights;
	uint numExternalLights;
	uint numTotalLights;
	uint frameNumber;
	uint specularOnly;
	PathInteraction pathFilter;
	uint minBounces;
	uint maxBounces;
	uint2 padding;
};

struct RayPayload
{
	float2 bary;
	float t;
	uint faceIndex;
	uint instanceIndex;
};



RWTexture2D<float4> gOutputDiff : register(u0);
RWTexture2D<float4> gOutputSpec : register(u1);
RWTexture2D<float4> gRadianceDiff : register(u2);
RWTexture2D<float4> gRadianceSpec : register(u3);
RWTexture2D<float4> gRayHitT : register(u4);

// SRVs
StructuredBuffer<float3> verts : register(t0);
StructuredBuffer<float2> texVerts : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<uint> indices : register(t3);
StructuredBuffer<float4x3> matrices : register(t4);
StructuredBuffer<float3x3> normalMatrices : register(t5);
StructuredBuffer<FaceAttributes> faceAttributes : register(t6);
StructuredBuffer<Material> materials : register(t7);
StructuredBuffer<AreaLight> lights : register(t8);
StructuredBuffer<ExternalLight> eLights : register(t9);

RaytracingAccelerationStructure gRtScene : register(t10);

Texture2D<float3> gTextures[]: register(t12);

// Sampler
SamplerState gSampler : register(s0);

// CBVs
cbuffer CB1 : register(b0)
{
	ConstBuff cBuffer;
}

// Util Functions
#include "RayTracingUtils.hlsli"

// Consider moving in RTUtils
struct ShadowPayload
{
	bool occluded;
};

bool isOccluded(RayDesc shadowRay)
{
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
	return shadowPayload.occluded;
}

// Kernels

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x, y point
	const uint2 launchIndex = DispatchRaysIndex().xy;

	// Dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions().xy;

	// Clear output pixel
	gOutputDiff[launchIndex] = gOutputSpec[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);

	// Early-exit checks
	if (cBuffer.numTotalLights == 0)
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

	// Path segment index
	uint i = 1;

	float3 localCoefficient = 1.f;
	float3 radiance = 0.f;
	
	PathInteraction prevInteraction = Light;

	float denoiserHitT = 0.f;
	bool isSpecularPath = cBuffer.specularOnly;

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
		// Second bounce for denoiser
		if (i == 2)
			denoiserHitT = rayPayload.t;

		// Get Face attributes
		const FaceAttributes fAttr = faceAttributes[rayPayload.faceIndex];
		const Material mat = materials[fAttr.MaterialId];
		
		const uint vertIndex = rayPayload.faceIndex * 3;
		const float3 unitFaceNormal = getUnitNormal(rayPayload.bary, vertIndex, rayPayload.instanceIndex);
		const float wiDot = dot(ray.Direction, unitFaceNormal);
		const bool isInternal = wiDot > 0.f;
		const float3 intersectionPoint = ray.Origin + rayPayload.t * ray.Direction;
		
		// Next ray origin
		ray.Origin = intersectionPoint;

		// Fresnel vars
		float n1, n2, dissolve, coeff;

		// Select only Specular Path
		if (cBuffer.specularOnly && i == 1)
		{
			n1 = 1.f;
			n2 = mat.RefractiveIndex;

			if (mat.Dissolve >= 1.f && mat.RefractiveIndex <= 1.f)
				break;
			
			++i;
			if (cBuffer.maxBounces != 0 && i > cBuffer.maxBounces)
				break;

			float fr = fresnel(-wiDot, n1, n2);
			if (rand_next(seed) <= fr)
			{
				ray.Direction = reflect(ray.Direction, unitFaceNormal);
				prevInteraction = Reflect;
				continue;
			}

			localCoefficient *= 1.f / (1.f - fr);

			// Transmission
			float3 dir = refract(ray.Direction, unitFaceNormal, n1 / n2);
			if (any(dir))
			{
				ray.Direction = dir;
				fr = fresnel(-dot(unitFaceNormal, ray.Direction), n2, n1);
				localCoefficient *= (1.f - fr) * (1.f - mat.Dissolve);
				prevInteraction = Refract;
			}
			else break; // Should never happen
			
			continue;
		}

		// Beer's law
		if (isInternal)
		{
            if (mat.Dissolve >= 1.f && mat.RefractiveIndex <= 1.f)
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
			if (((prevInteraction & cBuffer.pathFilter & (Light | Reflect | Refract)) != 0) && i >= cBuffer.minBounces && mat.EmissiveType != 1 && any(mat.Emissive) && i > 1)
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

		++i;
		if (cBuffer.maxBounces != 0 && i > cBuffer.maxBounces)
			break;

		// Russian roulette
		if (i >= 4)
		{
			const float probabilityOfContinuing = 0.5f;
			if (rand_next(seed) > probabilityOfContinuing)
				break;
			localCoefficient *= 1.f / probabilityOfContinuing;
		}

		// Compute Fresnel
		float fr = fresnel(-coeff * wiDot, n1, n2);

		// Should reflect?
		if (rand_next(seed) <= fr)
		{
			isSpecularPath |= i == 2;
			ray.Direction = reflect(ray.Direction, coeff * unitFaceNormal);
			prevInteraction = Reflect;
			continue;
		}

		localCoefficient *= 1.f / (1.f - fr);

		// Diffuse?
		if (rand_next(seed) <= dissolve)
		{
			if ((prevInteraction & cBuffer.pathFilter) != 0 && i >= cBuffer.minBounces)
			{
				uint lightIndex = chooseInRange(seed, 0, cBuffer.numTotalLights - 1);
				bool isExternalLight = lightIndex >= cBuffer.numLights; // i.e. emissive

				// Start constructing shadow ray
				RayDesc shadowRay;
				shadowRay.TMin = 0.001f;
				shadowRay.TMax = 0.999f;
				shadowRay.Origin = intersectionPoint;

				if (isExternalLight)
				{
					lightIndex -= cBuffer.numLights;
					ExternalLight eLight = eLights[lightIndex];
					float lightCoeff = 1.f;
					bool applyInv = true;
                    bool proceed = true;

                    if (eLight.Type == LT_POINT || eLight.Type == LT_SPOT)
					{
						shadowRay.Direction = eLight.Position.xyz - intersectionPoint;
						lightCoeff = 1.f / eLight.Attenuation[2];
						
                        if (eLight.Type == LT_SPOT)
                        {
                            const float lightDot = -dot(normalize(shadowRay.Direction), eLight.Direction.xyz);
                            proceed = (1.f - lightDot) <= eLight.InnerConeAngle;
                        }
                    }
					else if (eLight.Type == LT_DIRECTIONAL)
					{
						shadowRay.Direction = -eLight.Direction.xyz;
						shadowRay.TMax = 3.402823e+38;
						lightCoeff = 1.f / eLight.Attenuation[0];
						applyInv = false;
                    }
                    else
                    {
                        proceed = false;
                    }

					if (proceed)
                    {
                        float invShadowDistance = 1.f / length(shadowRay.Direction);
                        float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
                        float surfaceLightDot = dot(unitShadowRayDirection, unitFaceNormal);

                        if (surfaceLightDot > 0.f)
                        {
                            if (!isOccluded(shadowRay))
                            {
                                invShadowDistance = applyInv ? invShadowDistance : 1.f;
								// Assuming Intensity value
                                float3 lightRadiance = eLight.Diffuse * (cBuffer.numTotalLights * surfaceLightDot * lightCoeff * invShadowDistance * invShadowDistance);
                                float3 brdfDiff = mat.Diffuse * OneOverPI;
                                if (mat.DiffuseTextureId >= 0)
                                    brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
                                fr = fresnel(surfaceLightDot, n1, n2);
                                radiance += localCoefficient * lightRadiance * brdfDiff * (1.f - fr);
                            }
                        }
                    }
                }
				else
				{
					// NES - Cast a shadow ray and collect light
					//const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);
					const uint lightIndexId = lights[lightIndex].PrimitiveId * 3;
					AreaLight areaLight = lights[lightIndex];
					Material lightMat = materials[areaLight.MaterialId];
					const bool lightDirectional = lightMat.EmissiveType == 1;

					float3 unitLightNormal;
					float invShadowDistance = 1.f;
					float3 unitShadowRayDirection;
					float lightDot = 1.f;
					float triangleArea = 1.f;
					float2 lightBary = float2(0.5f, 0.5f);

					if (lightDirectional)
					{
						unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);
						shadowRay.TMax = 3.402823e+38;
						shadowRay.Direction = -unitLightNormal;

						// Constants
						unitShadowRayDirection = shadowRay.Direction;
					}
					else
					{
						// Compute light vertices
						float3 lv[3];
						getVertexWorldCoordinates(lv, lightIndexId, areaLight.InstanceIndex);

						// Generate a point on the light
						const float3 pointOnLightSource = samplePointOnTriangle(seed, lv, lightBary);
						unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);
						shadowRay.Direction = pointOnLightSource - intersectionPoint;

						// Constants
						invShadowDistance = 1.f / length(shadowRay.Direction);
						unitShadowRayDirection = shadowRay.Direction * invShadowDistance;
						lightDot = -dot(unitShadowRayDirection, unitLightNormal);
						triangleArea = getTriangleArea(lv);
					}

					const float surfaceLightDot = dot(unitShadowRayDirection, unitFaceNormal);

					if (lightDot > 0.f && surfaceLightDot > 0.f)
					{
						bool notOccluded;
						if (lightDirectional)
						{
							// Test using shadow ray
							RayPayload shadowPayload;
							TraceRay(
								gRtScene,	// Acceleration Structure
								0,			// Ray flags
								0xFF,		// Instance inclusion Mask (0xFF includes everything)
								0,			// RayContributionToHitGroupIndex (calls chs)
								1,			// MultiplierForGeometryContributionToShaderIndex
								0,			// Miss shader index (within the shader table) (calls miss)
								shadowRay,
								shadowPayload);
							notOccluded = shadowPayload.t != 0.f && shadowPayload.faceIndex == areaLight.PrimitiveId;
						}
						else // Use first hit approach to be more efficient
						{
							notOccluded = !isOccluded(shadowRay);
						}

						if (notOccluded)
						{
							float3 lightRadiance = lightMat.Emissive;
							if (lightMat.EmissiveTextureId >= 0)
								lightRadiance *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(lightBary, lightIndexId), 0);
							lightRadiance *= triangleArea * cBuffer.numTotalLights * surfaceLightDot * lightDot * invShadowDistance * invShadowDistance;
							float3 brdfDiff = mat.Diffuse * OneOverPI;
							if (mat.DiffuseTextureId >= 0)
								brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);
							fr = fresnel(surfaceLightDot, n1, n2);
							radiance += localCoefficient * lightRadiance * brdfDiff * (1.f - fr);
						}
					}
				}
			}

			// Proceed with normal diffuse hemispherical
			float pdf;
			ray.Direction = randomRayLobe(seed, unitFaceNormal, 1, pdf);
			float3 brdfDiff = mat.Diffuse * OneOverPI;
			if (mat.DiffuseTextureId >= 0)
				brdfDiff *= gTextures[mat.DiffuseTextureId].SampleLevel(gSampler, getTextureLocation(rayPayload.bary, vertIndex), 0);

			const float prDiffRef = 1.f / (2.f - mat.Dissolve);
			// Diffuse reflection
			if (rand_next(seed) <= prDiffRef)
			{
				fr = fresnel(dot(unitFaceNormal, ray.Direction), n1, n2);
				localCoefficient *= brdfDiff * dot(unitFaceNormal, ray.Direction) * (1.f - fr) / (pdf * prDiffRef);
				prevInteraction = Diffuse;
			}
			else // Diffuse Transmission
			{
				float3 dir = refract(ray.Direction, coeff * unitFaceNormal, n1 / n2);
				if (any(dir))
				{
					prevInteraction = Refract;
					fr = fresnel(-coeff * dot(unitFaceNormal, dir), n2, n1);
					localCoefficient *= brdfDiff * dot(unitFaceNormal, ray.Direction) * (1.f - fr) / (pdf * (1.f - prDiffRef));
					ray.Direction = dir;
				}
				else break;
			}
		}
		else
		{
			// Transmission
			isSpecularPath |= i == 2;
			float3 dir = refract(ray.Direction, coeff * unitFaceNormal, n1 / n2);
			// fr = fresnel(dot(unitFaceNormal, ray.Direction), n1, n2); TODO!!
			if (any(dir))
			{
				ray.Direction = dir;
				prevInteraction = Refract;
				fr = fresnel(-coeff * dot(unitFaceNormal, ray.Direction), n2, n1);
				localCoefficient *= (1.f - fr) * (1.f - mat.Dissolve) / (1.f - dissolve);
			}
			else break; // Should never happen
			
		}

		//ray.Direction = normalize(ray.Direction);
	}

	// Using this resource as a RADIANCE accumulator
	if (cBuffer.frameNumber == 1)
	{
		gRadianceSpec[launchIndex] = gRadianceDiff[launchIndex] = float4(0.f, 0.f, 0.f, 1.f);
		gRayHitT[launchIndex].y = 0.f;
		if (!cBuffer.specularOnly)
			gRayHitT[launchIndex].x = 0.f;
	}
	if (isSpecularPath)
	{
		gRadianceSpec[launchIndex] += float4(radiance, 0.f);
		gRayHitT[launchIndex].y = denoiserHitT;
	}
	else
	{
		gRadianceDiff[launchIndex] += float4(radiance, 0.f);
		gRayHitT[launchIndex].x = denoiserHitT;
	}
	
	gOutputDiff[launchIndex] = float4(gRadianceDiff[launchIndex].xyz / cBuffer.frameNumber, 1.f);
	gOutputSpec[launchIndex] = float4(gRadianceSpec[launchIndex].xyz / cBuffer.frameNumber, 1.f);
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
