#include <time.h>
#include <Windows.h>

#include "cHttp_Async.h"
#include "CNetServer.h"

#include "CBattleSnake_Match_Http.h"
#include "CBattleSnake_Match_NetServer.h"

#include "CBattleSnake_Match_LocalMonitor.h"

void CBattleSnake_Match_LocalMonitor::SetMonitorFactor(void *pThis, Monitor_Factor factor)
{
	switch (factor)
	{
	case Factor_Match_Http:
		_pMatchHttpClient = (CBattleSnake_Match_Http *)pThis;
		break;
	case Factor_Match_NetServer:
		_pMatchNetServer = (CBattleSnake_Match_NetServer*)pThis;
		break;
	};
}

void CBattleSnake_Match_LocalMonitor::MonitorMatchServer(void)
{
	static DWORD prevTime = GetTickCount();
	DWORD curTime = GetTickCount();
	DWORD interval = curTime - prevTime;

	time_t timer;
	tm t;

	time(&timer);
	localtime_s(&t, &timer);

	if (interval >= 1000)
	{

		int usedLanBlock = cLanPacketPool::GetUsedPoolSize();
		int totalLanBlock = cLanPacketPool::GetTolalPoolSize();

		int usedNetBlock = cNetPacketPool::GetUsedPoolSize();
		int totalNetBlock = cNetPacketPool::GetTolalPoolSize();

		wprintf(L"[%04d-%02d-%02d %02d:%02d:%02d] MATCH SERVER\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		wprintf(L"==================================================================\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"## MATCH NETWORK SERVER\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"	PROCESS CPU	:		[%8.2lf]\n", _pMatchNetServer->_monitor_process_cpu);
		wprintf(L"	COMMIT MBYTE	:		[%8.2lf]\n\n", _pMatchNetServer->_monitor_commit_MBytes);

		wprintf(L"	SEND TPS	:		[%8lld]\n", _pMatchNetServer->_monitor_sendTPS);
		wprintf(L"	RECV TPS	:		[%8lld]\n", _pMatchNetServer->_monitor_recvTPS);
		wprintf(L"	ACCEPT TPS	:		[%8lld]\n", _pMatchNetServer->_monitor_acceptTPS);
		wprintf(L"	ACCEPT TOTAL	:		[%8lld]\n", _pMatchNetServer->_monitor_totalAccept);
		wprintf(L"	ACCEPT POOL	:		[%8lld]\n", _pMatchNetServer->_monitor_AcceptSocketPool);
		wprintf(L"	ACCEPT QUEUE	:		[%8lld]\n\n", _pMatchNetServer->_monitor_AcceptSocketQueue);

		wprintf(L"	NET PACKET USE	:		[%8d]\n", usedNetBlock);
		wprintf(L"	NET PACKET TOTAL:		[%8d]\n", totalNetBlock);
		wprintf(L"	LAN PACKET USE	:		[%8d]\n", usedLanBlock);
		wprintf(L"	LAN PACKET TOTAL:		[%8d]\n\n", totalLanBlock);

		wprintf(L"	MATCH SESSION	:		[%8lld]\n", _pMatchNetServer->_monitor_session);
		wprintf(L"	MATCH LOGIN	:		[%8lld]\n", _pMatchNetServer->_monitor_login);
		wprintf(L"	MATCH TPS	:		[%8lld]\n\n", _pMatchNetServer->_monitor_matchTPS);

		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"## HTTP CLIENT\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"	SEND TPS	:		[%8lld]\n", _pMatchHttpClient->_monitor_sendTPS);
		wprintf(L"	RECV TPS	:		[%8lld]\n", _pMatchHttpClient->_monitor_recvTPS);
		wprintf(L"	SESSION		:		[%8lld]\n", _pMatchHttpClient->GetSessionCount());
		wprintf(L"	CONN TPS	:		[%8lld]\n", _pMatchHttpClient->_monitor_connectTPS);
		wprintf(L"	CONN TOTAL	:		[%8lld]\n\n", _pMatchHttpClient->_monitor_totalConnect);
		wprintf(L"	MSG QUEUE	:		[%8lld]\n", _pMatchHttpClient->_monitor_msgQueue);
		wprintf(L"	MSG POOL	:		[%8lld]\n", _pMatchHttpClient->_monitor_msgPool);
		wprintf(L"	INFO USER	:		[%8lld]\n", _pMatchHttpClient->_monitor_Info);
		wprintf(L"	INFO POOL	:		[%8lld]\n", _pMatchHttpClient->_monitor_InfoPool);
		wprintf(L"	REQ FAIL	:		[%8lld]\n", _pMatchHttpClient->_monitor_requestFail);
		wprintf(L"==================================================================\n");

		prevTime = GetTickCount();
	}
}