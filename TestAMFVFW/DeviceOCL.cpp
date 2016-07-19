#include "stdafx.h"
#include "DeviceOCL.h"
#include "Log.h"

extern HMODULE hmoduleVFW;

#define Log(...) Dbg(__VA_ARGS__)
#define RETURNIFERROR(s, txt) if(s != CL_SUCCESS) { Dbg(txt); return false; }
#define INITPFN(x) \
	x = static_cast<x ## _fn>(clGetExtensionFunctionAddressForPlatform(platformID, #x));\
	if(!x) { Dbg(TEXT("Cannot resolve ") TEXT(#x) TEXT(" function")); return false;}

DeviceOCL::DeviceOCL()
	: mDevice(nullptr)
	, mContext(nullptr)
	, mCmdQueue(nullptr)
	, mKernelY(nullptr)
	, mKernelUV(nullptr)
	, mProgram(nullptr)
	, mInBuf(nullptr)
	, mOutImgY(nullptr)
	, mOutImgUV(nullptr)
	, mBuffY(nullptr)
	, mBuffUV(nullptr)
	, mWidth(0)
	, mHeight(0)
	, mAlignedWidth(0)
	, mAlignedHeight(0)
{

}

DeviceOCL::~DeviceOCL()
{
	Terminate();
}

void DeviceOCL::Terminate()
{
	mBufferCopyManager.Stop();

	if (mInBuf)
		clReleaseMemObject(mInBuf);
	mInBuf = nullptr;

	if (mOutImgY)
		clReleaseMemObject(mOutImgY);
	mOutImgY = nullptr;

	if (mOutImgUV)
		clReleaseMemObject(mOutImgUV);
	mOutImgUV = nullptr;

	if (mKernelY)
		clReleaseKernel(mKernelY);
	mKernelY = nullptr;

	if (mKernelUV)
		clReleaseKernel(mKernelUV);
	mKernelUV = nullptr;

	if (mProgram)
		clReleaseProgram(mProgram);
	mProgram = nullptr;

	if (mCmdQueue)
		clReleaseCommandQueue(mCmdQueue);
	mCmdQueue = nullptr;

	if (mDevice)
		clReleaseDevice(mDevice);
	mDevice = nullptr;

	if (mContext)
		clReleaseContext(mContext);
	mContext = nullptr;
}

bool DeviceOCL::Init(ID3D11Device *pD3DDevice, int width, int height, COLORMATRIX matrix, bool useCPU)
{
	size_t strSize = 0;
	cl_int status = 0;
	std::vector<cl_context_properties> cps;
	cl_platform_id platformID = nullptr;

	if (!FindPlatformID(platformID))
		return false;

	if ((width & 1) || (height & 1)) // not divisible by 2, bugger off
		return false;

	mWidth = width;
	mHeight = height;
	mAlignedWidth = ((width + (256 - 1)) & ~(256 - 1));
	mAlignedHeight = ((height + (32 - 1)) & ~(32 - 1));

	status = clGetPlatformInfo(platformID, CL_PLATFORM_EXTENSIONS, 0, nullptr, &strSize);
	if (!status)
	{
		char *exts = new char[strSize];
		status = clGetPlatformInfo(platformID, CL_PLATFORM_EXTENSIONS, strSize, exts, nullptr);
		Log(L"CL Platform Extensions: %S.\n", exts);
		delete[] exts;
	}

	if (!useCPU)
	{
		clGetDeviceIDsFromD3D11KHR_fn p_clGetDeviceIDsFromD3D11KHR =
			static_cast<clGetDeviceIDsFromD3D11KHR_fn>
			(clGetExtensionFunctionAddressForPlatform(platformID, "clGetDeviceIDsFromD3D11KHR"));
		if (!p_clGetDeviceIDsFromD3D11KHR)
		{
			Log(L"Cannot resolve ClGetDeviceIDsFromD3D11KHR function.\n");
			return false;
		}

		status = p_clGetDeviceIDsFromD3D11KHR(platformID, CL_D3D11_DEVICE_KHR,
			(void*)pD3DDevice, CL_PREFERRED_DEVICES_FOR_D3D11_KHR, 1, &mDevice, NULL);
		RETURNIFERROR(status, L"clGetDeviceIDsFromD3D11KHR() failed.\n");

		status = clGetDeviceInfo(mDevice, CL_DEVICE_EXTENSIONS, 0, nullptr, &strSize);
		if (!status)
		{
			char *exts = new char[strSize];
			status = clGetDeviceInfo(mDevice, CL_DEVICE_EXTENSIONS, strSize, exts, nullptr);
			Log(L"CL Device Extensions: %S.\n", exts);
			delete[] exts;
		}

		cps.push_back(CL_CONTEXT_D3D11_DEVICE_KHR);
		cps.push_back((cl_context_properties)pD3DDevice);
	}
	else
	{
		status = clGetDeviceIDs(platformID, CL_DEVICE_TYPE_CPU, 1, &mDevice, nullptr);
		RETURNIFERROR(status, L"clGetDeviceIDs() failed.\n");
	}

	cps.push_back(CL_CONTEXT_PLATFORM);
	cps.push_back((cl_context_properties)platformID);
	cps.push_back(0);

	mContext = clCreateContext(&cps[0], 1, &mDevice, NULL, NULL, &status);
	RETURNIFERROR(status, L"clCreateContext() failed.\n");

	mCmdQueue = clCreateCommandQueue(mContext, mDevice, (cl_command_queue_properties)NULL, &status);
	RETURNIFERROR(status, L"clCreateCommandQueue() failed.\n");

	// ---------------------------

	HRSRC hResource = FindResourceExA(hmoduleVFW, "STRING",
			MAKEINTRESOURCEA(IDR_OPENCL),
			MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT));

	if (!hResource)
	{
		Log(L"Cannot load kernels source from resource.\n");
		return false;
	}

	const char* source = (const char*)LoadResource(hmoduleVFW, hResource);
	size_t srcSize[] = { SizeofResource(hmoduleVFW, hResource) };

	//Dbg(L"%S\n\n", source);
	mProgram = clCreateProgramWithSource(mContext, 1, &source, srcSize, &status);
	RETURNIFERROR(status, L"clCreateProgramWithSource failed.\n");

	std::string flagsStr(""); //"-save-temps"
	//if (mOptimize)
		flagsStr.append("-cl-single-precision-constant -cl-mad-enable "
			"-cl-fast-relaxed-math -cl-unsafe-math-optimizations ");

	switch (matrix)
	{
	case BT601_LIMITED:
		flagsStr.append("-DBT601_LIMITED ");
		break;
	case BT601_FULL:
		flagsStr.append("-DBT601_FULL ");
		break;
	case BT601_FULL_YCbCr:
		flagsStr.append("-DBT601_FULL_YCbCr ");
		break;
	case BT709_LIMITED:
		flagsStr.append("-DBT709_LIMITED ");
		break;
	case BT709_FULL:
		flagsStr.append("-DBT709_FULL ");
		break;
	case BT709_ALT1_LIMITED:
		flagsStr.append("-DBT709_ALT1_LIMITED ");
		break;
		/*case BT709_ALT1_FULL:
		flagsStr.append("-DBT709_ALT1_FULL ");
		break;*/
	default:
		flagsStr.append("-DBT601_LIMITED ");
		break;
	}
	if (flagsStr.size() != 0)
		Log(L"Build Options are : %S\n", flagsStr.c_str());

	status = clBuildProgram(mProgram, 1, &mDevice, flagsStr.c_str(), NULL, NULL);
	if (status != CL_SUCCESS)
	{
		if (status == CL_BUILD_PROGRAM_FAILURE)
		{
			cl_int logStatus;
			std::vector<char> buildLog;
			size_t logSize = 0;
			logStatus = clGetProgramBuildInfo(mProgram, mDevice,
				CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
			RETURNIFERROR(logStatus, L"clGetProgramBuildInfo failed.\n");

			buildLog.resize(logSize);
			logStatus = clGetProgramBuildInfo(mProgram, mDevice,
				CL_PROGRAM_BUILD_LOG, logSize, &buildLog[0], NULL);

			RETURNIFERROR(logStatus, L"clGetProgramBuildInfo failed.\n");
			Log(
				L"\n\t\t\tBUILD LOG\n"
				L" ************************************************\n"
				L" %S"
				L" ************************************************\n",
				&buildLog[0]);
		}

		RETURNIFERROR(status, L"clBuildProgram() failed.\n");
	}

	cl_image_format imgf;
	imgf.image_channel_order = CL_R;
	imgf.image_channel_data_type = CL_UNSIGNED_INT8;

	mOutImgY = clCreateImage2D(mContext,
		//TODO tweak these
		//CL_MEM_WRITE_ONLY,
		CL_MEM_WRITE_ONLY | CL_MEM_USE_PERSISTENT_MEM_AMD,
		&imgf, mAlignedWidth, mAlignedHeight, 0, nullptr, &status);
	RETURNIFERROR(status, L"Failed to create Y image.\n");

	imgf.image_channel_order = CL_RG;
	imgf.image_channel_data_type = CL_UNSIGNED_INT8;

	int uv_width = mAlignedWidth / 2;
	int uv_height = (mAlignedHeight + 1) / 2;

	mOutImgUV = clCreateImage2D(mContext,
		CL_MEM_WRITE_ONLY | CL_MEM_USE_PERSISTENT_MEM_AMD,
		&imgf, uv_width, uv_height,
		0, nullptr, &status);
	RETURNIFERROR(status, L"Failed to create UV image.\n");

	/*mBuffY = clCreateBuffer(mContext, CL_MEM_READ_ONLY, mAlignedWidth * mAlignedHeight, nullptr, &status);
	RETURNIFERROR(status, L"Failed to create Y buffer.\n");

	mBuffUV = clCreateBuffer(mContext, CL_MEM_READ_ONLY, uv_width * uv_height * 2, nullptr, &status);
	RETURNIFERROR(status, L"Failed to create Y buffer.\n");*/

	return true;
}

bool DeviceOCL::InitBGRAKernels(int bpp)
{
	if (bpp != 32 && bpp != 24)
	{
		Log(L"InitBGRAKernels: Wrong bits per pixel count.\n");
		return false;
	}

	const char* kernels[][2] = {
		{ "BGRAtoNV12_YUV", "BGRAtoNV12_UV" },
		{ "BGRtoNV12_YUV", "BGRtoNV12_UV" },
	};

	cl_int status = 0;
	int kernel = (bpp == 32) ? 0 : 1;

	mKernelY = clCreateKernel(mProgram, kernels[kernel][0], &status);
	RETURNIFERROR(status, L"clCreateKernel(Y) failed!\n");

	//mKernelUV = clCreateKernel(mProgram, kernels[kernel][1], &status);
	//RETURNIFERROR(status, L"clCreateKernel(UV) failed!\n");

	int inSize = mWidth * mHeight * bpp / 8 + ((mWidth * bpp / 8) % 4) * mHeight;
	int inSizeAligned = (inSize + 255) & ~255; //unnecessery?

	mInBuf = clCreateBuffer(
		mContext,
		CL_MEM_READ_ONLY | CL_MEM_USE_PERSISTENT_MEM_AMD,
		//CL_MEM_READ_ONLY|CL_MEM_ALLOC_HOST_PTR, //~6GB/s map/memcpy, slow NDR
		inSizeAligned,
		NULL,
		&status);
	RETURNIFERROR(status, L"clCreateBuffer failed!\n");

	/*if (!setKernelArgs(mKernelY, mInBuf, mOutImgY) ||
		!setKernelArgs(mKernelUV, mInBuf, mOutImgUV))
		return false;*/

	if (!setKernelArgs(mKernelY, mInBuf, mOutImgY, mOutImgUV))
		return false;

	mBufferCopyManager.Start(3);
	return true;
}

bool DeviceOCL::setKernelArgs(cl_kernel kernel, cl_mem input, cl_mem output, cl_mem outputUV)
{
	cl_int status = 0;

	// Set up kernel arguments
	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input);
	RETURNIFERROR(status, L"clSetKernelArg(input) failed!\n");

	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &output);
	RETURNIFERROR(status, L"clSetKernelArg(output) failed!\n");

	status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &outputUV);
	RETURNIFERROR(status, L"clSetKernelArg(output) failed!\n");

	//status = clSetKernelArg(kernel, 2, sizeof(int), &mAlignedWidth);
	//RETURNIFERROR(status, L"clSetKernelArg(alignedWidth) failed!\n");
	return true;
}

bool DeviceOCL::ConvertBuffer(void *inBuf, size_t size, void* dest, size_t destPitch)
{
	cl_int status = 0;
	cl_event unmapEvent;
	cl_event ndrEvents[2];
	size_t globalThreads[] = { mWidth>>1, mHeight>>1 };

	Profile(MapBuffer)
	void *mapPtr = clEnqueueMapBuffer(mCmdQueue,
		mInBuf,
		CL_TRUE,
		//CL_FALSE,
		CL_MAP_WRITE_INVALIDATE_REGION,
		0,
		size,
		0,
		NULL,
		NULL,
		&status);
	RETURNIFERROR(status, L"clEnqueueMapBuffer() failed.\n");

	//memcpy(mapPtr, inBuf, size);
	mBufferCopyManager.SetData(inBuf, mapPtr, size);
	mBufferCopyManager.Wait();

	status = clEnqueueUnmapMemObject(mCmdQueue,
		mInBuf,
		mapPtr,
		0,
		NULL,
		&unmapEvent);
	status = clFlush(mCmdQueue);
	status = clWaitForEvents(1, &unmapEvent);
	if (status != CL_SUCCESS) Log(L"clWaitForEvents(unmapEvent) failed: %d.\n", status);
	clReleaseEvent(unmapEvent);
	//status = clEnqueueBarrier(mCmdQueue);
	//if (status != CL_SUCCESS) Log(L"clEnqueueBarrier failed: %d.\n", status);
	EndProfile

	Profile(EnqNDR)

	status = clEnqueueNDRangeKernel(mCmdQueue, mKernelY, 2, nullptr,
		globalThreads, nullptr, 0, nullptr, &ndrEvents[0]);
	RETURNIFERROR(status, L"Failed to enqueue Y kernel.\n");

	//TODO off by one probably
	//globalThreads[0] = mWidth & 1 ? (mWidth + 1) / 2 : mWidth / 2;
	//globalThreads[1] = mHeight & 1 ? (mHeight - 1) / 2 : mHeight / 2;

	/*status = clEnqueueNDRangeKernel(mCmdQueue, mKernelUV, 2, nullptr,
		globalThreads, nullptr, 0, nullptr, &ndrEvents[1]);
	RETURNIFERROR(status, L"Failed to enqueue UV kernel.\n");*/

	status = clWaitForEvents(1, ndrEvents);
	if (status != CL_SUCCESS) Log(L"clWaitForEvents(ndrEvents) failed: %d.\n", status);
	clReleaseEvent(ndrEvents[0]);
	//clReleaseEvent(ndrEvents[1]);
	//clFinish(mCmdQueue);

	//slow and fuxxored
	if (dest)
	{
		size_t origin[] = { 0, 0, 0 };
		size_t region[] = { mAlignedWidth, mAlignedHeight, 1 };
		size_t image_row_pitchY, image_row_pitchUV;

		void *pY = clEnqueueMapImage(mCmdQueue, mOutImgY, CL_FALSE, CL_MAP_READ,
			origin, region,
			&image_row_pitchY, nullptr,
			0, nullptr,
			&ndrEvents[0], &status);
		RETURNIFERROR(status, L"Failed to map Y image.\n");

		region[0] /= 2;
		region[1] = (mAlignedHeight + 1) / 2;

		void *pUV = clEnqueueMapImage(mCmdQueue, mOutImgUV, CL_FALSE, CL_MAP_READ,
			origin, region,
			&image_row_pitchUV, nullptr,
			0, nullptr,
			&ndrEvents[1], &status);
		RETURNIFERROR(status, L"Failed to map UV image.\n");

		status = clWaitForEvents(2, ndrEvents);
		if (status != CL_SUCCESS) Log(L"clWaitForEvents for map events failed: %d.\n", status);
		clReleaseEvent(ndrEvents[0]);
		clReleaseEvent(ndrEvents[1]);

		/*if (destPitch == image_row_pitchY)
		{
			memcpy(dest, pY, destPitch * mHeight);
			memcpy((uint8_t*)dest + destPitch * mHeight, pUV, image_row_pitchUV * mHeight / 2);
		}
		else*/
		{
			uint8_t *pSrcY = (uint8_t *)pY;
			uint8_t *pSrcUV = (uint8_t *)pUV;
			uint8_t* pDst = (uint8_t*)dest;

			for (int y = 0; y < mHeight; y++, pSrcY += image_row_pitchY, pDst += destPitch)
				memcpy(pDst, pSrcY, mWidth);

			pDst = (uint8_t*)dest + destPitch * mHeight;
			for (int y = 0; y < mHeight / 2; y++, pSrcUV += image_row_pitchUV, pDst += destPitch)
				memcpy(pDst, pSrcUV, mWidth);
		}

		status = clEnqueueUnmapMemObject(mCmdQueue, mOutImgY, pY, 0, nullptr, nullptr);
		if (status != CL_SUCCESS) Log(L"clEnqueueUnmapMemObject(pY) failed: %d.\n", status);
		status = clEnqueueUnmapMemObject(mCmdQueue, mOutImgUV, pUV, 0, nullptr, nullptr);
		if (status != CL_SUCCESS) Log(L"clEnqueueUnmapMemObject(pUV) failed: %d.\n", status);
	}

	EndProfile

	return true;
}

bool DeviceOCL::FindPlatformID(cl_platform_id &platform)
{
	cl_int status = 0;
	cl_uint numPlatforms = 0;
	status = clGetPlatformIDs(0, NULL, &numPlatforms);
	RETURNIFERROR(status, L"clGetPlatformIDs() failed.\n");
	if (numPlatforms == 0)
	{
		Log(TEXT("clGetPlatformIDs() returned 0 platforms.\n"));
		return false;
	}

	std::vector<cl_platform_id> platforms;
	platforms.resize(numPlatforms);
	status = clGetPlatformIDs(numPlatforms, &platforms[0], NULL);
	RETURNIFERROR(status, L"clGetPlatformIDs() failed.\n");
	bool bFound = false;
	for (cl_uint i = 0; i < numPlatforms; ++i)
	{
		char pbuf[1000];
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(pbuf), pbuf, NULL);
		RETURNIFERROR(status, L"clGetPlatformInfo() failed.\n");
		if (!strcmp(pbuf, "Advanced Micro Devices, Inc."))
		{
			platform = platforms[i];
			bFound = true;
			return true;
		}
	}
	return false;
}