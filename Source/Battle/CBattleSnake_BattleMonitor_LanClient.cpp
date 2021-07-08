#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"
#include "CLanClient.h"
#include "CBattleSnake_BattleMonitor_LanClient.h"

#include <time.h>
#include <process.h>

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

CBattleSnake_BattleMonitor_LanClient::CBattleSnake_BattleMonitor_LanClient(CBattleSnake_Battle_MMOServer *pThis)
{
	_serverNo = dfBS_MONITOR_BATTLE_ID;
	_pBattleServer = pThis;
}

void CBattleSnake_BattleMonitor_LanClient::StartMonitorClient(void)
{
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}

void CBattleSnake_BattleMonitor_LanClient::StopMonitorClient(void)
{
	if (_bExitMonitor == true) return;
	_bExitMonitor = true;
	DWORD status = WaitForSingleObject(_hMonitorThread, INFINITE);
	if (status == WAIT_OBJECT_0)
	{
		wprintf(L"MONITOR EXITS SUCCEED! \n");
	}

	CloseHandle(_hMonitorThread);
}

void CBattleSnake_BattleMonitor_LanClient::OnEnterJoinServer(void) 
{
	StartMonitorClient();
}
void CBattleSnake_BattleMonitor_LanClient::OnLeaveServer(void) 
{
	StopMonitorClient();
}
void CBattleSnake_BattleMonitor_LanClient::OnRecv(cLanPacketPool *dsPacket) {}
void CBattleSnake_BattleMonitor_LanClient::OnSend(int sendSize) {}

void CBattleSnake_BattleMonitor_LanClient::OnError(int errorCode, WCHAR *errText)
{
	errno_t err;
	FILE *fp;
	time_t timer;
	struct tm t;
	WCHAR buf[256] = { 0, };
	WCHAR logBuf[256] = { 0, };
	wsprintf(logBuf, errText);

	time(&timer);
	localtime_s(&t, &timer);
	wsprintf(buf, L"GameMonitorLanClient_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

unsigned int CBattleSnake_BattleMonitor_LanClient::MonitorThread(void *arg)
{
	CBattleSnake_BattleMonitor_LanClient *pThis = (CBattleSnake_BattleMonitor_LanClient*)arg;
	return pThis->Update_MonitorThread();
}

unsigned int CBattleSnake_BattleMonitor_LanClient::Update_MonitorThread(void)
{
	bool *bExit = &_bExitMonitor;
	cLanPacketPool::AllocPacketChunk();

	RequestLogin();
	timeBeginPeriod(1);
	while (*bExit == false)
	{
		Send_HardwareCPU();
		Send_HardwareAvailableMemory();
		Send_HardwareRecvBytes();
		Send_HardwareSendBytes();
		Send_HardwareNonpagedMemory();

		Send_ServerStatus();
		Send_ServerCPU();
		Send_ServerMemory();
		Send_ServerPacketPool();
		Send_ServerAuthFPS();
		Send_ServerGameFPS();
		Send_ServerSessionAll();
		Send_ServerSessionAuth();
		Send_ServerSessionGame();
		Send_ServerWaitRoom();
		Send_ServerPlayRoom();
		Sleep(1000);
	};
	timeEndPeriod(1);
	cLanPacketPool::FreePacketChunk();

	return 0;
}

void CBattleSnake_BattleMonitor_LanClient::RequestLogin(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();

	WORD type = en_PACKET_SS_MONITOR_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&_serverNo, sizeof(int));

	SendPacket(sPacket);
}

void CBattleSnake_BattleMonitor_LanClient::Send_HardwareCPU(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_HardwareCpuUsage;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_HardwareCPU(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_HardwareAvailableMemory(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_HardwareAvailableMBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_HardwareAvailableMemory(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_HardwareRecvBytes(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_HardwareRecvKBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_HardwareRecvBytes(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_HardwareSendBytes(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_HardwareSendKBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_HardwareSendBytes(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_HardwareNonpagedMemory(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_HardwareNonpagedMBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_HardwareNonpagedMemory(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);

}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerStatus(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->IsServerOn();
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerStatus(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_BattleMonitor_LanClient::Send_ServerCPU(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_ProcessCpuUsage;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerCPU(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_BattleMonitor_LanClient::Send_ServerMemory(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_PrivateBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMemory(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerPacketPool(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_packetPool;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPacketPool(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerAuthFPS(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_AuthUpdateFPS;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerAuthFPS(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerGameFPS(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_GameUpdateFPS;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerGameFPS(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerSessionAll(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->getSessionCount();
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerSessionAll(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerSessionAuth(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_sessionMode_Auth;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerSessionAuth(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);

}
void CBattleSnake_BattleMonitor_LanClient::Send_ServerSessionGame(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_sessionMode_Game;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerSessionGame(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_BattleMonitor_LanClient::Send_ServerWaitRoom(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_battleWaitRoom;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerWaitRoom(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_BattleMonitor_LanClient::Send_ServerPlayRoom(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pBattleServer->_monitor_battlePlayRoom;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPlayRoom(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_BattleMonitor_LanClient::Packet_HardwareCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_CPU_TOTAL;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}

void CBattleSnake_BattleMonitor_LanClient::Packet_HardwareAvailableMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_AVAILABLE_MEMORY;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}

void CBattleSnake_BattleMonitor_LanClient::Packet_HardwareRecvBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_NETWORK_RECV;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_HardwareSendBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_NETWORK_SEND;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));

}
void CBattleSnake_BattleMonitor_LanClient::Packet_HardwareNonpagedMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_NONPAGED_MEMORY;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}

void CBattleSnake_BattleMonitor_LanClient::Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_SERVER_ON;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}

void CBattleSnake_BattleMonitor_LanClient::Packet_ServerCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_CPU;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}

void CBattleSnake_BattleMonitor_LanClient::Packet_ServerMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_MEMORY_COMMIT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_PACKET_POOL;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerAuthFPS(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_AUTH_FPS;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerGameFPS(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_GAME_FPS;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerSessionAll(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_SESSION_ALL;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerSessionAuth(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_SESSION_AUTH;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerSessionGame(cLanPacketPool * sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_SESSION_GAME;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}

void CBattleSnake_BattleMonitor_LanClient::Packet_ServerWaitRoom(cLanPacketPool * sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_ROOM_WAIT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_BattleMonitor_LanClient::Packet_ServerPlayRoom(cLanPacketPool * sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_BATTLE_ROOM_PLAY;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}