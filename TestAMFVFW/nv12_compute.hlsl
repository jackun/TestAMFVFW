static float4 YcoeffB = { 0.098f, 0.504f, 0.257f, 0.062745f };
static float4 UcoeffB = { 0.439f, -0.291f, -0.148f, 0.501961f };
static float4 VcoeffB = { -0.071f, -0.368f, 0.439f, 0.501961f };

//R8B8G8A8
StructuredBuffer<uint> Buffer0 : register(t0);
//RWStructuredBuffer<uint> BufferY : register(u0);
RWTexture2D<uint> BufferY : register(u0);
RWTexture2D<uint2> BufferUV : register(u1);

cbuffer globals : register(b0)
{
	uint inPitch;
	//uint height;
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

	for (int j = 0; j < 2; j++)
	for (int i = 0; i < 2; i++)
	{
		float4 pixel = readPixel(dispatchThreadID.x * 2 + i, dispatchThreadID.y * 2 + j);
		px[j * 2 + i] = pixel;
		//BufferY[uint2(dispatchThreadID.x * 2 + i, h - (dispatchThreadID.y * 2 + j) - 1)] = uint(dot(pixel, YcoeffB));
	}

	for (int j = 0; j < 2; j++)
		for (int i = 0; i < 2; i++)
		{
			BufferY[uint2(dispatchThreadID.x * 2 + i, h - (dispatchThreadID.y * 2 + j) - 1)] = uint(dot(px[j * 2 + i], YcoeffB));
		}

	float2 UV00 = float2(dot(px[0], UcoeffB), dot(px[0], VcoeffB));
	float2 UV10 = float2(dot(px[1], UcoeffB), dot(px[1], VcoeffB));
	float2 UV01 = float2(dot(px[2], UcoeffB), dot(px[2], VcoeffB));
	float2 UV11 = float2(dot(px[3], UcoeffB), dot(px[3], VcoeffB));

	float2 UV = (UV00 + UV10 + UV01 + UV11) / 4;
	BufferUV.GetDimensions(w, h);
	BufferUV[uint2(dispatchThreadID.x, h - dispatchThreadID.y - 1)] = uint2(UV.x, UV.y);
}
