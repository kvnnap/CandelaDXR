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
		localContribution *= lightMat.Emissive;
		localContribution *= getTriangleArea(lv);

		if (lightMat.EmissiveTextureId >= 0)
			localContribution *= gTextures[lightMat.EmissiveTextureId].SampleLevel(gSampler, getTextureLocation(lightBary, lightIndexId), 0);

		// First check if light normal is the right way round wrt camera
		const float3 unitLightNormal = getUnitNormal(lightBary, lightIndexId, areaLight.InstanceIndex);

		// Construct ray from light source to camera origin
		shadowRay.Origin = pointOnLightSource;
		shadowRay.Direction = cBuffer.position - pointOnLightSource;
		float invShadowDistance = 1.f / length(shadowRay.Direction);
		float3 unitShadowRayDirection = shadowRay.Direction * invShadowDistance;

		float lightDot = dot(unitShadowRayDirection, unitLightNormal);
		float cameraDot = -dot(unitShadowRayDirection, cBuffer.w);

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
					AddContribution(pixLaunchIndex, cBuffer.rangeBits, contrib);
				}
			}
		}

		// Construct ray from light source to random scene point
		ray.Origin = shadowRay.Origin;
		ray.Direction = unitLightNormal;