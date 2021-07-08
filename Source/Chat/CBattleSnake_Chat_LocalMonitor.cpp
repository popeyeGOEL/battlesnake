#include "CNetServer.h"
#include "CBattleSnake_Chat_NetServer.h"

#include "CBattleSnake_Chat_LocalMonitor.h"

#include <time.h>

void CBattleSnake_Chat_LocalMonitor::SetMonitorFactor(void *pThis)
{
	_pChatServer = (CBattleSnake_Chat_NetServer*)pThis;
}

void CBattleSnake_Chat_LocalMonitor::MonitorChatServer(void)
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

		wprintf(L"[%04d-%02d-%02d %02d:%02d:%02d] CHAT SERVER\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		wprintf(L"==================================================================\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"## CHAT SERVER\n");
		wprintf(L"------------------------------------------------------------------\n");
		wprintf(L"	PROCESS CPU	:		[%8.2lf]\n", _pChatServer->_monitor_processCpuUsage);
		wprintf(L"	COMMIT MBYTE	:		[%8.2lf]\n\n", _pChatServer->_monitor_privateBytes);

		wprintf(L"	SEND TPS	:		[%8lld]\n", _pChatServer->_monitor_sendTPS);
		wprintf(L"	RECV TPS	:		[%8lld]\n", _pChatServer->_monitor_recvTPS);
		wprintf(L"	ACCEPT TPS	:		[%8lld]\n", _pChatServer->_monitor_acceptTPS);
		wprintf(L"	ACCEPT POOL	:		[%8lld]\n", _pChatServer->_monitor_AcceptSocketPool);
		wprintf(L"	ACCEPT QUEUE	:		[%8lld]\n\n", _pChatServer->_monitor_AcceptSocketQueue);

		wprintf(L"	NET PACKET USE	:		[%8d]\n", usedNetBlock);
		wprintf(L"	NET PACKET TOTAL:		[%8d]\n", totalNetBlock);

		wprintf(L"	LAN PACKET USE	:		[%8d]\n", usedLanBlock);
		wprintf(L"	LAN PACKET TOTAL:		[%8d]\n\n", totalLanBlock);

		wprintf(L"	ACCEPT TOTAL	:		[%8lld]\n", _pChatServer->_monitor_totalAccept);
		wprintf(L"	SESSION		:		[%8lld]\n\n", _pChatServer->getSessionCount());
		wprintf(L"	PLAYER USED	:		[%8lld]\n", _pChatServer->_monitor_playerUsed);
		wprintf(L"	PLAYER POOL	:		[%8lld]\n\n", _pChatServer->_monitor_playerPool);
		wprintf(L"	ROOM USED	:		[%8lld]\n", _pChatServer->_monitor_roomUsed);
		wprintf(L"	ROOM POOL	:		[%8lld]\n\n", _pChatServer->_monitor_roomPool);
		wprintf(L"	MESSAGE USED	:		[%8lld]\n", _pChatServer->_monitor_msgUsed);
		wprintf(L"	MESSAGE POOL	:		[%8lld]\n\n", _pChatServer->_monitor_msgPool);

		wprintf(L"	UPDATE FPS	:		[%8lld]\n", _pChatServer->_monitor_updateFPS);
		wprintf(L"==================================================================\n");

		prevTime = GetTickCount();
	}
}