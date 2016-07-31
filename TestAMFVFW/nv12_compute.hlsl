//                        B       G        R      +16.f/+128.f
static float4 Yc601 = { 0.098f, 0.504f, 0.257f, 0.062745f };
static float4 Uc601 = { 0.439f, -0.291f, -0.148f, 0.501961f };
static float4 Vc601 = { -0.071f, -0.368f, 0.439f, 0.501961f };

static float4 Yc709 = { 0.062007f, 0.614231f, 0.182586f, 0.062745f };
static float4 Uc709 = { 0.439216f, -0.338572f, -0.100644f, 0.501961f };
static float4 Vc709 = { -0.040274f, -0.398942f, 0.439216f, 0.501961f };

//R8B8G8A8
StructuredBuffer<uint> Buffer0 : register(t0);
//RWStructuredBuffer<uint> BufferY : register(u0);
RWTexture2D<uint> BufferY : register(u0);
RWTexture2D<uint2> BufferUV : register(u1);

cbuffer globals : register(b0)
{
	uint inPitch;
	uint colorspace;
};

float4 readPixel(int x, int y)
{
	float4 output;
	uint index = (x + y * inPitch);
	
	output.x = float(Buffer0[index] & 0x000000ff);
	output.y = float((Buffer0[index] & 0x0000ff00) >> 8);
	output.z = float((Buffer0[index] & 0x00ff0000) >> 16);
	output.w = 255.f;
	
	return output;
}

[numthreads(16, 8, 1)]
void CSMain( uint3 dispatchThreadID : SV_DispatchThreadID )
{
	uint w, h;
	BufferY.GetDimensions(w, h);
	float4 px[4];
	float4 Ycoeff, Ucoeff, Vcoeff;
	if (colorspace == 0)
	{
		Ycoeff = Yc601;
		Ucoeff = Uc601;
		Vcoeff = Vc601;
	}
	else
	{
		Ycoeff = Yc709;
		Ucoeff = Uc709;
		Vcoeff = Vc709;
	}

	for (int j = 0; j < 2; j++)
	for (int i = 0; i < 2; i++)
	{
		float4 pixel = readPixel(dispatchThreadID.x * 2 + i, dispatchThreadID.y * 2 + j);
		px[j * 2 + i] = pixel;
	}

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			BufferY[uint2(dispatchThreadID.x * 2 + i, h - (dispatchThreadID.y * 2 + j) - 1)] = uint(dot(px[j * 2 + i], Ycoeff));
		}
	}

	float2 UV00 = float2(dot(px[0], Ucoeff), dot(px[0], Vcoeff));
	float2 UV10 = float2(dot(px[1], Ucoeff), dot(px[1], Vcoeff));
	float2 UV01 = float2(dot(px[2], Ucoeff), dot(px[2], Vcoeff));
	float2 UV11 = float2(dot(px[3], Ucoeff), dot(px[3], Vcoeff));

	float2 UV = (UV00 + UV10 + UV01 + UV11) / 4;
	BufferUV.GetDimensions(w, h);
	BufferUV[uint2(dispatchThreadID.x, h - dispatchThreadID.y - 1)] = uint2(UV.x, UV.y);
}
