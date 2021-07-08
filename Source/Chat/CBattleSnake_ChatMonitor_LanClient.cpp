#include "CNetServer.h"
#include "CBattleSnake_Chat_NetServer.h"

#include "CLanClient.h"
#include "CBattleSnake_ChatMonitor_LanClient.h"

#include <time.h>
#include <process.h>

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

CBattleSnake_ChatMonitor_LanClient::CBattleSnake_ChatMonitor_LanClient(CBattleSnake_Chat_NetServer* pThis)
{
	_serverNo = dfBS_MONITOR_CHAT_ID;
	_pChatServer = pThis;
}

void CBattleSnake_ChatMonitor_LanClient::StartMonitorClient(void)
{
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_ChatMonitor_LanClient::StopMonitorClient(void)
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

void CBattleSnake_ChatMonitor_LanClient::OnEnterJoinServer(void)
{
	StartMonitorClient();
}
void CBattleSnake_ChatMonitor_LanClient::OnLeaveServer(void)
{
	StopMonitorClient();
}

unsigned int CBattleSnake_ChatMonitor_LanClient::MonitorThread(void *arg)
{
	CBattleSnake_ChatMonitor_LanClient *pThis = (CBattleSnake_ChatMonitor_LanClient*)arg;
	return pThis->Update_MonitorThread();
}
unsigned int CBattleSnake_ChatMonitor_LanClient::Update_MonitorThread(void)
{
	bool *bExit = &_bExitMonitor;
	cLanPacketPool::AllocPacketChunk();

	RequestLogin();
	timeBeginPeriod(1);
	while (*bExit == false)
	{
		Send_ServerStatus();
		Send_ServerCPU();
		Send_ServerMemory();
		Send_ServerPacketPool();
		Send_ServerSession();
		Send_ServerPlayer();
		Send_ServerChatRoom();
		Sleep(1000);
	};
	timeEndPeriod(1);
	cLanPacketPool::FreePacketChunk();

	return 0;
}

void CBattleSnake_ChatMonitor_LanClient::RequestLogin(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();

	WORD type = en_PACKET_SS_MONITOR_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&_serverNo, sizeof(int));

	SendPacket(sPacket);
}

void CBattleSnake_ChatMonitor_LanClient::Send_ServerStatus(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->IsServerOn();
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerStatus(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_ChatMonitor_LanClient::Send_ServerCPU(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->_monitor_processCpuUsage;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerProcessCPU(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_ChatMonitor_LanClient::Send_ServerMemory(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->_monitor_privateBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMemory(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_ChatMonitor_LanClient::Send_ServerPacketPool(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->_monitor_packetPool;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPacketPool(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_ChatMonitor_LanClient::Send_ServerSession(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->getSessionCount();
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerSession(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_ChatMonitor_LanClient::Send_ServerPlayer(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->_monitor_playerUsed;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPlayer(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_ChatMonitor_LanClient::Send_ServerChatRoom(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pChatServer->_monitor_roomUsed;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerChatRoom(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_ChatMonitor_LanClient::Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_SERVER_ON;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_ChatMonitor_LanClient::Packet_ServerProcessCPU(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_CPU;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_ChatMonitor_LanClient::Packet_ServerMemory(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_MEMORY_COMMIT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_ChatMonitor_LanClient::Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_ChatMonitor_LanClient::Packet_ServerSession(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_SESSION;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_ChatMonitor_LanClient::Packet_ServerPlayer(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_PLAYER;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_ChatMonitor_LanClient::Packet_ServerChatRoom(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_CHAT_ROOM;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}