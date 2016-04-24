#ifndef _LOGGER
#define _LOGGER

#include <debugapi.h>
#if _DEBUG
#define Dbg(fmt, ...) OutDebug(fmt, ##__VA_ARGS__)
#else
#define Dbg
#endif

void OutDebug(const wchar_t* psz_fmt, ...);

struct Timing
{
public:
	Timing(char *name)
		: mName(name)
		, startP(std::chrono::system_clock::now())
	{

	}

	~Timing()
	{
		auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>
			(std::chrono::system_clock::now() - startP).count();
		Dbg(L"Timing(%S): %fms\n", mName, (duration / 1.0E6));
	}

	std::chrono::time_point<std::chrono::system_clock> startP;
	char* mName;
};

#ifdef _DEBUG
#define Profile(name) { Timing timing ## name(#name);
#define EndProfile }
#else
#define Profile(name)
#define EndProfile
#endif

class Logger
{
private:
	FILE* mLog;
	bool mWritelog;
	std::wstring mUserProfile;
public:
	Logger(bool _log);
	~Logger();
	void enableLog(bool b);
	bool open();
	void close();

	void Log_internal(const wchar_t *psz_fmt, va_list arg);
	void Log(const wchar_t *psz_fmt, ...);
};

#endif