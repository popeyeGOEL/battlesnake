#ifndef __TUNJI_LIBRARY_CRASH_DUMP__
#define __TUNJI_LIBRARY_CRASH_DUMP__

#include <stdio.h>
#include <signal.h>
#include <windows.h>
#include <crtdbg.h>
#include <Psapi.h>
#include <DbgHelp.h>
//#include "APIHook.h"
#pragma comment(lib, "dbghelp.lib")
#define MAXPATH 256

namespace TUNJI_LIBRARY
{
	class cCrashDump
	{
	public:
		cCrashDump()
		{
			_invalid_parameter_handler	oldHandler, newHandler;

			newHandler = myInvalidParameterHandler;
			oldHandler = _set_invalid_parameter_handler(newHandler);
			_CrtSetReportMode(_CRT_WARN, 0);
			_CrtSetReportMode(_CRT_ASSERT, 0);
			_CrtSetReportMode(_CRT_ERROR, 0);

			_CrtSetReportHook(_custom_Report_hook);

			_set_purecall_handler(myPureCallHandler);

			_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
			signal(SIGABRT, signalHandler);
			signal(SIGINT, signalHandler);
			signal(SIGILL, signalHandler);
			signal(SIGFPE, signalHandler);
			signal(SIGSEGV, signalHandler);
			signal(SIGTERM, signalHandler);

			SetHandlerDump();
		}


		static void Crash(void)
		{
			int *p = nullptr;

			*p = 0;
		}

		static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
		{
			int iWorkingMemory = 0;
			SYSTEMTIME stNowTime;

			long dumpCount = InterlockedIncrement(&_dumpCount);

			//	현재 프로세스의 메모리 사용량을 얻어온다. 
			HANDLE hProcess = 0;
			PROCESS_MEMORY_COUNTERS pmc;

			hProcess = GetCurrentProcess();
			if (NULL == hProcess)
			{
				return 0;
			}

			if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
			{
				iWorkingMemory = (int)(pmc.WorkingSetSize / 1024 / 1024);
			}
			CloseHandle(hProcess);

			//	현재 날짜와 시간을 얻어온다. 
			WCHAR fileName[MAXPATH];
			GetLocalTime(&stNowTime);
			wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d.%02d_%d.%dMB.dmp", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
				stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond,
				dumpCount, iWorkingMemory);
			wprintf(L"\n\n\n CRASH ERROR !!!  %d.%d.%d / %d:%d:%d \n", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
				stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);

			wprintf(L"Dump file saved...\n");

			HANDLE hDumpFile = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hDumpFile != INVALID_HANDLE_VALUE)
			{
				_MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInformation;
				miniDumpExceptionInformation.ClientPointers = TRUE;
				miniDumpExceptionInformation.ExceptionPointers = pExceptionPointer;
				miniDumpExceptionInformation.ThreadId = ::GetCurrentThreadId();

				MiniDumpWriteDump(GetCurrentProcess(),
					GetCurrentProcessId(),
					hDumpFile,
					MiniDumpWithFullMemory,
					&miniDumpExceptionInformation,
					NULL,
					NULL);

				CloseHandle(hDumpFile);
				wprintf(L"saving dump complete !\n");
			}
			return EXCEPTION_EXECUTE_HANDLER;
		}

		static LONG WINAPI RedirectedSetUnhandledExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
		{
			MyExceptionFilter(exceptionInfo);
			return EXCEPTION_EXECUTE_HANDLER;
		}

		static void SetHandlerDump(void)
		{
			SetUnhandledExceptionFilter(MyExceptionFilter);

			//	C 런타임 라이브러리 내부의 예외 핸들러 등록을 막기 위해서 API 후킹
			//static CAPIHook apiHook("kernel32.dll", "SetUnhandledExceptionFilter", (PROC)RedirectedSetUnhandledExceptionFilter, true);
		}

		static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
		{
			Crash();
		}

		static int _custom_Report_hook(int ireposttype, char *message, int *returnvalue)
		{
			Crash();
			return true;
		}

		static void myPureCallHandler(void)
		{
			Crash();
		}

		static void signalHandler(int Error)
		{
			Crash();
		}
		static long _dumpCount;
	};
};

#endif