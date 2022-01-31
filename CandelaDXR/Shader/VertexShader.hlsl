cbuffer CB1 : register(b0)
{
	matrix MVP;
}

struct MyInput
{
	float4 pos : POSITION;
	float4 normal : NORMAL;
};

struct MyOutput
{
	float4 Position : SV_POSITION;
	float4 Pos : VS_POSITION;
	float4 Normal : VS_NORMAL;
};

MyOutput main(MyInput myInput)
{
	MyOutput myOutput;
	myOutput.Pos = myInput.pos;
	myOutput.Position = mul(MVP, myInput.pos);
	myOutput.Normal = myInput.normal;
	return myOutput;
}
