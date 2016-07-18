#pragma once
#include <d3d11.h>
#include <CL/cl.h>
#include <CL/cl_ext.h>
#include <CL/cl_d3d11.h>
#include "Colorspace.h"
#include "BufferCopyManager.h"

class DeviceOCL
{
public:
	DeviceOCL();
	~DeviceOCL();
	bool Init(ID3D11Device *pD3DDevice, int width, int height, COLORMATRIX matrix, bool useCPU = false);
	bool InitBGRAKernels(int bpp);
	bool ConvertBuffer(void *inBuf, size_t size, void* dest = nullptr, size_t destPitch = 0);
	void Terminate();

	cl_device_id GetDevice()
	{
		return mDevice;
	}

	cl_command_queue GetCmdQueue()
	{
		return mCmdQueue;
	}

	void GetYUVImages(void* imgs[2])
	{
		imgs[0] = mOutImgY;
		imgs[1] = mOutImgUV;
	}

private:
	bool FindPlatformID(cl_platform_id &platform);
	bool setKernelArgs(cl_kernel kernel, cl_mem input, cl_mem outputY, cl_mem outputUV);

	cl_device_id mDevice;
	cl_context mContext;
	cl_command_queue mCmdQueue;
	cl_kernel mKernelY, mKernelUV;
	cl_program mProgram;
	cl_mem mInBuf, mOutImgY, mOutImgUV, mBuffY, mBuffUV;
	int mWidth, mHeight, mAlignedWidth, mAlignedHeight;
	BufferCopyManager mBufferCopyManager;
};