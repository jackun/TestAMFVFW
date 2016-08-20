/******************************************************************************
Copyright (C) 2015 by jackun <jack.un@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "stdafx.h"
#include "TestAMFVFW.h"

#pragma comment(lib, "WinMM")
#pragma comment(lib, "d3dcompiler.lib")


CRITICAL_SECTION lockCS;
HMODULE hmoduleVFW = 0;

amf_pts AMF_STD_CALL amf_high_precision_clock()
{
	static int state = 0;
	static LARGE_INTEGER Frequency;
	if (state == 0)
	{
		if (QueryPerformanceFrequency(&Frequency))
		{
			state = 1;
		}
		else
		{
			state = 2;
		}
	}
	if (state == 1)
	{
		LARGE_INTEGER PerformanceCount;
		if (QueryPerformanceCounter(&PerformanceCount))
		{
			return static_cast<amf_pts>(PerformanceCount.QuadPart * 10000000.0 / Frequency.QuadPart);
		}
	}
	return GetTickCount() * 10;
}

void AMF_STD_CALL amf_increase_timer_precision()
{
	typedef LONG NTSTATUS;
	typedef NTSTATUS(CALLBACK * NTSETTIMERRESOLUTION)(IN ULONG DesiredTime, IN BOOLEAN SetResolution, OUT PULONG ActualTime);
	typedef NTSTATUS(CALLBACK * NTQUERYTIMERRESOLUTION)(OUT PULONG MaximumTime, OUT PULONG MinimumTime, OUT PULONG CurrentTime);

	HINSTANCE hNtDll = LoadLibrary(L"NTDLL.dll");
	if (hNtDll != NULL)
	{
		ULONG MinimumResolution = 0;
		ULONG MaximumResolution = 0;
		ULONG ActualResolution = 0;

		NTQUERYTIMERRESOLUTION NtQueryTimerResolution = (NTQUERYTIMERRESOLUTION)GetProcAddress(hNtDll, "NtQueryTimerResolution");
		NTSETTIMERRESOLUTION NtSetTimerResolution = (NTSETTIMERRESOLUTION)GetProcAddress(hNtDll, "NtSetTimerResolution");

		if (NtQueryTimerResolution != NULL && NtSetTimerResolution != NULL)
		{
			NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &ActualResolution);
			if (MaximumResolution != 0)
			{
				NtSetTimerResolution(MaximumResolution, TRUE, &ActualResolution);
				NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &ActualResolution);

				// if call NtQueryTimerResolution() again it will return the same values but precision is actually increased
			}
		}
		FreeLibrary(hNtDll);
	}
}

CodecInst::CodecInst()
	: mLog(nullptr)
	, fps_num(30)
	, fps_den(1)
	, mCLConv(true)
	, mSubmitter(nullptr)
{
	InitSettings();
	ReadRegistry();
	mLog = new Logger(!!mConfigTable[S_LOG]);
	mCLConv = !mConfigTable[S_DISABLE_OCL];
	if (mConfigTable[S_FPS_ENABLED])
	{
		fps_num = mConfigTable[S_FPS_NUM];
		fps_den = mConfigTable[S_FPS_DEN];
	}
#if _DEBUG
	BindDLLs();
#endif
}

CodecInst::~CodecInst(){
	try {
		if (started == 0x1337){
			CompressEnd();
		}
		started = 0;
	}
	catch (...) {};

	if (hModRuntime)
	{
		FreeLibrary(hModRuntime);
		hModRuntime = nullptr;
		AMFInit = nullptr;
		AMFQueryVersion = nullptr;
	}
}

bool CodecInst::BindDLLs()
{
	if (hModRuntime && AMFInit)
		return true;

	hModRuntime = LoadLibraryW(AMF_DLL_NAME);
	if (hModRuntime)
	{
		AMFInit = (AMFInit_Fn)GetProcAddress(hModRuntime, AMF_INIT_FUNCTION_NAME);
		AMFQueryVersion = (AMFQueryVersion_Fn)GetProcAddress(hModRuntime, AMF_QUERY_VERSION_FUNCTION_NAME);
	}
	else
		LogMsg(true, L"Failed to load AMF runtime DLL (%s)!", AMF_DLL_NAME);

	return !!hModRuntime && !!AMFInit;
}

// some programs assume that the codec is not configurable if GetState
// and SetState are not supported.
DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize){
	Dbg(L"GetState\n");
	if (pv == NULL){
		return sizeof(VfwState);
	}
	else if (dwSize < sizeof(VfwState)){
		return ICERR_BADSIZE;
	}

	VfwState state;
	SaveState(state);
	memcpy(pv, &state, sizeof(VfwState));
	return ICERR_OK;
}

// See GetState comment
DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) {
	Dbg(L"SetState\n");
	if (pv && dwSize == sizeof(VfwState)){

		VfwState state;
		memcpy(&state, pv, sizeof(VfwState));
		LoadState(state);

		return sizeof(VfwState);
	}
	else {
		return 0;
	}
}

// return information about the codec
DWORD CodecInst::GetInfo(ICINFO* icinfo, DWORD dwSize) {
	if (icinfo == NULL)
		return sizeof(ICINFO);

	if (dwSize < sizeof(ICINFO))
		return 0;

	icinfo->dwSize = sizeof(ICINFO);
	icinfo->fccType = ICTYPE_VIDEO;
	icinfo->fccHandler = FOURCC_H264;
	icinfo->dwFlags = VIDCF_FASTTEMPORALC | VIDCF_FASTTEMPORALD | VIDCF_COMPRESSFRAMES;
	icinfo->dwVersion = 0x00010000;
	icinfo->dwVersionICM = ICVERSION;

#ifdef _M_X64
	wcscpy_s(icinfo->szName, L"TestAMFVFW64");
	wcscpy_s(icinfo->szDescription, L"TestAMFVFW64 Encoder"
#if _DEBUG
		L" (Debug)"
#endif
		);
#else
	wcscpy_s(icinfo->szName, L"TestAMFVFW");
	//Well, no decoder part
	wcscpy_s(icinfo->szDescription, L"TestAMFVFW Encoder"
#if _DEBUG
		L" (Debug)"
#endif
		);
#endif

	return sizeof(ICINFO);
}

void CodecInst::LogMsg(bool msgBox, const wchar_t *psz_fmt, ...)
{
	va_list arg;
	va_start(arg, psz_fmt);
	Dbg_va(psz_fmt, arg);
	Dbg(L"%s", L"\n");

	if (mLog)
	{
		mLog->Log_internal(psz_fmt, arg);
	}

	if (msgBox)
	{
		int bufsize = _vscwprintf(psz_fmt, arg) + 1;
		std::vector<wchar_t> msg(bufsize);
		_vsnwprintf_s(&msg[0], bufsize, bufsize - 1, psz_fmt, arg);
		MessageBox(NULL, &msg[0], L"Warning", 0);
	}
	va_end(arg);
}

// ---------------------

extern "C" void WINAPI Configure(HWND hwnd, HINSTANCE hinst, LPTSTR lpCmdLine, int nCmdShow)
{
	CodecInst codec;
	codec.Configure(GetDesktopWindow());
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD r, LPVOID) {
	hmoduleVFW = (HMODULE)hinstDLL;

	switch (r)
	{
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&lockCS);
		break;

	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&lockCS);
		break;
	}
	return TRUE;
}

CodecInst* Open(ICOPEN* icinfo) {
	if ((icinfo && icinfo->fccType != ICTYPE_VIDEO))
		return DRVCNF_CANCEL;

	CodecInst* pinst = NULL;

	if (icinfo)
	{
		try {
			pinst = new CodecInst();
		}
		catch (...) {

		}
		icinfo->dwError = pinst ? ICERR_OK : ICERR_MEMORY;
	}
	//TODO Sometimes DRV_OPEN is called with lParam2 == 0, just return 1 then
	return pinst ? pinst : (CodecInst*)1;
}

DWORD Close(CodecInst* pinst) {
	try {
		if (pinst) {
			delete pinst;
		}
	}
	catch (...){};
	return 1;
}