#include <time.h>
#include <process.h>
#include <Windows.h>

#include "CommonProtocol.h"

#include "CLanServer.h"
#include "CBattleSnake_Master_LanServer.h"
#include "CLanClient.h"
#include "CBattleSnake_MasterMonitor_LanClient.h"

#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

CBattleSnake_MasterMonitor_LanClient::CBattleSnake_MasterMonitor_LanClient(CBattleSnake_Master_LanServer *pThis)
{
	_serverNo = dfBS_MONITOR_MASTER_ID;
	_pMasterServer = pThis;
}

void CBattleSnake_MasterMonitor_LanClient::StartMonitorClient(void)
{
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}

void CBattleSnake_MasterMonitor_LanClient::StopMonitorClient(void)
{
	if (_bExitMonitor == true) return;
	_bExitMonitor = true;
	DWORD status = WaitForSingleObject(_hMonitorThread, INFINITE);
	if (status == WAIT_OBJECT_0)
	{
		wprintf(L"LanClient monitor thread termniated! \n");
	}
	CloseHandle(_hMonitorThread);
}

void CBattleSnake_MasterMonitor_LanClient::OnEnterJoinServer(void)
{
	StartMonitorClient();
}

void CBattleSnake_MasterMonitor_LanClient::OnLeaveServer(void)
{
	StopMonitorClient();
}
void CBattleSnake_MasterMonitor_LanClient::OnRecv(cLanPacketPool *dsPacket)
{
	//	사용안함 
}
void CBattleSnake_MasterMonitor_LanClient::OnSend(int sendSize)
{
	//	사용안함 
}

void CBattleSnake_MasterMonitor_LanClient::OnError(int errorCode, WCHAR *errText)
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
	wsprintf(buf, L"CBattleSnake_Monitor_MasterLanClient_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

unsigned int CBattleSnake_MasterMonitor_LanClient::MonitorThread(void *arg)
{
	CBattleSnake_MasterMonitor_LanClient *pThis = (CBattleSnake_MasterMonitor_LanClient*)arg;
	return pThis->Update_MonitorThread();
}

unsigned int CBattleSnake_MasterMonitor_LanClient::Update_MonitorThread(void)
{
	bool *bExit = &_bExitMonitor;
	cLanPacketPool::AllocPacketChunk();

	RequestLogin();

	timeBeginPeriod(1);
	while (*bExit == false)
	{
		Send_ServerStatus();
		Send_ServerProcessCPU();
		Send_ServerUnitCPU();
		Send_ServerMemory();
		Send_ServerPacketPool();
		Send_ServerMatchConnect();
		Send_ServerMatchLogin();
		Send_ServerWaitClient();
		Send_ServerBattleConnect();
		Send_ServerBattleLogin();
		Send_ServerWaitRoom();
		Sleep(1000);
	}
	timeEndPeriod(1);
	cLanPacketPool::FreePacketChunk();

	return 0;
}

void CBattleSnake_MasterMonitor_LanClient::RequestLogin(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	WORD type = en_PACKET_SS_MONITOR_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&_serverNo, sizeof(int));

	SendPacket(sPacket);
}

void CBattleSnake_MasterMonitor_LanClient::Send_ServerStatus(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->IsServerOn();
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerStatus(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_MasterMonitor_LanClient::Send_ServerProcessCPU(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_process_cpu;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerProcessCPU(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerUnitCPU(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_unit_cpu;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerUnitCPU(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerMemory(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_privateBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMemory(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerPacketPool(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_packetPool;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPacketPool(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerMatchConnect(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_matchServerConnection;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMatchConnect(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerMatchLogin(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_matchServerLogin;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMatchLogin(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerWaitClient(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_waitSessionUsed;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerWaitClient(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerBattleConnect(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_battleServerConnection;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerBattleConnect(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerBattleLogin(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_battleServerLogin;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerBattleLogin(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MasterMonitor_LanClient::Send_ServerWaitRoom(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMasterServer->_monitor_waitRoomUsed;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerWaitRoom(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_MasterMonitor_LanClient::Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_SERVER_ON;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerProcessCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_CPU;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerUnitCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_CPU_SERVER;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_MEMORY_COMMIT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_PACKET_POOL;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerMatchConnect(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_MATCH_CONNECT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerMatchLogin(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_MATCH_LOGIN;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerWaitClient(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_STAY_CLIENT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerBattleConnect(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_BATTLE_CONNECT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerBattleLogin(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_BATTLE_LOGIN;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MasterMonitor_LanClient::Packet_ServerWaitRoom(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MASTER_BATTLE_STANDBY_ROOM;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}