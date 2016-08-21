#include "stdafx.h"
#include "TestAMFVFW.h"
#include "DeviceOCL.h"
#include <VersionHelpers.h>

using hrc = std::chrono::high_resolution_clock;

#define Log(...) LogMsg(false, __VA_ARGS__)
#define AMFLOGIFFAILED(r, ...) \
	do { if(r != AMF_OK) LogMsg(false, __VA_ARGS__); } while(0)
#define AMFLOGSET(component, var, val) \
	do { \
		if(!component) break;\
		AMF_RESULT r = component->SetProperty(var, val); \
		if(r != AMF_OK){ \
			LogMsg(false, L"Failed to set %s.", L#var); \
		}\
	} while(0)

void CodecInst::PrintProps(amf::AMFPropertyStorage *props)
{
	amf::AMFBuffer* buffer = nullptr;
	if (!props) return;
	amf_size count = props->GetPropertyCount();
	for (amf_size i = 0; i < count; i++)
	{
		wchar_t name[256];
		amf::AMFVariant var;
		if (AMF_OK == props->GetPropertyAt(i, name, 256, &var))
		{
			switch (var.type)
			{
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_EMPTY:
				Log(TEXT("%s = <empty>"), name);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_BOOL:
				Log(TEXT("%s = <bool>%d"), name, var.boolValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_INT64:
				Log(TEXT("%s = %lld"), name, var.int64Value);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_STRING:
				Log(TEXT("%s = <str>%S"), name, var.stringValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_WSTRING:
				Log(TEXT("%s = <wstr>%s"), name, var.wstringValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_SIZE:
				Log(TEXT("%s = <size>%dx%d"), name, var.sizeValue.width, var.sizeValue.height);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_RATE:
				Log(TEXT("%s = <rate>%d/%d"), name, var.rateValue.num, var.rateValue.den);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_RATIO:
				Log(TEXT("%s = <ratio>%d/%d"), name, var.ratioValue.num, var.ratioValue.den);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_INTERFACE:
				Log(TEXT("%s = <interface>"), name);
				break;
			default:
				Log(TEXT("%s = <type %d>"), name, var.type);
			}
		}
		else
		{
			Log(TEXT("Failed to get property at index %d"), i);
		}
	}
}

// check if the codec can compress the given format to the desired format
DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut){

	if (mLog) mLog->enableLog(!!mConfigTable[S_LOG]);
	Log(L"Compression query: %d %x %dx%d", lpbiIn->biBitCount, lpbiIn->biCompression, lpbiIn->biWidth, lpbiIn->biHeight);

	// check for valid format and bitdepth
	if (lpbiIn->biCompression == 0){
		/*if (!mCLConv && lpbiIn->biBitCount == 24)
			return (DWORD)ICERR_BADFORMAT;
		else*/ if (lpbiIn->biBitCount != 24 && lpbiIn->biBitCount != 32)
			return (DWORD)ICERR_BADFORMAT;
	}
	/*else if ( lpbiIn->biCompression == FOURCC_YUY2 || lpbiIn->biCompression == FOURCC_UYVY || lpbiIn->biCompression == FOURCC_YV16 ){
	if ( lpbiIn->biBitCount != 16 ) {
	return_badformat()
	}
	}*/
	else if (lpbiIn->biCompression == FOURCC_YV12 ||
		lpbiIn->biCompression == FOURCC_NV12)
	{
		if (lpbiIn->biBitCount != 12 && lpbiIn->biBitCount != 16) { //Virtualdub sends NV12 as 16bits???
			return (DWORD)ICERR_BADFORMAT;
		}
	}
	else {
		return (DWORD)ICERR_BADFORMAT;
	}

	mWidth = lpbiIn->biWidth;
	mHeight = lpbiIn->biHeight;

	/* We need x2 width/height although it gets aligned to 16 by 16
	*/
	if (mWidth % 2 || mHeight % 2)// || (mWidth * mHeight > 1920 * 1088)) //TODO get max w/h from caps
	{
		Log(L"Width/height is not multiple of 2.");
		return ICERR_BADFORMAT; //Should probably be ICERR_BADIMAGESIZE
	}

	Log(L"CompressQuery OK");
	return (DWORD)ICERR_OK;
}

/* Return the maximum number of bytes a single compressed frame can occupy */
DWORD x264vfw_compress_get_size(LPBITMAPINFOHEADER lpbiOut)
{
	return ((lpbiOut->biWidth + 15) & ~15) * ((lpbiOut->biHeight + 31) & ~31) * 3 + 4096;
}

// get the maximum size a compressed frame will take;
// 105% of image size + 1KB should be plenty even for random static
DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut){
	//return (DWORD)( align_round(lpbiIn->biWidth,16)*lpbiIn->biHeight*lpbiIn->biBitCount/8*1.05 + 1024);
	return x264vfw_compress_get_size(lpbiOut);
}

// return the intended compress format for the given input format
DWORD CodecInst::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut){

	if (!lpbiOut){
		return sizeof(BITMAPINFOHEADER);
	}

	if (mLog) mLog->enableLog(!!mConfigTable[S_LOG]);

	// make sure the input is an acceptable format
	if (CompressQuery(lpbiIn, NULL) == ICERR_BADFORMAT){
		return (DWORD)ICERR_BADFORMAT;
	}

	//FIXME 
	*lpbiOut = *lpbiIn;
	lpbiOut->biSize = sizeof(BITMAPINFOHEADER);
	lpbiOut->biCompression = FOURCC_H264;
	//x264vfw
	lpbiOut->biBitCount = (lpbiIn->biBitCount == 32 || lpbiIn->biBitCount == 24) ? lpbiIn->biBitCount : 24;
	lpbiOut->biPlanes = 1;
	lpbiOut->biSizeImage = x264vfw_compress_get_size(lpbiOut);
	lpbiOut->biXPelsPerMeter = 0;
	lpbiOut->biYPelsPerMeter = 0;
	lpbiOut->biClrUsed = 0;
	lpbiOut->biClrImportant = 0;

	return (DWORD)ICERR_OK;
}

// initalize the codec for compression
DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut){
	AMF_RESULT res = AMF_OK;
	//amf::H264EncoderCapsPtr encCaps;
	amf::AMFCapsPtr encCaps;
	bool usingAMFConv = false;
	amf::AMFComputeFactoryPtr amfComputeFactory;
	amf::AMFTrace *trace = nullptr;

	if (started == 0x1337){
		LogMsg(false, L"CompressBegin: already began compressing, exiting...");
		CompressEnd();
	}
	started = 0;

	int error = ICERR_OK;
	if ((error = CompressQuery(lpbiIn, lpbiOut)) != ICERR_OK){
		return error;
	}

	if (!BindDLLs())
		return ICERR_INTERNAL;

	started = 0x1337;
	mWidth = lpbiIn->biWidth;
	mHeight = lpbiIn->biHeight;
	mIDRPeriod = mConfigTable[S_IDR];
	mCLConv = !mConfigTable[S_DISABLE_OCL];

	//TODO Check math. In 100-nanoseconds
	if (fps_num && fps_den)
		mFrameDuration = ((amf_pts)10000000000 / ((amf_pts)fps_num * 1000 / fps_den));

	amf_uint64 version = 0;
	AMFQueryVersion(&version);

#define STRFYVER(x) \
	(int)(x>>48) & 0xFFFF, (int)(x>>32) & 0xFFFF, (int)(x>>16) & 0xFFFF, (int)x & 0xFFFF

	LogMsg(false, L"AMF version %d.%d.%d.%d, trying to init with version %d.%d.%d.%d",
		STRFYVER(version),
		AMF_VERSION_MAJOR, AMF_VERSION_MINOR, AMF_VERSION_RELEASE, AMF_VERSION_BUILD_NUM);

#undef STRFYVER

	res = AMFInit(AMF_FULL_VERSION, &mAMFFactory);
	if (res != AMF_OK)
		goto fail;

	mAMFFactory->GetTrace(&trace);

#ifndef NDEBUG
	amf::AMFDebug *amfDebug = nullptr;
	if (mAMFFactory->GetDebug(&amfDebug) == AMF_OK && amfDebug)
	{
		amfDebug->AssertsEnable(true);
		amfDebug->EnablePerformanceMonitor(true);
	}

	if (trace)
	{
		trace->SetGlobalLevel(AMF_TRACE_TEST);
	}
#endif

	res = mAMFFactory->CreateContext(&mContext);
	if (res != AMF_OK)
		goto fail;

	// if gonna decide to use AMFCompute for OpenCL
	/*mContext->GetOpenCLComputeFactory(&amfComputeFactory);
	Dbg(L"Compute devices: %d\n", amfComputeFactory->GetDeviceCount());

	if (amfComputeFactory->GetDeviceCount())
	{
		amfComputeFactory->GetDeviceAt(0, &mComputeDev);
		PrintProps(mComputeDev);
		if (mComputeDev->CreateCompute(nullptr, &mCompute) != AMF_OK)
			goto fail;
	}*/

	mFmtIn = amf::AMF_SURFACE_BGRA;
	switch (lpbiIn->biCompression)
	{
	case FOURCC_NV12: mFmtIn = amf::AMF_SURFACE_NV12; break;
	case FOURCC_YV12: mFmtIn = amf::AMF_SURFACE_YV12; break;
	case 0:
		//if (lpbiIn->biBitCount == 32 
		//	|| (mCLConv && (lpbiIn->biBitCount == 32 || lpbiIn->biBitCount == 24)))
			mFmtIn = amf::AMF_SURFACE_BGRA;
		/*else
		{
			LogMsg(true, L"24 bpp is not supported.");
			goto fail;
		}*/
		break;
	default:
		LogMsg(true, L"Input format is not supported: %04X", lpbiIn->biCompression);
		goto fail;
	}

	if (!BindDLLs())
		goto fail;

	if (!mDeviceDX11.Create(mConfigTable[S_DEVICEIDX], false))
		goto fail;

	if (!mDeviceCL.Init(mDeviceDX11.GetDevice(), nullptr, mWidth, mHeight,
	//if (!mDeviceCL.Init(nullptr, mCompute, mWidth, mHeight,
			mConfigTable[S_COLORPROF] == AMF_VIDEO_CONVERTER_COLOR_PROFILE_601 ? BT601_FULL : BT709_FULL))
		mCLConv = false;

	if ((mFmtIn != amf::AMF_SURFACE_BGRA) || !mDeviceCL.InitBGRAKernels(lpbiIn->biBitCount))
	{
		mCLConv = false;
		//TODO 24bit RGB non-CL conversion
		if (lpbiIn->biBitCount == 24)
			goto fail;
	}

	// Preselect NV12 converter
	switch (mFmtIn)
	{
	case amf::AMF_SURFACE_BGRA:
	{
		// 0 - DXCompute, 1 - AMF, 2 - openCL, 3 - CPU
		if (mCLConv /*|| lpbiIn->biBitCount == 24*/ && mConfigTable[S_CONVTYPE] == 2)
		{
			mSubmitter = std::unique_ptr<OpenCLSubmitter>(new OpenCLSubmitter(this));
			LogMsg(false, L"Selected converter: OpenCLSubmitter");
		}
		else
		{
			if (mConfigTable[S_CONVTYPE] == 3)
			{
				if (IsWindows8OrGreater())
				{
					mSubmitter = std::unique_ptr<DX11Submitter>(new DX11Submitter(this));
					LogMsg(false, L"Selected converter: DX11Submitter");
				}
				else
				{
					mSubmitter = std::unique_ptr<HostSubmitter>(new HostSubmitter(this));
					LogMsg(false, L"Selected converter: HostSubmitter");
				}
			}
			else if (lpbiIn->biBitCount == 32)
			{
				if (mConfigTable[S_CONVTYPE] == 0 && IsWindows8OrGreater())
				{
					mSubmitter = std::unique_ptr<DX11ComputeSubmitter>(new DX11ComputeSubmitter(this));
					LogMsg(false, L"Selected converter: DX11ComputeSubmitter");
				}
				else
				{
					//mSubmitter = new DX11Submitter(this);
					mSubmitter = std::unique_ptr<AMFConverterSubmitter>(new AMFConverterSubmitter(this));
					usingAMFConv = true;
					LogMsg(false, L"Selected converter: fallback AMFConverterSubmitter");
				}
			}
			else
			{
				mSubmitter = std::unique_ptr<HostSubmitter>(new HostSubmitter(this)); // slow CPU conversion
				LogMsg(false, L"Selected converter: fallback HostSubmitter");
			}
		}
	}
	break;
	case amf::AMF_SURFACE_NV12:
		mSubmitter = std::unique_ptr<NV12Submitter>(new NV12Submitter(this));
		LogMsg(false, L"Selected converter: NV12Submitter");
		break;
	default:
		mSubmitter = std::unique_ptr<AMFConverterSubmitter>(new AMFConverterSubmitter(this));
		LogMsg(false, L"Selected converter: AMFConverterSubmitter");
		usingAMFConv = true;
		break;
	}

	res = mContext->InitDX11(mDeviceDX11.GetDevice(), amf::AMF_DX11_0);
	if (res != AMF_OK)
		goto fail;

	// Some speed up
	if (mCLConv)
	{
		res = mContext->InitOpenCL(mDeviceCL.GetCmdQueue());
		//res = mContext->InitOpenCLEx(mComputeDev);
		if (res != AMF_OK)
			goto fail;
	}

	if (usingAMFConv)
	{
		res = mAMFFactory->CreateComponent(mContext, AMFVideoConverter, &mConverter);
		if (res != AMF_OK)
			goto fail;

		AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_MEMORY_TYPE, amf::AMF_MEMORY_OPENCL);
		AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, amf::AMF_SURFACE_NV12);
		AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_COLOR_PROFILE, mConfigTable[S_COLORPROF]); //AMF_VIDEO_CONVERTER_COLOR_PROFILE_709
		AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_OUTPUT_SIZE, ::AMFConstructSize(mWidth, mHeight));

		res = mConverter->Init(mFmtIn, mWidth, mHeight);
		if (res != AMF_OK)
			goto fail;
	}

	res = mAMFFactory->CreateComponent(mContext, AMFVideoEncoderVCE_AVC, &mEncoder);
	if (res != AMF_OK)
		goto fail;

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCONDING);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_SCANTYPE, AMF_VIDEO_ENCODER_SCANTYPE_PROGRESSIVE);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_PROFILE, mConfigTable[S_PROFILE]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_PROFILE_LEVEL, mConfigTable[S_LEVEL]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QUALITY_PRESET, mConfigTable[S_PRESET]);

	res = mEncoder->Init(amf::AMF_SURFACE_NV12, mWidth, mHeight);
	if (res != AMF_OK)
		goto fail;

	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_GOP_SIZE, mConfigTable[S_GOP]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_IDR_PERIOD, mConfigTable[S_IDR]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING, mConfigTable[S_IDR]);

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, mConfigTable[S_RCM]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_TARGET_BITRATE, mConfigTable[S_BITRATE] * 1000);
	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_PEAK_BITRATE, mConfigTable[S_BITRATE] * 1000);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE, mConfigTable[S_BITRATE] * 2000);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(mWidth, mHeight));
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(fps_num, fps_den));

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QP_I, mConfigTable[S_QPI]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QP_P, mConfigTable[S_QPI]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QP_B, mConfigTable[S_QPI]);

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
	//sdk 1.1
	AMFLOGSET(mEncoder, L"CABACEnable", !!mConfigTable[S_CABAC]);
	//AMFLOGSET(mEncoder, L"LowLatencyInternal", 1);
	//AMFLOGSET(mEncoder, L"MaxNumRefFrames", 4);
	

	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_B_PIC_DELTA_QP, qpBDelta);
	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_REF_B_PIC_DELTA_QP, qpBDelta);

	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_MIN_QP, 18);
	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_MAX_QP, 51);

	//amf_increase_timer_precision();

	if (!mSubmitter->Init())
		goto fail;

	if (mEncoder->GetCaps(&encCaps) == AMF_OK && trace)
	//if (mEncoder->QueryInterface(amf::AMFH264EncoderCaps::IID(), (void**)&encCaps) == AMF_OK)
	{
		const TCHAR* accelType[] = {
			TEXT("NOT_SUPPORTED"),
			TEXT("HARDWARE"),
			TEXT("GPU"),
			TEXT("SOFTWARE")
		};
		Log(TEXT("Capabilities:"));
		Log(TEXT("  Accel type: %s"), accelType[(encCaps->GetAccelerationType() + 1) % 4]);

		std::vector<std::pair<const wchar_t*, const wchar_t*>> capVals = {
			{AMF_VIDEO_ENCODER_CAP_MAX_BITRATE, L"Max bitrate"},
			{AMF_VIDEO_ENCODER_CAP_NUM_OF_STREAMS, L"Num of streams"},
			{AMF_VIDEO_ENCODER_CAP_MAX_PROFILE, L"Max profile"},
			{AMF_VIDEO_ENCODER_CAP_MAX_LEVEL, L"Max level"},
			{AMF_VIDEO_ENCODER_CAP_BFRAMES, L"B-frames supported"},
			{AMF_VIDEO_ENCODER_CAP_MIN_REFERENCE_FRAMES, L"Min reference frames"},
			{AMF_VIDEO_ENCODER_CAP_MAX_REFERENCE_FRAMES, L"Max reference frames"},
			{AMF_VIDEO_ENCODER_CAP_MAX_TEMPORAL_LAYERS, L"Max temporal layers"},
			{AMF_VIDEO_ENCODER_CAP_FIXED_SLICE_MODE, L"Fixed slice mode"},
			{AMF_VIDEO_ENCODER_CAP_NUM_OF_HW_INSTANCES, L"Num of HW instances"}
		};

		amf_int64 val;
		for (auto& capVal : capVals)
		{
			if (encCaps->GetProperty(capVal.first, &val) == AMF_OK)
			{
				Log(L"  %s: %lld", capVal.second, val);
			}
		}

		//PrintProps(encCaps);

		amf::AMFIOCapsPtr iocaps;
		encCaps->GetInputCaps(&iocaps);
		Log(TEXT("  Input mem types:"));
		for (int i = 0; i < iocaps->GetNumOfMemoryTypes(); i++)
		{
			bool native;
			amf::AMF_MEMORY_TYPE memType;
			iocaps->GetMemoryTypeAt(i, &memType, &native);
			Log(TEXT("    %s, native: %s"), trace->GetMemoryTypeName(memType),
				native ? TEXT("Yes") : TEXT("No"));
			if (native && (mNativeMemType == amf::AMF_MEMORY_UNKNOWN))
				mNativeMemType = memType;
		}

		Log(TEXT("  Input formats:"));
		for (int i = 0; i < iocaps->GetNumOfFormats(); i++)
		{
			amf_bool native;
			amf::AMF_SURFACE_FORMAT fmt;
			iocaps->GetFormatAt(i, &fmt, &native);
			Log(TEXT("    %s, native: %s"), trace->SurfaceGetFormatName(fmt),
				native ? TEXT("Yes") : TEXT("No"));
		}

		amf_int32 imin, imax;
		iocaps->GetWidthRange(&imin, &imax);
		Log(TEXT("  Width min/max: %d/%d"), imin, imax);
		iocaps->GetHeightRange(&imin, &imax);
		Log(TEXT("  Height min/max: %d/%d"), imin, imax);

		encCaps->GetOutputCaps(&iocaps);
		Log(TEXT("  Output mem types:"));
		for (int i = 0; i < iocaps->GetNumOfMemoryTypes(); i++)
		{
			bool native;
			amf::AMF_MEMORY_TYPE memType;
			iocaps->GetMemoryTypeAt(i, &memType, &native);
			Log(TEXT("    %s, native: %s"), trace->GetMemoryTypeName(memType),
				native ? TEXT("Yes") : TEXT("No"));
			if (native && (mNativeMemType == amf::AMF_MEMORY_UNKNOWN))
				mNativeMemType = memType;
		}

		Log(TEXT("  Output formats:"));
		for (int i = 0; i < iocaps->GetNumOfFormats(); i++)
		{
			amf_bool native;
			amf::AMF_SURFACE_FORMAT fmt;
			iocaps->GetFormatAt(i, &fmt, &native);
			Log(TEXT("    %s, native: %s"), trace->SurfaceGetFormatName(fmt),
				native ? TEXT("Yes") : TEXT("No"));
		}
	}

	Log(L"\r\nConverter props:\r\n----------------");
	PrintProps(mConverter);
	Log(L"\r\nEncoder props:\r\n--------------");
	PrintProps(mEncoder);
	Log(L"CompressBegin");
	return ICERR_OK;

fail:
	if (res != AMF_OK)
	{
		LogMsg(true, L"AMF failed with error: %d, %s", res, trace ? trace->GetResultText(res) : L"");
	}
	CompressEnd();
	return ICERR_INTERNAL;
}

DWORD CodecInst::CompressEnd(){
	Log(L"CompressEnd");
	started = 0;
	AMF_RESULT res = AMF_REPEAT;
	mCLConv = !mConfigTable[S_DISABLE_OCL]; //reset as class instance is not always dtor'ed

	mSubmitter = nullptr;

	if (mConverter)
	{
		res = mConverter->Drain();
		res = mConverter->Flush();
		res = AMF_REPEAT;
		do
		{
			amf::AMFDataPtr buffer;
			res = mConverter->QueryOutput(&buffer);
		} while (res == AMF_OK); // gets AMF_EOF
		mConverter->Terminate();
		mConverter = nullptr;
	}

	if (mEncoder)
	{
		res = mEncoder->Drain();
		res = mEncoder->Flush();
		res = AMF_REPEAT;
		do
		{
			amf::AMFDataPtr buffer;
			res = mEncoder->QueryOutput(&buffer);
		} while (res == AMF_OK); // gets AMF_EOF
		mEncoder->Terminate();
		mEncoder = nullptr;
	}

	if (mContext)
		mContext->Terminate();
	mContext = nullptr;

	mDeviceCL.Terminate();
	mDeviceDX11.Free();
	mCompute.Release();
	mComputeDev.Release();
	return ICERR_OK;
}

DWORD CodecInst::CompressFramesInfo(ICCOMPRESSFRAMES *icf)
{
	frame_total = icf->lFrameCount;
	fps_num = icf->dwRate;
	fps_den = icf->dwScale;
	return ICERR_OK;
}

DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD dwSize)
{
#if _DEBUG
	uint64_t duration;
	std::chrono::time_point<hrc> startP, endP;
	startP = hrc::now();
#endif

	AMF_RESULT res = AMF_OK;
	amf::AMFSurfacePtr surfSrc;
	amf::AMFDataPtr    frameData;
	amf::AMFDataPtr    convData;
	void * in  = icinfo->lpInput;
	void * out = icinfo->lpOutput;
	BITMAPINFOHEADER *inhdr = icinfo->lpbiInput;
	BITMAPINFOHEADER *outhdr = icinfo->lpbiOutput;
	int frameType = -1;
	DWORD icerr;

	outhdr->biCompression = FOURCC_H264;

	//mFrameNum = icinfo->lFrameNum;
	if (icinfo->lFrameNum == 0){
		if (started != 0x1337){
			if ((icerr = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput)) != ICERR_OK)
				return icerr;
		}
	}

	if (icinfo->lpckid){
		*icinfo->lpckid = 'cd'; // 'dc' Compressed video frame
	}

	icerr = mSubmitter->Submit(in, inhdr, mFrameDuration * icinfo->lFrameNum);
	if (icerr != ICERR_OK)
		return icerr;

	Profile(EncoderPoll)

	do
	{
		res = mEncoder->QueryOutput(&frameData);
		if (res != AMF_REPEAT) //OK or some error maybe
			break;
		//Sleep(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	} while (true);

	if (frameData == NULL)
		return ICERR_INTERNAL;
	//if (res == AMF_EOF)
	//	return ICERR_OK;

	amf::AMFBufferPtr buffer(frameData);
	//res = buffer->Convert(amf::AMF_MEMORY_HOST); //Maybe?
	memcpy(out, buffer->GetNative(), buffer->GetSize());
	outhdr->biSizeImage = (DWORD)buffer->GetSize();
	buffer->GetProperty(L"OutputDataType", &frameType);

	EndProfile

	if (frameType == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR)
	{
		*icinfo->lpdwFlags = AVIIF_KEYFRAME;
		//Dbg(L"Keyframe: %d\n", icinfo->lFrameNum);
	}
	else
		*icinfo->lpdwFlags = 0;

#if _DEBUG
	endP = hrc::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endP - startP).count();
	Dbg(L"Encoding: %fms\n", (duration / 1.0E6));
#endif

	return ICERR_OK;
}

DWORD NV12Submitter::Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts)
{
	amf::AMFSurfacePtr surface;
	AMF_RESULT res;

	Profile(AllocSurface)

	res = mInstance->mContext->CreateSurfaceFromHostNative(mInstance->mFmtIn,
		mInstance->mWidth, mInstance->mHeight,
		mInstance->mWidth, mInstance->mHeight,
		data, &surface, nullptr);
	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile

	Profile(EncoderSubmit)
	// In 100-nanoseconds
	surface->SetPts(pts);
	res = mInstance->mEncoder->SubmitInput(surface);

	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile
		return ICERR_OK;
}

DWORD AMFConverterSubmitter::Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts)
{
	amf::AMFSurfacePtr surface;
	amf::AMFDataPtr convData;
	AMF_RESULT res;

	static bool isWin8OrGreater = IsWindows8OrGreater();
	Profile(AllocSurface)

	switch (mInstance->mFmtIn)
	{
	case amf::AMF_SURFACE_BGRA:
	if (isWin8OrGreater)
	{
		// Copy and flip RGB32 to AMF surface
		res = mInstance->mContext->AllocSurface(amf::AMF_MEMORY_DX11, amf::AMF_SURFACE_BGRA, mInstance->mWidth, mInstance->mHeight, &surface);
		if (res != AMF_OK)
			return ICERR_INTERNAL;

		ID3D11Texture2D *pTexDst = (ID3D11Texture2D *)surface->GetPlaneAt(0)->GetNative(); //no ref incr
		ID3D11Device *pDevice = nullptr;
		ID3D11DeviceContext* pCtx = nullptr;

		pTexDst->GetDevice(&pDevice);
		pDevice->GetImmediateContext(&pCtx);

		if (!mTexStaging)
		{
			D3D11_TEXTURE2D_DESC desc = { 0 };
			pTexDst->GetDesc(&desc);
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.MiscFlags = 0;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			//desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
			//desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			if (FAILED(pDevice->CreateTexture2D(&desc, NULL, &mTexStaging)))
			{
				pCtx->Release();
				pDevice->Release();
				return ICERR_INTERNAL;
			}
		}

		int inPitch = inhdr->biBitCount / 8 * mInstance->mWidth;
		D3D11_MAPPED_SUBRESOURCE lockedRect;

		if (SUCCEEDED(pCtx->Map(mTexStaging, 0, D3D11_MAP_WRITE_DISCARD, 0, &lockedRect)))
		{
			uint8_t *ptr = (uint8_t *)lockedRect.pData;
			// Flip it
			for (int h = mInstance->mHeight - 1; h >= 0; h--, ptr += lockedRect.RowPitch)
			{
				memcpy(ptr, (uint8_t*)data + h * inPitch, inPitch);
			}
			pCtx->Unmap(mTexStaging, 0);
			pCtx->CopySubresourceRegion(pTexDst, 0, 0, 0, 0, mTexStaging, 0, NULL);
		}

		pCtx->Release();
		pDevice->Release();
	}
	else
	{
		// Copy and flip RGB32 to AMF surface
		res = mInstance->mContext->AllocSurface(amf::AMF_MEMORY_HOST, mInstance->mFmtIn, mInstance->mWidth, mInstance->mHeight, &surface);
		if (res != AMF_OK)
			return ICERR_INTERNAL;
		amf::AMFPlanePtr plane = surface->GetPlaneAt(0);
		int pitch = plane->GetHPitch();
		auto ptr = (uint8_t*)plane->GetNative();
		int inPitch = inhdr->biBitCount / 8 * mInstance->mWidth;

		// Flip it
		for (int h = mInstance->mHeight - 1; h >= 0; h--, ptr += pitch)
		{
			memcpy(ptr, (uint8_t*)data + h * inPitch, inPitch);
		}
	}
	break;
	case amf::AMF_SURFACE_YV12:
		res = mInstance->mContext->CreateSurfaceFromHostNative(mInstance->mFmtIn,
			mInstance->mWidth, mInstance->mHeight,
			mInstance->mWidth, mInstance->mHeight,
			data, &surface, nullptr);

		if (res != AMF_OK)
			return ICERR_INTERNAL;
		break;
	default:
		return ICERR_INTERNAL;
	}

	surface->SetPts(pts);

	EndProfile

	Profile(Conversion)

	res = mInstance->mConverter->SubmitInput(surface);
	if (res != AMF_OK)
		return ICERR_INTERNAL;

	do
	{
		res = mInstance->mConverter->QueryOutput(&convData);
		if (res != AMF_REPEAT) //OK or some error maybe
			break;
		//Sleep(1); //Sleep period can be too random though
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	} while (true);

	if (convData == NULL)
		return ICERR_INTERNAL;

	EndProfile

	Profile(EncoderSubmit)
	res = mInstance->mEncoder->SubmitInput(convData);

	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile
	return ICERR_OK;
}

DWORD OpenCLSubmitter::Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts)
{
	amf::AMFSurfacePtr surface;
	AMF_RESULT res;

	// Convert RGB24|32 to NV12 with OpenCL
	if (!mInstance->mDeviceCL.ConvertBuffer(data, inhdr->biSizeImage))
		return ICERR_INTERNAL;

	Profile(AllocSurface)

	void* imgs[2];
	mInstance->mDeviceCL.GetYUVImages(imgs);

	res = mInstance->mContext->CreateSurfaceFromOpenCLNative(amf::AMF_SURFACE_NV12,
		mInstance->mWidth, mInstance->mHeight, imgs, &surface, nullptr);
	if (res != AMF_OK)
		return ICERR_INTERNAL;

	surface->SetPts(pts);

	EndProfile

	Profile(EncoderSubmit)
	res = mInstance->mEncoder->SubmitInput(surface);

	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile
	return ICERR_OK;
}

bool DX11Submitter::Init()
{
	AMF_RESULT res = mInstance->mContext->AllocSurface(amf::AMF_MEMORY_DX11, amf::AMF_SURFACE_NV12,
		mInstance->mWidth, mInstance->mHeight, &surface);
	if (res != AMF_OK)
		return false;
	mBufferCopyManager.Start(3);
	return true;
}

DWORD DX11Submitter::Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts)
{
	//amf::AMFSurfacePtr surface;
	AMF_RESULT res;
	LONG w = mInstance->mWidth, h = mInstance->mHeight;

	/*Profile(AllocSurface)
	res = mInstance->mContext->AllocSurface(amf::AMF_MEMORY_DX11, amf::AMF_SURFACE_NV12, w, h, &surface);
	if (res != AMF_OK)
		return ICERR_INTERNAL;
	EndProfile*/

	//Profile(Upload)
	ID3D11Device *pDevice = (ID3D11Device *)mInstance->mContext->GetDX11Device();
	ID3D11Texture2D *pTexDst = (ID3D11Texture2D *) surface->GetPlaneAt(0)->GetNative();

	if (pDevice && pTexDst)
	{
		LONG w = mInstance->mWidth, h = mInstance->mHeight;
		ID3D11DeviceContext* pCtx = NULL;
		pDevice->GetImmediateContext(&pCtx);
		if (pCtx)
		{
			if (!mTexStaging)
			{
				D3D11_TEXTURE2D_DESC desc = { 0 };
				desc.Width = w;
				desc.Height = h;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_NV12;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_STAGING;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				//desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
				//desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				if (FAILED(pDevice->CreateTexture2D(&desc, NULL, &mTexStaging)))
					return ICERR_INTERNAL;
			}


			D3D11_MAPPED_SUBRESOURCE lockedRect;

			if (SUCCEEDED(pCtx->Map(mTexStaging, 0, D3D11_MAP_WRITE, 0, &lockedRect)))
			{
				Profile(Conversion)

				//memset(lockedRect.pData, 128, lockedRect.RowPitch * h * 3 / 2);
				//BGRtoNV12((const uint8_t*)data, (uint8_t*)lockedRect.pData, inhdr->biBitCount / 8, 1, w, h, lockedRect.RowPitch);
				/*if (inhdr->biBitCount == 24)
					ConvertRGB24toNV12_SSE2((const uint8_t*)data, (uint8_t*)lockedRect.pData, w, h, 0, lockedRect.RowPitch, h);
				else
					ConvertRGB32toNV12_SSE2((const uint8_t*)data, (uint8_t*)lockedRect.pData, w, h, 0, lockedRect.RowPitch, h);*/

				mBufferCopyManager.SetData(data, lockedRect.pData, inhdr->biBitCount, w, h, lockedRect.RowPitch, h);
				if (mBufferCopyManager.Wait() == WAIT_FAILED)
					return ICERR_INTERNAL;

				// Set useCPU to true
				//if (!mInstance->mDeviceCL.ConvertBuffer(data, inhdr->biSizeImage, lockedRect.pData, lockedRect.RowPitch))
				//	return ICERR_INTERNAL;

				EndProfile

				Profile(Upload)

				pCtx->Unmap(mTexStaging, 0);

				pCtx->CopySubresourceRegion(pTexDst, 0, 0, 0, 0, mTexStaging, 0, NULL);
				//mInstance->mContext->CreateSurfaceFromDX11Native(mTexStaging, &surface, nullptr);
				EndProfile
			}

			pCtx->Release();
		}
	}
	else
		return ICERR_INTERNAL;

	//EndProfile

	Profile(EncoderSubmit)

	surface->SetPts(pts);
	res = mInstance->mEncoder->SubmitInput(surface);

	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile
	return ICERR_OK;
}


DWORD HostSubmitter::Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts)
{
	amf::AMFSurfacePtr surface;
	AMF_RESULT res;
	LONG w = mInstance->mWidth, h = mInstance->mHeight;
	res = mInstance->mContext->AllocSurface(amf::AMF_MEMORY_HOST, amf::AMF_SURFACE_NV12, w, h, &surface);
	if (res != AMF_OK)
		return ICERR_INTERNAL;

	void *dst = surface->GetPlaneAt(0)->GetNative();
	int32_t hpitch = surface->GetPlaneAt(0)->GetHPitch();
	int32_t vpitch = surface->GetPlaneAt(0)->GetVPitch();

	mBufferCopyManager.SetData(data, dst, inhdr->biBitCount, w, h, hpitch, vpitch);
	if (mBufferCopyManager.Wait() == WAIT_FAILED)
		return ICERR_INTERNAL;

	/*uint32_t hh = h / 2;
	if (inhdr->biBitCount == 24)
		ConvertRGB24toNV12_SSE2((const uint8_t*)data, (uint8_t*)dst, w, h, 0, hpitch, vpitch);
	else
	{
		ConvertRGB32toNV12_SSE2((const uint8_t*)data, (uint8_t*)dst, w, h, 0, hh, hpitch, vpitch);
		ConvertRGB32toNV12_SSE2((const uint8_t*)data, (uint8_t*)dst, w, h, hh, h, hpitch, vpitch);
	}*/

	mInstance->mEncoder->SubmitInput(surface);

	return ICERR_OK;
}

bool DX11ComputeSubmitter::Init()
{
	if (mSrcBufferView) //TODO check something more sane
		return true;
	if (!loadComputeShader())
		return false;

	auto pDev = mInstance->mDeviceDX11.GetDevice();
	pDev->GetImmediateContext(&mImmediateContext);

	int w = mInstance->mWidth, h = mInstance->mHeight;
	mInstance->mContext->AllocSurface(amf::AMF_MEMORY_DX11, amf::AMF_SURFACE_NV12, w, h, &mSurface);
	mTexStaging = (ID3D11Texture2D*) mSurface->GetPlaneAt(0)->GetNative();

	D3D11_BUFFER_DESC descBuf;
	ZeroMemory(&descBuf, sizeof(descBuf));
	descBuf.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//Align to shader thread count just in case
	descBuf.ByteWidth = ((w + 31) & ~31) * ((h + 15) & ~15) * 4;
	descBuf.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	descBuf.StructureByteStride = 4;	// Assume RGBA format
	descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBuf.Usage = D3D11_USAGE_DYNAMIC;

	HRESULT hr;
	if (FAILED((hr = pDev->CreateBuffer(&descBuf, nullptr, &mSrcBuffer))))
	{
		Dbg(L"Failed to create buffer\n");
		return false;
	}

	ConstBuffer in;
	in.inPitch = w;
	in.colorspace = mInstance->mConfigTable[S_COLORPROF];

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = &in;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	ZeroMemory(&descBuf, sizeof(descBuf));
	descBuf.ByteWidth = sizeof(ConstBuffer);
	descBuf.Usage = D3D11_USAGE_DYNAMIC;
	descBuf.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBuf.MiscFlags = 0;
	descBuf.StructureByteStride = 0;

	if (FAILED(pDev->CreateBuffer(&descBuf, &InitData, &mConstantBuf)))
		return false;

	ZeroMemory(&descBuf, sizeof(descBuf));
	mSrcBuffer->GetDesc(&descBuf);

	D3D11_SHADER_RESOURCE_VIEW_DESC descView;
	ZeroMemory(&descView, sizeof(descView));
	descView.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	descView.BufferEx.FirstElement = 0;

	descView.Format = DXGI_FORMAT_UNKNOWN;
	descView.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;

	if (FAILED(pDev->CreateShaderResourceView(mSrcBuffer, &descView, &mSrcBufferView)))
	{
		Dbg(L"Failed to create buffer view\n");
		return false;
	}

	/*
	D3D11_TEXTURE2D_DESC descTex;
	ZeroMemory(&descTex, sizeof(descTex));
	descTex.Width = w;
	descTex.Height = h;
	descTex.ArraySize = 1;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Format = DXGI_FORMAT_NV12;
	descTex.Usage = D3D11_USAGE_DEFAULT;
	descTex.MipLevels = 1;
	descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	
	//TODO maybe get from AMF by GetNative()
	if (FAILED(pDev->CreateTexture2D(&descTex, NULL, &mTexStaging)))
	{
		Dbg(L"Failed to create NV12 texture\n");
		return false;
	}*/

	// Shader has to access NV12 texture with 2 views: R8_UINT for luma, R8G8_UINT for chroma
	D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
	ZeroMemory(&descUAV, sizeof(descUAV));
	descUAV.Format = DXGI_FORMAT_R8_UINT;
	descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	descUAV.Texture2D.MipSlice = 0;

	if (FAILED(pDev->CreateUnorderedAccessView(mTexStaging, &descUAV, &mUavNV12[0])))
	{
		mInstance->LogMsg(true, L"Failed to create NV12 texture luma uav\n");
		return false;
	}

	descUAV.Format = DXGI_FORMAT_R8G8_UINT;

	if (FAILED(pDev->CreateUnorderedAccessView(mTexStaging, &descUAV, &mUavNV12[1])))
	{
		mInstance->LogMsg(true, L"Failed to create NV12 texture chroma uav\n");
		return false;
	}

	mBufferCopyManager.Start(3);

	return true;
}

bool DX11ComputeSubmitter::loadComputeShader()
{
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	auto pDev = mInstance->mDeviceDX11.GetDevice();
	LPCSTR pProfile = (pDev->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";

	ID3DBlob* pErrorBlob = NULL;
	ID3DBlob* pBlob = NULL;

	HRSRC hResource = FindResourceExA(hmoduleVFW, "STRING",
		MAKEINTRESOURCEA(IDR_DX11COMPUTE),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT));

	if (!hResource)
	{
		mInstance->LogMsg(true, L"Cannot load kernels source from resource.\n");
		return false;
	}

	const char* source = (const char*)LoadResource(hmoduleVFW, hResource);
	size_t srcSize = SizeofResource(hmoduleVFW, hResource);

	bool colSpace = mInstance->mConfigTable[S_COLORPROF] == AMF_VIDEO_CONVERTER_COLOR_PROFILE_601;
	D3D_SHADER_MACRO macros[] = { D3D_SHADER_MACRO { "BT601", colSpace ? "1" : "0" }, {} };

	HRESULT hr = D3DCompile(source, srcSize, nullptr, macros,
		nullptr, "CSMain", pProfile, dwShaderFlags, 0, &pBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		SafeRelease(&pErrorBlob);
		SafeRelease(&pBlob);
		mInstance->LogMsg(true, L"D3DCompile failed with hr: %x", hr);
		return false;
	}

	hr = pDev->CreateComputeShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &mComputeShader);
	SafeRelease(&pErrorBlob);
	SafeRelease(&pBlob);

	return hr == S_OK;
}

DWORD DX11ComputeSubmitter::Submit(void *data, BITMAPINFOHEADER *inhdr, amf_int64 pts)
{
	amf::AMFSurfacePtr surface;
	AMF_RESULT res;
	LONG w = mInstance->mWidth, h = mInstance->mHeight;

	ID3D11ShaderResourceView* ppSRVPrev = nullptr;
	ID3D11UnorderedAccessView* ppUAViewPrev[2] = { nullptr };

	Profile(Upload)
	D3D11_MAPPED_SUBRESOURCE map;
	if (FAILED(mImmediateContext->Map(mSrcBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map)))
		return ICERR_INTERNAL;

	//memcpy(map.pData, data, inhdr->biSizeImage);
	// YMMV if any speed-up
	mBufferCopyManager.SetData(data, map.pData, inhdr->biSizeImage);
	DWORD ret = mBufferCopyManager.Wait();

	mImmediateContext->Unmap(mSrcBuffer, 0);
	if (ret == WAIT_TIMEOUT)
		return ICERR_INTERNAL;

	EndProfile

	Profile(Compute)
	// Save state; nothing to save though so skip it for now
	//ID3D11Buffer *tmpBuf = nullptr;
	//ID3D11ComputeShader *tmpShader = nullptr;
	//std::vector<ID3D11ClassInstance*> tmpClass;
	//UINT numClass = 0;
	//mImmediateContext->CSGetShader(&tmpShader, nullptr, &numClass);
	//if (numClass > 0)
	//{
	//	tmpClass.resize(numClass);
	//	mImmediateContext->CSGetShader(&tmpShader, tmpClass.data(), &numClass);
	//	if (tmpShader) //ref incremented already, release first query
	//		tmpShader->Release();
	//}
	//mImmediateContext->CSGetConstantBuffers(0, 1, &tmpBuf);
	//mImmediateContext->CSGetUnorderedAccessViews(0, 2, ppUAViewPrev);
	//mImmediateContext->CSGetShaderResources(0, 1, &ppSRVPrev);

	mImmediateContext->CSSetShader(mComputeShader, NULL, 0);
	mImmediateContext->CSSetShaderResources(0, 1, &mSrcBufferView);
	mImmediateContext->CSSetUnorderedAccessViews(0, 2, mUavNV12, NULL);
	mImmediateContext->CSSetConstantBuffers(0, 1, &mConstantBuf);

	// shader samples 2x2 box so run over half the image size and divided by numthreads
	//TODO aligning over image bounds for non-multiple of 32x16 frames, shader didn't seem to mind
	int dx = ((w + 31) & ~31) / 32;
	int dy = ((h + 15) & ~15) / 16;
	mImmediateContext->Dispatch(dx, dy, 1);

	// Restore state
	/*mImmediateContext->CSSetShader(tmpShader, tmpClass.data(), tmpClass.size());
	mImmediateContext->CSSetShaderResources(0, 1, &ppSRVPrev);
	mImmediateContext->CSSetUnorderedAccessViews(0, 2, ppUAViewPrev, NULL);
	mImmediateContext->CSSetConstantBuffers(0, 1, &tmpBuf);

	if (tmpShader)
		tmpShader->Release();
	for (auto& c : tmpClass)
		c->Release();
	if (tmpBuf)
		tmpBuf->Release();

	SafeRelease(&ppSRVPrev);
	SafeRelease(&ppUAViewPrev[0]);
	SafeRelease(&ppUAViewPrev[1]);*/

	EndProfile

	Profile(Submit)
	res = mInstance->mEncoder->SubmitInput(mSurface);
	// AMF_INPUT_FULL should be impossible so error out if it happens anyway
	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile

	return ICERR_OK;
}