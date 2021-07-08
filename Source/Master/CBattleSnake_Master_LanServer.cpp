#include <process.h>
#include <Windows.h>

#include "CLanClient.h"
#include "CBattleSnake_MasterMonitor_LanClient.h"
#include "CLanServer.h"
#include "CBattleSnake_Master_LanServer.h"
#include "CBattleSnake_Master_LocalMonitor.h"

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

CBattleSnake_Master_LanServer::CBattleSnake_Master_LanServer(char *masterToken)
{
	memcpy(_masterToken, masterToken, sizeof(char) * 32);

	_pdhCounter.InitProcessCounter(L"BattleSnake_Master");

	_pMonitorClient = new CBattleSnake_MasterMonitor_LanClient(this);
	CBattleSnake_Master_LocalMonitor *g_pMonitor = CBattleSnake_Master_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this);

	InitializeSRWLock(&_serverLock);
	InitializeSRWLock(&_battleInfoLock);
	InitializeCriticalSection(&_waitRoomLock);
	//InitializeSRWLock(&_waitRoomLock);
	InitializeCriticalSection(&_sessionLock);
	//InitializeSRWLock(&_sessionLock);

	_battleLoginServer = 0;
	_matchLoginServer = 0;
	_battleConnectServer = 0;
	_matchConnectServer = 0;

	_battleMaskID = 0xB000;
	_curBattleID = 0;
}

CBattleSnake_Master_LanServer::~CBattleSnake_Master_LanServer()
{
	delete _pMonitorClient;
	//	Server List 정리(Server Info, Battle Room)
	//	Session Map 정리 
}

void CBattleSnake_Master_LanServer::Start_Master(void)
{
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_Master_LanServer::Stop_Master(void)
{
	_bExitMonitor = true;
	DWORD status = WaitForSingleObject(_hMonitorThread, INFINITE);
	if (status == WAIT_OBJECT_0)
	{
		wprintf(L"Master monitor thread terminated! \n");
	}

	CloseHandle(_hMonitorThread);
}

void CBattleSnake_Master_LanServer::SetMonitorClientConfig(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_monitorIP, ip, sizeof(WCHAR) * 16);
	_monitorPort = port;
	_monitorThreadCnt = threadCnt;
	_monitorbNagle = bNagle;
	_bMonitorConfigSet = true;
}

bool CBattleSnake_Master_LanServer::ConnectMonitorClient(void)
{
	if (_bMonitorConfigSet == false) return false;
	return _pMonitorClient->Connect(_monitorIP, _monitorPort, _monitorThreadCnt, _monitorbNagle);
}

void CBattleSnake_Master_LanServer::DisconnectMonitorClient(void)
{
	_pMonitorClient->Disconnect();
}

unsigned int CBattleSnake_Master_LanServer::MonitorThread(void *arg)
{
	CBattleSnake_Master_LanServer *pThis = (CBattleSnake_Master_LanServer*)arg;
	return pThis->Update_MonitorThread();
}

unsigned int CBattleSnake_Master_LanServer::Update_MonitorThread(void)
{
	bool *bExitMonitor = &_bExitMonitor;
	long prevTime = timeGetTime();
	long curTime;
	long timeInterval;

	double bytes = 0;
	timeBeginPeriod(1);

	while (*bExitMonitor == false)
	{
		curTime = timeGetTime();
		timeInterval = curTime - prevTime;

		if (timeInterval < 1000)
		{
			Sleep(100);
			continue;
		}

		_monitor_battleServerConnection = _battleConnectServer;
		_monitor_battleServerLogin = _battleLoginServer;
		_monitor_matchServerConnection = _matchConnectServer;
		_monitor_matchServerLogin = _matchLoginServer;

		_monitor_packetPool = cLanPacketPool::GetUsedPoolSize();
		_monitor_waitSessionUsed = _sessionPool.GetUsedBlockCount();
		_monitor_waitSessionPool = _sessionPool.GetTotalBlockCount();
		_monitor_waitRoomUsed = _roomPool.GetUsedBlockCount();
		_monitor_waitRoomPool = _roomPool.GetTotalBlockCount();

		_pdhCounter.UpdatePdhQueryData();
		_pdhCounter.GetProcessStatus(bytes);
		_monitor_privateBytes = bytes / 1024 / 1024;
		
		_cpuCounter.UpdateCpuTime();
		_monitor_process_cpu = _cpuCounter.GetProcessTotalUsage();
		_monitor_unit_cpu = _cpuCounter.GetUnitTotalUsage();

		if(IsServerOn() && _pMonitorClient->isConnected() == false)
		{
			ConnectMonitorClient();
		}

		prevTime = curTime;
	}

	return 0;
}

void CBattleSnake_Master_LanServer::OnClientJoin(UINT64 clientID)
{
	Alloc_Server(clientID);
}

void CBattleSnake_Master_LanServer::OnClientLeave(UINT64 clientID)
{
	//	서버 리스트에서 해제하기 전에 
	//	만약 서버 타입이 배틀이라면 Room, Info를 정리하고, 
	//	그와함께 각각의 Room에 들어가 있는 유저들을 정리한다. 

	st_SERVER *pServer = Find_ServerByClientID(clientID);
	if (pServer == nullptr)
	{
		//LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - OnClientLeave : NO EXIST CLIENT ID %d", clientID);
		return;
	}

	if (pServer->serverType == BATTLE_SERVER)
	{
		if (pServer->bLogin)
		{
			InterlockedDecrement((long*)&_battleLoginServer);
		}

		st_BATTLE_INFO *pBattle = Find_BattleInfo(pServer->serverNo);
		if (pBattle != nullptr)
		{
			Free_BattleInfo(pBattle);
		}

		InterlockedDecrement((long*)&_battleConnectServer);
	}
	else if (pServer->serverType == MATCH_SERVER)
	{
		if (pServer->bLogin)
		{
			InterlockedDecrement((long*)&_matchLoginServer);
		}

		InterlockedDecrement((long*)&_matchConnectServer);
	}

	Free_Server(pServer);
}

bool CBattleSnake_Master_LanServer::OnConnectionRequest(WCHAR *ip, short port)
{
	return true;
}

void CBattleSnake_Master_LanServer::OnRecv(UINT64 clientID, cLanPacketPool *dsPacket)
{
	st_SERVER *pServer = Find_ServerByClientID(clientID);
	if (pServer == nullptr)
	{
		LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - OnRecv : CAN'T FIND CLIENT ID: %d", clientID);
		disconnect(clientID);
		return;
	}

	PacketProc(pServer, dsPacket);
}

void CBattleSnake_Master_LanServer::OnSend(UINT64 clientID, int sendSize)
{
	//	비어 있음 
}
void CBattleSnake_Master_LanServer::OnError(int errorCode, WCHAR* errText)
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
	wsprintf(buf, L"MasterLanServer_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

void CBattleSnake_Master_LanServer::Alloc_Server(UINT64 clientID)
{
	st_SERVER *pServer = new st_SERVER;
	Init_Server(pServer, clientID);
	
	AcquireSRWLockExclusive(&_serverLock);
	_serverList.push_back(pServer);
	ReleaseSRWLockExclusive(&_serverLock);
}

void CBattleSnake_Master_LanServer::Free_Server(st_SERVER * pServer)
{
	std::list<st_SERVER*>::iterator iter;
	std::list<st_SERVER*> *pList = &_serverList;
	AcquireSRWLockExclusive(&_serverLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		if (*iter == pServer)
		{
			delete *iter;
			pList->erase(iter);
			ReleaseSRWLockExclusive(&_serverLock);
			return;
		}
		++iter;
	}

	ReleaseSRWLockExclusive(&_serverLock);
	LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Free_Server : CAN'T DELETE SERVER: %d", pServer->clientID);
}

CBattleSnake_Master_LanServer::st_SERVER* CBattleSnake_Master_LanServer::Find_ServerByClientID(UINT64 clientID)
{
	std::list<st_SERVER*>::iterator iter;
	std::list<st_SERVER*> *pList = &_serverList;
	st_SERVER *pServer = nullptr;

	AcquireSRWLockShared(&_serverLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pServer = *iter;
		if (pServer->clientID == clientID)
		{
			ReleaseSRWLockShared(&_serverLock);
			return pServer;
		}
		++iter;
	}
	ReleaseSRWLockShared(&_serverLock);

	//LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Find_ServerByClientID : CAN'T FIND CLIENT ID: %d", clientID);
	return nullptr;
}

CBattleSnake_Master_LanServer::st_SERVER* CBattleSnake_Master_LanServer::Find_ServerByServerNo(int serverNo)
{
	std::list<st_SERVER*>::iterator iter;
	std::list<st_SERVER*> *pList = &_serverList;
	st_SERVER *pServer = nullptr;
	AcquireSRWLockShared(&_serverLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pServer = *iter;
		if (pServer->clientID == serverNo)
		{
			ReleaseSRWLockShared(&_serverLock);
			return pServer;
		}
		++iter;
	}
	ReleaseSRWLockShared(&_serverLock);

	LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Find_ServerByServerNo:  CAN'T FIND SERVER NO %d", serverNo);
	return nullptr;
}

void CBattleSnake_Master_LanServer::Init_Server(st_SERVER *pServer, UINT64 clientID)
{
	pServer->clientID = clientID;
	pServer->serverNo = 0;
	pServer->serverType = NONE;
	pServer->bLogin = false;
}

int CBattleSnake_Master_LanServer::Create_BattleServerNo(void)
{
	//	배틀서버 고유번호 

	return InterlockedIncrement((long*)&_curBattleID);
}

void CBattleSnake_Master_LanServer::Alloc_BattleInfo(int serverNo, char *connectionToken, WCHAR*battleIP, short battlePort, WCHAR *chatIP, short chatPort)
{
	st_BATTLE_INFO *pBattle = new st_BATTLE_INFO;
	Init_BattleInfo(pBattle, serverNo, connectionToken, battleIP, battlePort, chatIP, chatPort);

	AcquireSRWLockExclusive(&_battleInfoLock);
	_battleInfoList.push_back(pBattle);
	ReleaseSRWLockExclusive(&_battleInfoLock);
}

void CBattleSnake_Master_LanServer::Free_BattleInfo(st_BATTLE_INFO *pBattle)
{
	std::list<st_BATTLE_INFO*>::iterator iter;
	std::list<st_BATTLE_INFO*> *pList = &_battleInfoList;
	std::list<st_WAIT_ROOM*>::iterator roomIter;
	std::list<st_WAIT_ROOM*> *pRoomList = &_roomList;
	st_WAIT_ROOM *pRoom = nullptr;

	// Clear wait Room Map
	EnterCriticalSection(&_waitRoomLock);
	//AcquireSRWLockExclusive(&_waitRoomLock);
	roomIter = pRoomList->begin();
	while (roomIter != pRoomList->end())
	{
		pRoom = *roomIter;
		if (pRoom->serverNo == pBattle->serverNo)
		{
			roomIter = pRoomList->erase(roomIter);
			Clear_WaitRoom(pRoom->serverNo, pRoom->roomNo);
			_roomPool.Free(pRoom);
			continue;
		}
		++iter;
	}

	LeaveCriticalSection(&_waitRoomLock);
	//ReleaseSRWLockExclusive(&_waitRoomLock);

	//	Erase pBattle from List
	AcquireSRWLockExclusive(&_battleInfoLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		if (*iter == pBattle)
		{
			delete *iter;
			pList->erase(iter);
			ReleaseSRWLockExclusive(&_battleInfoLock);
			return;
		}
		++iter;
	}

	ReleaseSRWLockExclusive(&_battleInfoLock);
	LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Free_BattleInfo: CAN'T DELETE BATTLE INFO %d", pBattle->serverNo);
}

CBattleSnake_Master_LanServer::st_BATTLE_INFO* CBattleSnake_Master_LanServer::Find_BattleInfo(int serverNo)
{
	std::list<st_BATTLE_INFO*>::iterator iter;
	std::list<st_BATTLE_INFO*> *pList = &_battleInfoList;
	st_BATTLE_INFO *pBattle = nullptr;

	AcquireSRWLockShared(&_battleInfoLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pBattle = *iter;
		if (pBattle->serverNo == serverNo)
		{
			ReleaseSRWLockShared(&_battleInfoLock);
			return pBattle;
		}
		++iter;
	}

	ReleaseSRWLockShared(&_battleInfoLock);
	LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Find_BattleInfo : CAN'T FIND BATTLE INFO: %d", serverNo);
	return nullptr;
}

void CBattleSnake_Master_LanServer::Init_BattleInfo(st_BATTLE_INFO *pBattle, int serverNo, char* connectionToken,WCHAR* battleIP, short battlePort, WCHAR *chatIP, short chatPort)
{
	pBattle->serverNo = serverNo;
	memcpy(pBattle->connectionToken, connectionToken, sizeof(char) * 32);
	memcpy(pBattle->battleServerIP, battleIP, sizeof(WCHAR) * 16);
	memcpy(pBattle->chatServerIP, chatIP, sizeof(WCHAR) * 16);
	pBattle->battleServerPort = battlePort;
	pBattle->chatServerPort = chatPort;
}

void CBattleSnake_Master_LanServer::Track_WaitRoom(st_WAIT_ROOM *pRoom, Room_Tracker type, int available, UINT64 accountNo)
{
	int count = (InterlockedIncrement((long*)&pRoom->trackCount) % 500);
	pRoom->tracker[count].type = type;
	pRoom->tracker[count].available = available;
	pRoom->tracker[count].accountNo = accountNo;
}

void CBattleSnake_Master_LanServer::Alloc_WaitRoom(int serverNo, int roomNo, int maxUser, char* enterToken)
{
	st_WAIT_ROOM *pRoom = _roomPool.Alloc();
	Init_WaitRoom(pRoom, serverNo, roomNo, maxUser, enterToken);

	EnterCriticalSection(&_waitRoomLock);
	//AcquireSRWLockExclusive(&_waitRoomLock);

	Track_WaitRoom(pRoom, Room_Tracker::Battle_RoomCreated, pRoom->available,0);
	_roomList.push_back(pRoom);
	LeaveCriticalSection(&_waitRoomLock);
	//ReleaseSRWLockExclusive(&_waitRoomLock);
}

void CBattleSnake_Master_LanServer::Free_WaitRoom(int serverNo, int roomNo)
{
	std::list<st_WAIT_ROOM*>::iterator iter;
	std::list<st_WAIT_ROOM*> *pList = &_roomList;
	st_WAIT_ROOM *pRoom;

	EnterCriticalSection(&_waitRoomLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pRoom = *iter;
		if (pRoom->serverNo == serverNo && pRoom->roomNo == roomNo)
		{
			Track_WaitRoom(pRoom, Room_Tracker::Battle_RoomClosed, pRoom->available, 0);
			pList->erase(iter);
			_roomPool.Free(pRoom);
			break;
		}
		++iter;
	}

	LeaveCriticalSection(&_waitRoomLock);

	//	방 인원 정리
	Clear_WaitRoom(serverNo, roomNo);

}

void CBattleSnake_Master_LanServer::Init_WaitRoom(st_WAIT_ROOM *pRoom, int serverNo, int roomNo, int maxUser, char* enterToken)
{
	pRoom->serverNo = serverNo;
	pRoom->roomNo = roomNo;
	pRoom->openTime = timeGetTime();
	pRoom->available = maxUser;
	pRoom->maxUser = maxUser;
	memcpy(pRoom->enterToken, enterToken, sizeof(char) * 32);
}

CBattleSnake_Master_LanServer::st_WAIT_ROOM* CBattleSnake_Master_LanServer::Find_WaitRoom(int serverNo, int roomNo)
{
	std::list<st_WAIT_ROOM*>::iterator iter;
	std::list<st_WAIT_ROOM*> *pList = &_roomList;
	st_WAIT_ROOM *pRoom = nullptr;

	EnterCriticalSection(&_waitRoomLock);
	//AcquireSRWLockShared(&_waitRoomLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pRoom = *iter;
		if (pRoom->roomNo == roomNo && pRoom->serverNo == serverNo)
		{
			LeaveCriticalSection(&_waitRoomLock);
			//ReleaseSRWLockShared(&_waitRoomLock);
			return pRoom;
		}
		++iter;
	}

	//LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CAN'T FIND ROOM: server %d, room %d", serverNo, roomNo );
	LeaveCriticalSection(&_waitRoomLock);
	//ReleaseSRWLockShared(&_waitRoomLock);

	return nullptr;
}
void CBattleSnake_Master_LanServer::Increment_WaitRoom(int serverNo, int roomNo, Room_Tracker type, UINT64 accountNo)
{
	std::list<st_WAIT_ROOM*>::iterator iter;
	std::list<st_WAIT_ROOM*> *pList = &_roomList;
	st_WAIT_ROOM *pRoom = nullptr;

	EnterCriticalSection(&_waitRoomLock);
	//AcquireSRWLockShared(&_waitRoomLock);
	iter = pList->begin();
	while (iter != pList->end())
	{
		pRoom = *iter;
		if (pRoom->roomNo == roomNo && pRoom->serverNo == serverNo)
		{
			++pRoom->available;
			Track_WaitRoom(pRoom, type, pRoom->available, accountNo);
			LeaveCriticalSection(&_waitRoomLock);
			//ReleaseSRWLockShared(&_waitRoomLock);
			return;
		}
		++iter;
	}

	LeaveCriticalSection(&_waitRoomLock);
	//ReleaseSRWLockShared(&_waitRoomLock);
}

CBattleSnake_Master_LanServer::st_WAIT_ROOM* CBattleSnake_Master_LanServer::Find_HighPriorityWaitRoom(UINT64 accountNo)
{
	//	Priority = Waiting Sessions
	//	Sort List by available count 
	st_WAIT_ROOM *pRoomMin = nullptr;
	st_WAIT_ROOM *pRoom = nullptr;
	EnterCriticalSection(&_waitRoomLock);
	//AcquireSRWLockExclusive(&_waitRoomLock);
	if (_roomList.size() == 0)
	{
		LeaveCriticalSection(&_waitRoomLock);
		//ReleaseSRWLockExclusive(&_waitRoomLock);
		return nullptr;
	}

	std::list<st_WAIT_ROOM*> *pList = &_roomList;
	std::list<st_WAIT_ROOM*>::iterator iter = pList->begin();
	pRoomMin = *iter;

	
	while (iter != pList->end())
	{
		pRoom = *iter;

		if (pRoomMin->available == 0 ||
			(pRoomMin->available > pRoom->available && pRoom->available > 0))
		{
			pRoomMin = pRoom;
		}

		++iter;
	}

	if (pRoomMin->available == 0)
	{
		cCrashDump::Crash();
		//ReleaseSRWLockExclusive(&_waitRoomLock);
		LeaveCriticalSection(&_waitRoomLock);
		return nullptr;
	}

	--pRoomMin->available;

	Track_WaitRoom(pRoomMin, Room_Tracker::Match_RoomInfo, pRoomMin->available, accountNo);

	LeaveCriticalSection(&_waitRoomLock);
	//ReleaseSRWLockExclusive(&_waitRoomLock);

	// LOG(L"MasterServer", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Master_LanServer - Find_HighPriorityWaitRoom : roomNo [%d] available [%d]", pRoomMin->roomNo, pRoomMin->available);
	return pRoomMin;
}

void CBattleSnake_Master_LanServer::Clear_WaitRoom(int serverNo, int roomNo)
{
	std::map<UINT64, st_SESSION*>::iterator sessionIter;
	std::map<UINT64, st_SESSION*> *pMap = &_sessionMap;

	EnterCriticalSection(&_sessionLock);
	//AcquireSRWLockExclusive(&_sessionLock);
	sessionIter = pMap->begin();
	while (sessionIter != pMap->end())
	{
		if (sessionIter->second->roomNo == roomNo && sessionIter->second->serverNo == serverNo)
		{
			st_SESSION *pSession = sessionIter->second;
			sessionIter = pMap->erase(sessionIter);
			_sessionPool.Free(pSession);
			continue;
		}
		++sessionIter;
	}
	LeaveCriticalSection(&_sessionLock);
	//ReleaseSRWLockExclusive(&_sessionLock);
}

CBattleSnake_Master_LanServer::st_SESSION* CBattleSnake_Master_LanServer::Alloc_Session(UINT64 clientKey, UINT64 accountNo)
{
	st_SESSION *pSession = _sessionPool.Alloc();
	Init_Session(pSession, clientKey, accountNo);

	EnterCriticalSection(&_sessionLock);
	//AcquireSRWLockExclusive(&_sessionLock);
	_sessionMap.insert(std::map<UINT64, st_SESSION*>::value_type(clientKey, pSession));
	LeaveCriticalSection(&_sessionLock);
	//ReleaseSRWLockExclusive(&_sessionLock);

	return pSession;
}

void CBattleSnake_Master_LanServer::Free_Session(UINT64 clientKey)
{
	std::map<UINT64, st_SESSION*>::iterator iter;
	st_SESSION *pSession;

	EnterCriticalSection(&_sessionLock);
	iter = _sessionMap.find(clientKey);
	if (iter == _sessionMap.end())
	{
		LeaveCriticalSection(&_sessionLock);
		return;
	}

	pSession = iter->second;
	_sessionMap.erase(iter);
	LeaveCriticalSection(&_sessionLock);

	_sessionPool.Free(pSession);
}

CBattleSnake_Master_LanServer::st_SESSION* CBattleSnake_Master_LanServer::Find_SessionByClientKey(UINT64 clientKey)
{
	std::map<UINT64, st_SESSION*>::iterator iter;
	st_SESSION* pSession = nullptr;
	EnterCriticalSection(&_sessionLock);
	//AcquireSRWLockShared(&_sessionLock);
	iter = _sessionMap.find(clientKey);
	if (iter == _sessionMap.end())
	{
		LeaveCriticalSection(&_sessionLock);
		//ReleaseSRWLockShared(&_sessionLock);
		return nullptr;
	}
	pSession = iter->second;
	LeaveCriticalSection(&_sessionLock);
	//ReleaseSRWLockShared(&_sessionLock);

	return pSession;
}

CBattleSnake_Master_LanServer::st_SESSION* CBattleSnake_Master_LanServer::Find_SessionByAccountNo(UINT64 accountNo, int roomNo)
{
	std::map<UINT64, st_SESSION*>::iterator iter;
	std::map<UINT64, st_SESSION*> *pMap = &_sessionMap;
	st_SESSION *pSession = nullptr;

	EnterCriticalSection(&_sessionLock);
	//AcquireSRWLockShared(&_sessionLock);
	iter = pMap->begin();
	while (iter != pMap->end())
	{
		if (iter->second->accountNo == accountNo && iter->second->roomNo == roomNo)
		{
			pSession = iter->second;
			LeaveCriticalSection(&_sessionLock);
			//ReleaseSRWLockShared(&_sessionLock);
			return pSession;
		}
		++iter;
	}

	LeaveCriticalSection(&_sessionLock);
	//ReleaseSRWLockShared(&_sessionLock);
	//LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Find_SessionByAccountNo : CAN'T FIND SESSION %d", accountNo);
	return nullptr;
}


void CBattleSnake_Master_LanServer::Init_Session(st_SESSION *pSession, UINT64 clientKey, UINT64 accountNo)
{
	pSession->bLogOut = false;
	pSession->clientKey = clientKey;
	pSession->accountNo = accountNo;
	pSession->roomNo = 0;
	pSession->serverNo = 0;
}

void CBattleSnake_Master_LanServer::PacketProc(st_SERVER *pServer, cLanPacketPool *dsPacket)
{
	WORD type;
	dsPacket->Deserialize((char*)&type, sizeof(WORD));

	switch (type)
	{
	case en_PACKET_MAT_MAS_REQ_SERVER_ON:
		Request_Match_Login(pServer, dsPacket);
		break;
	case en_PACKET_MAT_MAS_REQ_GAME_ROOM:
		Request_Match_RoomInfo(pServer, dsPacket);
		break;
	case en_PACKET_MAT_MAS_REQ_ROOM_ENTER_SUCCESS:
		Request_Match_UserEnterSuccess(pServer, dsPacket);
		break;
	case en_PACKET_MAT_MAS_REQ_ROOM_ENTER_FAIL:
		Request_Match_UserEnterFail(pServer, dsPacket);
		break;
	case en_PACKET_BAT_MAS_REQ_SERVER_ON:
		Request_Battle_Login(pServer, dsPacket);
		break;
	case en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN:
		Request_Battle_ReissueToken(pServer, dsPacket);
		break;
	case en_PACKET_BAT_MAS_REQ_CREATED_ROOM:
		Request_Battle_RoomCreated(pServer, dsPacket);
		break;
	case en_PACKET_BAT_MAS_REQ_CLOSED_ROOM:
		Request_Battle_RoomClosed(pServer, dsPacket);
		break;
	case en_PACKET_BAT_MAS_REQ_LEFT_USER:
		Request_Battle_UserLogout(pServer, dsPacket);
		break;
	default:
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - PacketProc : Wrong packet type");
		disconnect(pServer->clientID);
		break;
	};
}

void CBattleSnake_Master_LanServer::Request_Match_Login(st_SERVER *pServer, cLanPacketPool *dsPacket)
{
	int serverNo;
	char masterToken[32];

	dsPacket->Deserialize((char*)&serverNo, sizeof(int));
	dsPacket->Deserialize(masterToken, sizeof(char) * 32);

	pServer->serverType = MATCH_SERVER;

	InterlockedIncrement((long*)&_matchConnectServer);

	if (memcmp(masterToken, _masterToken, sizeof(char) * 32) != 0)
	{
		LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Request_Match_Login: DIFFERENT MASTER TOKEN [%d] [%d]", pServer->serverNo);
		return;
	}

	pServer->serverNo = serverNo;
	pServer->bLogin = true;
	Response_Match_Login(pServer);
}

void CBattleSnake_Master_LanServer::Request_Match_RoomInfo(st_SERVER *pServer, cLanPacketPool *dsPacket)
{
	UINT64 clientKey;
	UINT64 accountNo;

	dsPacket->Deserialize((char*)&clientKey, sizeof(UINT64));
	dsPacket->Deserialize((char*)&accountNo, sizeof(UINT64));

	st_SESSION *pSession = Find_SessionByClientKey(clientKey);
	if (pSession == nullptr)
	{
		pSession = Alloc_Session(clientKey, accountNo);
	}

	Response_Match_RoomInfo(pServer, pSession);
}

void CBattleSnake_Master_LanServer::Request_Match_UserEnterSuccess(st_SERVER *pServer, cLanPacketPool *dsPacket)
{
	WORD battleServerNo;
	int roomNo;
	UINT64 clientKey;
	st_SESSION *pSession;
	st_WAIT_ROOM *pRoom;

	dsPacket->Deserialize((char*)&battleServerNo, sizeof(WORD));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&clientKey, sizeof(UINT64));

	pSession = Find_SessionByClientKey(clientKey);

	if (pSession != nullptr)
	{
		//Response_Match_RoomEnter(pServer, clientKey);
	}
}

void CBattleSnake_Master_LanServer::Request_Match_UserEnterFail(st_SERVER *pServer, cLanPacketPool *dsPacket)
{
	UINT64 clientKey;
	dsPacket->Deserialize((char*)&clientKey, sizeof(UINT64));

	st_SESSION *pSession = Find_SessionByClientKey(clientKey);

	if (pSession != nullptr)
	{
		//	해당 방의 가용 인원 수를 갱신한다.

		if (InterlockedExchange8((char*)&pSession->bLogOut, 1) == 0)
		{
			Increment_WaitRoom(pSession->serverNo, pSession->roomNo, Room_Tracker::Match_EnterRoomFail, pSession->accountNo);
			Free_Session(clientKey);
		}
	}
}

void CBattleSnake_Master_LanServer::Response_Match_Login(st_SERVER *pServer)
{
	InterlockedIncrement((long*)&_matchLoginServer);

	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Match_Login(sPacket, pServer->serverNo);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Response_Match_RoomInfo(st_SERVER *pServer, st_SESSION *pSession)
{
	cLanPacketPool *sPacket;

	st_WAIT_ROOM *pRoom = Find_HighPriorityWaitRoom(pSession->accountNo);

	sPacket = cLanPacketPool::Alloc();

	if (pRoom == nullptr)
	{
		//LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Response_Match_RoomInfo : Fail to find wait room");
		SerializePacket_Match_RoomInfo(sPacket, pSession->clientKey, 0);
	}
	else
	{
		st_BATTLE_INFO *pBattle = Find_BattleInfo(pRoom->serverNo);
		if (pBattle == nullptr)
		{
			SerializePacket_Match_RoomInfo(sPacket, pSession->clientKey, 0);
		}
		else
		{
			pSession->roomNo = pRoom->roomNo;
			pSession->serverNo = pRoom->serverNo;

			SerializePacket_Match_RoomInfo(sPacket, pSession->clientKey, 1, (WORD)pBattle->serverNo, pBattle->battleServerIP, pBattle->battleServerPort,
										   pRoom->roomNo, pBattle->connectionToken, pRoom->enterToken, pBattle->chatServerIP, pBattle->chatServerPort);
		}
	}
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Response_Match_RoomEnter(st_SERVER *pServer, UINT64 clientKey)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Match_RoomEnter(sPacket, clientKey);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Request_Battle_Login(st_SERVER* pServer, cLanPacketPool *dsPacket)
{
	WCHAR	serverIP[16];
	WORD	serverPort;
	char	connectToken[32];
	char	masterToken[32];
	WCHAR	chatIP[16];
	WORD	chatPort;

	dsPacket->Deserialize((char*)serverIP, sizeof(WCHAR) * 16);
	dsPacket->Deserialize((char*)&serverPort, sizeof(WORD));
	dsPacket->Deserialize(connectToken, sizeof(char) * 32);
	dsPacket->Deserialize(masterToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)chatIP, sizeof(WCHAR) * 16);
	dsPacket->Deserialize((char*)&chatPort, sizeof(WORD));

	pServer->serverType = BATTLE_SERVER;
	InterlockedIncrement((long*)&_battleConnectServer);

	if (memcmp(masterToken, _masterToken, sizeof(char) * 32) != 0)
	{
		LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Request_Battle_Login : DIFFERENT MASTER TOKEN [%d] [%d]", pServer->serverNo);
		disconnect(pServer->clientID);
		return;
	}

	InterlockedIncrement((long*)&_battleLoginServer);
	pServer->serverNo = Create_BattleServerNo();
	pServer->bLogin = true;

	Alloc_BattleInfo(pServer->serverNo, connectToken, serverIP, serverPort, chatIP, chatPort);
	Response_Battle_Login(pServer);
}

void CBattleSnake_Master_LanServer::Request_Battle_ReissueToken(st_SERVER* pServer, cLanPacketPool *dsPacket)
{
	UINT req;
	st_BATTLE_INFO *pBattle = Find_BattleInfo(pServer->serverNo);
	dsPacket->Deserialize(pBattle->connectionToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	Response_Battle_ReissueToken(pServer, pBattle, req);
}

void CBattleSnake_Master_LanServer::Request_Battle_RoomCreated(st_SERVER* pServer, cLanPacketPool *dsPacket)
{
	int serverNo;
	int roomNo;
	int maxUser;
	char enterToken[32];
	UINT req;
	
	dsPacket->Deserialize((char*)&serverNo, sizeof(int));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&maxUser, sizeof(int));
	dsPacket->Deserialize(enterToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	if (serverNo != pServer->serverNo)
	{
		LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Request_Battle_RoomCreated : DIFFERENT MASTER SERVERNO [%d]", serverNo);
		return;
	}

	if (Find_WaitRoom(serverNo, roomNo) != nullptr)
	{
		LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Master_LanServer - Request_Battle_RoomCreated : ALEADY EXISTS ROOM [%d] [%d]", serverNo,  roomNo);
		return;
	}

	Alloc_WaitRoom(serverNo, roomNo, maxUser, enterToken);
	Response_Battle_RoomCreated(pServer, roomNo, req);
}
void CBattleSnake_Master_LanServer::Request_Battle_RoomClosed(st_SERVER* pServer, cLanPacketPool *dsPacket)
{
	int roomNo;
	UINT req;

	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	Free_WaitRoom(pServer->serverNo, roomNo);
	Response_Battle_RoomClosed(pServer, roomNo, req);
}

void CBattleSnake_Master_LanServer::Request_Battle_UserLogout(st_SERVER *pServer, cLanPacketPool *dsPacket)
{
	int roomNo;
	INT64 accountNo;
	UINT req;
	st_WAIT_ROOM *pRoom;
	st_SESSION *pSession;

	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&accountNo, sizeof(UINT64));
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	pSession = Find_SessionByAccountNo(accountNo, roomNo);

	if (pSession != nullptr)
	{
		if (InterlockedExchange8((char*)&pSession->bLogOut, 1) == 0)
		{
			Increment_WaitRoom(pSession->serverNo, roomNo, Room_Tracker::Battle_LeaveRoom, pSession->accountNo);
			Free_Session(pSession->clientKey);
		}
	}

	Response_Battle_UserLogout(pServer, roomNo, req);
}

void CBattleSnake_Master_LanServer::Response_Battle_Login(st_SERVER *pServer)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_Login(sPacket, pServer->serverNo);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Response_Battle_ReissueToken(st_SERVER* pServer, st_BATTLE_INFO *pBattle, UINT req)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_ReissueToken(sPacket, pBattle->connectionToken, req);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Response_Battle_RoomCreated(st_SERVER* pServer, int roomNo, UINT req)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_RoomCreated(sPacket, roomNo, req);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Response_Battle_RoomClosed(st_SERVER *pServer, int roomNo, UINT req)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_RoomClosed(sPacket, roomNo, req);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::Response_Battle_UserLogout(st_SERVER *pServer, int roomNo, UINT req )
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_UserLogout(sPacket, roomNo, req);
	sendPacket(pServer->clientID, sPacket);
}

void CBattleSnake_Master_LanServer::SerializePacket_Match_Login(cLanPacketPool *sPacket, int serverNo)
{
	WORD type = en_PACKET_MAT_MAS_RES_SERVER_ON;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&serverNo, sizeof(int));
}

void CBattleSnake_Master_LanServer::SerializePacket_Match_RoomInfo(cLanPacketPool *sPacket, UINT64 clientKey, BYTE status,
																   WORD battleServerNo, WCHAR *battleIP, WORD battlePort, int roomNo,
																   char *connectToken, char *enterToken,
																   WCHAR *chatIP, WORD chatPort)
{
	WORD type = en_PACKET_MAT_MAS_RES_GAME_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&clientKey, sizeof(UINT64));
	sPacket->Serialize((char*)&status, sizeof(BYTE));

	if (status == 0) return;

	sPacket->Serialize((char*)&battleServerNo, sizeof(WORD));
	sPacket->Serialize((char*)battleIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&battlePort, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize(connectToken, sizeof(char) * 32);
	sPacket->Serialize(enterToken, sizeof(char) * 32);
	sPacket->Serialize((char*)chatIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&chatPort, sizeof(WORD));
}

void CBattleSnake_Master_LanServer::SerializePacket_Match_RoomEnter(cLanPacketPool *sPacket, UINT64 clientKey)
{
	WORD type = en_PACKET_MAT_MAS_RES_ROOM_ENTER_SUCCESS;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&clientKey, sizeof(UINT64));
}

void CBattleSnake_Master_LanServer::SerializePacket_Battle_Login(cLanPacketPool *sPacket, int serverNo)
{
	WORD type = en_PACKET_BAT_MAS_RES_SERVER_ON;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&serverNo, sizeof(int));
}

void CBattleSnake_Master_LanServer::SerializePacket_Battle_ReissueToken(cLanPacketPool *sPacket, char *connectToken, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_RES_CONNECT_TOKEN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}

void CBattleSnake_Master_LanServer::SerializePacket_Battle_RoomCreated(cLanPacketPool *sPacket, int roomNo, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_RES_CREATED_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}

void CBattleSnake_Master_LanServer::SerializePacket_Battle_RoomClosed(cLanPacketPool *sPacket, int roomNo, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_RES_CLOSED_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}

void CBattleSnake_Master_LanServer::SerializePacket_Battle_UserLogout(cLanPacketPool *sPacket, int roomNo, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_RES_LEFT_USER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}