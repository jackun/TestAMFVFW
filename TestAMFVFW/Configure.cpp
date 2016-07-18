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
#include <CommCtrl.h>

//FIXME Proper maximum, 100 Mbps with 4.1+ level, probably
#define MAX_BITRATE 50000 //kbit/s
#define MAX_QUANT 51 //0...51 for I/P/B

static bool firstInit = true;
static HWND hwndToolTip = NULL;
static TOOLINFO ti;

typedef struct SpinnerInts
{
	char *param;
	int editCtrl;
	int spinCtrl;
	int def;
	int min;
	int max;
} SpinnerInts;

#define SETTOOLTIP(ctrl,str)\
	do{\
		ti.lpszText = (PTCHAR)TEXT(str); \
		ti.uId = (UINT_PTR)GetDlgItem(hwndDlg, ctrl);\
		SendMessageW(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);\
	}while(0)

//x264vfw
static void CheckControlTextIsNumber(HWND hDlgItem, int bSigned, int iDecimalPlacesNum)
{
	wchar_t text_old[MAX_PATH];
	wchar_t text_new[MAX_PATH];
	wchar_t *src, *dest;
	DWORD start, end, num, pos;
	int bChanged = FALSE;
	int bCopy = FALSE;
	int q = !bSigned;

	SendMessage(hDlgItem, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
	num = SendMessage(hDlgItem, WM_GETTEXT, MAX_PATH, (LPARAM)text_old);
	src = text_old;
	dest = text_new;
	pos = 0;
	while (num > 0)
	{
		bCopy = TRUE;
		if (q == 0 && *src == '-')
		{
			q = 1;
		}
		else if ((q == 0 || q == 1) && *src >= '0' && *src <= '9')
		{
			q = 2;
		}
		else if (q == 2 && *src >= '0' && *src <= '9')
		{
		}
		else if (q == 2 && iDecimalPlacesNum > 0 && *src == '.')
		{
			q = 3;
		}
		else if (q == 3 && iDecimalPlacesNum > 0 && *src >= '0' && *src <= '9')
		{
			iDecimalPlacesNum--;
		}
		else
			bCopy = FALSE;
		if (bCopy)
		{
			*dest = *src;
			dest++;
			pos++;
		}
		else
		{
			bChanged = TRUE;
			if (pos < start)
				start--;
			if (pos < end)
				end--;
		}
		src++;
		num--;
	}
	*dest = 0;
	if (bChanged)
	{
		SendMessage(hDlgItem, WM_SETTEXT, 0, (LPARAM)text_new);
		SendMessage(hDlgItem, EM_SETSEL, start, end);
	}
}

#define CHECKED_SET_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
do {\
	CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
	var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
	if (var < min)\
		{\
		var = min;\
		SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
		SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
		}\
		else if (var > max)\
	{\
		var = max;\
		SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
		SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
	}\
} while (0)

#define CHECKED_SET_MIN_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
do {\
	CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
	var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
	if (var < min)\
		{\
		var = min;\
		SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
		SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
		}\
		else if (var > max)\
		var = max;\
} while (0)

#define CHECKED_SET_MAX_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
do {\
	CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
	var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
	if (var < min)\
		var = min;\
		else if (var > max)\
	{\
		var = max;\
		SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
		SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
	}\
} while (0)

#define CHECKED_SET_SHOW_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
do {\
	CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
	var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
	if (var < min)\
		var = min;\
		else if (var > max)\
		var = max;\
	SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
} while (0)

#define LevelToIdx(l)\
do {\
	switch (l)\
	{\
		case 30: l = 0; break;\
		case 31: l = 1; break;\
		case 32: l = 2; break;\
		case 40: l = 3; break;\
		case 41: l = 4; break;\
		case 42: l = 5; break;\
		case 50: l = 6; break;\
		case 51: l = 7; break;\
		default:\
		  l = 4;\
		  break;\
	}\
} while (0)

#define IdxToLevel(l)\
do {\
	switch (l)\
	{\
		case 0: l = 30; break;\
		case 1: l = 31; break;\
		case 2: l = 32; break;\
		case 3: l = 40; break;\
		case 4: l = 41; break;\
		case 5: l = 42; break;\
		case 6: l = 50; break;\
		case 7: l = 51; break;\
		default:\
		  l = 41;\
		  break;\
	}\
} while (0)

void CodecInst::InitSettings()
{
	mConfigTable[S_LOG] = 0;
	mConfigTable[S_CABAC] = 1;
	mConfigTable[S_RCM] = RCM_CBR;
	mConfigTable[S_PRESET] = AMF_VIDEO_ENCODER_QUALITY_PRESET_BALANCED;
	mConfigTable[S_PROFILE] = AMF_VIDEO_ENCODER_PROFILE_HIGH;
	mConfigTable[S_LEVEL] = 51;
	mConfigTable[S_IDR] = 120;
	mConfigTable[S_GOP] = 20;
	mConfigTable[S_BITRATE] = 15000;
	mConfigTable[S_QPI] = 20;
	mConfigTable[S_QP_MIN] = 18;
	mConfigTable[S_QP_MAX] = 51;
	mConfigTable[S_COLORPROF] = AMF_VIDEO_CONVERTER_COLOR_PROFILE_709;
	mConfigTable[S_DEVICEIDX] = 0;
	mConfigTable[S_FPS_NUM] = 30;
	mConfigTable[S_FPS_DEN] = 1;
	mConfigTable[S_FPS_ENABLED] = 0;
	mConfigTable[S_DISABLE_OCL] = 0;
}

bool CodecInst::ReadRegistry()
{
	HKEY    hKey;
	DWORD   i_size;
	int32_t   i;

	if (RegOpenKeyEx(OVE_REG_KEY, OVE_REG_PARENT TEXT("\\") OVE_REG_CHILD, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		return false;
	}

	EnterCriticalSection(&lockCS);

	for (auto &it : mConfigTable)
	{
		i_size = sizeof(int32_t);
		if (RegQueryValueExA(hKey, it.first.c_str(), 0, 0, (LPBYTE)&i, &i_size) == ERROR_SUCCESS)
			it.second = i;
	}

	LeaveCriticalSection(&lockCS);
	RegCloseKey(hKey);

	return true;
}

bool CodecInst::SaveRegistry()
{
	HKEY    hKey;
	DWORD   dwDisposition;
	LSTATUS s;

	if (RegCreateKeyEx(OVE_REG_KEY, OVE_REG_PARENT TEXT("\\") OVE_REG_CHILD, 0, OVE_REG_CLASS, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &hKey, &dwDisposition) != ERROR_SUCCESS)
		return false;

	EnterCriticalSection(&lockCS);

	/* Save all integers */
	for (auto &it : mConfigTable)
		s = RegSetValueExA(hKey, it.first.c_str(), 0, REG_DWORD, (LPBYTE)&(it.second), sizeof(int32_t));

	//RegSetValueEx(hKey, TEXT("StringKey"), 0, REG_SZ, (LPBYTE)TEXT("Value"), wcslen(TEXT("Value") + 1);
	LeaveCriticalSection(&lockCS);
	RegCloseKey(hKey);
	return true;
}

static void DialogUpdate(HWND hwndDlg, CodecInst* pinst) {
	wchar_t temp[1024];

	if (firstInit)
	{
		firstInit = false;
		hwndToolTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hwndDlg, NULL, hmoduleVFW, NULL);


		ZeroMemory(&ti, sizeof(ti));
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
		ti.hwnd = hwndDlg;

		SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 500);
		SendMessage(hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000);
	}

	{
		SETTOOLTIP(IDC_GOP, "Set GOP size.");
		SETTOOLTIP(IDC_IDR, "Generate key frame every N frames.");
		SETTOOLTIP(IDC_QP_MIN, "Minimum QP. Default is 18.");
		SETTOOLTIP(IDC_QP_MAX, "Maximum QP. Default is 51.");
		SETTOOLTIP(IDC_FPS_NUM, "Numerator.");
		SETTOOLTIP(IDC_FPS_DEN, "Denominator.");
		SETTOOLTIP(IDC_FPS, "If encoder app does not set fps, fps defaults to 30 fps. Override it here.");

		const std::vector<SpinnerInts> spinners = {
			{ S_GOP, IDC_GOP, IDC_GOP_SPIN, 20, 0, 1000 }, //1000 to keep in range for header insertion
			{ S_IDR , IDC_IDR, IDC_IDR_SPIN, 120, 0, 1000 },
			//{ S_BFRAMES, IDC_BFRAMES, IDC_BFRAMES_SPIN, 0, 0, 16 },
			{ S_QP_MIN, IDC_QP_MIN, IDC_SPIN_QP_MIN, 18, 0, 51 },
			{ S_QP_MAX, IDC_QP_MAX, IDC_SPIN_QP_MAX, 51, 0, 51 },
			{ S_FPS_NUM, IDC_FPS_NUM, IDC_SPIN_FPS_NUM, 30, 1, 0x7FFFF },
			{ S_FPS_DEN, IDC_FPS_DEN, IDC_SPIN_FPS_DEN, 1, 1, 0x7FFFF },
			/*{ S_QPI, IDC_QPI, IDC_QPI_SPIN, 25, 0, 51 },
			{ S_QPP, IDC_QPP, IDC_QPP_SPIN, 25, 0, 51 },
			{ S_QPB, IDC_QPB, IDC_QPB_SPIN, 25, 0, 51 },
			{ S_QPB_DELTA, IDC_QPB_DELTA, IDC_QPB_DELTA_SPIN, 4, 0, 51 },*/
		};

		for (auto v : spinners)
		{
			int32_t val = pinst->mConfigTable[v.param];
			val = val < v.min ? v.def : (val > v.max ? v.def : val);
			pinst->mConfigTable[v.param] = val;
			//iInt = AppConfig->GetInt(TEXT("VCE Settings"), v.param, v.def);
			SendMessage(GetDlgItem(hwndDlg, v.spinCtrl), UDM_SETRANGE32, v.min, v.max);
			SendMessage(GetDlgItem(hwndDlg, v.spinCtrl), UDM_SETPOS32, 0, val);
			swprintf(temp, 1023, TEXT("%d"), val);
			SetDlgItemTextW(hwndDlg, v.editCtrl, temp);
		}
	}

	if (SendMessage(GetDlgItem(hwndDlg, IDC_RCM), CB_GETCOUNT, 0, 0) == 0)
	{
		SendDlgItemMessage(hwndDlg, IDC_RCM, CB_ADDSTRING, 0, (LPARAM)TEXT("Constrained QP"));
		SendDlgItemMessage(hwndDlg, IDC_RCM, CB_ADDSTRING, 0, (LPARAM)TEXT("Constant Bit Rate"));
		SendDlgItemMessage(hwndDlg, IDC_RCM, CB_ADDSTRING, 0, (LPARAM)TEXT("Peak Constrained VBR"));
		SendDlgItemMessage(hwndDlg, IDC_RCM, CB_ADDSTRING, 0, (LPARAM)TEXT("Latency Constrained VBR"));
	}

	if (SendMessage(GetDlgItem(hwndDlg, IDC_DEVICEIDX), CB_GETCOUNT, 0, 0) == 0)
	{
		std::vector<std::wstring> devices;
		pinst->mDeviceDX11.EnumerateDevices(false, &devices);
		for (auto it: devices)
			SendDlgItemMessageW(hwndDlg, IDC_DEVICEIDX, CB_ADDSTRING, 0, (LPARAM)it.c_str());
		int32_t idx = pinst->mConfigTable[S_DEVICEIDX];
		SendDlgItemMessage(hwndDlg, IDC_DEVICEIDX, CB_SETCURSEL, idx, 0);
	}

	CheckDlgButton(hwndDlg, IDC_PROF_BASE, 0);
	CheckDlgButton(hwndDlg, IDC_PROF_MAIN, 0);
	CheckDlgButton(hwndDlg, IDC_PROF_HIGH, 0);

	if (pinst->mConfigTable[S_PROFILE] == AMF_VIDEO_ENCODER_PROFILE_MAIN)
		CheckDlgButton(hwndDlg, IDC_PROF_MAIN, 1);
	else if (pinst->mConfigTable[S_PROFILE] == AMF_VIDEO_ENCODER_PROFILE_HIGH)
		CheckDlgButton(hwndDlg, IDC_PROF_HIGH, 1);
	else
		CheckDlgButton(hwndDlg, IDC_PROF_BASE, 1);

	if (SendMessage(GetDlgItem(hwndDlg, IDC_LEVEL), CB_GETCOUNT, 0, 0) == 0)
	{
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("3.0"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("3.1"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("3.2"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("4.0"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("4.1"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("4.2"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("5.0"));
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)TEXT("5.1"));
		int32_t level = pinst->mConfigTable[S_LEVEL];
		LevelToIdx(level);
		SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_SETCURSEL, level, 0);
	}

	if (SendMessage(GetDlgItem(hwndDlg, IDC_PRESET), CB_GETCOUNT, 0, 0) == 0)
	{
		SendDlgItemMessage(hwndDlg, IDC_PRESET, CB_ADDSTRING, 0, (LPARAM)TEXT("Balanced"));
		SendDlgItemMessage(hwndDlg, IDC_PRESET, CB_ADDSTRING, 0, (LPARAM)TEXT("Speed"));
		SendDlgItemMessage(hwndDlg, IDC_PRESET, CB_ADDSTRING, 0, (LPARAM)TEXT("Quality"));
		auto preset = pinst->mConfigTable[S_PRESET];
		SendDlgItemMessage(hwndDlg, IDC_PRESET, CB_SETCURSEL, preset, 0);
	}

	if (SendMessage(GetDlgItem(hwndDlg, IDC_COLOR_PROFILE), CB_GETCOUNT, 0, 0) == 0)
	{
		SendDlgItemMessage(hwndDlg, IDC_COLOR_PROFILE, CB_ADDSTRING, 0, (LPARAM)TEXT("BT.601 (SD)"));
		SendDlgItemMessage(hwndDlg, IDC_COLOR_PROFILE, CB_ADDSTRING, 0, (LPARAM)TEXT("BT.709 (HD)"));
		auto idx = pinst->mConfigTable[S_COLORPROF];
		SendDlgItemMessage(hwndDlg, IDC_COLOR_PROFILE, CB_SETCURSEL, idx, 0);
	}

	SendDlgItemMessage(hwndDlg, IDC_RCM, CB_SETCURSEL, pinst->mConfigTable[S_RCM], 0);
	switch (pinst->mConfigTable[S_RCM])
	{
	case RCM_CQP:
		SetDlgItemText(hwndDlg, IDC_RC_LABEL, TEXT("Quantizer"));
		SetDlgItemText(hwndDlg, IDC_RC_LOW_LABEL, TEXT("0 (High quality)"));
		swprintf(temp, 1023, TEXT("(Low quality) %d"), MAX_QUANT);
		SetDlgItemText(hwndDlg, IDC_RC_HIGH_LABEL, temp);
		SetDlgItemInt(hwndDlg, IDC_RC_VAL, pinst->mConfigTable[S_QPI], FALSE);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 0);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, MAX_QUANT);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, pinst->mConfigTable[S_QPI]);
		break;

	case RCM_CBR:
		SetDlgItemText(hwndDlg, IDC_RC_LABEL, TEXT("Constant bitrate (kbit/s)"));
		SetDlgItemText(hwndDlg, IDC_RC_LOW_LABEL, TEXT("1"));
		swprintf(temp, 1023, TEXT("%d"), MAX_BITRATE);
		SetDlgItemText(hwndDlg, IDC_RC_HIGH_LABEL, temp);
		SetDlgItemInt(hwndDlg, IDC_RC_VAL, pinst->mConfigTable[S_BITRATE], FALSE);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
		//SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, scale2pos(MAX_BITRATE));
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, MAX_BITRATE);
		//SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, scale2pos(pinst->mConfigTable[S_BITRATE]));
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, pinst->mConfigTable[S_BITRATE]);
		break;

	case RCM_PCVBR:
	case RCM_LCVBR:
		SetDlgItemText(hwndDlg, IDC_RC_LABEL, TEXT("Variable bitrate (kbit/s)"));
		SetDlgItemText(hwndDlg, IDC_RC_LOW_LABEL, TEXT("1"));
		wsprintf(temp, TEXT("%d"), MAX_BITRATE);
		SetDlgItemText(hwndDlg, IDC_RC_HIGH_LABEL, temp);
		SetDlgItemInt(hwndDlg, IDC_RC_VAL, pinst->mConfigTable[S_BITRATE], FALSE);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, MAX_BITRATE);
		SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, pinst->mConfigTable[S_BITRATE]);
		break;
	}

	CheckDlgButton(hwndDlg, IDC_CABAC, pinst->mConfigTable[S_CABAC]);
	CheckDlgButton(hwndDlg, IDC_LOG, pinst->mConfigTable[S_LOG]);
	CheckDlgButton(hwndDlg, IDC_FPS, pinst->mConfigTable[S_FPS_ENABLED]);
	CheckDlgButton(hwndDlg, IDC_OPENCL, pinst->mConfigTable[S_DISABLE_OCL]);

	swprintf(temp, 1023, TEXT("Build date: %S %S"), __DATE__, __TIME__);
	SetDlgItemText(hwndDlg, IDC_BUILD_DATE, temp);
}

static BOOL CALLBACK ConfigureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	CodecInst *pinst = (CodecInst *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	int32_t rate = 0, qp = 0;

	if (uMsg == WM_INITDIALOG) {
		CodecInst *pinst = (CodecInst *)lParam;
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		pinst->mDialogUpdated = false;
		DialogUpdate(hwndDlg, (CodecInst*)lParam);
		pinst->mDialogUpdated = true;
		return TRUE;
	}
	else if (uMsg == WM_CLOSE){
		EndDialog(hwndDlg, 0);
	}

	if (!(pinst && pinst->mDialogUpdated)) return FALSE;
	switch (uMsg){
	case WM_COMMAND:

		switch (HIWORD(wParam))
		{
		case LBN_SELCHANGE:
			switch (LOWORD(wParam))
			{
			case IDC_RCM:
				pinst->mConfigTable[S_RCM] = (int)SendDlgItemMessage(hwndDlg, IDC_RCM, CB_GETCURSEL, 0, 0);

				pinst->mDialogUpdated = false;
				DialogUpdate(hwndDlg, pinst);
				pinst->mDialogUpdated = true;

				/* Ugly hack for fixing visualization bug of IDC_RC_VAL_SLIDER */
				//ShowWindow(GetDlgItem(hwndDlg, IDC_RC_VAL_SLIDER), FALSE);
				//ShowWindow(GetDlgItem(hwndDlg, IDC_RC_VAL_SLIDER), 1);

				break;

			case IDC_LEVEL:
				pinst->mConfigTable[S_LEVEL] = (int)SendDlgItemMessage(hwndDlg, IDC_LEVEL, CB_GETCURSEL, 0, 0);
				IdxToLevel(pinst->mConfigTable[S_LEVEL]);

				break;
			case IDC_PRESET:
				pinst->mConfigTable[S_PRESET] = (int)SendDlgItemMessage(hwndDlg, IDC_PRESET, CB_GETCURSEL, 0, 0);
				break;
			case IDC_COLOR_PROFILE:
				pinst->mConfigTable[S_COLORPROF] = (int)SendDlgItemMessage(hwndDlg, IDC_COLOR_PROFILE, CB_GETCURSEL, 0, 0);
				break;
			default:
				return FALSE;
			}
			break;

		case BN_CLICKED:
			switch (LOWORD(wParam))
			{
			case IDOK:
				pinst->SaveRegistry();
				EndDialog(hwndDlg, LOWORD(wParam));
				break;
			case IDCANCEL:
				pinst->ReadRegistry();
				EndDialog(hwndDlg, LOWORD(wParam));
				break;
			case IDC_CABAC:
				pinst->mConfigTable[S_CABAC] = IsDlgButtonChecked(hwndDlg, IDC_CABAC);
				break;
			case IDC_LOG:
				pinst->mConfigTable[S_LOG] = IsDlgButtonChecked(hwndDlg, IDC_LOG);
				break;
			case IDC_OPENCL:
				pinst->mConfigTable[S_DISABLE_OCL] = IsDlgButtonChecked(hwndDlg, IDC_OPENCL);
				break;
			case IDC_FPS:
				pinst->mConfigTable[S_FPS_ENABLED] = IsDlgButtonChecked(hwndDlg, IDC_FPS);
				break;
			case IDC_PROF_BASE:
				pinst->mConfigTable[S_PROFILE] = AMF_VIDEO_ENCODER_PROFILE_BASELINE;
				break;
			case IDC_PROF_MAIN:
				pinst->mConfigTable[S_PROFILE] = AMF_VIDEO_ENCODER_PROFILE_MAIN;
				break;
			case IDC_PROF_HIGH:
				pinst->mConfigTable[S_PROFILE] = AMF_VIDEO_ENCODER_PROFILE_HIGH;
				break;
			}
			break;

		case EN_CHANGE:
			switch (LOWORD(wParam))
			{
			case IDC_IDR:
				CHECKED_SET_MAX_INT(rate, hwndDlg, IDC_IDR, FALSE, 1, 0xFFFFFFFF);//TODO max
				SendMessage(GetDlgItem(hwndDlg, IDC_IDR_SPIN), UDM_SETPOS32, 0, rate);
				pinst->mConfigTable[S_IDR] = rate;
				break;
			case IDC_GOP:
				CHECKED_SET_MAX_INT(rate, hwndDlg, IDC_GOP, FALSE, 0, 0xFFFFFFFF);//TODO max
				SendMessage(GetDlgItem(hwndDlg, IDC_GOP_SPIN), UDM_SETPOS32, 0, rate);
				pinst->mConfigTable[S_GOP] = rate;
				break;
			case IDC_RC_VAL:
				switch (pinst->mConfigTable[S_RCM])
				{
				case RCM_CQP:
					CHECKED_SET_MAX_INT(qp, hwndDlg, IDC_RC_VAL, FALSE, 0, MAX_QUANT);
					SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, qp);
					pinst->mConfigTable[S_QPI] = qp;
					break;
				case RCM_CBR:
				case RCM_PCVBR:
				case RCM_LCVBR:
					CHECKED_SET_MAX_INT(rate, hwndDlg, IDC_RC_VAL, FALSE, 1, MAX_BITRATE);
					SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, rate);
					pinst->mConfigTable[S_BITRATE] = rate;
					break;
				}
				break;
			case IDC_QP_MIN:
				CHECKED_SET_MAX_INT(qp, hwndDlg, IDC_QP_MIN, FALSE, 0, MAX_QUANT);
				SendMessage(GetDlgItem(hwndDlg, IDC_SPIN_QP_MIN), UDM_SETPOS32, 0, qp);
				pinst->mConfigTable[S_QP_MIN] = qp;
				break;
			case IDC_QP_MAX:
				CHECKED_SET_MAX_INT(qp, hwndDlg, IDC_QP_MAX, FALSE, 0, MAX_QUANT);
				SendMessage(GetDlgItem(hwndDlg, IDC_SPIN_QP_MAX), UDM_SETPOS32, 0, qp);
				pinst->mConfigTable[S_QP_MAX] = qp;
				break;
			case IDC_FPS_NUM:
				CHECKED_SET_MAX_INT(qp, hwndDlg, IDC_FPS_NUM, FALSE, 1, 0x7FFF);
				SendMessage(GetDlgItem(hwndDlg, IDC_SPIN_FPS_NUM), UDM_SETPOS32, 0, qp);
				pinst->mConfigTable[S_FPS_NUM] = qp;
				break;
			case IDC_FPS_DEN:
				CHECKED_SET_MAX_INT(qp, hwndDlg, IDC_FPS_DEN, FALSE, 1, 0x7FFF);
				SendMessage(GetDlgItem(hwndDlg, IDC_SPIN_FPS_DEN), UDM_SETPOS32, 0, qp);
				pinst->mConfigTable[S_FPS_DEN] = qp;
				break;
			}
			break;
		}
	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_RC_VAL_SLIDER))
		{
			switch (pinst->mConfigTable[S_RCM])
			{
			case RCM_CQP:
				pinst->mConfigTable[S_QPI] =
					(int)SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
				SetDlgItemInt(hwndDlg, IDC_RC_VAL, pinst->mConfigTable[S_QPI], FALSE);
				break;

			case RCM_CBR:
			case RCM_PCVBR:
			case RCM_LCVBR:
				//rate = pos2scale(SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_GETPOS, 0, 0));
				rate = SendDlgItemMessage(hwndDlg, IDC_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
				rate = CLIP(rate, 1, MAX_BITRATE);
				pinst->mConfigTable[S_BITRATE] = rate;
				SetDlgItemInt(hwndDlg, IDC_RC_VAL, rate, FALSE);
				break;

			default:
				assert(0);
				break;
			}
		}
		else
			return FALSE;
	break;

	case WM_NOTIFY:
	{
		UINT nCode;
		nCode = ((LPNMHDR)lParam)->code;
		switch (nCode)
		{
		case UDN_DELTAPOS:
			LPNMUPDOWN lpnmud;
			lpnmud = (LPNMUPDOWN)lParam;
			//Dbg(L"Spinner: %d %d\n", lpnmud->iPos, lpnmud->iDelta);
			int val = lpnmud->iPos;// +lpnmud->iDelta;
			if (val < 0)
				val = 0;

			int ctrl = 0;
			switch (((LPNMHDR)lParam)->idFrom)
			{
			case IDC_SPIN_QP_MIN: ctrl = IDC_QP_MIN; break;
			case IDC_SPIN_QP_MAX: ctrl = IDC_QP_MAX; break;
			case IDC_IDR_SPIN: ctrl = IDC_IDR; break;
			case IDC_GOP_SPIN: ctrl = IDC_GOP; break;
			case IDC_SPIN_FPS_NUM: ctrl = IDC_FPS_NUM; break;
			case IDC_SPIN_FPS_DEN: ctrl = IDC_FPS_DEN; break;
			}

			switch (((LPNMHDR)lParam)->idFrom)
			{
			case IDC_SPIN_QP_MIN:
			case IDC_SPIN_QP_MAX:
				val = CLIP(val, 0, MAX_QUANT);
				break;
			case IDC_IDR_SPIN:
			case IDC_GOP_SPIN:
				val = val > 1000 ? 1000 : val;
				break;
			case IDC_FPS_NUM:
			case IDC_FPS_DEN:
				val = val < 1 ? 1 : val;
				break;
			}

			if (ctrl > 0)
			{
				char temp[256];
				sprintf_s(temp, "%d", val);
				SetDlgItemTextA(hwndDlg, ctrl, temp);
			}
			break;
		}
	}
	break;
	}

	return 0;
}

BOOL CodecInst::QueryConfigure() {
	return TRUE;
}

DWORD CodecInst::Configure(HWND hwnd) {
	DialogBoxParam(hmoduleVFW, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, (DLGPROC)ConfigureDialogProc, (LPARAM)this);
	return ICERR_OK;
}