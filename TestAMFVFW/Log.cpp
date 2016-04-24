#include "stdafx.h"
#include "Log.h"
#include <Shlobj.h>

Logger::Logger(bool _log) : mLog(NULL), mWritelog(_log)
{
	WCHAR path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
		mUserProfile = path;
	}
}

int GetFmtSize(wchar_t *buf, const wchar_t *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int size = _vscwprintf(fmt, args) + 1;
	if (buf)
		_vsnwprintf_s(buf, size, size - 1, fmt, args);
	va_end(args);
	return size;
}

bool Logger::open()
{
	if (!mWritelog) return false;
	close();
	const wchar_t *fmt = L"TestAMFVFW_%hu%02hu%02hu_%02hu%02hu%02hu.log";
	SYSTEMTIME time;
	GetLocalTime(&time);

	int sz = GetFmtSize(nullptr, fmt,
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond);

	std::vector<wchar_t> msg(sz);
	GetFmtSize(&msg[0], fmt,
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond);

	std::wstring path;
	if (mUserProfile.size())
	{
		path.append(L"\\\\?\\"); //Should enable 32k path length
		path.append(mUserProfile);
		path.append(L"\\TestAMFVFW\\");
		if (!CreateDirectory(path.c_str(), NULL) &&
			GetLastError() != ERROR_ALREADY_EXISTS)
		{
			path = L"";
		}
	}

	path.append(&msg[0]);

	errno_t err = _wfopen_s(&mLog, path.c_str(), L"w, ccs=UNICODE");
	return mLog != NULL;
}

void Logger::close()
{
	if (mLog) fclose(mLog);
	mLog = NULL;
}

Logger::~Logger()
{
	if (mLog) fclose(mLog);
}

void Logger::Log_internal(const wchar_t *psz_fmt, va_list args)
{
	if (!mLog) open();

	if (mLog)
	{
		int bufsize = _vscwprintf(psz_fmt, args) + 1;
		std::vector<wchar_t> msg(bufsize);
		_vsnwprintf_s(&msg[0], bufsize, bufsize - 1, psz_fmt, args);
		fwrite(&msg[0], sizeof(wchar_t), bufsize - 1, mLog);
		fwrite(L"\r\n", sizeof(wchar_t), 2, mLog);
		fflush(mLog);
	}
}

void Logger::Log(const wchar_t *psz_fmt, ...)
{
	if (!mWritelog) return;
	va_list arg;
	va_start(arg, psz_fmt);
	Log_internal(psz_fmt, arg);
	va_end(arg);
}

void Logger::enableLog(bool b)
{
	mWritelog = b;
	if (!b) close();
}

void OutDebug(const wchar_t* psz_fmt, ...)
{
	va_list args;
	va_start(args, psz_fmt);
	int bufsize = _vscwprintf(psz_fmt, args) + 1;
	std::vector<wchar_t> msg(bufsize);
	_vsnwprintf_s(&msg[0], bufsize, bufsize - 1, psz_fmt, args);
	OutputDebugStringW(&msg[0]);
	va_end(args);
	
}