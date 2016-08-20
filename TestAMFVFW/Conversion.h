#pragma once
#include <cstdint>
void ConvertRGB24toNV12_SSE2(const uint8_t *src, uint8_t *ydest, /*uint8_t *udest, uint8_t *vdest, */size_t w, size_t h, size_t sh, size_t eh, size_t hpitch, size_t vpitch);
void ConvertRGB32toNV12_SSE2(const uint8_t *src, uint8_t *ydest, /*uint8_t *udest, uint8_t *vdest, */size_t w, size_t h, size_t sh, size_t eh, size_t hpitch, size_t vpitch);
void BGRtoNV12(const uint8_t * src, uint8_t * yuv, unsigned bytesPerPixel, uint8_t flip, int srcFrameWidth, int srcFrameHeight, uint32_t yuvPitch);
