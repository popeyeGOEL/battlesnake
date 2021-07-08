#include <time.h>
#include <Windows.h>

#include "CLanClient.h"
#include "CLanServer.h"
#include "CBattleSnake_MasterMonitor_LanClient.h"
#include "CBattleSnake_Master_LanServer.h"
#include "CBattleSnake_Master_LocalMonitor.h"

void CBattleSnake_Master_LocalMonitor::SetMonitorFactor(CBattleSnake_Master_LanServer* pThis)
{
	_pMasterLanServer = pThis;
}

void CBattleSnake_Master_LocalMonitor::MonitorMasterServer(void)
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

		wprintf(L"[%04d-%02d-%02d %02d:%02d:%02d] MASTER SERVER\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		wprintf(L"==================================================================\n");
		wprintf(L"	UNIT CPU	:		[%8.2lf]\n", _pMasterLanServer->_monitor_unit_cpu);
		wprintf(L"	PROCESS CPU	:		[%8.2lf]\n", _pMasterLanServer->_monitor_process_cpu);
		wprintf(L"	COMMIT BYTE	:		[%8.2llf]\n\n", _pMasterLanServer->_monitor_privateBytes);

		wprintf(L"	SEND TPS	:		[%8lld]\n", _pMasterLanServer->_monitor_sendTPS);
		wprintf(L"	RECV TPS	:		[%8lld]\n", _pMasterLanServer->_monitor_recvTPS);
		wprintf(L"	ACCEPT TPS	:		[%8lld]\n\n", _pMasterLanServer->_monitor_acceptTPS);
		wprintf(L"	ACCEPT TOTAL	:		[%8lld]\n", _pMasterLanServer->_monitor_totalAccept);

		wprintf(L"	PACKET USE	:		[%8lld]\n", usedLanBlock);
		wprintf(L"	PACKET TOTAL	:		[%8lld]\n\n", totalLanBlock);

		wprintf(L"	ROOM USED	:		[%8ld]\n", _pMasterLanServer->_monitor_waitRoomUsed);
		wprintf(L"	ROOM POOL	:		[%8ld]\n", _pMasterLanServer->_monitor_waitRoomPool);

		wprintf(L"	SESSION USED	:		[%8ld]\n", _pMasterLanServer->_monitor_waitSessionUsed);
		wprintf(L"	SESSION POOL	:		[%8ld]\n", _pMasterLanServer->_monitor_waitSessionPool);

		wprintf(L"	BATTLE LOGIN	:		[%8ld]\n", _pMasterLanServer->_monitor_battleServerLogin);
		wprintf(L"	BATTLE CONNECT	:		[%8ld]\n", _pMasterLanServer->_monitor_battleServerConnection);

		wprintf(L"	MATCH LOGIN	:		[%8ld]\n", _pMasterLanServer->_monitor_matchServerLogin);
		wprintf(L"	MATCH CONNECT	:		[%8ld]\n\n", _pMasterLanServer->_monitor_matchServerConnection);
		wprintf(L"==================================================================\n");
		prevTime = GetTickCount();
	}
}