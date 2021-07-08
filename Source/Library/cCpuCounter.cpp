#include <Windows.h>
#include "cCpuCounter.h"

namespace TUNJI_LIBRARY
{
	cCpuCounter::cCpuCounter(HANDLE hProcess)
	{
		//	프로세스 핸들 입력이 없을 시에는 자기 자신을 사용
		if(hProcess == INVALID_HANDLE_VALUE)
		{
			_hProcess = GetCurrentProcess();
		}

		//	Unit(CPU) 개수를 확인한다 
		///	프로세스 실행률 계신시 cpu 개수로 나눠 실제 사용률을 구함
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		_unitNum = systemInfo.dwNumberOfProcessors;

		_units_totalPercent = 0;
		_units_userPercent = 0;
		_units_kernelPercent = 0;

		_process_totalPercent = 0;
		_process_userPercent = 0;
		_process_kernelPercent = 0;

		_units_lastKernelUsedTime.QuadPart = 0;
		_units_lastUserUsedTime.QuadPart = 0;
		_units_lastIdleTime.QuadPart = 0;

		_process_lastKernelUsedTime.QuadPart = 0;
		_process_lastUserUsedTime.QuadPart = 0;
		_process_lastTime.QuadPart = 0;

		UpdateCpuTime();
	}

	//	CPU 사용률 갱신. 
	//	0.5초~1초 단위 호출 
	void cCpuCounter::UpdateCpuTime(void)
	{
		///	본래의 사용 구조체는 FILETIME이지만, ULARGE_INTEGER와 구조가 같으므로 이것을 사용한다. 
		///	FILETIME 구조체는 100 ns 단위까지 표현 가능

		ULARGE_INTEGER idleTime;
		ULARGE_INTEGER kernelTime;
		ULARGE_INTEGER userTime;

		//	시스템 사용 시간 
		///	Idle Time / Kernel Time(including Idle) / User Time 
		if(GetSystemTimes((PFILETIME)&idleTime, (PFILETIME)&kernelTime, (PFILETIME)&userTime) == false)
		{
			return;
		}

		ULONG64	kernelInterval	= kernelTime.QuadPart - _units_lastKernelUsedTime.QuadPart;
		ULONG64 userInterval	= userTime.QuadPart - _units_lastUserUsedTime.QuadPart;
		ULONG64 idleInterval	= idleTime.QuadPart - _units_lastIdleTime.QuadPart;

		ULONG64 totalInterval	= kernelInterval + userInterval;
		

		if(totalInterval == 0)
		{
			_units_userPercent = 0.0f;
			_units_kernelPercent = 0.0f;
			_units_totalPercent = 0.0f;
		}
		else 
		{
			_units_totalPercent = (float)((double)(totalInterval - idleInterval) / totalInterval * 100.0f);
			_units_userPercent = (float)((double)(userInterval / totalInterval * 100.0f));
			_units_kernelPercent = (float)((double)(kernelInterval - idleInterval) / totalInterval * 100.0f);
		}

		_units_lastKernelUsedTime = kernelTime;
		_units_lastUserUsedTime = userTime;
		_units_lastIdleTime = idleTime;


		//	프로세스 사용률 계산 
		ULARGE_INTEGER none;
		ULARGE_INTEGER curTime;

		///	프로세스 사용률 판단 공식
		///	a = 샘플 간격의 시스템 시간을 구함 (실제로 지나간 시간)
		///	b = 프로세스의 CPU 사용시간을 구함 
		///	a : 100 = b: 사용률

		//	현재 시간
		GetSystemTimeAsFileTime((LPFILETIME)&curTime);
		
		//	프로세스가 사용한 시간
		///	두번째, 세번째 인자는 실행 종료 시간으로 사용하지 않는다
		GetProcessTimes(_hProcess, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernelTime, (LPFILETIME)&userTime);

		//	이전에 저장된 프로세스 시간과의 차를 구해서 실제로 얼마만큼의 시간이 지났는지 확인
		//	실제 지나온 시간으로 나눈다.
		ULONG64 processInterval = curTime.QuadPart - _process_lastTime.QuadPart;
		kernelInterval = kernelTime.QuadPart - _process_lastKernelUsedTime.QuadPart;
		userInterval = userTime.QuadPart - _process_lastUserUsedTime.QuadPart;

		totalInterval = kernelInterval + userInterval;

		if (totalInterval == 0)
		{
			_process_totalPercent = 0.0f;
			_process_userPercent = 0.0f;
			_process_kernelPercent = 0.0f;
		}
		else
		{
			_process_totalPercent = (float)(totalInterval / (double)_unitNum / (double)processInterval * 100.0f);
			_process_userPercent = (float)(userInterval / (double)_unitNum / (double)processInterval * 100.0f);
			_process_kernelPercent = (float)(kernelInterval / (double)_unitNum / (double)processInterval * 100.0f);
		}

		_process_lastTime = curTime;
		_process_lastUserUsedTime = userTime;
		_process_lastKernelUsedTime = kernelTime;
	}
};


//cCpuCounter	cUsage;

//while(1)
//{
//	cUsage.UpdateCpuTime();
//	wprintf(L"Units		:	%f / Process	:	%f \n", cUsage.GetUnitTotalUsage(), cUsage.GetProcessTotalUsage());
//	wprintf(L"Units Kernel	:	%f / Process Kernel	:	%f \n", cUsage.GetUnitKernelUsage(), cUsage.GetProcessKernelUsage());
//	wprintf(L"Units User	:	%f / Process User	:	%f \n\n", cUsage.GetUnitUserUsage(), cUsage.GetProcessUserUsage());
//	Sleep(1000);