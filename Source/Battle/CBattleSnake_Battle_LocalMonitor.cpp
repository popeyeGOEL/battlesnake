#include <time.h>

#include "cHttp_Async.h"
#include "CBattleSnake_Battle_Http.h"

#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"

#include "CLanServer.h"
#include "CBattleSnake_Battle_LanServer.h"

#include "CBattleSnake_Battle_LocalMonitor.h"

void CBattleSnake_Battle_LocalMonitor::SetMonitorFactor(void *pThis, Monitor_Factor factor)
{
	switch (factor)
	{
	case Factor_Battle_Http:
		_pHttpClient = (CBattleSnake_Battle_Http*)pThis;
		break;
	case Factor_Battle_LanServer:
		_pChatLanServer = (CBattleSnake_Battle_LanServer*)pThis;
		break;
	case Factor_Battle_MMOServer:
		_pBattleServer = (CBattleSnake_Battle_MMOServer*)pThis;
		break;
	};
}

void CBattleSnake_Battle_LocalMonitor::MonitorBattleServer(void)
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

		wprintf(L"[%04d-%02d-%02d %02d:%02d:%02d] BATTLE SERVER\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		wprintf(L"==================================================================\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"## MMO SERVER\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"	PROCESS CPU	:		[%8.2lf]\n", _pBattleServer->_monitor_ProcessCpuUsage);
		wprintf(L"	COMMIT MBYTE	:		[%8.2lf]\n\n", _pBattleServer->_monitor_PrivateBytes);

		wprintf(L"	SEND TPS	:		[%8lld]\n", _pBattleServer->_monitor_PacketSendTPS);
		wprintf(L"	GAME PROC TPS	:		[%8lld]\n", _pBattleServer->_monitor_GamePacketProcTPS);
		wprintf(L"	AUTH PROC TPS	:		[%8lld]\n", _pBattleServer->_monitor_AuthPacketProcTPS);
		wprintf(L"	ACCEPT TPS	:		[%8lld]\n", _pBattleServer->_monitor_AcceptTPS);
		wprintf(L"	ACCEPT POOL	:		[%8lld]\n", _pBattleServer->_monitor_AcceptSocketPool);
		wprintf(L"	ACCEPT QUEUE	:		[%8lld]\n\n", _pBattleServer->_monitor_AcceptSocketQueue);

		wprintf(L"	AUTH UPDATE FPS	:		[%8lld]\n", _pBattleServer->_monitor_AuthUpdateFPS);
		wprintf(L"	GAME UPDATE FPS	:		[%8lld]\n", _pBattleServer->_monitor_GameUpdateFPS);

		wprintf(L"	MODE_AUTH	:		[%8lld]\n", _pBattleServer->_monitor_sessionMode_Auth);
		wprintf(L"	MODE_GAME	:		[%8lld]\n\n", _pBattleServer->_monitor_sessionMode_Game);

		wprintf(L"	NET PACKET USE	:		[%8d]\n", usedNetBlock);
		wprintf(L"	NET PACKET TOTAL:		[%8d]\n", totalNetBlock);

		wprintf(L"	LAN PACKET USE	:		[%8d]\n", usedLanBlock);
		wprintf(L"	LAN PACKET TOTAL:		[%8d]\n\n", totalLanBlock);

		wprintf(L"	ACCEPT TOTAL	:		[%8lld]\n", _pBattleServer->_monitor_AcceptSocket);
		wprintf(L"	SESSION		:		[%8lld]\n", _pBattleServer->getSessionCount());
		wprintf(L"	LOGIN USER	:		[%8lld]\n", _pBattleServer->_monitor_loginPlayer);

		wprintf(L"	WAIT ROOM	:		[%8lld]\n", _pBattleServer->_monitor_battleWaitRoom);
		wprintf(L"	PLAY ROOM	:		[%8lld]\n\n", _pBattleServer->_monitor_battlePlayRoom);

		wprintf(L"	MED USED	:		[%8lld]\n", _pBattleServer->_monitor_medKitUsed);
		wprintf(L"	MED POOL	:		[%8lld]\n\n", _pBattleServer->_monitor_medKitPool);

		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"## CHAT LAN SERVER\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"	CHAT ROOM	:		[%8lld]\n\n", _pChatLanServer->_curOpenRoom);

		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"## HTTP CLIENT\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"	SEND TPS	:		[%8lld]\n", _pHttpClient->_monitor_sendTPS);
		wprintf(L"	RECV TPS	:		[%8lld]\n", _pHttpClient->_monitor_recvTPS);
		wprintf(L"	SESSION		:		[%8lld]\n", _pHttpClient->GetSessionCount());
		wprintf(L"	CONN TPS	:		[%8lld]\n", _pHttpClient->_monitor_connectTPS);
		wprintf(L"	CONN TOTAL	:		[%8lld]\n\n", _pHttpClient->_monitor_totalConnect);
		wprintf(L"	MSG QUEUE	:		[%8lld]\n", _pHttpClient->_monitor_msgQueue);
		wprintf(L"	MSG POOL	:		[%8lld]\n", _pHttpClient->_monitor_msgPool);
		wprintf(L"	INFO USER	:		[%8lld]\n", _pHttpClient->_monitor_Info);
		wprintf(L"	INFO POOL	:		[%8lld]\n", _pHttpClient->_monitor_InfoPool);
		wprintf(L"	REQ FAIL	:		[%8lld]\n", _pHttpClient->_monitor_requestFail);
		wprintf(L"==================================================================\n");

		prevTime = GetTickCount();
	}
}