float4 main(float4 pos : POSITION) : SV_POSITION
{
	return pos;
	/*return float4(
		pos.x < 0.f ? 0.f : 0.5f,
		pos.y < 1.f ? 0.f : 0.5f, 
		pos.z < 0.f ? 0.f : 0.5f, 1.0f);*/
}