#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>

//#define NUMTHREADS 3 // doesn't seem to scale beyond 2
struct BufferCopyState
{
	HANDLE sigCopy, sigDone;
	uint8_t *pSrc;
	uint8_t *pDst;
	size_t size;
};

struct BufferCopyManager
{
	std::vector<BufferCopyState> state;
	std::vector<HANDLE> threads;
	std::vector<HANDLE> evtHandles;

	BufferCopyManager()
	{
	}

	~BufferCopyManager()
	{
		Stop();
	}

	void Start(int thrcount)
	{
		threads.resize(thrcount);
		state.resize(threads.size(), BufferCopyState{ INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, 0,0,0 });
		evtHandles.reserve(threads.size());
		for (size_t i = 0; i < threads.size(); i++)
		{
			state[i].sigCopy = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			state[i].sigDone = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			evtHandles.push_back(state[i].sigDone);
		}

		for (size_t i = 0; i < threads.size(); i++)
			threads[i] = CreateThread(nullptr, 0, BufferCopyThread, &state[i], 0, nullptr);
	}

	void Stop()
	{
		for (size_t i = 0; i < threads.size(); i++)
		{
			state[i].pSrc = nullptr;
			if (state[i].sigCopy != INVALID_HANDLE_VALUE)
				SetEvent(state[i].sigCopy);
		}
		WaitForMultipleObjects(threads.size(), threads.data(), TRUE, 3001); //Join threads

		for (size_t i = 0; i < state.size(); i++)
		{
			CloseHandle(state[i].sigCopy);
			CloseHandle(state[i].sigDone);
			CloseHandle(threads[i]);
		}
		threads.resize(0);
		state.resize(0);
	}

	DWORD Wait()
	{
		return WaitForMultipleObjects(evtHandles.size(), evtHandles.data(), TRUE, 10000);
	}

	void SetData(void *src, void *dst, size_t size)
	{
		size_t threadcount = threads.size();
		for (size_t i = 0; i < threadcount; i++)
		{
			if (i < threadcount - 1)
				state[i].size = size / threadcount;
			else
				state[i].size = size - state[0].size * (threadcount - 1);

			if (i == 0)
			{
				state[i].pSrc = (uint8_t*)src;
				state[i].pDst = (uint8_t*)dst;
			}
			else
			{
				state[i].pSrc = state[i - 1].pSrc + state[i - 1].size;
				state[i].pDst = state[i - 1].pDst + state[i - 1].size;
			}

			SetEvent(state[i].sigCopy);
		}
	}

	static DWORD WINAPI BufferCopyThread(LPVOID param)
	{
		BufferCopyState *state = static_cast<BufferCopyState*>(param);
		while (true)
		{
			DWORD ret = WaitForSingleObject(state->sigCopy, INFINITE);
			if (ret != WAIT_OBJECT_0 || state->pSrc == nullptr)
				break;
			memcpy(state->pDst, state->pSrc, state->size);
			SetEvent(state->sigDone);
		}
		return 0;
	}
};