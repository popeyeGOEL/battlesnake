#ifndef __TUNJI_LIBRARY_TRHEAD_PROFILER__
#define __TUNJI_LIBRARY_TRHEAD_PROFILER__
#include <list>
#include <Windows.h>
#include <ctime>

namespace TUNJI_LIBRARY
{
//#define THREAD_PROFILE_CHECK
	class cThreadProfiler
	{
	public:
		enum
		{
			en_MAX_SAMPLE = 100
		};

		struct st_COUNTING_INFO
		{
			bool	isUsing;
			WCHAR	tag[32];
			UINT64	beginTime;
			UINT64	maxTime[2];
			UINT64	minTime[2];
			UINT64	totalTime;
			UINT64	count;
		};

		struct st_PROFILE_SAMPLE
		{
			UINT64				threadID;
			st_COUNTING_INFO	sample[en_MAX_SAMPLE];
		};

		cThreadProfiler()
		{
			InitializeSRWLock(&_sampleLock);
			QueryPerformanceFrequency(&_freq);
			_microSecPerFreq = (float)_freq.QuadPart / (float)1000000;
		}
		virtual ~cThreadProfiler()
		{
			st_PROFILE_SAMPLE *pSample;
			std::list<st_PROFILE_SAMPLE*>::iterator iter = _sampleList.begin();
			while (iter != _sampleList.end())
			{
				pSample = *iter;
				delete pSample;
				iter = _sampleList.erase(iter);
			}
		}
		void CreateThreadID(UINT64 threadID)
		{
			_pSample = new st_PROFILE_SAMPLE;
			_pSample->threadID = threadID;
			memset(_pSample->sample, 0, sizeof(st_COUNTING_INFO) * en_MAX_SAMPLE);

			LockSampleList();
			_sampleList.push_back(_pSample);
			UnlockSampleList();
		}

		bool printKey(WCHAR key)
		{
			if (key == L'p' || key == L'P')
			{
				PrintProfileHistory();
				return true;
			}
			return false;
		}

		void PrintProfileHistory(void)
		{

			#ifdef THREAD_PROFILE_CHECK

			int count;
			FILE *fp;
			errno_t err;
			time_t timer;
			tm t;
			WCHAR title[128];
			float freq;

			freq = _microSecPerFreq;

			time(&timer);
			localtime_s(&t, &timer);
			wsprintf(title, L"ProfileThread_%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
			err = _wfopen_s(&fp, title, L"wt");

			count = 0;

			std::list<st_PROFILE_SAMPLE*>::iterator iter;
			fwprintf_s(fp, L"%5s	|%20s	|%15s	|%15s	|%15s	|%15s	|\n", L"ID", L"TAG", L"AVERAGE", L"MIN", L"MAX", L"CALL");
			fwprintf(fp, L"-----------------------------------------------------------------------------------------------------------------------------------------\n");
			iter = _sampleList.begin();
			while (iter != _sampleList.end())
			{
				st_PROFILE_SAMPLE *pSample = *iter;
				fwprintf(fp, L"%5lld", pSample->threadID);
				for (int i = 0; i < en_MAX_SAMPLE; ++i)
				{
					if (!pSample->sample[i].isUsing) continue;
					st_COUNTING_INFO *pCount = &pSample->sample[i];
					if (pCount->minTime[0] == 0x7fffffffffffffff)
						pCount->minTime[0] = pCount->minTime[1];
					if (pCount->maxTime[0] == 0)
						pCount->maxTime[0] = pCount->maxTime[1];
					fwprintf(fp, L"	|%20s	|%15fus	|%15fus	|%15fus	|%15lld	| \n",
						pCount->tag, (double)((double)(pCount->totalTime - pCount->maxTime[1] - pCount->minTime[1]) / (double)(pCount->count - 2)) / freq,
						(double)(pCount->minTime[0] / freq), (double)(pCount->maxTime[0] / freq), 
						pCount->count - 2);
					fwprintf(fp, L"\n");
				}
				fwprintf(fp, L"\n");
				fwprintf(fp, L"-----------------------------------------------------------------------------------------------------------------------------------------\n");
				++iter;
			}
			fwprintf(fp, L"-----------------------------------------------------------------------------------------------------------------------------------------\n");
			fclose(fp);
			#endif
		}

		static __declspec(thread) st_PROFILE_SAMPLE *_pSample;

	private:
		void LockSampleList(void)
		{
			AcquireSRWLockExclusive(&_sampleLock);
		}
		void UnlockSampleList(void)
		{
			ReleaseSRWLockExclusive(&_sampleLock);
		}
		SRWLOCK	_sampleLock;
		std::list<st_PROFILE_SAMPLE*> _sampleList;
		LARGE_INTEGER _freq; 
		float _microSecPerFreq;

	};

	static cThreadProfiler::st_COUNTING_INFO* CreateTag(cThreadProfiler::st_PROFILE_SAMPLE* pSample, WCHAR* tag)
	{
		cThreadProfiler::st_COUNTING_INFO *pCount = nullptr;
		int i;
		for (i = 0; i < cThreadProfiler::en_MAX_SAMPLE; ++i)
		{
			if (!pSample->sample[i].isUsing)
			{
				pCount = &pSample->sample[i];
				break;
			}
		}
		if (i == cThreadProfiler::en_MAX_SAMPLE) return nullptr;

		memcpy(pCount->tag, tag, sizeof(WCHAR) * 32);
		pCount->isUsing = true;
		pCount->minTime[0] = 0x7fffffffffffffff;
		pCount->minTime[1] = 0x7fffffffffffffff;
		return pCount;
	}

	static cThreadProfiler::st_COUNTING_INFO* FindTag(cThreadProfiler::st_PROFILE_SAMPLE* pSample, WCHAR* tag)
	{
		for (int i = 0; i < cThreadProfiler::en_MAX_SAMPLE; ++i)
		{
			if (wcscmp(pSample->sample[i].tag, tag) == 0)
			{
				return &pSample->sample[i];
			}
		}

		return CreateTag(pSample, tag);
	}

	static void EnterSample(WCHAR *tag)
	{
		LARGE_INTEGER start;
		QueryPerformanceCounter(&start);
		cThreadProfiler::st_PROFILE_SAMPLE *pSample = cThreadProfiler::_pSample;
		cThreadProfiler::st_COUNTING_INFO *pCount = FindTag(pSample, tag);
		if (pCount == nullptr) return;
		if (pCount->beginTime != 0) return;
		pCount->beginTime = start.QuadPart;
	}

	static void LeaveSample(WCHAR *tag)
	{
		LARGE_INTEGER end;
		UINT64 timeInterval;
		QueryPerformanceCounter(&end);
		cThreadProfiler::st_PROFILE_SAMPLE *pSample = cThreadProfiler::_pSample;
		cThreadProfiler::st_COUNTING_INFO* pCount = FindTag(pSample, tag);
		if (pCount == nullptr) return;
		if (pCount->beginTime == 0) return;
		timeInterval = end.QuadPart - pCount->beginTime;
		++pCount->count;
		pCount->totalTime += timeInterval;

		if (pCount->maxTime[1] < timeInterval)		//	제일 큰것보다 클때 
		{
			pCount->maxTime[0] = pCount->maxTime[1];	// 두번째로 큰값에 원래값을 넣는다 
			pCount->maxTime[1] = timeInterval;
		}
		
		if (pCount->minTime[1] > timeInterval)		//	제일 작은것보다 작을때 
		{
			pCount->minTime[0] = pCount->minTime[1];	//	두번째로 작은값에 원래값을 넣는다. 
			pCount->minTime[1] = timeInterval;
		}

		pCount->beginTime = 0;
	}

	extern cThreadProfiler g_profiler;

#ifdef THREAD_PROFILE_CHECK
#define EnterProfile(Tag) EnterSample(Tag)
#define LeaveProfile(Tag) LeaveSample(Tag)
#endif

#ifndef THREAD_PROFILE_CHECK
#define EnterProfile(Tag)
#define LeaveProfile(Tag)
#endif
}

#endif