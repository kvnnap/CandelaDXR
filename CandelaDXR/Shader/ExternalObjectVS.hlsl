cbuffer CB1 : register(b0)
{
	matrix ViewPerspective;
}

float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return mul(pos, ViewPerspective);
}