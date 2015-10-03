#include "stdafx.h"
#include "TestAMFVFW.h"
#include "DeviceOCL.h"

#define Log(...) LogMsg(false, __VA_ARGS__)
#define AMFLOGIFFAILED(r, ...) \
	do { if(r != AMF_OK) LogMsg(false, __VA_ARGS__); } while(0)
#define AMFLOGSET(component, var, val) \
	do { \
		if(!component) break;\
		AMF_RESULT r = component->SetProperty(var, val); \
		if(r != AMF_OK){ \
			LogMsg(false, L"Failed to set %s.", L#var); \
			Dbg(L"Failed to set %s.\n", L#var); \
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
				Dbg(TEXT("%s = <empty>\n"), name);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_BOOL:
				Log(TEXT("%s = <bool>%d"), name, var.boolValue);
				Dbg(TEXT("%s = <bool>%d\n"), name, var.boolValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_INT64:
				Log(TEXT("%s = %lld"), name, var.int64Value);
				Dbg(TEXT("%s = %lld\n"), name, var.int64Value);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_STRING:
				Log(TEXT("%s = <str>%s"), name, var.stringValue);
				Dbg(TEXT("%s = <str>%s\n"), name, var.stringValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_WSTRING:
				Log(TEXT("%s = <wstr>%s"), name, var.wstringValue);
				Dbg(TEXT("%s = <wstr>%s\n"), name, var.wstringValue);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_SIZE:
				Log(TEXT("%s = <size>%dx%d"), name, var.sizeValue.width, var.sizeValue.height);
				Dbg(TEXT("%s = <size>%dx%d\n"), name, var.sizeValue.width, var.sizeValue.height);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_RATE:
				Log(TEXT("%s = <rate>%d/%d"), name, var.rateValue.num, var.rateValue.den);
				Dbg(TEXT("%s = <rate>%d/%d\n"), name, var.rateValue.num, var.rateValue.den);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_RATIO:
				Log(TEXT("%s = <ratio>%d/%d"), name, var.ratioValue.num, var.ratioValue.den);
				Dbg(TEXT("%s = <ratio>%d/%d\n"), name, var.ratioValue.num, var.ratioValue.den);
				break;
			case amf::AMF_VARIANT_TYPE::AMF_VARIANT_INTERFACE:
				Log(TEXT("%s = <interface>"), name);
				Dbg(TEXT("%s = <interface>\n"), name);
				break;
			default:
				Log(TEXT("%s = <type %d>"), name, var.type);
				Dbg(TEXT("%s = <type %d>\n"), name, var.type);
			}
		}
		else
		{
			Log(TEXT("Failed to get property at index %d"), i);
			Dbg(TEXT("Failed to get property at index %d\n"), i);
		}
	}
}

// check if the codec can compress the given format to the desired format
DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut){

	if (mLog) mLog->enableLog(!!mConfigTable[S_LOG]);
	Log(L"Compression query: %d %x %dx%d", lpbiIn->biBitCount, lpbiIn->biCompression, lpbiIn->biWidth, lpbiIn->biHeight);

	// check for valid format and bitdepth
	if (lpbiIn->biCompression == 0){
		if (/*lpbiIn->biBitCount != 24 &&*/ lpbiIn->biBitCount != 32)
			return (DWORD)ICERR_BADFORMAT;
	}
	/*else if ( lpbiIn->biCompression == FOURCC_YUY2 || lpbiIn->biCompression == FOURCC_UYVY || lpbiIn->biCompression == FOURCC_YV16 ){
	if ( lpbiIn->biBitCount != 16 ) {
	return_badformat()
	}
	}*/
	else if (lpbiIn->biCompression == FOURCC_YV12 || lpbiIn->biCompression == FOURCC_NV12){
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
	if (mWidth % 2 || mHeight % 2)// || mWidth > 1920 || mHeight > 1088) //TODO get max w/h from caps
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
	amf::H264EncoderCapsPtr encCaps;

	if (mLog) mLog->enableLog(!!mConfigTable[S_LOG]);

	if (started == 0x1337){
		CompressEnd();
	}
	started = 0;

	if (int error = CompressQuery(lpbiIn, lpbiOut) != ICERR_OK){
		return error;
	}

	started = 0x1337;
	mWidth = lpbiIn->biWidth;
	mHeight = lpbiIn->biHeight;

	//TODO Check math. In 100-nanoseconds
	if (fps_num && fps_den)
		mFrameDuration = ((amf_pts)10000000000 / ((amf_pts)fps_num * 1000 / fps_den));
	mIDRPeriod = mConfigTable[S_IDR];

	mFmtIn = amf::AMF_SURFACE_BGRA;
	switch (lpbiIn->biCompression)
	{
	case FOURCC_NV12: mFmtIn = amf::AMF_SURFACE_NV12; break;
	case FOURCC_YV12: mFmtIn = amf::AMF_SURFACE_YV12; break;
	case 0:
		if (lpbiIn->biBitCount == 32)
			mFmtIn = amf::AMF_SURFACE_BGRA;
		else
		{
			LogMsg(true, L"24 bpp is not supported.");
			goto fail;
		}
		break;
	default:
		LogMsg(true, L"Input format is not supported: %04X", lpbiIn->biCompression);
		goto fail;
	}

	if (!FindDLLs())
		goto fail;

	if (!mDeviceDX11.Create(mConfigTable[S_DEVICEIDX], false))
		goto fail;

	res = AMFCreateContext(&mContext);
	if (res != AMF_OK)
		goto fail;

	res = mContext->InitDX11(mDeviceDX11.GetDevice(), amf::AMF_DX11_0);
	if (res != AMF_OK)
		goto fail;

	mDeviceCL.Init(mDeviceDX11.GetDevice(), mWidth, mHeight,
		mConfigTable[S_COLORPROF] == AMF_VIDEO_CONVERTER_COLOR_PROFILE_601 ? BT601_FULL : BT709_FULL);

	if ((mFmtIn != amf::AMF_SURFACE_BGRA) || !mDeviceCL.InitBGRAKernels(lpbiIn->biBitCount))
	{
		mCLConv = false;
		//goto fail;
	}

	// Some speed up
	res = mContext->InitOpenCL(/*NULL*/ mDeviceCL.GetCmdQueue());
	if (res != AMF_OK)
		goto fail;

	res = AMFCreateComponent(mContext, AMFVideoConverter, &mConverter);
	if (res != AMF_OK)
		goto fail;

	AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_MEMORY_TYPE, amf::AMF_MEMORY_OPENCL);
	AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, amf::AMF_SURFACE_NV12);
	AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_COLOR_PROFILE, mConfigTable[S_COLORPROF]); //AMF_VIDEO_CONVERTER_COLOR_PROFILE_709
	AMFLOGSET(mConverter, AMF_VIDEO_CONVERTER_OUTPUT_SIZE, ::AMFConstructSize(mWidth, mHeight));

	res = mConverter->Init(mFmtIn , mWidth, mHeight);
	if (res != AMF_OK)
		goto fail;

	res = AMFCreateComponent(mContext, AMFVideoEncoderVCE_AVC, &mEncoder);
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

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_GOP_SIZE, mConfigTable[S_GOP]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_IDR_PERIOD, mConfigTable[S_IDR]);

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, mConfigTable[S_RCM]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_TARGET_BITRATE, mConfigTable[S_BITRATE] * 1000);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_PEAK_BITRATE, mConfigTable[S_BITRATE] * 1000);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(mWidth, mHeight));
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(fps_num, fps_den));

	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QP_I, mConfigTable[S_QPI]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QP_P, mConfigTable[S_QPI]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_QP_B, mConfigTable[S_QPI]);

	AMFLOGSET(mEncoder, L"CABACEnable", !!mConfigTable[S_CABAC]);
	AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);

	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_B_PIC_DELTA_QP, qpBDelta);
	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_REF_B_PIC_DELTA_QP, qpBDelta);

	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_MIN_QP, 18);
	//AMFLOGSET(mEncoder, AMF_VIDEO_ENCODER_MAX_QP, 51);

	//amf_increase_timer_precision();

	if (mEncoder->QueryInterface(amf::AMFH264EncoderCaps::IID(), (void**)&encCaps) == AMF_OK)
	{
		TCHAR* accelType[] = {
			TEXT("NOT_SUPPORTED"),
			TEXT("HARDWARE"),
			TEXT("GPU"),
			TEXT("SOFTWARE")
		};
		Log(TEXT("Capabilities:"));
		Log(TEXT("  Accel type: %s"), accelType[(encCaps->GetAccelerationType() + 1) % 4]);
		Log(TEXT("  Max bitrate: %d"), encCaps->GetMaxBitrate());
		//Log(TEXT("  Max priority: %d"), encCaps->GetMaxSupportedJobPriority());

		std::stringstream str;
		str << "  Levels: ";
		for (int i = 0; i < encCaps->GetNumOfSupportedLevels(); i++)
		{
			amf_uint32 level = encCaps->GetLevel(i);
			str << level << " ";
		}

		str << "\r\n  Profiles: ";
		for (int i = 0; i < encCaps->GetNumOfSupportedProfiles(); i++)
		{
			str << encCaps->GetProfile(i) << " ";
		}
		Log(L"%S", str.str().c_str());

		amf::AMFIOCapsPtr iocaps;
		encCaps->GetInputCaps(&iocaps);
		Log(TEXT("  Input mem types:"));
		for (int i = 0; i < iocaps->GetNumOfMemoryTypes(); i++)
		{
			bool native;
			amf::AMF_MEMORY_TYPE memType;
			iocaps->GetMemoryTypeAt(i, &memType, &native);
			Log(TEXT("    %s, native: %s"), amf::AMFGetMemoryTypeName(memType),
				native ? TEXT("Yes") : TEXT("No"));
			if (native && (mNativeMemType == amf::AMF_MEMORY_UNKNOWN))
				mNativeMemType = memType;
		}

		amf_int32 imin, imax;
		iocaps->GetWidthRange(&imin, &imax);
		Log(TEXT("  Width min/max: %d/%d"), imin, imax);
		iocaps->GetHeightRange(&imin, &imax);
		Log(TEXT("  Height min/max: %d/%d"), imin, imax);
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
		LogMsg(true, L"AMF failed with error: %s", amf::AMFGetResultText(res));
		Dbg(L"AMF failed with error: %s\n", amf::AMFGetResultText(res));
	}
	CompressEnd();
	return ICERR_INTERNAL;
}

DWORD CodecInst::CompressEnd(){
	Log(L"CompressEnd");
	Dbg(L"CompressEnd\n");
	started = 0;
	AMF_RESULT res = AMF_REPEAT;

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
	std::chrono::time_point<std::chrono::system_clock> startP, endP;
	startP = std::chrono::system_clock::now();
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

	outhdr->biCompression = FOURCC_H264;

	//mFrameNum = icinfo->lFrameNum;
	if (icinfo->lFrameNum == 0){
		if (started != 0x1337){
			if (int error = CompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput) != ICERR_OK)
				return error;
		}
	}

	if (icinfo->lpckid){
		*icinfo->lpckid = 'cd'; // 'dc' Compressed video frame
	}

	Profile(AllocSurface)

	if (mFmtIn == amf::AMF_SURFACE_BGRA)
	{
		if (mCLConv)
		{
			if (!mDeviceCL.ConvertBuffer(in, inhdr->biSizeImage))
				return ICERR_INTERNAL;

			void* imgs[2];
			mDeviceCL.GetYUVImages(imgs);
			res = mContext->CreateSurfaceFromOpenCLNative(amf::AMF_SURFACE_NV12,
				mWidth, mHeight, imgs, &surfSrc, nullptr);
			if (res != AMF_OK)
				return ICERR_INTERNAL;
		}
		else
		{
			res = mContext->AllocSurface(amf::AMF_MEMORY_HOST, mFmtIn, mWidth, mHeight, &surfSrc);
			if (res != AMF_OK)
				return ICERR_INTERNAL;

			amf::AMFPlanePtr plane = surfSrc->GetPlaneAt(0);
			int pitch = plane->GetHPitch();
			auto ptr = (uint8_t*)plane->GetNative();
			int inPitch = inhdr->biBitCount / 8 * mWidth;

			// Flip it
			for (int h = mHeight - 1; h >= 0; h--, ptr += pitch)
			{
				memcpy(ptr, (uint8_t*)in + h * inPitch, inPitch);
			}
		}

	}
	else
	{
		res = mContext->CreateSurfaceFromHostNative(mFmtIn, mWidth, mHeight,
			mWidth, mHeight, in, &surfSrc, nullptr);
		if (res != AMF_OK)
			return ICERR_INTERNAL;
	}

	// In 100-nanoseconds
	surfSrc->SetPts(mFrameDuration * icinfo->lFrameNum);

	EndProfile

	Profile(Conversion)

	if (!mCLConv && mFmtIn != amf::AMF_SURFACE_NV12)
	{
		res = mConverter->SubmitInput(surfSrc);
		if (res != AMF_OK)
			return ICERR_INTERNAL;

		do
		{
			res = mConverter->QueryOutput(&convData);
			if (res != AMF_REPEAT) //OK or some error maybe
				break;
			Sleep(1); //Sleep period can be too random though
		} while (true);

		if (convData == NULL)
			return ICERR_INTERNAL;
	}

	EndProfile

	Profile(EncoderSubmit)

	if (!mCLConv && mFmtIn != amf::AMF_SURFACE_NV12)
		res = mEncoder->SubmitInput(convData);
	else
		res = mEncoder->SubmitInput(surfSrc);

	if (res != AMF_OK)
		return ICERR_INTERNAL;

	EndProfile

	Profile(EncoderPoll)

	do
	{
		res = mEncoder->QueryOutput(&frameData);
		if (res != AMF_REPEAT) //OK or some error maybe
			break;
		Sleep(1);
	} while (true);

	if (frameData == NULL)
		return ICERR_INTERNAL;
	//if (res == AMF_EOF)
	//	return ICERR_OK;

	amf::AMFBufferPtr buffer(frameData);
	//res = buffer->Convert(amf::AMF_MEMORY_HOST); //Maybe?
	memcpy(out, buffer->GetNative(), buffer->GetSize());
	outhdr->biSizeImage = buffer->GetSize();
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
	endP = std::chrono::system_clock::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endP - startP).count();
	Dbg(L"Encoding: %fms\n", (duration / 1.0E6));
#endif

	return ICERR_OK;
}