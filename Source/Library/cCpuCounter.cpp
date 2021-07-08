#include <Windows.h>
#include "cCpuCounter.h"

namespace TUNJI_LIBRARY
{
	cCpuCounter::cCpuCounter(HANDLE hProcess)
	{
		//	���μ��� �ڵ� �Է��� ���� �ÿ��� �ڱ� �ڽ��� ���
		if(hProcess == INVALID_HANDLE_VALUE)
		{
			_hProcess = GetCurrentProcess();
		}

		//	Unit(CPU) ������ Ȯ���Ѵ� 
		///	���μ��� ����� ��Ž� cpu ������ ���� ���� ������ ����
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

	//	CPU ���� ����. 
	//	0.5��~1�� ���� ȣ�� 
	void cCpuCounter::UpdateCpuTime(void)
	{
		///	������ ��� ����ü�� FILETIME������, ULARGE_INTEGER�� ������ �����Ƿ� �̰��� ����Ѵ�. 
		///	FILETIME ����ü�� 100 ns �������� ǥ�� ����

		ULARGE_INTEGER idleTime;
		ULARGE_INTEGER kernelTime;
		ULARGE_INTEGER userTime;

		//	�ý��� ��� �ð� 
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


		//	���μ��� ���� ��� 
		ULARGE_INTEGER none;
		ULARGE_INTEGER curTime;

		///	���μ��� ���� �Ǵ� ����
		///	a = ���� ������ �ý��� �ð��� ���� (������ ������ �ð�)
		///	b = ���μ����� CPU ���ð��� ���� 
		///	a : 100 = b: ����

		//	���� �ð�
		GetSystemTimeAsFileTime((LPFILETIME)&curTime);
		
		//	���μ����� ����� �ð�
		///	�ι�°, ����° ���ڴ� ���� ���� �ð����� ������� �ʴ´�
		GetProcessTimes(_hProcess, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernelTime, (LPFILETIME)&userTime);

		//	������ ����� ���μ��� �ð����� ���� ���ؼ� ������ �󸶸�ŭ�� �ð��� �������� Ȯ��
		//	���� ������ �ð����� ������.
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