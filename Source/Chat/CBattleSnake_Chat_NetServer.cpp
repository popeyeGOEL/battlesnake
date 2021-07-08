#include "CLanClient.h"
#include "CBattleSnake_ChatMonitor_LanClient.h"
#include "CBattleSnake_Chat_LanClient.h"

#include "CNetServer.h"
#include "CBattleSnake_Chat_NetServer.h"

#include "CBattleSnake_Chat_LocalMonitor.h"

#include <process.h>
#include "CommonProtocol.h"
#include "cCrashDump.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"

CBattleSnake_Chat_NetServer::CBattleSnake_Chat_NetServer(BYTE packetCode, BYTE packetKey1, BYTE packetKey2, WCHAR *accessIP) : CNetServer(packetCode, packetKey1, packetKey2)
{
	memcpy(_accessIP, accessIP, sizeof(WCHAR) * 16);
	InitializeConditionVariable(&_cnBufNotEmpty);
	InitializeSRWLock(&_threadLock);
	//InitializeCriticalSection(&_threadLock);

	_pChatClient = new CBattleSnake_Chat_LanClient(this);
	_pMonitorClient = new CBattleSnake_ChatMonitor_LanClient(this);

	CBattleSnake_Chat_LocalMonitor *g_pMonitor = CBattleSnake_Chat_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this);

}
CBattleSnake_Chat_NetServer::~CBattleSnake_Chat_NetServer()
{
	_msgQueue.ClearBuffer();
	_msgPool.ClearBuffer();

	Clear_RoomMap();
	_roomPool.ClearBuffer();

	Clear_PlayerMap();
	_playerPool.ClearBuffer();
	delete _pChatClient;
	delete _pMonitorClient;
}

void CBattleSnake_Chat_NetServer::OnClientJoin(UINT64 clientID)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_JOIN;
	pMsg->clientID = clientID;

	_msgQueue.Enqueue(pMsg);

	WakeConditionVariable(&_cnBufNotEmpty);
}
void CBattleSnake_Chat_NetServer::OnClientLeave(UINT64 clientID)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_LEAVE;
	pMsg->clientID = clientID;

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);
}
bool CBattleSnake_Chat_NetServer::OnConnectionRequest(WCHAR *ip, short port)
{
	return true;
}
void CBattleSnake_Chat_NetServer::OnRecv(UINT64 clientID, cNetPacketPool *dsPacket)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_PACKET;
	pMsg->clientID = clientID;

	cNetPacketPool::AddRef(dsPacket);
	pMsg->pPacket = dsPacket;

	_msgQueue.Enqueue(pMsg);
	WakeAllConditionVariable(&_cnBufNotEmpty);
}
void CBattleSnake_Chat_NetServer::OnSend(UINT64 clientID, int sendSize)
{

}
void CBattleSnake_Chat_NetServer::OnError(int errorCode, WCHAR *errText)
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
	wsprintf(buf, L"ChatNetServer_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

///-----------------------------------------------------
///	Start Chat NetServer
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Start_Chat_NetServer(void)
{
	DWORD dwThreadID;
	_bExitMonitor = false;

	memset(_connectToken, 0, sizeof(char) * 32);
	

	_pdhCounter.InitProcessCounter(L"BattleSnake_Chat");
	_hMessageThread = (HANDLE)_beginthreadex(NULL, 0, MessageThread, (PVOID)this, 0, (unsigned int*)&dwThreadID);
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&dwThreadID);
}
void CBattleSnake_Chat_NetServer::Stop_Chat_Server(void)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_TERMINATE;
	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);

	DWORD status = WaitForSingleObject(_hMessageThread, INFINITE);
	CloseHandle(_hMessageThread);

	_bExitMonitor = true;
	status = WaitForSingleObject(_hMonitorThread, INFINITE);
	CloseHandle(_hMonitorThread);

	_msgQueue.ClearBuffer();

	//	채팅방 정리
	Clear_RoomMap();
	_roomPool.ClearBuffer();

	//	플레이어 정리
	Clear_PlayerMap();
	_playerPool.ClearBuffer();

	_pdhCounter.TerminatePdhCounter();
}

///-----------------------------------------------------
///	Start Chat LanClient
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Set_ChatLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_chatLanClient_IP, ip, sizeof(WCHAR) * 16);
	_chatLanClient_port = port;
	_chatLanClient_threadCnt = threadCnt;
	_chatLanClient_nagle = bNagle;
	_chatLanClient_config = true;
}
bool CBattleSnake_Chat_NetServer::Connect_Chat_LanClient(void)
{
	if (_chatLanClient_config == false) return false;
	if (_pChatClient->Connect(_chatLanClient_IP, _chatLanClient_port, _chatLanClient_threadCnt, _chatLanClient_nagle) == false) return false;
	return true;
}
bool CBattleSnake_Chat_NetServer::Disconnect_Chat_LanClient(void)
{
	return _pChatClient->Disconnect();
}

///-----------------------------------------------------
///	Start Monitor LanClient
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Set_MonitorLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_monitorLanClient_IP, ip, sizeof(WCHAR) * 16);
	_monitorLanClient_port = port;
	_monitorLanClient_threadCnt = threadCnt;
	_monitorLanClient_nagle = bNagle;
	_monitorLanClient_config = true;
}
bool CBattleSnake_Chat_NetServer::Connect_Monitor_LanClient(void)
{
	if (_monitorLanClient_config == false) return false;
	return _pMonitorClient->Connect(_monitorLanClient_IP, _monitorLanClient_port, _monitorLanClient_threadCnt, _monitorLanClient_nagle);
}
bool CBattleSnake_Chat_NetServer::Disconnect_Monitor_LanClient(void)
{
	return _pMonitorClient->Disconnect();
}

///-----------------------------------------------------
///	Response from battle server
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Receive_BattleRequest_Logout(void)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_DISCONNECTED;

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);
}
void CBattleSnake_Chat_NetServer::Receive_BattleRequest_ReissueConnectToken(cLanPacketPool * dsPacket)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_REISSUE_TOKEN;

	cLanPacketPool::AddRef(dsPacket);
	pMsg->pPacket = dsPacket;

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);
}
void CBattleSnake_Chat_NetServer::Receive_BattleRequest_CreateRoom(cLanPacketPool *dsPacket)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_CREATE_ROOM;

	cLanPacketPool::AddRef(dsPacket);
	pMsg->pPacket = dsPacket;

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);
}

void CBattleSnake_Chat_NetServer::Receive_BattleRequest_DeleteRoom(cLanPacketPool *dsPacket)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_DELETE_ROOM;

	cLanPacketPool::AddRef(dsPacket);
	pMsg->pPacket = dsPacket;

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);
}


///-----------------------------------------------------
///	HeartBeat
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Request_Check_NotPlayingUser(void)
{
	static int prevTime = timeGetTime();
	int curTime = timeGetTime();
	if (curTime - prevTime < 10000) return;

	st_MESSAGE *pMsg = _msgPool.Alloc();

	pMsg->type = en_MSG_TYPE_HEARTBEAT;

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnBufNotEmpty);

	prevTime = timeGetTime();
}

void CBattleSnake_Chat_NetServer::Check_NotPlayingUser(void)
{
	std::map<UINT64, st_PLAYER*> *map = &_playerMap;
	std::map<UINT64, st_PLAYER*>::iterator iter = map->begin();
	DWORD curTime;

	while (iter != map->end())
	{
		curTime = timeGetTime();
		if (curTime - iter->second->lastHeartBeat > 40000)
		{
			LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Check_NotPlayingUser : Not Playing User diff[%d] accountNo[%d] roomNo[%d]", 
				curTime - iter->second->lastHeartBeat, iter->second->accountNo, iter->second->roomNo);
			disconnect(iter->second->clientID);
		}
		++iter;
	}
}

///-----------------------------------------------------
///	Message Thread
///-----------------------------------------------------
unsigned int CBattleSnake_Chat_NetServer::MessageThread(void *arg)
{
	CBattleSnake_Chat_NetServer *pThis = (CBattleSnake_Chat_NetServer*)arg;
	return pThis->Update_MessageThread();
}
unsigned int CBattleSnake_Chat_NetServer::Update_MessageThread(void)
{
	st_MESSAGE *pMsg;
	st_PLAYER *pPlayer;
	cNetPacketPool::AllocPacketChunk();
	cLanPacketPool::AllocPacketChunk();

	while (1)
	{
		pMsg = nullptr;
		AcquireSRWLockExclusive(&_threadLock);
		//EnterCriticalSection(&_threadLock);
		while (_msgQueue.IsEmpty())
		{
			SleepConditionVariableSRW(&_cnBufNotEmpty, &_threadLock, INFINITE, 0);
			//SleepConditionVariableCS(&_cnBufNotEmpty, &_threadLock, INFINITE);
		}
		_msgQueue.Dequeue(pMsg);

		switch (pMsg->type)
		{
		case en_MSG_TYPE_PACKET:
		{
			pPlayer = Find_PlayerByClientID(pMsg->clientID);
			if (pPlayer == nullptr)
			{
				LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Update_MessageThread(en_MSG_TYPE_PACKET) : Not login user [%lld]", pPlayer->clientID);
				disconnect(pPlayer->clientID);
			}
			else if (Player_PacketProc(pPlayer, (cNetPacketPool*)pMsg->pPacket) == false)
			{
				LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Update_MessageThread(en_MSG_TYPE_PACKET) : Fail packet proc [%lld]", pPlayer->clientID);
				disconnect(pPlayer->clientID);
			}

			cNetPacketPool::Free((cNetPacketPool*)pMsg->pPacket);
			break;
		}
		case en_MSG_TYPE_JOIN:
		{
			Alloc_Player(pMsg->clientID);
			break;
		}
		case en_MSG_TYPE_LEAVE:
		{
			pPlayer = Find_PlayerByClientID(pMsg->clientID);
			if (pPlayer == nullptr)
			{
				LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Update_MessageThread(en_MSG_TYPE_LEAVE) : Can't find user [%lld]", pPlayer->clientID);
				disconnect(pPlayer->clientID);
				break;
			}

			if (pPlayer->bEnter)
			{
				DeletePlayer_Room(pPlayer, pPlayer->roomNo);
			}

			Free_Player(pPlayer);
			break;
		}
		case en_MSG_TYPE_CREATE_ROOM:
		{
			Request_Server_CreateRoom((cLanPacketPool*)pMsg->pPacket);
			break;
		}
		case en_MSG_TYPE_DELETE_ROOM:
		{
			Request_Server_DeleteRoom((cLanPacketPool*)pMsg->pPacket);
			break;
		}
		case en_MSG_TYPE_REISSUE_TOKEN:
		{
			Request_Server_ReissueToken((cLanPacketPool*)pMsg->pPacket);
			break;
		}
		case en_MSG_TYPE_DISCONNECTED:
		{
			Request_Server_Disconnected();
			break;
		}
		case en_MSG_TYPE_HEARTBEAT:
		{
			Check_NotPlayingUser();
			break;
		}
		case en_MSG_TYPE_TERMINATE:
		{
			_msgPool.Free(pMsg);
			ReleaseSRWLockExclusive(&_threadLock);
			//LeaveCriticalSection(&_threadLock);
			goto stop;
		}
		};

		_msgPool.Free(pMsg);
		++_updateCounter;
		ReleaseSRWLockExclusive(&_threadLock);
		//LeaveCriticalSection(&_threadLock);
		//Sleep(0);	//	 채팅서버 과부하 방지용 Sleep
	};


stop:
	cLanPacketPool::FreePacketChunk();
	cNetPacketPool::FreePacketChunk();
	return 0;
}

///-----------------------------------------------------
///	Monitor Thread
///-----------------------------------------------------
unsigned int CBattleSnake_Chat_NetServer::MonitorThread(void *arg)
{
	CBattleSnake_Chat_NetServer *pThis = (CBattleSnake_Chat_NetServer*)arg;
	return pThis->Update_MonitorThread();
}
unsigned int CBattleSnake_Chat_NetServer::Update_MonitorThread(void)
{
	bool *bExitMonitor = &_bExitMonitor;

	long prevTime = timeGetTime();
	long curTime = 0;
	long timeInterval;

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

		_monitor_updateFPS = _updateCounter / timeInterval;
		_monitor_playerPool = _playerPool.GetTotalBlockCount();
		_monitor_playerUsed = _playerPool.GetUsedBlockCount();
		_monitor_roomPool = _roomPool.GetTotalBlockCount();
		_monitor_roomUsed = _roomPool.GetUsedBlockCount();
		_monitor_msgPool = _msgPool.GetTotalBlockCount();
		_monitor_msgUsed = _msgPool.GetUsedBlockCount();
		_monitor_packetPool = cLanPacketPool::GetUsedPoolSize() + cNetPacketPool::GetUsedPoolSize();

		_pdhCounter.UpdatePdhQueryData();
		_pdhCounter.GetProcessStatus(bytes);
		_monitor_privateBytes = bytes / 1048576;

		_cpuCounter.UpdateCpuTime();
		_monitor_processCpuUsage = _cpuCounter.GetProcessTotalUsage();

		InterlockedExchange((long*)&_updateCounter, 0);

		if (IsServerOn())
		{
			if (_pChatClient->isConnected() == false)
			{
				Connect_Chat_LanClient();
			}

			if (_pMonitorClient->isConnected() == false)
			{
				Connect_Monitor_LanClient();
			}
		}

		prevTime = curTime;
	}

	cLanPacketPool::FreePacketChunk();
	timeEndPeriod(1);
	return 0;
}


///-----------------------------------------------------
///	Player Allocation
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Alloc_Player(UINT64 clientID)
{
	st_PLAYER *newPlayer = _playerPool.Alloc();
	newPlayer->clientID = clientID;
	_playerMap.insert(std::map<UINT64, st_PLAYER*>::value_type(clientID, newPlayer));
}
void CBattleSnake_Chat_NetServer::Free_Player(st_PLAYER *pPlayer)
{
	_playerMap.erase(pPlayer->clientID);
	Init_Player(pPlayer);
	_playerPool.Free(pPlayer);
}
CBattleSnake_Chat_NetServer::st_PLAYER * CBattleSnake_Chat_NetServer::Find_PlayerByClientID(UINT64 clientID)
{
	std::map<UINT64, st_PLAYER*>::iterator iter;
	std::map<UINT64, st_PLAYER*> *pMap = &_playerMap;

	iter = pMap->find(clientID);
	if (iter == pMap->end())
	{
		return nullptr;
	}

	return iter->second;
}

CBattleSnake_Chat_NetServer::st_PLAYER * CBattleSnake_Chat_NetServer::Find_PlayerByAccountNo(INT64 accountNo)
{
	std::map<UINT64, st_PLAYER*>::iterator iter;
	std::map<UINT64, st_PLAYER*> *pMap = &_playerMap;

	iter = pMap->begin();
	while (iter != pMap->end())
	{
		if (iter->second->accountNo == accountNo)
		{
			return iter->second;
		}
		++iter;
	}

	return nullptr;
}

void CBattleSnake_Chat_NetServer::Init_Player(st_PLAYER *pPlayer)
{
	pPlayer->accountNo = 0;
	pPlayer->clientID = 0;
	pPlayer->roomNo = 0;
	pPlayer->bEnter = false;
	memset(pPlayer->ID, 0, sizeof(WCHAR) * 20);
	memset(pPlayer->nick, 0, sizeof(WCHAR) * 20);
	pPlayer->lastHeartBeat = timeGetTime();
}

void CBattleSnake_Chat_NetServer::Clear_PlayerMap(void)
{
	std::map<UINT64, st_PLAYER*> *pMap = &_playerMap;
	std::map<UINT64, st_PLAYER*>::iterator iter = pMap->begin();

	st_PLAYER *pPlayer;

	while (iter != pMap->end())
	{
		pPlayer = iter->second;
		_playerPool.Free(pPlayer);
		iter = pMap->erase(iter);
	}
}

///-----------------------------------------------------
///	Room Allocation
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::AddPlayer_Room(st_PLAYER *pPlayer, st_ROOM *pRoom)
{
	pRoom->playerList.push_back(pPlayer);
	pPlayer->roomNo = pRoom->roomNo;
	pPlayer->bEnter = true;
	++pRoom->curUser;

	LOG(L"Enter Room", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Chat_NetServer - AddPlayer_Room : player accountNo [%lld] roomNo[%d] curUser[%d]", pPlayer->accountNo, pRoom->roomNo, pRoom->curUser);
}
void CBattleSnake_Chat_NetServer::DeletePlayer_Room(st_PLAYER *pPlayer, int roomNo)
{
	std::map<int, st_ROOM*>::iterator iter;
	iter = _roomMap.find(roomNo);

	if (iter == _roomMap.end()) return;

	st_ROOM *pRoom = iter->second;
	--pRoom->curUser;

	std::list<st_PLAYER*>::iterator playerIter = pRoom->playerList.begin();
	while (playerIter != pRoom->playerList.end())
	{
		if (*playerIter == pPlayer)
		{
			pPlayer->bEnter = false;
			pRoom->playerList.erase(playerIter);
			return;
		}
		playerIter++;
	}

	LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - DeletePlayer_Room : Can't delete user");
}

void CBattleSnake_Chat_NetServer::Alloc_Room(int roomNo, int maxUser, char * enterToken)
{
	st_ROOM *pRoom = _roomPool.Alloc();
	pRoom->roomNo = roomNo;
	pRoom->maxUser = maxUser;
	memcpy(pRoom->enterToken, enterToken, sizeof(char) * 32);
	pRoom->curUser = 0;
	pRoom->playerList.clear();
	_roomMap.insert(std::map<int, st_ROOM*>::value_type(pRoom->roomNo, pRoom));
}
void CBattleSnake_Chat_NetServer::Free_Room(st_ROOM *pRoom)
{
	_roomMap.erase(pRoom->roomNo);
	Clear_Room(pRoom);
	_roomPool.Free(pRoom);
}
bool CBattleSnake_Chat_NetServer::Free_Room(int roomNo)
{
	st_ROOM *pRoom = Find_Room(roomNo);
	if (pRoom == nullptr) return false;
	Clear_Room(pRoom);
	_roomPool.Free(pRoom);
	return true;
}
CBattleSnake_Chat_NetServer::st_ROOM* CBattleSnake_Chat_NetServer::Find_Room(int roomNo)
{
	std::map<int, st_ROOM*>::iterator iter;
	iter = _roomMap.find(roomNo);

	if (iter == _roomMap.end()) return nullptr;
	return iter->second;
}

void CBattleSnake_Chat_NetServer::Clear_Room(st_ROOM *pRoom)
{
	//	아직 방에서 나가지 않은 유저들은 퇴장시킨다. 
	std::list<st_PLAYER*>::iterator iter = pRoom->playerList.begin();
	st_PLAYER *pPlayer;
	while (iter != pRoom->playerList.end())
	{
		pPlayer = *iter;
		iter = pRoom->playerList.erase(iter);
		pPlayer->bEnter = false;
		disconnect(pPlayer->clientID);
	}
}

void CBattleSnake_Chat_NetServer::Clear_RoomMap(void)
{
	std::map<int, st_ROOM*> *pMap = &_roomMap;
	std::map<int, st_ROOM*>::iterator iter;
	st_ROOM *pRoom;

	iter = pMap->begin();
	while (iter != pMap->end())
	{
		//	Clear
		//	Erase
		//	Free
		pRoom = iter->second;
		Clear_Room(pRoom);
		iter = pMap->erase(iter);
		_roomPool.Free(pRoom);
	}
}

///-----------------------------------------------------
///	Packet Proc
///-----------------------------------------------------
void CBattleSnake_Chat_NetServer::Request_Server_Disconnected(void)
{
	Clear_RoomMap();
}
void CBattleSnake_Chat_NetServer::Request_Server_ReissueToken(cLanPacketPool *dsPacket)
{
	UINT req;
	dsPacket->Deserialize(_connectToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	cLanPacketPool::Free(dsPacket);
	_pChatClient->Response_Battle_ReissueConnectToken(req);
}

void CBattleSnake_Chat_NetServer::Request_Server_CreateRoom(cLanPacketPool *dsPacket)
{
	int battleNo;
	int roomNo;
	int maxUser;
	char enterToken[32];
	UINT req;

	dsPacket->Deserialize((char*)&battleNo, sizeof(int));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&maxUser, sizeof(int));
	dsPacket->Deserialize(enterToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	if (Find_Room(roomNo) != nullptr)
	{
		cCrashDump::Crash();
	}

	cLanPacketPool::Free(dsPacket);
	Alloc_Room(roomNo, maxUser, enterToken);
	_pChatClient->Response_Battle_CreateRoom(roomNo, req);
}

void CBattleSnake_Chat_NetServer::Request_Server_DeleteRoom(cLanPacketPool *dsPacket)
{
	int battleNo;
	int roomNo;
	UINT req;

	dsPacket->Deserialize((char*)&battleNo, sizeof(int));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&req, sizeof(UINT));

	st_ROOM *pRoom = Find_Room(roomNo);
	if (pRoom == nullptr)
	{
		cCrashDump::Crash();
	}

	cLanPacketPool::Free(dsPacket);
	Free_Room(pRoom);
	_pChatClient->Response_Battle_CloseRoom(roomNo, req);
}

bool CBattleSnake_Chat_NetServer::Player_PacketProc(st_PLAYER *pPlayer, cNetPacketPool *dsPacket)
{
	WORD type;

	try
	{
		dsPacket->Deserialize((char*)&type, sizeof(WORD));
		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
			Request_Player_Login(pPlayer, dsPacket);
			break;
		case en_PACKET_CS_CHAT_REQ_ENTER_ROOM:
			Request_Player_EnterRoom(pPlayer, dsPacket);
			break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
			Request_Player_SendChat(pPlayer, dsPacket);
			break;
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			Request_Player_HeartBeat(pPlayer, dsPacket);
			break;
		default:
			LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - PacketProc : Packet type error");
			return false;
		};
	}
	catch (cNetPacketPool::packetException &e)
	{
		return false;
	};

	return true;
}

void CBattleSnake_Chat_NetServer::Request_Player_Login(st_PLAYER *pPlayer, cNetPacketPool *dsPacket)
{
	INT64 accountNo;
	char connectToken[32];
	BYTE status = 1;
	dsPacket->Deserialize((char*)&accountNo, sizeof(INT64));

	//st_PLAYER *pUser = Find_PlayerByAccountNo(accountNo);
	//if (pUser != nullptr && pUser->bEnter == true)
	//{
	//	LOG(L"ChatServer", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Chat_NetServer - Request_Player_Login : Already login user %lld, %lld", pUser->accountNo, accountNo);
	//	disconnect(pUser->clientID);
	//	//status = 0;
	//}

	pPlayer->accountNo = accountNo;
	dsPacket->Deserialize((char*)pPlayer->ID, sizeof(WCHAR) * 20);
	dsPacket->Deserialize((char*)pPlayer->nick, sizeof(WCHAR) * 20);
	dsPacket->Deserialize(connectToken, sizeof(char) * 32);

	if (memcmp(_connectToken, connectToken, sizeof(char) * 32) != 0)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_Login : Not match connectToken");
		status = 0;
	}

	pPlayer->lastHeartBeat = timeGetTime();

	Response_Player_Login(pPlayer, accountNo, status);
}
void CBattleSnake_Chat_NetServer::Request_Player_EnterRoom(st_PLAYER *pPlayer, cNetPacketPool *dsPacket)
{
	BYTE status = 1;
	INT64 accountNo;
	int roomNo;
	char enterToken[32];

	dsPacket->Deserialize((char*)&accountNo, sizeof(INT64));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize(enterToken, sizeof(char) * 32);

	if (pPlayer->accountNo != accountNo)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_EnterRoom : Not same accountno");
		disconnect(pPlayer->accountNo);
		return;
	}

	st_ROOM *pRoom = Find_Room(roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_EnterRoom : No exist room");
		status = 3;
	}
	else if (memcmp(pRoom->enterToken, enterToken, sizeof(char) * 32) != 0)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_EnterRoom : Not match entertoken");
		status = 2;
	}
	//else if (pRoom->curUser == pRoom->maxUser)
	//{
	//	LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_EnterRoom : Can't add player to room [No %d] [curUser %d]", pRoom->roomNo, pRoom->curUser);
	//	cCrashDump::Crash();
	//	disconnect(pPlayer->clientID);
	//}
	else
	{
		AddPlayer_Room(pPlayer, pRoom);
	}

	pPlayer->lastHeartBeat = timeGetTime();

	Response_Player_EnterRoom(pPlayer, roomNo, status);
}
void CBattleSnake_Chat_NetServer::Request_Player_SendChat(st_PLAYER *pPlayer, cNetPacketPool *dsPacket)
{
	INT64 accountNo;
	WORD msgLen;

	dsPacket->Deserialize((char*)&accountNo, sizeof(INT64));
	if (accountNo != pPlayer->accountNo)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_SendChat : Not match accountNo");
		disconnect(pPlayer->clientID);
		return;
	}

	dsPacket->Deserialize((char*)&msgLen, sizeof(WORD));

	if (msgLen > dsPacket->GetDataSize())
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Request_Player_SendChat : Message length is too long");
		disconnect(pPlayer->clientID);
		return;
	}

	WCHAR *msg = new WCHAR[msgLen / 2];
	dsPacket->Deserialize((char*)msg, msgLen);
	pPlayer->lastHeartBeat = timeGetTime();
	
	Response_Player_SendChat(pPlayer, msg, msgLen);
	delete[] msg;
}
void CBattleSnake_Chat_NetServer::Request_Player_HeartBeat(st_PLAYER *pPlayer, cNetPacketPool *dsPacket)
{
	pPlayer->lastHeartBeat = timeGetTime();
}

void CBattleSnake_Chat_NetServer::Response_Player_Login(st_PLAYER *pPlayer, INT64 accountNo, BYTE status)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_Login(sPacket, status, accountNo);
	sendPacket(pPlayer->clientID, sPacket);
}
void CBattleSnake_Chat_NetServer::Response_Player_EnterRoom(st_PLAYER *pPlayer, int roomNo, BYTE status)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_EnterRoom(sPacket, pPlayer->accountNo, roomNo, status);
	sendPacket(pPlayer->clientID, sPacket);
}
void CBattleSnake_Chat_NetServer::Response_Player_SendChat(st_PLAYER *pPlayer, WCHAR *msg, WORD msgLen)
{
	cNetPacketPool  *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_SendChat(sPacket, pPlayer->accountNo, pPlayer->ID, pPlayer->nick, msgLen, msg);
	Send_RoomCast(pPlayer, sPacket);
}

void CBattleSnake_Chat_NetServer::SerializePacket_Player_Login(cNetPacketPool *sPacket, BYTE status, INT64 accountNo)
{
	WORD type = en_PACKET_CS_CHAT_RES_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&status, sizeof(BYTE));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
}
void CBattleSnake_Chat_NetServer::SerializePacket_Player_EnterRoom(cNetPacketPool *sPacket, INT64 accountNo, int roomNo, BYTE status)
{
	WORD type = en_PACKET_CS_CHAT_RES_ENTER_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&status, sizeof(BYTE));
}
void CBattleSnake_Chat_NetServer::SerializePacket_Player_SendChat(cNetPacketPool *sPacket, INT64 accountNo, WCHAR *ID, WCHAR *nick, WORD msgLen, WCHAR *msg)
{
	WORD type = en_PACKET_CS_CHAT_RES_MESSAGE;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)ID, sizeof(WCHAR) * 20);
	sPacket->Serialize((char*)nick, sizeof(WCHAR) * 20);
	sPacket->Serialize((char*)&msgLen, sizeof(WORD));
	sPacket->Serialize((char*)msg, msgLen);
}

void CBattleSnake_Chat_NetServer::Send_RoomCast(st_PLAYER *pPlayer, cNetPacketPool *sPacket)
{
	st_ROOM *pRoom = Find_Room(pPlayer->roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_NetServer - Send_RoomCast : Can't find room accountNo : %d", pPlayer->accountNo);
		cNetPacketPool::Free(sPacket);
		disconnect(pPlayer->clientID);
		return;
	}

	std::list<st_PLAYER*>::iterator iter = pRoom->playerList.begin();
	st_PLAYER* pUser;
	while (iter != pRoom->playerList.end())
	{
		pUser = *iter;
		cNetPacketPool::AddRef(sPacket);
		sendPacket(pUser->clientID, sPacket);
		++iter;
	}

	cNetPacketPool::Free(sPacket);
}