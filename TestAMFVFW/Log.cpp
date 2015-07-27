#include "stdafx.h"
#include "Log.h"

Logger::Logger(bool _log) : mLog(NULL), mWritelog(_log)
{
}

bool Logger::open()
{
	if (!mWritelog) return false;
	close();
	errno_t err = fopen_s(&mLog, "TestAMFVFW.log", "w, ccs=UNICODE");
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