#include "stdafx.h"
#include "Conversion.h"

//AviSynth toyv12
void BGRtoNV12(const uint8_t * src,
	uint8_t * yuv,
	unsigned bytesPerPixel,
	uint8_t flip,
	int srcFrameWidth, int srcFrameHeight, uint32_t yuvPitch)
{

	// Colour conversion from
	// http://www.poynton.com/notes/colour_and_gamma/ColorFAQ.html#RTFToC30
	//
	// YCbCr in Rec. 601 format
	// RGB values are in the range [0..255]
	//
	// [ Y  ]   [  16 ]    1    [  65.738    129.057    25.064  ]   [ R ]
	// [ Cb ] = [ 128 ] + --- * [ -37.945    -74.494    112.439 ] * [ G ]
	// [ Cr ]   [ 128 ]   256   [ 112.439    -94.154    -18.285 ]   [ B ]

	int rgbPitch = srcFrameWidth;
	unsigned int planeSize;
	unsigned int halfWidth;

	uint8_t * Y;
	uint8_t * UV;
	//uint8 * V;
	int x, y;

	planeSize = yuvPitch * srcFrameHeight;
	halfWidth = yuvPitch >> 1;

	// get pointers to the data
	Y = yuv;
	UV = yuv + planeSize;

	long RtoYCoeff = long(65.738 * 256 + 0.5);
	long GtoYCoeff = long(129.057 * 256 + 0.5);
	long BtoYCoeff = long(25.064 * 256 + 0.5);

	long RtoUCoeff = long(-37.945 * 256 + 0.5);
	long GtoUCoeff = long(-74.494 * 256 + 0.5);
	long BtoUCoeff = long(112.439 * 256 + 0.5);

	long RtoVCoeff = long(112.439 * 256 + 0.5);
	long GtoVCoeff = long(-94.154 * 256 + 0.5);
	long BtoVCoeff = long(-18.285 * 256 + 0.5);

	uint32_t U00, U01, U10, U11;
	uint32_t V00, V01, V10, V11;

	//#pragma omp parallel
	{
		////#pragma omp section
		{
			//Y plane
			//#pragma omp parallel for 
			for (y = 0; y < srcFrameHeight; y++)
			{
				uint8_t *lY;
				if (!!flip)
					lY = Y + yuvPitch * (srcFrameHeight - y - 1);
				else
					lY = Y + yuvPitch * y;//, src += padRGB
				const uint8_t *lsrc = src + (srcFrameWidth*(y)*bytesPerPixel);
				for (x = srcFrameWidth; x > 0; x--)
				{
					// No need to saturate between 16 and 235
					*(lY++) = uint8_t(16 + ((32768 + RtoYCoeff * lsrc[2] + GtoYCoeff * lsrc[1] + BtoYCoeff * lsrc[0]) >> 16));
					lsrc += bytesPerPixel;
				}
			}
		}

		//U and V planes

		//#pragma omp for 
		for (y = 0; y < (srcFrameHeight >> 1); y++)
		{
			uint8_t *lUV;
			if (!!flip)
				lUV = UV + yuvPitch * ((srcFrameHeight >> 1) - y - 1);
			else
				lUV = UV + yuvPitch * y;

			const uint8_t *pPx00 = src + rgbPitch * bytesPerPixel * y * 2, *pPx01, *pPx10, *pPx11;

			for (x = 0; x < (srcFrameWidth >> 1); x++)
			{
				pPx01 = pPx00 + bytesPerPixel;
				pPx10 = pPx00 + rgbPitch * bytesPerPixel;
				pPx11 = pPx10 + bytesPerPixel;

				// No need to saturate between 16 and 240
				// Sample pixels from 2x2 box
				U00 = 128 + ((32768 + RtoUCoeff * pPx00[2] + GtoUCoeff * pPx00[1] + BtoUCoeff * pPx00[0]) >> 16);
				V00 = 128 + ((32768 + RtoVCoeff * pPx00[2] + GtoVCoeff * pPx00[1] + BtoVCoeff * pPx00[0]) >> 16);

				U01 = 128 + ((32768 + RtoUCoeff * pPx01[2] + GtoUCoeff * pPx01[1] + BtoUCoeff * pPx01[0]) >> 16);
				V01 = 128 + ((32768 + RtoVCoeff * pPx01[2] + GtoVCoeff * pPx01[1] + BtoVCoeff * pPx01[0]) >> 16);

				U10 = 128 + ((32768 + RtoUCoeff * pPx10[2] + GtoUCoeff * pPx10[1] + BtoUCoeff * pPx10[0]) >> 16);
				V10 = 128 + ((32768 + RtoVCoeff * pPx10[2] + GtoVCoeff * pPx10[1] + BtoVCoeff * pPx10[0]) >> 16);

				U11 = 128 + ((32768 + RtoUCoeff * pPx11[2] + GtoUCoeff * pPx11[1] + BtoUCoeff * pPx11[0]) >> 16);
				V11 = 128 + ((32768 + RtoVCoeff * pPx11[2] + GtoVCoeff * pPx11[1] + BtoVCoeff * pPx11[0]) >> 16);

				lUV[0] = uint8_t((2 + U00 + U01 + U10 + U11) >> 2);
				lUV[1] = uint8_t((2 + V00 + V01 + V10 + V11) >> 2);

				lUV += 2;
				pPx00 += 2 * bytesPerPixel;

				//UV[0] = -0.14713f * src[0] - 0.28886f * src[1] + 0.436f * src[2] + 128;
				//UV[1] = 0.615f * src[0] - 0.51499f * src[1] - 0.10001f * src[2] + 128;
			}
		}
	}

}

// Rec.601
void ConvertRGB24toNV12_SSE2(const uint8_t *src, uint8_t *ydest, /*uint8_t *udest, uint8_t *vdest, */size_t w, size_t h, size_t sh, size_t eh, size_t hpitch, size_t vpitch) {
	const __m128i fraction = _mm_setr_epi32(0x84000, 0x84000, 0x84000, 0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32 = _mm_setr_epi32(-32, -32, -32, -32);
	const __m128i y1y2_mult = _mm_setr_epi32(0x4A85, 0x4A85, 0x4A85, 0x4A85);
	const __m128i fpix_add = _mm_setr_epi32(0x808000, 0x808000, 0x808000, 0x808000);
	const __m128i fpix_mul = _mm_setr_epi32(0x1fb, 0x282, 0x1fb, 0x282);

	// 0x0c88 == BtoYCoeff / 2, 0x4087 == GtoYCoeff / 2, 0x20DE == RtoYCoeff / 2
	const __m128i cybgr_64 = _mm_setr_epi16(0, 0x0c88, 0x4087, 0x20DE, 0x0c88, 0x4087, 0x20DE, 0);

	//for (unsigned int y = 0; y<h; y += 2) {
	for (size_t y = sh; y < eh; y += 2) {
		uint8_t *ydst = ydest + (h - y - 1) * hpitch;
		//YV12
		//uint8_t *udst = udest + (h - y - 2) / 2 * hpitch / 2;
		//uint8_t *vdst = vdest + (h - y - 2) / 2 * hpitch / 2;
		//NV12
		uint8_t *uvdst = ydest + hpitch * vpitch + (h - y - 2) / 2 * hpitch;

		for (unsigned int x = 0; x<w; x += 4) {
			__m128i rgb0 = _mm_cvtsi32_si128(*(int*)&src[y*w * 3 + x * 3]);
			__m128i rgb1 = _mm_loadl_epi64((__m128i*)&src[y*w * 3 + x * 3 + 4]);
			__m128i rgb2 = _mm_cvtsi32_si128(*(int*)&src[y*w * 3 + x * 3 + w * 3]);
			__m128i rgb3 = _mm_loadl_epi64((__m128i*)&src[y*w * 3 + x * 3 + 4 + w * 3]);
			rgb0 = _mm_unpacklo_epi32(rgb0, rgb1);
			rgb0 = _mm_slli_si128(rgb0, 1);
			rgb1 = _mm_srli_si128(rgb1, 1);

			rgb2 = _mm_unpacklo_epi32(rgb2, rgb3);
			rgb2 = _mm_slli_si128(rgb2, 1);
			rgb3 = _mm_srli_si128(rgb3, 1);

			rgb0 = _mm_unpacklo_epi8(rgb0, _mm_setzero_si128());
			rgb1 = _mm_unpacklo_epi8(rgb1, _mm_setzero_si128());
			rgb2 = _mm_unpacklo_epi8(rgb2, _mm_setzero_si128());
			rgb3 = _mm_unpacklo_epi8(rgb3, _mm_setzero_si128());

			__m128i luma0 = _mm_madd_epi16(rgb0, cybgr_64);
			__m128i luma1 = _mm_madd_epi16(rgb1, cybgr_64);
			__m128i luma2 = _mm_madd_epi16(rgb2, cybgr_64);
			__m128i luma3 = _mm_madd_epi16(rgb3, cybgr_64);

			rgb0 = _mm_add_epi16(rgb0, _mm_srli_si128(rgb0, 6));
			rgb1 = _mm_add_epi16(rgb1, _mm_srli_si128(rgb1, 6));
			rgb2 = _mm_add_epi16(rgb2, _mm_srli_si128(rgb2, 6));
			rgb3 = _mm_add_epi16(rgb3, _mm_srli_si128(rgb3, 6));

			__m128i chroma0 = _mm_unpacklo_epi64(rgb0, rgb1);
			__m128i chroma1 = _mm_unpacklo_epi64(rgb2, rgb3);
			chroma0 = _mm_srli_epi32(chroma0, 16); // remove green channel
			chroma1 = _mm_srli_epi32(chroma1, 16); // remove green channel

			luma0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma1 = _mm_add_epi32(luma1, _mm_shuffle_epi32(luma1, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma2 = _mm_add_epi32(luma2, _mm_shuffle_epi32(luma2, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma3 = _mm_add_epi32(luma3, _mm_shuffle_epi32(luma3, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma0 = _mm_srli_si128(luma0, 4);
			luma1 = _mm_srli_si128(luma1, 4);
			luma2 = _mm_srli_si128(luma2, 4);
			luma3 = _mm_srli_si128(luma3, 4);
			luma0 = _mm_unpacklo_epi64(luma0, luma1);
			luma2 = _mm_unpacklo_epi64(luma2, luma3); // luma1, luma3 no longer used

			luma0 = _mm_add_epi32(luma0, fraction);
			luma2 = _mm_add_epi32(luma2, fraction);
			luma0 = _mm_srli_epi32(luma0, 15);
			luma2 = _mm_srli_epi32(luma2, 15);

			__m128i temp0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0, 1 + (0 << 2) + (3 << 4) + (2 << 6)));
			__m128i temp1 = _mm_add_epi32(luma2, _mm_shuffle_epi32(luma2, 1 + (0 << 2) + (3 << 4) + (2 << 6)));
			temp0 = _mm_add_epi32(temp0, neg32);
			temp1 = _mm_add_epi32(temp1, neg32);
			temp0 = _mm_madd_epi16(temp0, y1y2_mult);
			temp1 = _mm_madd_epi16(temp1, y1y2_mult);

			luma0 = _mm_packs_epi32(luma0, luma0);
			luma2 = _mm_packs_epi32(luma2, luma2);
			luma0 = _mm_packus_epi16(luma0, luma0);
			luma2 = _mm_packus_epi16(luma2, luma2);

			//if ( *(int *)&ydst[x]!=_mm_cvtsi128_si32(luma0)||
			//	*(int *)&ydst[x-w]!=_mm_cvtsi128_si32(luma2) ){
			//	__asm int 3;
			//}

			*(int *)&ydst[x] = _mm_cvtsi128_si32(luma0);
			*(int *)&ydst[x - size_t(hpitch)] = _mm_cvtsi128_si32(luma2);


			chroma0 = _mm_slli_epi64(chroma0, 14);
			chroma1 = _mm_slli_epi64(chroma1, 14);
			chroma0 = _mm_sub_epi32(chroma0, temp0);
			chroma1 = _mm_sub_epi32(chroma1, temp1);
			chroma0 = _mm_srli_epi32(chroma0, 9);
			chroma1 = _mm_srli_epi32(chroma1, 9);
			chroma0 = _mm_madd_epi16(chroma0, fpix_mul);
			chroma1 = _mm_madd_epi16(chroma1, fpix_mul);
			chroma0 = _mm_add_epi32(chroma0, fpix_add);
			chroma1 = _mm_add_epi32(chroma1, fpix_add);
			chroma0 = _mm_packus_epi16(chroma0, chroma0);
			chroma1 = _mm_packus_epi16(chroma1, chroma1);

			chroma0 = _mm_avg_epu8(chroma0, chroma1);

			chroma0 = _mm_srli_epi16(chroma0, 8);
			// Pack UVUV into UUVV for YV12
			//chroma0 = _mm_shufflelo_epi16(chroma0, 0 + (2 << 2) + (1 << 4) + (3 << 6));
			chroma0 = _mm_packus_epi16(chroma0, chroma0);

			//if ( *(unsigned short *)&udst[x/2]!=_mm_extract_epi16(chroma0,0) ||
			//	*(unsigned short *)&vdst[x/2]!=_mm_extract_epi16(chroma0,1)){
			//	__asm int 3;
			//}

			//YV12
			//*(unsigned short *)&udst[x / 2] = _mm_extract_epi16(chroma0, 0);
			//*(unsigned short *)&vdst[x / 2] = _mm_extract_epi16(chroma0, 1);

			//NV12
			//*(int *)&uvdst[x] = _mm_extract_epi32(chroma0, 0); // SSE4
			*(unsigned short *)&uvdst[x] = _mm_extract_epi16(chroma0, 0);
			*(unsigned short *)&uvdst[x + 2] = _mm_extract_epi16(chroma0, 1);
		}
	}
}

void ConvertRGB32toNV12_SSE2(const uint8_t *src, uint8_t *ydest, size_t w, size_t h, size_t sh, size_t eh, size_t hpitch, size_t vpitch) {
	const __m128i fraction = _mm_setr_epi32(0x84000, 0x84000, 0x84000, 0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32 = _mm_setr_epi32(-32, -32, -32, -32);
	const __m128i y1y2_mult = _mm_setr_epi32(0x4A85, 0x4A85, 0x4A85, 0x4A85);
	const __m128i fpix_add = _mm_setr_epi32(0x808000, 0x808000, 0x808000, 0x808000);
	const __m128i fpix_mul = _mm_setr_epi32(0x1fb, 0x282, 0x1fb, 0x282);
	const __m128i cybgr_64 = _mm_setr_epi16(0x0c88, 0x4087, 0x20DE, 0, 0x0c88, 0x4087, 0x20DE, 0);

	for (size_t y = sh; y < eh; y += 2) {
		uint8_t *ydst = ydest + (h - y - 1) * hpitch;
		//YV12
		//uint8_t *udst = udest + (h - y - 2) / 2 * hpitch / 2;
		//uint8_t *vdst = vdest + (h - y - 2) / 2 * hpitch / 2;
		//NV12
		uint8_t *uvdst = ydest + hpitch * vpitch + (h - y - 2) / 2 * hpitch;

		for (unsigned int x = 0; x<w; x += 4) {
			__m128i rgb0 = _mm_loadl_epi64((__m128i*)&src[y*w * 4 + x * 4]);
			__m128i rgb1 = _mm_loadl_epi64((__m128i*)&src[y*w * 4 + x * 4 + 8]);
			__m128i rgb2 = _mm_loadl_epi64((__m128i*)&src[y*w * 4 + x * 4 + w * 4]);
			__m128i rgb3 = _mm_loadl_epi64((__m128i*)&src[y*w * 4 + x * 4 + 8 + w * 4]);

			rgb0 = _mm_unpacklo_epi8(rgb0, _mm_setzero_si128());
			rgb1 = _mm_unpacklo_epi8(rgb1, _mm_setzero_si128());
			rgb2 = _mm_unpacklo_epi8(rgb2, _mm_setzero_si128());
			rgb3 = _mm_unpacklo_epi8(rgb3, _mm_setzero_si128());

			__m128i luma0 = _mm_madd_epi16(rgb0, cybgr_64);
			__m128i luma1 = _mm_madd_epi16(rgb1, cybgr_64);
			__m128i luma2 = _mm_madd_epi16(rgb2, cybgr_64);
			__m128i luma3 = _mm_madd_epi16(rgb3, cybgr_64);

			rgb0 = _mm_add_epi16(rgb0, _mm_shuffle_epi32(rgb0, (2 << 0) + (3 << 2) + (0 << 4) + (1 << 6)));
			rgb1 = _mm_add_epi16(rgb1, _mm_shuffle_epi32(rgb1, (2 << 0) + (3 << 2) + (0 << 4) + (1 << 6)));
			rgb2 = _mm_add_epi16(rgb2, _mm_shuffle_epi32(rgb2, (2 << 0) + (3 << 2) + (0 << 4) + (1 << 6)));
			rgb3 = _mm_add_epi16(rgb3, _mm_shuffle_epi32(rgb3, (2 << 0) + (3 << 2) + (0 << 4) + (1 << 6)));

			__m128i chroma0 = _mm_unpacklo_epi64(rgb0, rgb1);
			__m128i chroma1 = _mm_unpacklo_epi64(rgb2, rgb3);
			chroma0 = _mm_slli_epi32(chroma0, 16); // remove green channel
			chroma1 = _mm_slli_epi32(chroma1, 16);

			luma0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma1 = _mm_add_epi32(luma1, _mm_shuffle_epi32(luma1, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma2 = _mm_add_epi32(luma2, _mm_shuffle_epi32(luma2, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma3 = _mm_add_epi32(luma3, _mm_shuffle_epi32(luma3, (1 << 0) + (0 << 2) + (3 << 4) + (2 << 6)));
			luma0 = _mm_srli_si128(luma0, 4);
			luma1 = _mm_srli_si128(luma1, 4);
			luma2 = _mm_srli_si128(luma2, 4);
			luma3 = _mm_srli_si128(luma3, 4);
			luma0 = _mm_unpacklo_epi64(luma0, luma1);
			luma2 = _mm_unpacklo_epi64(luma2, luma3);

			luma0 = _mm_add_epi32(luma0, fraction);
			luma2 = _mm_add_epi32(luma2, fraction);
			luma0 = _mm_srli_epi32(luma0, 15);
			luma2 = _mm_srli_epi32(luma2, 15);

			__m128i temp0 = _mm_add_epi32(luma0, _mm_shuffle_epi32(luma0, 1 + (0 << 2) + (3 << 4) + (2 << 6)));
			__m128i temp1 = _mm_add_epi32(luma2, _mm_shuffle_epi32(luma2, 1 + (0 << 2) + (3 << 4) + (2 << 6)));
			temp0 = _mm_add_epi32(temp0, neg32);
			temp1 = _mm_add_epi32(temp1, neg32);
			temp0 = _mm_madd_epi16(temp0, y1y2_mult);
			temp1 = _mm_madd_epi16(temp1, y1y2_mult);

			luma0 = _mm_packs_epi32(luma0, luma0);
			luma2 = _mm_packs_epi32(luma2, luma2);
			luma0 = _mm_packus_epi16(luma0, luma0);
			luma2 = _mm_packus_epi16(luma2, luma2);

			//if ( *(int *)&ydst[x]!=_mm_cvtsi128_si32(luma0)||
			//	*(int *)&ydst[x-w]!=_mm_cvtsi128_si32(luma2)){
			//	__asm int 3;
			//}

			*(int *)&ydst[x] = _mm_cvtsi128_si32(luma0);
			*(int *)&ydst[x - size_t(hpitch)] = _mm_cvtsi128_si32(luma2);

			chroma0 = _mm_srli_epi64(chroma0, 2);
			chroma1 = _mm_srli_epi64(chroma1, 2);
			chroma0 = _mm_sub_epi32(chroma0, temp0);
			chroma1 = _mm_sub_epi32(chroma1, temp1);
			chroma0 = _mm_srli_epi32(chroma0, 9);
			chroma1 = _mm_srli_epi32(chroma1, 9);
			chroma0 = _mm_madd_epi16(chroma0, fpix_mul);
			chroma1 = _mm_madd_epi16(chroma1, fpix_mul);
			chroma0 = _mm_add_epi32(chroma0, fpix_add);
			chroma1 = _mm_add_epi32(chroma1, fpix_add);
			chroma0 = _mm_packus_epi16(chroma0, chroma0);
			chroma1 = _mm_packus_epi16(chroma1, chroma1);

			chroma0 = _mm_avg_epu8(chroma0, chroma1);

			chroma0 = _mm_srli_epi16(chroma0, 8);
			//YV12
			//chroma0 = _mm_shufflelo_epi16(chroma0, 0 + (2 << 2) + (1 << 4) + (3 << 6));
			chroma0 = _mm_packus_epi16(chroma0, chroma0);

			//if ( *(unsigned short *)&udst[x/2]!=_mm_extract_epi16(chroma0,0) ||
			//	*(unsigned short *)&vdst[x/2]!=_mm_extract_epi16(chroma0,1)){
			//	__asm int 3;
			//}

			//*(unsigned short *)&udst[x / 2] = _mm_extract_epi16(chroma0, 0);
			//*(unsigned short *)&vdst[x / 2] = _mm_extract_epi16(chroma0, 1);
			//*(int *)&uvdst[x] = _mm_extract_epi32(chroma0, 0); //SSE4
			*(unsigned short *)&uvdst[x] = _mm_extract_epi16(chroma0, 0);
			*(unsigned short *)&uvdst[x + 2] = _mm_extract_epi16(chroma0, 1);
		}
	}
}