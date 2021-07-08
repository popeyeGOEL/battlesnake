#include <time.h>
#include <process.h>
#include <Windows.h>

#include "CNetServer.h"
#include "CBattleSnake_Match_NetServer.h"
#include "CLanClient.h"
#include "CBattleSnake_MatchMonitor_LanClient.h"

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"


CBattleSnake_MatchMonitor_LanClient::CBattleSnake_MatchMonitor_LanClient(CBattleSnake_Match_NetServer* pThis)
{
	_serverNo = dfBS_MONTIOR_MATCH_ID;
	_pMatchNetServer = pThis;
}

void CBattleSnake_MatchMonitor_LanClient::StartMonitorClient(void)
{
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_MatchMonitor_LanClient::StopMonitorClient(void)
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

void CBattleSnake_MatchMonitor_LanClient::OnEnterJoinServer(void) 
{
	StartMonitorClient();
}
void CBattleSnake_MatchMonitor_LanClient::OnLeaveServer(void)
{
	StopMonitorClient();
}
void CBattleSnake_MatchMonitor_LanClient::OnRecv(cLanPacketPool *dsPacket){}
void CBattleSnake_MatchMonitor_LanClient::OnSend(int sendSize){}
void CBattleSnake_MatchMonitor_LanClient::OnError(int errorCode, WCHAR *errText)
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
	wsprintf(buf, L"CBattleSnake_Monitor_MatchLanClient_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}


unsigned int CBattleSnake_MatchMonitor_LanClient::MonitorThread(void *arg)
{
	CBattleSnake_MatchMonitor_LanClient *pThis = (CBattleSnake_MatchMonitor_LanClient*)arg;
	return pThis->Update_MonitorThread();
}
unsigned int CBattleSnake_MatchMonitor_LanClient::Update_MonitorThread(void)
{
	bool *bExit = &_bExitMonitor;
	cLanPacketPool::AllocPacketChunk();

	RequestLogin();

	timeBeginPeriod(1);
	while (*bExit == false)
	{
		//Send_HardwareCPU();
		//Send_HardwareAvailableMemory();
		//Send_HardwareRecvBytes();
		//Send_HardwareSendBytes();
		//Send_HardwareNonpagedMemory();

		Send_ServerStatus();
		Send_ServerProcessCPU();
		Send_ServerMemory();
		Send_ServerPacketPool();
		Send_ServerSession();
		Send_ServerPlayer();
		Send_ServerMatchTPS();
		Sleep(1000);
	}
	timeEndPeriod(1);
	cLanPacketPool::FreePacketChunk();

	return 0;
}

void CBattleSnake_MatchMonitor_LanClient::RequestLogin(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	WORD type = en_PACKET_SS_MONITOR_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&_serverNo, sizeof(int));

	SendPacket(sPacket);
}

//void CBattleSnake_MatchMonitor_LanClient::Send_HardwareCPU(void)
//{
//	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
//	int dataValue = (int)_pMatchNetServer->_monitor_HardwareCpuUsage;
//	time_t timer;
//	int timeStamp;
//	time(&timer);
//	timeStamp = (int)timer;
//
//	Packet_HardwareCPU(sPacket, dataValue, timeStamp);
//	SendPacket(sPacket);
//}
//void CBattleSnake_MatchMonitor_LanClient::Send_HardwareAvailableMemory(void)
//{
//	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
//	int dataValue = (int)_pMatchNetServer->_monitor_HardwareAvailableMBytes;
//	time_t timer;
//	int timeStamp;
//	time(&timer);
//	timeStamp = (int)timer;
//
//	Packet_HardwareAvailableMemory(sPacket, dataValue, timeStamp);
//	SendPacket(sPacket);
//}
//void CBattleSnake_MatchMonitor_LanClient::Send_HardwareRecvBytes(void)
//{
//	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
//	int dataValue = (int)_pMatchNetServer->_monitor_HardwareRecvKBytes;
//	time_t timer;
//	int timeStamp;
//	time(&timer);
//	timeStamp = (int)timer;
//
//	Packet_HardwareRecvBytes(sPacket, dataValue, timeStamp);
//	SendPacket(sPacket);
//}
//void CBattleSnake_MatchMonitor_LanClient::Send_HardwareSendBytes(void)
//{
//	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
//	int dataValue = (int)_pMatchNetServer->_monitor_HardwareSendKBytes;
//	time_t timer;
//	int timeStamp;
//	time(&timer);
//	timeStamp = (int)timer;
//
//	Packet_HardwareSendBytes(sPacket, dataValue, timeStamp);
//	SendPacket(sPacket);
//}
//void CBattleSnake_MatchMonitor_LanClient::Send_HardwareNonpagedMemory(void)
//{
//	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
//	int dataValue = (int)_pMatchNetServer->_monitor_HardwareNonpagedMBytes;
//	time_t timer;
//	int timeStamp;
//	time(&timer);
//	timeStamp = (int)timer;
//
//	Packet_HardwareNonpagedMemory(sPacket, dataValue, timeStamp);
//	SendPacket(sPacket);
//
//}

void CBattleSnake_MatchMonitor_LanClient::Send_ServerStatus(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->IsServerOn();
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerStatus(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_MatchMonitor_LanClient::Send_ServerProcessCPU(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->_monitor_process_cpu;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerProcessCPU(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MatchMonitor_LanClient::Send_ServerMemory(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->_monitor_commit_MBytes;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMemory(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_MatchMonitor_LanClient::Send_ServerPacketPool(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->_monitor_packetPool;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPacketPool(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

void CBattleSnake_MatchMonitor_LanClient::Send_ServerSession(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->_monitor_session;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerSession(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MatchMonitor_LanClient::Send_ServerPlayer(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->_monitor_login;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerPlayer(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}
void CBattleSnake_MatchMonitor_LanClient::Send_ServerMatchTPS(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	int dataValue = (int)_pMatchNetServer->_monitor_matchTPS;
	time_t timer;
	int timeStamp;
	time(&timer);
	timeStamp = (int)timer;

	Packet_ServerMatchTPS(sPacket, dataValue, timeStamp);
	SendPacket(sPacket);
}

//void CBattleSnake_MatchMonitor_LanClient::Packet_HardwareCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp)
//{
//	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
//	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_CPU_TOTAL;
//
//	sPacket->Serialize((char*)&type, sizeof(WORD));
//	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
//	sPacket->Serialize((char*)&dataValue, sizeof(int));
//	sPacket->Serialize((char*)&timeStamp, sizeof(int));
//}
//
//void CBattleSnake_MatchMonitor_LanClient::Packet_HardwareAvailableMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp)
//{
//	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
//	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_AVAILABLE_MEMORY;
//
//	sPacket->Serialize((char*)&type, sizeof(WORD));
//	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
//	sPacket->Serialize((char*)&dataValue, sizeof(int));
//	sPacket->Serialize((char*)&timeStamp, sizeof(int));
//}
//
//void CBattleSnake_MatchMonitor_LanClient::Packet_HardwareRecvBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp)
//{
//	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
//	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_NETWORK_RECV;
//
//	sPacket->Serialize((char*)&type, sizeof(WORD));
//	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
//	sPacket->Serialize((char*)&dataValue, sizeof(int));
//	sPacket->Serialize((char*)&timeStamp, sizeof(int));
//}
//void CBattleSnake_MatchMonitor_LanClient::Packet_HardwareSendBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp)
//{
//	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
//	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_NETWORK_SEND;
//
//	sPacket->Serialize((char*)&type, sizeof(WORD));
//	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
//	sPacket->Serialize((char*)&dataValue, sizeof(int));
//	sPacket->Serialize((char*)&timeStamp, sizeof(int));
//
//}
//void CBattleSnake_MatchMonitor_LanClient::Packet_HardwareNonpagedMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp)
//{
//	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
//	BYTE dataType = dfMONITOR_DATA_TYPE_SERVER_NONPAGED_MEMORY;
//
//	sPacket->Serialize((char*)&type, sizeof(WORD));
//	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
//	sPacket->Serialize((char*)&dataValue, sizeof(int));
//	sPacket->Serialize((char*)&timeStamp, sizeof(int));
//}

void CBattleSnake_MatchMonitor_LanClient::Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_SERVER_ON;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MatchMonitor_LanClient::Packet_ServerProcessCPU(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_CPU;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MatchMonitor_LanClient::Packet_ServerMemory(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_MEMORY_COMMIT;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MatchMonitor_LanClient::Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_PACKET_POOL;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));

}
void CBattleSnake_MatchMonitor_LanClient::Packet_ServerSession(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_SESSION;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MatchMonitor_LanClient::Packet_ServerPlayer(cLanPacketPool* sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_PLAYER;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}
void CBattleSnake_MatchMonitor_LanClient::Packet_ServerMatchTPS(cLanPacketPool *sPacket, int dataValue, int timeStamp)
{
	WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	BYTE dataType = dfMONITOR_DATA_TYPE_MATCH_MATCHSUCCESS;

	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&dataType, sizeof(BYTE));
	sPacket->Serialize((char*)&dataValue, sizeof(int));
	sPacket->Serialize((char*)&timeStamp, sizeof(int));
}