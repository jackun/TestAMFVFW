#include "stdafx.h"
#include "TestAMFVFW.h"
#include "DeviceOCL.h"
#include <VersionHelpers.h>

using hrc = std::chrono::high_resolution_clock;
void ConvertRGB24toNV12_SSE2(const uint8_t *src, uint8_t *ydest, /*uint8_t *udest, uint8_t *vdest, */unsigned int w, unsigned int h, unsigned int hpitch, unsigned int vpitch);
void ConvertRGB32toNV12_SSE2(const uint8_t *src, uint8_t *ydest, /*uint8_t *udest, uint8_t *vdest, */unsigned int w, unsigned int h, unsigned int hpitch, unsigned int vpitch);
void BGRtoNV12(const uint8_t * src, uint8_t * yuv, unsigned bytesPerPixel, uint8_t flip, int srcFrameWidth, int srcFrameHeight, uint32_t yuvPitch);

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
	amf::H264EncoderCapsPtr encCaps;
	bool usingAMFConv = false;

	if (started == 0x1337){
		LogMsg(false, L"CompressBegin: already began compressing, exiting...");
		CompressEnd();
	}
	started = 0;

	if (int error = CompressQuery(lpbiIn, lpbiOut) != ICERR_OK){
		return error;
	}

	started = 0x1337;
	mWidth = lpbiIn->biWidth;
	mHeight = lpbiIn->biHeight;
	mIDRPeriod = mConfigTable[S_IDR];
	mCLConv = !mConfigTable[S_DISABLE_OCL];

	//TODO Check math. In 100-nanoseconds
	if (fps_num && fps_den)
		mFrameDuration = ((amf_pts)10000000000 / ((amf_pts)fps_num * 1000 / fps_den));

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

	if (!FindDLLs())
		goto fail;

	if (!mDeviceDX11.Create(mConfigTable[S_DEVICEIDX], false))
		goto fail;

	if (!mDeviceCL.Init(mDeviceDX11.GetDevice(), mWidth, mHeight,
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
			mSubmitter = new OpenCLSubmitter(this);
		else
		{
			if (mConfigTable[S_CONVTYPE] == 3)
			{
				if (IsWindows8OrGreater())
					mSubmitter = new DX11Submitter(this);
				else
					mSubmitter = new HostSubmitter(this);
			}
			else if (lpbiIn->biBitCount == 32)
			{
				if (mConfigTable[S_CONVTYPE] == 0 && IsWindows8OrGreater())
					mSubmitter = new DX11ComputeSubmitter(this);
				else
				{
					//mSubmitter = new DX11Submitter(this);
					mSubmitter = new AMFConverterSubmitter(this);
					usingAMFConv = true;
				}
			}
			else
				mSubmitter = new HostSubmitter(this); // slow CPU conversion
		}
	}
	break;
	case amf::AMF_SURFACE_NV12:
		mSubmitter = new NV12Submitter(this);
		break;
	default:
		mSubmitter = new AMFConverterSubmitter(this);
		usingAMFConv = true;
		break;
	}

	res = AMFCreateContext(&mContext);
	if (res != AMF_OK)
		goto fail;

	res = mContext->InitDX11(mDeviceDX11.GetDevice(), amf::AMF_DX11_0);
	if (res != AMF_OK)
		goto fail;

	// Some speed up
	if (mCLConv)
	{
		res = mContext->InitOpenCL(mDeviceCL.GetCmdQueue());
		if (res != AMF_OK)
			goto fail;
	}

	if (usingAMFConv)
	{
		res = AMFCreateComponent(mContext, AMFVideoConverter, &mConverter);
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
	mCLConv = !mConfigTable[S_DISABLE_OCL]; //reset as class instance is not always dtor'ed

	delete mSubmitter;
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

	DWORD icerr = mSubmitter->Submit(in, inhdr, mFrameDuration * icinfo->lFrameNum);
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
				if (inhdr->biBitCount == 24)
					ConvertRGB24toNV12_SSE2((const uint8_t*)data, (uint8_t*)lockedRect.pData, w, h, lockedRect.RowPitch, h);
				else
					ConvertRGB32toNV12_SSE2((const uint8_t*)data, (uint8_t*)lockedRect.pData, w, h, lockedRect.RowPitch, h);

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

	if (inhdr->biBitCount == 24)
		ConvertRGB24toNV12_SSE2((const uint8_t*)data, (uint8_t*)dst, w, h, hpitch, vpitch);
	else
		ConvertRGB32toNV12_SSE2((const uint8_t*)data, (uint8_t*)dst, w, h, hpitch, vpitch);

	mInstance->mEncoder->SubmitInput(surface);
	return ICERR_OK;
}

struct InputBuffer
{
	int inPitch;
	int colorspace;
	//IDK, 16byte align
	int pad1;
	int pad2;
};

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

	InputBuffer in;
	in.inPitch = w;
	in.colorspace = mInstance->mConfigTable[S_COLORPROF];

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = &in;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	ZeroMemory(&descBuf, sizeof(descBuf));
	descBuf.ByteWidth = sizeof(InputBuffer);
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
		Dbg(L"Failed to create NV12 texture luma uav\n");
		return false;
	}

	descUAV.Format = DXGI_FORMAT_R8G8_UINT;

	if (FAILED(pDev->CreateUnorderedAccessView(mTexStaging, &descUAV, &mUavNV12[1])))
	{
		Dbg(L"Failed to create NV12 texture chroma uav\n");
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
		Dbg(L"Cannot load kernels source from resource.\n");
		return false;
	}

	const char* source = (const char*)LoadResource(hmoduleVFW, hResource);
	size_t srcSize = SizeofResource(hmoduleVFW, hResource);

	HRESULT hr = D3DCompile(source, srcSize, nullptr, nullptr, nullptr, "CSMain", pProfile, dwShaderFlags, 0, &pBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		SafeRelease(&pErrorBlob);
		SafeRelease(&pBlob);

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
void ConvertRGB24toNV12_SSE2(const uint8_t *src, uint8_t *ydest, /*uint8_t *udest, uint8_t *vdest, */unsigned int w, unsigned int h, unsigned int hpitch, unsigned int vpitch) {
	const __m128i fraction = _mm_setr_epi32(0x84000, 0x84000, 0x84000, 0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32 = _mm_setr_epi32(-32, -32, -32, -32);
	const __m128i y1y2_mult = _mm_setr_epi32(0x4A85, 0x4A85, 0x4A85, 0x4A85);
	const __m128i fpix_add = _mm_setr_epi32(0x808000, 0x808000, 0x808000, 0x808000);
	const __m128i fpix_mul = _mm_setr_epi32(0x1fb, 0x282, 0x1fb, 0x282);

	// 0x0c88 == BtoYCoeff / 2, 0x4087 == GtoYCoeff / 2, 0x20DE == RtoYCoeff / 2
	const __m128i cybgr_64 = _mm_setr_epi16(0, 0x0c88, 0x4087, 0x20DE, 0x0c88, 0x4087, 0x20DE, 0);

	for (unsigned int y = 0; y<h; y += 2) {
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
			*(int *)&ydst[x - hpitch] = _mm_cvtsi128_si32(luma2);


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

void ConvertRGB32toNV12_SSE2(const unsigned char *src, unsigned char *ydest, unsigned int w, unsigned int h, unsigned int hpitch, unsigned int vpitch) {
	const __m128i fraction = _mm_setr_epi32(0x84000, 0x84000, 0x84000, 0x84000);    //= 0x108000/2 = 0x84000
	const __m128i neg32 = _mm_setr_epi32(-32, -32, -32, -32);
	const __m128i y1y2_mult = _mm_setr_epi32(0x4A85, 0x4A85, 0x4A85, 0x4A85);
	const __m128i fpix_add = _mm_setr_epi32(0x808000, 0x808000, 0x808000, 0x808000);
	const __m128i fpix_mul = _mm_setr_epi32(0x1fb, 0x282, 0x1fb, 0x282);
	const __m128i cybgr_64 = _mm_setr_epi16(0x0c88, 0x4087, 0x20DE, 0, 0x0c88, 0x4087, 0x20DE, 0);

	for (unsigned int y = 0; y<h; y += 2) {
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
			*(int *)&ydst[x - hpitch] = _mm_cvtsi128_si32(luma2);

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