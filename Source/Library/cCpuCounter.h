#pragma once

namespace TUNJI_LIBRARY
{
	class cCpuCounter
	{
	public:
		cCpuCounter(HANDLE hProcess = INVALID_HANDLE_VALUE);
		
		void UpdateCpuTime(void);

		float GetUnitTotalUsage(void) {return _units_totalPercent;}
		float GetUnitUserUsage(void) {return _units_userPercent;}
		float GetUnitKernelUsage(void) {return _units_kernelPercent;}

		float GetProcessTotalUsage(void) {return _process_totalPercent;}
		float GetProcessUserUsage(void) {return _process_userPercent;}
		float GetProcessKernelUsage(void) {	return _process_kernelPercent;}

	private:

		HANDLE _hProcess;
		int _unitNum;

		float				_units_totalPercent;
		float				_units_userPercent;
		float				_units_kernelPercent;

		float				_process_totalPercent;
		float				_process_userPercent;
		float				_process_kernelPercent;

		ULARGE_INTEGER		_units_lastKernelUsedTime;
		ULARGE_INTEGER		_units_lastUserUsedTime;
		ULARGE_INTEGER		_units_lastIdleTime;

		ULARGE_INTEGER		_process_lastKernelUsedTime;
		ULARGE_INTEGER		_process_lastUserUsedTime;
		ULARGE_INTEGER		_process_lastTime;
	};

};