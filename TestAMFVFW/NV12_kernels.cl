//BMP is usually upside-down
#define FLIP
//TODO RGB_LIMITED stuff is iffy maybe
//0.062745f <- 16.f / 255.f
//0.501961f <- 128.f / 255.f

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

#ifdef BT601_FULL
	// Y + 16.f
	//#define NO_OFFSET
	//#define RGB_LIMITED
	//RGB in full range [0...255]
	//http://www.mplayerhq.hu/DOCS/tech/colorspaces.txt
	#define Ycoeff ((float4)(0.257f, 0.504f, 0.098f, 0.062745f))
	#define Ucoeff ((float4)(-0.148f, -0.291f, 0.439f, 0.501961f))
	#define Vcoeff ((float4)(0.439f, -0.368f, -0.071f, 0.501961f))

	#define YcoeffB ((float4)(0.098f, 0.504f, 0.257f, 0.062745f))
	#define UcoeffB ((float4)(0.439f, -0.291f, -0.148f, 0.501961f))
	#define VcoeffB ((float4)(-0.071f, -0.368f, 0.439f, 0.501961f))
#endif

#ifdef BT601_LIMITED
	#define RGB_LIMITED
	#define Ycoeff ((float4)(0.299f, 0.587f, 0.114f, 0.f))
	#define Ucoeff ((float4)(-0.14713f, -0.28886f, 0.436f, 0.501961f))
	#define Vcoeff ((float4)(0.615f, -0.51499f, -0.10001f, 0.501961f))

	//BGR
	#define YcoeffB ((float4)(0.114f, 0.587f, 0.299f, 0.f))
	#define UcoeffB ((float4)(0.436f, -0.28886f, -0.14713f, 0.501961f))
	#define VcoeffB ((float4)(-0.10001f, -0.51499f, 0.615f, 0.501961f))
#endif

#ifdef BT601_FULL_YCbCr
	//RGB 0..255
	//YCbCr 0..255
	#define Ycoeff ((float4)(0.299f, 0.587f, 0.114f, 0.f))
	#define Ucoeff ((float4)(-0.169f, -0.331f, 0.5f, 0.501961f))
	#define Vcoeff ((float4)(0.5f, -0.419f, -0.081f, 0.501961f))

	//BGR
	#define YcoeffB ((float4)(0.114f, 0.587f, 0.299f, 0.f))
	#define UcoeffB ((float4)(0.5f, -0.331f, -0.169f, 0.501961f))
	#define VcoeffB ((float4)(-0.081f, -0.419f, 0.5f, 0.501961f))
#endif

#ifdef BT709_FULL2
	// Y + 16.f
	//RGB in full range [0...255]
	#define Ycoeff ((float4)(0.1826f, 0.6142f, 0.0620f, 0.062745f))
	#define Ucoeff ((float4)(-0.1006f, -0.3386f, 0.4392f, 0.501961f))
	#define Vcoeff ((float4)(0.4392f, -0.3989f, -0.0403f, 0.501961f))

	//BGR
	#define YcoeffB ((float4)(0.0620f, 0.6142f, 0.1826f, 0.062745f))
	#define UcoeffB ((float4)(0.4392f, -0.3386f, -0.1006f, 0.501961f))
	#define VcoeffB ((float4)(-0.0403f, -0.3989f, 0.4392f, 0.501961f))
#endif

//#ifdef BT709_ALT2_FULL
#ifdef BT709_FULL
	// Y + 16.f, from OBS
	//RGB in full range [0...255]
	#define Ycoeff ((float4)(0.182586f, 0.614231f, 0.062007f, 0.062745f))
	#define Ucoeff ((float4)(-0.100644f, -0.338572f, 0.439216f, 0.501961f))
	#define Vcoeff ((float4)(0.439216f, -0.398942f, -0.040274f, 0.501961f))

	//BGR
	#define YcoeffB ((float4)(0.062007f, 0.614231f, 0.182586f, 0.062745f))
	#define UcoeffB ((float4)(0.439216f, -0.338572f, -0.100644f, 0.501961f))
	#define VcoeffB ((float4)(-0.040274f, -0.398942f, 0.439216f, 0.501961f))
#endif

#ifdef BT709_LIMITED
	#define RGB_LIMITED
	#define Ycoeff ((float4)(0.2126f, 0.7152f, 0.0722f, 0.f))
	#define Ucoeff ((float4)(-0.09991f, -0.33609f, 0.436f, 0.501961f))
	#define Vcoeff ((float4)(0.615f, -0.55861f, -0.05639f, 0.501961f))

	//BGR
	#define YcoeffB ((float4)(0.0722f, 0.7152f, 0.2126f, 0.f))
	#define UcoeffB ((float4)(0.436f, -0.33609f, -0.09991f, 0.501961f))
	#define VcoeffB ((float4)(-0.05639f, -0.55861f, 0.615f, 0.501961f))
#endif

#ifdef BT709_ALT1_LIMITED
	//RGB limited to [16...235]
	#define RGB_LIMITED
	#define Ycoeff ((float4)(0.2126f, 0.7152f, 0.0722f, 0.f))
	#define Ucoeff ((float4)(-0.1146f, -0.3854f, 0.5000f, 0.501961f))
	#define Vcoeff ((float4)(0.5000f, -0.4542f, -0.0468f, 0.501961f))

	//BGR
	#define YcoeffB ((float4)(0.0722f, 0.7152f, 0.2126f, 0.f))
	#define UcoeffB ((float4)(0.5000f, -0.3854f, -0.1146f, 0.501961f))
	#define VcoeffB ((float4)(-0.0468f, -0.4542f, 0.5000f, 0.501961f))
#endif

// ------------------------------

__kernel void BGRAtoNV12_YUV(const __global uchar4 *input,
						__write_only image2d_t outputY,
						__write_only image2d_t outputUV)
{
	int2 id = (int2)(get_global_id(0), get_global_id(1));

	int width = get_global_size(0) * 2;
	int height = get_global_size(1) * 2;
	int heightHalf = get_global_size(1);

	float4 px[2][2];
	//Some speed-up from prefetch: 0.1ms -> 0.07ms
	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			px[i][j] = (float4)(convert_float3(input[id.x * 2 + i + width * id.y * 2 + width * j].xyz), 255.f);

			#ifdef RGB_LIMITED
				px[i][j].xyz = 16.f + px[i][j].xyz * 219.f / 255.f;
			#endif
		}
	}

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			uint Y = convert_uint_sat_rte(dot(YcoeffB, px[i][j]));

#ifdef FLIP
			write_imageui(outputY, (int2)(id.x * 2 + i, height - (id.y * 2 + j) - 1), (uint4)(Y, 0, 0, 255));
#else
			write_imageui(outputY, (int2)(id.x * 2 + i, id.y * 2 + j), (uint4)(Y, 0, 0, 255));
#endif
		}
	}

	float2 UV00 = (float2)(dot(px[0][0], UcoeffB), dot(px[0][0], VcoeffB));
	float2 UV01 = (float2)(dot(px[0][1], UcoeffB), dot(px[0][1], VcoeffB));
	float2 UV10 = (float2)(dot(px[1][0], UcoeffB), dot(px[1][0], VcoeffB));
	float2 UV11 = (float2)(dot(px[1][1], UcoeffB), dot(px[1][1], VcoeffB));

	uint2 UV = convert_uint2_sat_rte((UV00 + UV01 + UV10 + UV11) / 4);

#ifdef FLIP
	write_imageui(outputUV, (int2)(id.x, heightHalf - id.y - 1), (uint4)(UV.x, UV.y, 0, 255));
#else
	write_imageui(outputUV, id, (uint4)(UV.x, UV.y, 0, 255));
#endif

}

__kernel void BGRtoNV12_YUV(const __global uchar *input,
						__write_only image2d_t outputY,
						__write_only image2d_t outputUV)
{
	int2 id = (int2)(get_global_id(0), get_global_id(1));

	int width = get_global_size(0) * 2;
	int height = get_global_size(1) * 2;
	int heightHalf = get_global_size(1);

	float4 px[2][2];
	//Some speed-up from prefetch
	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			px[i][j] = (float4)(convert_float3(vload3(id.x * 2 + i + width * id.y *2 + width * j, input)), 255.0f);

			#ifdef RGB_LIMITED
				px[i][j].xyz = 16.f + px[i][j].xyz * 219.f / 255.f;
			#endif
		}
	}

	for (int j = 0; j < 2; j++)
	for (int i = 0; i < 2; i++)
	{
		uint Y = convert_uint_sat_rte(dot(YcoeffB, px[i][j]));

		#ifdef FLIP
			write_imageui(outputY, (int2)(id.x * 2 + i, height - (id.y * 2 + j) - 1), (uint4)(Y, 0, 0, 255));
		#else
			write_imageui(outputY, (int2)(id.x * 2 + i, id.y * 2 + j), (uint4)(Y, 0, 0, 255));
		#endif
	}

	float2 UV00 = (float2)(dot(px[0][0], UcoeffB), dot(px[0][0], VcoeffB));
	float2 UV01 = (float2)(dot(px[0][1], UcoeffB), dot(px[0][1], VcoeffB));
	float2 UV10 = (float2)(dot(px[1][0], UcoeffB), dot(px[1][0], VcoeffB));
	float2 UV11 = (float2)(dot(px[1][1], UcoeffB), dot(px[1][1], VcoeffB));

	uint2 UV = convert_uint2_sat_rte((UV00 + UV01 + UV10 + UV11) / 4);

#ifdef FLIP
	write_imageui(outputUV, (int2)(id.x, heightHalf - id.y - 1), (uint4)(UV.x, UV.y, 0, 255));
#else
	write_imageui(outputUV, id, (uint4)(UV.x, UV.y, 0, 255));
#endif

}
// ------------------------------

__kernel void BGRAtoNV12_Y(const __global uchar4 *input,
						__write_only image2d_t output,
						int alignedWidth)
{
	int2 id = (int2)(get_global_id(0), get_global_id(1));

	int width = get_global_size(0);
	int height = get_global_size(1);

	float4 bgra = (float4)(convert_float3(input[id.x + width * id.y].xyz), 255.f);

#ifdef RGB_LIMITED
	bgra.xyz = 16.f + bgra.xyz * 219.f / 255.f;
#endif

	uint Y = convert_uint_sat_rte(dot(YcoeffB, bgra));

#ifdef FLIP
	//output[id.x + (height - id.y - 1) * alignedWidth] = Y;
	write_imageui(output, (int2)(id.x, height - id.y - 1), (uint4)(Y, 0, 0, 255));
#else
	//output[id.x + id.y * alignedWidth] = Y;
	write_imageui(output, id, (uint4)(Y, 0, 0, 255));
#endif
}

// ------------------------------

__kernel void BGRAtoNV12_UV(const __global uchar4 *input,
						__write_only image2d_t output,
						int alignedWidth)
{
	int2 id = (int2)(get_global_id(0), get_global_id(1));

	uint width = get_global_size(0) * 2;
	uint src = id.x * 2 + width * id.y * 2;
	uint heightHalf = get_global_size(1);
	uint height = get_global_size(1) * 2;

//#ifdef FLIP
//	uint uv_offset = alignedWidth * height + //Skip luma bytes
//					(heightHalf - id.y - 1) * alignedWidth + id.x * 2;
//#else
//	uint uv_offset = alignedWidth * height + id.y * alignedWidth + id.x * 2;
//#endif

	// sample 2x2 square
	float4 bgr00 = (float4)(convert_float3(input[src].xyz), 255.f);
	float4 bgr01 = (float4)(convert_float3(input[src + 1].xyz), 255.f);
	//next line
	float4 bgr10 = (float4)(convert_float3(input[src + width].xyz), 255.f);
	float4 bgr11 = (float4)(convert_float3(input[src + width + 1].xyz), 255.f);

#ifdef RGB_LIMITED
	bgr00.xyz = 16.f + bgr00.xyz * 219.f / 255.f;
	bgr01.xyz = 16.f + bgr01.xyz * 219.f / 255.f;
	bgr10.xyz = 16.f + bgr10.xyz * 219.f / 255.f;
	bgr11.xyz = 16.f + bgr11.xyz * 219.f / 255.f;
#endif

	//Seems like no difference between dot() and plain mul/add on GPU atleast
	float2 UV00 = (float2)(dot(bgr00, UcoeffB), dot(bgr00, VcoeffB));
	float2 UV01 = (float2)(dot(bgr01, UcoeffB), dot(bgr01, VcoeffB));
	float2 UV10 = (float2)(dot(bgr10, UcoeffB), dot(bgr10, VcoeffB));
	float2 UV11 = (float2)(dot(bgr11, UcoeffB), dot(bgr11, VcoeffB));

	uint2 UV = convert_uint2_sat_rte((UV00 + UV01 + UV10 + UV11) / 4);

	//output[uv_offset]     = UV.x;
	//output[uv_offset + 1] = UV.y;
#ifdef FLIP
	write_imageui(output, (int2)(id.x, heightHalf - id.y - 1), (uint4)(UV.x, UV.y, 0, 255));
#else
	write_imageui(output, id, (uint4)(UV.x, UV.y, 0, 255));
#endif
}

// ------------------------------

__kernel void BGRtoNV12_Y(__global uchar *input,
						__write_only image2d_t output,
						int alignedWidth)
{
	int2 id = (int2)(get_global_id(0), get_global_id(1));

	uint width = get_global_size(0);
	uint height = get_global_size(1);

	//Unaligned read and probably slooooow
	float4 bgra = (float4)(convert_float3(vload3(id.x + width * id.y, input)), 255.0f);

#ifdef RGB_LIMITED
	bgra.xyz = 16.f + bgra.xyz * 219.f / 255.f;
#endif

	uint Y = convert_uint_sat_rte(dot(YcoeffB, bgra));

#ifdef FLIP
	//output[id.x + (height- id.y - 1) * alignedWidth] = Y;
	write_imageui(output, (int2)(id.x, height - id.y - 1), (uint4)(Y, 0, 0, 255));
#else
	//output[id.x + id.y * alignedWidth] = Y;
	write_imageui(output, id, (uint4)(Y, 0, 0, 255));
#endif
}

// ------------------------------

// Run over half width/height
__kernel void BGRtoNV12_UV(__global uchar *input,
						__write_only image2d_t output,
						int alignedWidth)
{
	int2 id = (int2)(get_global_id(0), get_global_id(1));

	uint width = get_global_size(0) * 2;
	uint heightHalf = get_global_size(1);
	uint height = get_global_size(1) * 2;

//#ifdef FLIP
//	uint uv_offset = alignedWidth * height + //Skip luma bytes
//					(heightHalf - id.y - 1) * alignedWidth + id.x * 2;
//#else
//	uint uv_offset = alignedWidth * height + id.y * alignedWidth + id.x * 2;
//#endif

	uint src = id.x * 2 + width * id.y * 2;

	// sample 2x2 square
	float4 bgr00 = (float4)(convert_float3(vload3(src, input)), 255.0f);
	float4 bgr01 = (float4)(convert_float3(vload3(src + 1, input)), 255.0f);
	//next line
	float4 bgr10 = (float4)(convert_float3(vload3(src + width, input)), 255.0f);
	float4 bgr11 = (float4)(convert_float3(vload3(src + width + 1, input)), 255.0f);

#ifdef RGB_LIMITED
	bgr00.xyz = 16.f + bgr00.xyz * 219.f / 255.f;
	bgr01.xyz = 16.f + bgr01.xyz * 219.f / 255.f;
	bgr10.xyz = 16.f + bgr10.xyz * 219.f / 255.f;
	bgr11.xyz = 16.f + bgr11.xyz * 219.f / 255.f;
#endif

	float2 UV00 = (float2)(dot(bgr00, UcoeffB), dot(bgr00, VcoeffB));
	float2 UV01 = (float2)(dot(bgr01, UcoeffB), dot(bgr01, VcoeffB));
	float2 UV10 = (float2)(dot(bgr10, UcoeffB), dot(bgr10, VcoeffB));
	float2 UV11 = (float2)(dot(bgr11, UcoeffB), dot(bgr11, VcoeffB));

	uint2 UV = convert_uint2_sat_rte((UV00 + UV01 + UV10 + UV11) / 4);

	//output[uv_offset]     = UV.x;
	//output[uv_offset + 1] = UV.y;
#ifdef FLIP
	write_imageui(output, (int2)(id.x, heightHalf - id.y - 1), (uint4)(UV.x, UV.y, 0, 255));
#else
	write_imageui(output, id, (uint4)(UV.x, UV.y, 0, 255));
#endif
}

// Might read too much from resource pointer, comment it out :P
//