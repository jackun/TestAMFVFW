static float4 YcoeffB = { 0.098f, 0.504f, 0.257f, 0.062745f };
static float4 UcoeffB = { 0.439f, -0.291f, -0.148f, 0.501961f };
static float4 VcoeffB = { -0.071f, -0.368f, 0.439f, 0.501961f };

//R8B8G8A8
StructuredBuffer<uint> Buffer0 : register(t0);
//RWStructuredBuffer<uint> BufferOut : register(u0);
RWTexture2D<uint> BufferOut : register(u0);
RWTexture2D<uint2> BufferUV : register(u1);

cbuffer globals : register(b0)
{
	uint inPitch;
	//uint height;
};

float4 readPixel(int x, int y)
{
	float4 output;
	uint w, h;
	Buffer0.GetDimensions(w, h);
	uint index = (x + y * inPitch);
	
	output.x = float(Buffer0[index] & 0x000000ff);
	output.y = float((Buffer0[index] & 0x0000ff00) >> 8);
	output.z = float((Buffer0[index] & 0x00ff0000) >> 16);
	output.w = 255.f;
	
	return output;
}

[numthreads(1, 1, 1)]
void CSMain( uint3 dispatchThreadID : SV_DispatchThreadID )
{
	uint w, h;
	BufferOut.GetDimensions(w, h);
	//BufferOut[dispatchThreadID.xy] = 0xFF000000 | (uint)((dispatchThreadID.x) / 32.f * 0xFF) | (uint)(dispatchThreadID.y / (float)h * 0xFF) << 8;

	for (int j = 0; j < 2; j++)
	for (int i = 0; i < 2; i++)
	{
		float4 pixel = readPixel(dispatchThreadID.x * 2 + i, dispatchThreadID.y * 2 + j);
		//writeToPixel(dispatchThreadID.x * 2 + i, dispatchThreadID.y + j, float4(Y, Y, Y, 255.f));
		BufferOut[uint2(dispatchThreadID.x * 2 + i, h - (dispatchThreadID.y * 2 + j) - 1)] = uint(dot(pixel, YcoeffB));
	}

	float4 p00 = readPixel(dispatchThreadID.x * 2, dispatchThreadID.y * 2);
	float4 p10 = readPixel(dispatchThreadID.x * 2 + 1, dispatchThreadID.y * 2);
	float4 p01 = readPixel(dispatchThreadID.x * 2, dispatchThreadID.y * 2 + 1);
	float4 p11 = readPixel(dispatchThreadID.x * 2 + 1, dispatchThreadID.y * 2 + 1);

	float2 UV00 = float2(dot(p00, UcoeffB), dot(p00, VcoeffB));
	float2 UV10 = float2(dot(p10, UcoeffB), dot(p10, VcoeffB));
	float2 UV01 = float2(dot(p01, UcoeffB), dot(p01, VcoeffB));
	float2 UV11 = float2(dot(p11, UcoeffB), dot(p11, VcoeffB));

	float2 UV = (UV00 + UV10 + UV01 + UV11) / 4;
	BufferUV.GetDimensions(w, h);
	BufferUV[uint2(dispatchThreadID.x, h - dispatchThreadID.y - 1)] = uint2(UV.x, UV.y);
}
