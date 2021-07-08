#include <process.h>
#include <Windows.h>

#include "cHttp_Async.h"
#include "CBattleSnake_Match_Http.h"
#include "CLanClient.h"
#include "CBattleSnake_MatchMonitor_LanClient.h"
#include "CBattleSnake_Match_LanClient.h"
#include "CNetServer.h"
#include "CBattleSnake_Match_NetServer.h"

#include "cDBConnector.h"
#include "CBattleSnake_Match_StatusDB.h"

#include "CBattleSnake_Match_LocalMonitor.h"
#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

CBattleSnake_Match_NetServer::CBattleSnake_Match_NetServer(UINT verCode, BYTE packetCode, BYTE packetKey1, BYTE packetKey2, int serverNo, char *masterToken, WCHAR *accessIP) : CNetServer(packetCode, packetKey1, packetKey2)
{
	_serverNo = serverNo;
	_verCode = verCode;
	memcpy(_publicIP, accessIP, sizeof(WCHAR) * 16);

	//	Initialize Counter 
	_pdhCounter.InitProcessCounter(L"BattleSnake_Match");
	_pdhCounter.InitEthernetCounter();
	_pdhCounter.InitMemoryCounter();

	//	Initialize Variables 
	_matchCounter = 0;
	_pMatchClient = new CBattleSnake_Match_LanClient(this, serverNo, masterToken);
	_pMonitorClient = new CBattleSnake_MatchMonitor_LanClient(this);
	_pHttpClient = new CBattleSnake_Match_Http(this);

	InitializeCriticalSection(&_sessionLock);

	CBattleSnake_Match_LocalMonitor *g_pMonitor = CBattleSnake_Match_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this, CBattleSnake_Match_LocalMonitor::Factor_Match_NetServer);
}

CBattleSnake_Match_NetServer::~CBattleSnake_Match_NetServer()
{
	//	Delete Monitor Lan, Http , Master Lan Pointer
	delete _pMonitorClient;
	delete _pMatchClient;
	delete _pHttpClient;
}

void CBattleSnake_Match_NetServer::OnClientJoin(UINT64 clientID)
{
	Alloc_Session(clientID);
}

void CBattleSnake_Match_NetServer::OnClientLeave(UINT64 clientID)
{
	st_SESSION *pSession = Find_SessionByClientID(clientID);
	if (pSession == nullptr)
	{
		return;
	}

	if (pSession->curStatus >= Status_Success_Login)
	{
		InterlockedDecrement(&_curLogin);

		if (pSession->curStatus == Status_Request_RoomInfo || pSession->curStatus == Status_Success_RoomInfo)
		{
			_pMatchClient->Send_Master_EnterRoomFail(pSession->clientKey);
		}
	}

	Free_Session(pSession);
}
bool CBattleSnake_Match_NetServer::OnConnectionRequest(WCHAR *ip, short port)
{
	return true;
}
void CBattleSnake_Match_NetServer::OnRecv(UINT64 clientID, cNetPacketPool *dsPacket)
{
	st_SESSION *pSession = Find_SessionByClientID(clientID);
	if (pSession == nullptr)
	{
		disconnect(clientID);
		return;
	}

	if (!PacketProc(pSession, dsPacket))
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - OnRecv : PACKET PROC ERROR ID %d", clientID);
		disconnect(clientID);
		return;
	}
}
void CBattleSnake_Match_NetServer::OnSend(UINT64 clientID, int sendSize)
{

}
void CBattleSnake_Match_NetServer::OnError(int errorCode, WCHAR* errText) 
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
	wsprintf(buf, L"MatchNetServer_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}
void CBattleSnake_Match_NetServer::Start_Match(void)
{
	//	Create Monitor Thread
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_Match_NetServer::Stop_Match(void)
{
	//	Exit Monitor Thread
	_bExitMonitor = true;
	DWORD status = WaitForSingleObject(_hMonitorThread, INFINITE);
	if (status == WAIT_OBJECT_0)
	{
		wprintf(L"Match monitor thread terminated!\n");
	}

	CloseHandle(_hMonitorThread);
}

bool CBattleSnake_Match_NetServer::Connect_MatchStatusDB(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, short dbPort)
{
	if (!cDBConnector::ConstructConnectorPool(dbIP, userName, password, dbName, dbPort)) return false;

	//	연결 생성
	CBattleSnake_Match_StatusDB *pMatchDB = (CBattleSnake_Match_StatusDB*)cDBConnector::GetDBConnector();
	if (pMatchDB == nullptr) return false;
	return true;
}

bool CBattleSnake_Match_NetServer::Disconnect_MatchStatusDB(void)
{
	//	연결 끊기
	cDBConnector::DestructConnectPool();
	return true;
}

void CBattleSnake_Match_NetServer::Set_MatchLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_matchLanClient_IP, ip, sizeof(WCHAR) * 16);
	_matchLanClient_port = port;
	_matchLanClient_threadCnt = threadCnt;
	_matchLanCliebt_nagle = bNagle;
	_matchLanClient_config = true;
}
bool CBattleSnake_Match_NetServer::ConnectLanClient_Match(void) 
{
	if (_matchLanClient_config == false) return false;
	return _pMatchClient->Connect(_matchLanClient_IP, _matchLanClient_port, _matchLanClient_threadCnt, _matchLanCliebt_nagle);
}
bool CBattleSnake_Match_NetServer::DisconnectLanClient_Match(void)
{
	return _pMatchClient->Disconnect();
}

void CBattleSnake_Match_NetServer::Set_MonitorLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_monitorLanClient_IP, ip, sizeof(WCHAR) * 16);
	_monitorLanClient_port = port;
	_monitorLanClient_threadCnt = threadCnt;
	_monitorLanCliebt_nagle = bNagle;
	_monitorLanClient_config = true;
}
bool CBattleSnake_Match_NetServer::ConnectLanClient_Monitor(void) 
{
	if (_monitorLanClient_config == false) return false;
	return _pMonitorClient->Connect(_monitorLanClient_IP, _monitorLanClient_port, _monitorLanClient_threadCnt, _monitorLanCliebt_nagle);
}
bool CBattleSnake_Match_NetServer::DisconnectLanClient_Monitor(void) 
{
	return _pMonitorClient->Disconnect();
}
bool CBattleSnake_Match_NetServer::Start_HttpClient(int threadCnt, bool bNagle, int maxUser) 
{
	if (_pHttpClient->StartHttpClient(threadCnt, bNagle, maxUser) == false) return false;
	_pHttpClient->Start_MessageThread();

	return true;
}
bool CBattleSnake_Match_NetServer::Stop_HttpClient(void) 
{
	if (_pHttpClient->StopHttpClient() == false) return false;
	_pHttpClient->Stop_MessageThread();

	return true;
}
void CBattleSnake_Match_NetServer::Set_HttpURL(WCHAR *url, int len)
{
	_pHttpClient->Set_SelectAccountURL(url, len);
}

void CBattleSnake_Match_NetServer::Fail_HttpRequest(UINT64 clientKey) 
{
	//	Http Server Failed
	LOG(L"MatchServer", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Match_NetServer - Fail_HttpRequest: KEY %d", clientKey);
	st_SESSION *pSession = Find_SessionByClientKey(clientKey);	
	if (pSession == nullptr)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Match_NetServer - Fail_HttpRequest : CAN'T FIND SESSION %d", clientKey);
		return;
	}

	//	재전송 
	if (InterlockedIncrement(&pSession->httpFailCount) > 3)
	{
		disconnect(pSession->clientID);
		return;
	}
	_pHttpClient->Send_Select_Account(pSession->clientKey, pSession->accountNo);
}	

bool CBattleSnake_Match_NetServer::Receive_HttpRequest_SessionKey(UINT64 clientKey, int result, char *sessionKey) 
{
	BYTE status;
	st_SESSION *pSession = Find_SessionByClientKey(clientKey);
	if (pSession == nullptr)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - Receive_HttpRequest_SessionKey : CAN'T FIND SESSION %d", clientKey);
		return false;
	}

	switch (result)
	{
	case 1:
		status = Login_Result::SUCCESS;
		if (memcmp(pSession->sessionKey, sessionKey, sizeof(char) * 64) != 0)
		{
			status = Login_Result::ERROR_NOTMATCH_SESSIONKEY;
			LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#Receive_HttpRequest_SessionKey : NOT MATCH SESSIONKEY [result: %s, user: %s]", sessionKey, pSession->sessionKey);
		}
		break;
	case -10:
	case -11:
	case -12:
		status = Login_Result::ERROR_NOEXIST_ACCOUNTNO;
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - Receive_HttpRequest_SessionKey : NO EXIST ACCOUNTNO [user: %d]", pSession->accountNo);
		break;
	default:
		status = Login_Result::ERROR_ETC;
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - Receive_HttpRequest_SessionKey : SHDB ERROR %d", result);
		break;
	}
	
	Response_Session_Login(pSession, status);
	return true;
}

bool CBattleSnake_Match_NetServer::Recieve_MasterRequest_Login(int serverNo)
{
	if (serverNo != _serverNo)
		return false;
	
	//	현재 Request_Login 중인 모든 유저들의 상태를 전부 보낸다. 

	std::list<st_SESSION*>::iterator iter;
	std::list<st_SESSION*> *pList = &_sessionList;
	st_SESSION *pSession = nullptr;

	//AcquireSRWLockShared(&_sessionLock);
	EnterCriticalSection(&_sessionLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pSession = *iter;
		if (pSession->curStatus == Status_Request_RoomInfo)
		{
			_pMatchClient->Send_Master_Roominfo(pSession->clientKey, pSession->accountNo);
		}
		else if (pSession->curStatus == Status_Request_RoomEnter)
		{
			_pMatchClient->Send_Master_EnterRoomSuccess(pSession->battleServerNo, pSession->roomNo, pSession->clientKey);
		}

		++iter;
	}

	//ReleaseSRWLockShared(&_sessionLock);
	LeaveCriticalSection(&_sessionLock);

	return true;
}

void CBattleSnake_Match_NetServer::Recieve_MasterRequest_RoomInfo(UINT64 clientKey, BYTE status, WORD battleServerNo, WCHAR *battleIP, WORD battlePort,
																  int roomNo, char* connectToken, char* enterToken,
																  WCHAR *chatIP, WORD chatPort)
{
	st_SESSION * pSession = Find_SessionByClientKey(clientKey);
	if (pSession == nullptr)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - Recieve_MasterRequest_RoomInfo : CAN'T FIND SESSION %d", clientKey);
		return;
	}

	Response_Session_RoomInfo(pSession, status, battleServerNo, battleIP, battlePort, roomNo, connectToken, enterToken, chatIP, chatPort);
}

void CBattleSnake_Match_NetServer::Receive_MasterRequest_RoomEnter(UINT64 clientKey)
{
	st_SESSION *pSession = Find_SessionByClientKey(clientKey);
	if (pSession != nullptr)
	{
		Response_Session_RoomEnter(pSession);
	}
}

bool CBattleSnake_Match_NetServer::PacketProc(st_SESSION *pSession, cNetPacketPool *dsPacket) 
{
	WORD type;
	dsPacket->Deserialize((char*)&type, sizeof(WORD));

	switch (type)
	{
	case en_PACKET_CS_MATCH_REQ_LOGIN:
		return Request_Session_Login(pSession, dsPacket);
	case en_PACKET_CS_MATCH_REQ_GAME_ROOM:
		return Request_Session_RoomInfo(pSession, dsPacket);
	case en_PACKET_CS_MATCH_REQ_GAME_ROOM_ENTER:
		return Request_Session_RoomEnter(pSession, dsPacket);
	default:
		return false;
	};
}

bool CBattleSnake_Match_NetServer::Request_Session_Login(st_SESSION *pSession, cNetPacketPool *dsPacket) 
{
	dsPacket->Deserialize((char*)&pSession->accountNo, sizeof(UINT64));
	dsPacket->Deserialize(pSession->sessionKey, sizeof(char) * 64);
	dsPacket->Deserialize((char*)&pSession->verCode, sizeof(UINT));

	if (pSession->verCode != _verCode)
	{
		BYTE status = Login_Result::ERROR_VERSION;
		Response_Session_Login(pSession, status);
		return true;
	}

	pSession->curStatus = Status_Request_Login;
	return _pHttpClient->Send_Select_Account(pSession->clientKey, pSession->accountNo);
}

bool CBattleSnake_Match_NetServer::Request_Session_RoomInfo(st_SESSION *pSession, cNetPacketPool *dsPacket) 
{
	pSession->curStatus = Status_Request_RoomInfo;
	_pMatchClient->Send_Master_Roominfo(pSession->clientKey, pSession->accountNo);

	return true;
}

bool CBattleSnake_Match_NetServer::Request_Session_RoomEnter(st_SESSION *pSession, cNetPacketPool *dsPacket) 
{
	WORD battleServerNo;
	int roomNo;

	dsPacket->Deserialize((char*)&battleServerNo, sizeof(WORD));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));

	pSession->curStatus = Status_Request_RoomEnter;

	UINT64 clientKey = pSession->clientKey;
	_pMatchClient->Send_Master_EnterRoomSuccess(battleServerNo, roomNo, clientKey);
	Response_Session_RoomEnter(pSession);
	return true;
}

void CBattleSnake_Match_NetServer::Response_Session_Login(st_SESSION *pSession, BYTE status) 
{
	pSession->curStatus = Status_Success_Login;
	InterlockedIncrement(&_curLogin);
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Session_Login(sPacket, status);
	sendPacket(pSession->clientID, sPacket);
}

void CBattleSnake_Match_NetServer::Response_Session_RoomInfo(st_SESSION *pSession, BYTE status,
															 WORD battleServerNo, WCHAR *battleIP, WORD battlePort, int roomNo, char *connectToken, char* enterToken,
															 WCHAR *chatIP, WORD chatPort) 
{
	pSession->curStatus = Status_Success_RoomInfo;
	InterlockedIncrement(&_matchCounter);
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Session_RoomInfo(sPacket, status, battleServerNo, battleIP, battlePort, roomNo, connectToken, enterToken, chatIP, chatPort);
	sendPacket(pSession->clientID, sPacket);
}
void CBattleSnake_Match_NetServer::Response_Session_RoomEnter(st_SESSION *pSession) 
{
	pSession->curStatus = Status_Success_RoomEnter;
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Session_RoomEnter(sPacket);
	sendPacket(pSession->clientID, sPacket);
}

void CBattleSnake_Match_NetServer::SerializePacket_Session_Login(cNetPacketPool *sPacket, BYTE status) 
{
	WORD type = en_PACKET_CS_MATCH_RES_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&status, sizeof(BYTE));
}
void CBattleSnake_Match_NetServer::SerializePacket_Session_RoomInfo(cNetPacketPool *sPacket, BYTE status,
																	WORD battleServerNo, WCHAR *battleIP, WORD battlePort, int roomNo, char *connectToken, char* enterToken,
																	WCHAR *chatIP, WORD chatPort) 
{
	WORD type = en_PACKET_CS_MATCH_RES_GAME_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&status, sizeof(BYTE));
	sPacket->Serialize((char*)&battleServerNo, sizeof(WORD));
	sPacket->Serialize((char*)battleIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&battlePort, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize(connectToken, sizeof(char) * 32);
	sPacket->Serialize(enterToken, sizeof(char) * 32);
	sPacket->Serialize((char*)chatIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&chatPort, sizeof(WORD));
}
void CBattleSnake_Match_NetServer::SerializePacket_Session_RoomEnter(cNetPacketPool *sPacket) 
{
	WORD type = en_PACKET_CS_MATCH_RES_GAME_ROOM_ENTER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}


void CBattleSnake_Match_NetServer::Alloc_Session(UINT64 clientID)
{	
	st_SESSION *pSession = _sessionPool.Alloc();
	UINT64 clientKey = Create_ClientKey(clientID);
	Init_Session(pSession, clientID, clientKey);

	//AcquireSRWLockExclusive(&_sessionLock);
	EnterCriticalSection(&_sessionLock);
	_sessionList.push_back(pSession);
	//ReleaseSRWLockExclusive(&_sessionLock);
	LeaveCriticalSection(&_sessionLock);
}
void CBattleSnake_Match_NetServer::Free_Session(st_SESSION *pSession) 
{
	std::list<st_SESSION*>::iterator iter;
	std::list<st_SESSION*> *pList = &_sessionList;
	//AcquireSRWLockExclusive(&_sessionLock);
	EnterCriticalSection(&_sessionLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		if (*iter == pSession)
		{
			pList->erase(iter);
			_sessionPool.Free(pSession);
			//ReleaseSRWLockExclusive(&_sessionLock);
			LeaveCriticalSection(&_sessionLock);
			return;
		}
		++iter;
	}

	//ReleaseSRWLockExclusive(&_sessionLock);
	LeaveCriticalSection(&_sessionLock);
	LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - Free_Session : CAN'T DELETE SESSION: %d", pSession->clientID);
}

CBattleSnake_Match_NetServer::st_SESSION* CBattleSnake_Match_NetServer::Find_SessionByClientID(UINT64 clientID) 
{
	std::list<st_SESSION*>::iterator iter;
	std::list<st_SESSION*> *pList = &_sessionList;
	st_SESSION *pSession = nullptr;

	//AcquireSRWLockShared(&_sessionLock);
	EnterCriticalSection(&_sessionLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pSession = *iter;
		if (pSession->clientID == clientID)
		{
			//ReleaseSRWLockShared(&_sessionLock);
			LeaveCriticalSection(&_sessionLock);
			return pSession;
		}
		++iter;
	}

	//ReleaseSRWLockShared(&_sessionLock);
	LeaveCriticalSection(&_sessionLock);
	LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_NetServer - Find_SessionByClientID : CAN'T FIND SESSION %d", clientID);

	return nullptr;
}
CBattleSnake_Match_NetServer::st_SESSION* CBattleSnake_Match_NetServer::Find_SessionByClientKey(UINT64 clientKey) 
{
	std::list<st_SESSION*>::iterator iter;
	std::list<st_SESSION*> *pList = &_sessionList;
	st_SESSION *pSession = nullptr;

	//AcquireSRWLockShared(&_sessionLock);
	EnterCriticalSection(&_sessionLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pSession = *iter;
		if (pSession->clientKey == clientKey)
		{
			//ReleaseSRWLockShared(&_sessionLock);
			LeaveCriticalSection(&_sessionLock);
			return pSession;
		}
		++iter;
	}

	//ReleaseSRWLockShared(&_sessionLock);
	LeaveCriticalSection(&_sessionLock);

	return nullptr;
}
void CBattleSnake_Match_NetServer::Init_Session(st_SESSION *pSession, UINT64 clientID, UINT64 clientKey) 
{
	pSession->curStatus = Request_Status::none;
	pSession->clientID = clientID;
	pSession->clientKey = clientKey;
	pSession->httpFailCount = 0;
}

UINT64 CBattleSnake_Match_NetServer::Create_ClientKey(UINT64 clientID) 
{
	UINT64 key = InterlockedIncrement(&_curID);
	return (key << 8) | _serverNo;
}

unsigned int CBattleSnake_Match_NetServer::MonitorThread(void *arg)
{
	CBattleSnake_Match_NetServer *pThis = (CBattleSnake_Match_NetServer*)arg;
	return pThis->Update_MonitorThread();
}
unsigned int CBattleSnake_Match_NetServer::Update_MonitorThread(void) 
{
	static int dbCounter = 0;
	int connectVar = 0;
	int prevConnect = 0;

	bool *bExitMonitor = &_bExitMonitor;
	long prevTime = timeGetTime();
	long curTime;
	long timeInterval;

	double recv, send;
	double bytes = 0;
	timeBeginPeriod(1);

	cLanPacketPool::AllocPacketChunk();
	while (*bExitMonitor == false)
	{
		curTime = timeGetTime();
		timeInterval = curTime - prevTime;

		if (timeInterval < 1000)
		{
			Sleep(100);
			continue;
		}

		timeInterval /= 1000;

		prevConnect = (int)_monitor_session;

		_monitor_packetPool = cNetPacketPool::GetUsedPoolSize() + cLanPacketPool::GetUsedPoolSize();
		_monitor_session = getSessionCount();
		_monitor_login = _curLogin;
		_monitor_matchTPS = _matchCounter / timeInterval;
		_matchCounter = 0;

		//	process commit memory
		_pdhCounter.UpdatePdhQueryData();
		_pdhCounter.GetProcessStatus(bytes);
		_monitor_commit_MBytes = bytes / 1024 / 1024;

		////	Ethernet Recv, Send
		//_pdhCounter.GetNetworkStatus(recv, send);
		//_monitor_HardwareRecvKBytes = recv / 1024;
		//_monitor_HardwareSendKBytes = send / 1024;
		
		//	CPU usage
		_cpuCounter.UpdateCpuTime();
		_monitor_process_cpu = _cpuCounter.GetProcessTotalUsage();
		//_monitor_HardwareCpuUsage = _cpuCounter.GetUnitTotalUsage();

		////	Nonpaged memory
		////	Available memory
		//_pdhCounter.GetMemoryStatus(_monitor_HardwareAvailableMBytes, _monitor_HardwareNonpagedMBytes);

		//	매초마다  ConnectUser 변화량을 체크하고 
		//	변화량이 100명이상이면 바로 전송, 
		//	그리고 다시 리셋, 
		//	만약 3초가 될때까지 100명 미만의 변화량을 가진다면 
		//	그대로 전송한다. 
	
		if (IsServerOn())
		{
			connectVar = (int)_monitor_session - prevConnect;
			if (dbCounter == 2 || connectVar >= 100 || connectVar <= -100)
			{
				// 전송
				CBattleSnake_Match_StatusDB::Match_Status status;
				//memcpy(status.serverIP, dfPUBLic_MATCH_IP, sizeof(WCHAR) * 16);
				status.serverIP = _publicIP;
				status.serverPort = _serverPort;
				status.serverNo = _serverNo;
				status.connectUser = _monitor_session;
				CBattleSnake_Match_StatusDB* pWriter = (CBattleSnake_Match_StatusDB*)cDBConnector::GetDBConnector();
				pWriter->DBWrite(&status);
				dbCounter = 0;
			}
			else
			{
				++dbCounter;
			}

			if (_pMatchClient->isConnected() == false)
			{
				ConnectLanClient_Match();
			}

			if (_pMonitorClient->isConnected() == false)
			{
				ConnectLanClient_Monitor();
			}
		}


		//	Check Http Heartbeat
		_pHttpClient->Send_Http_Heartbeat();
		prevTime = curTime;
	}
	timeEndPeriod(1);
	cLanPacketPool::FreePacketChunk();
	return 0;
}