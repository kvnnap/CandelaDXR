#ifndef RAY_TRACING_UTILS
#define RAY_TRACING_UTILS

uint getFaceIndex()
{
	return InstanceID() / 3 + PrimitiveIndex();
}

float3 getUnitNormal(float2 bary, uint vertBaseId, uint matrixId)
{
	float3 ln[3];
	ln[0] = mul(normals[indices[vertBaseId + 0]], normalMatrices[matrixId]);
	ln[1] = mul(normals[indices[vertBaseId + 1]], normalMatrices[matrixId]);
	ln[2] = mul(normals[indices[vertBaseId + 2]], normalMatrices[matrixId]);
	return normalize(interpolateVertices(bary, ln));
}

float2 getTextureLocation(float2 bary, uint vertBaseId)
{
	float2 lt[3];
	lt[0] = texVerts[indices[vertBaseId + 0]];
	lt[1] = texVerts[indices[vertBaseId + 1]];
	lt[2] = texVerts[indices[vertBaseId + 2]];
	return pointOnTriangle(bary, lt);
}

void getVertexWorldCoordinates(inout float3 lv[3], uint vertBaseId, uint matrixId)
{
	lv[0] = mul(float4(verts[indices[vertBaseId + 0]], 1.f), matrices[matrixId]);
	lv[1] = mul(float4(verts[indices[vertBaseId + 1]], 1.f), matrices[matrixId]);
	lv[2] = mul(float4(verts[indices[vertBaseId + 2]], 1.f), matrices[matrixId]);
}

#endif