#include "CLanServer.h"
#include "CBattleSnake_Battle_LanServer.h"

#include "CLanClient.h"
#include "CBattleSnake_Battle_LanClient.h"
#include "CBattleSnake_BattleMonitor_LanClient.h"

#include "cHttp_Async.h"
#include "CBattleSnake_Battle_Http.h"

#include "CNetSession.h"
#include "CBattleSnake_BattlePlayer_NetSession.h"

#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"

#include "CBattleSnake_Battle_LocalMonitor.h"

#include <math.h>
#include <process.h>
#include "ContentsData.h"
#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

CBattleSnake_Battle_MMOServer *g_pBattle = nullptr;

CBattleSnake_Battle_MMOServer::CBattleSnake_Battle_MMOServer(int maxSession, int maxBattleRoom, int defaultWaitRoom, int maxRoomPlayer, int authSleepCount, int gameSleepCount, int sendSleepCount,
							  UINT verCode, char *connectToken, char* masterToken, BYTE packetCode, BYTE packetKey1, BYTE packetKey2, WCHAR *accessIP) : CMMOServer(maxSession, authSleepCount, gameSleepCount, sendSleepCount, packetCode, packetKey1, packetKey2)
{
	_maxRoom = maxBattleRoom;
	_verCode = verCode;
	_curRoomID = 0;
	_curOpenRoom = 0;
	_monitor_loginPlayer = 0;
	_defaultWaitRoom = defaultWaitRoom;
	_maxRoomPlayer = maxRoomPlayer;

	memcpy(_connectToken, connectToken, sizeof(char) * 32);
	memcpy(_accessIP, accessIP, sizeof(WCHAR) * 16);

	_playerArray = new CBattleSnake_BattlePlayer_NetSession[dfMAX_BATTLE_PLAYER];
	for (int i = 0; i < maxSession; ++i)
	{
		//_playerArray[i].InitPlayer();
		_playerArray[i].SetPacketCode(packetCode, packetKey1, packetKey2);
		_playerArray[i].InitSession();
		_playerArray[i].InitBuffer();
		_playerArray[i]._acceptOl.arrayIndex = i;
		SetSessionArray((CNetSession*)&_playerArray[i], _playerArray[i]._acceptOl.arrayIndex);
	}

	_roomArray = new st_ROOM[maxBattleRoom];
	for (int i = 0; i < maxBattleRoom; ++i)
	{
		_roomArray[i].maxUser = _maxRoomPlayer;
		_roomArray[i].roomPlayer = new CBattleSnake_BattlePlayer_NetSession*[_maxRoomPlayer];

		_roomIndexQueue.Enqueue(i);
		InitRoom(&_roomArray[i]);
	}

	CBattleSnake_Battle_LocalMonitor *g_pMonitor = CBattleSnake_Battle_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this, CBattleSnake_Battle_LocalMonitor::Factor_Battle_MMOServer);


	g_pHttp = new CBattleSnake_Battle_Http;	
	_pLanClient = new CBattleSnake_Battle_LanClient(this, masterToken);
	_pLanServer = new CBattleSnake_Battle_LanServer(this);
	_pMonitorClient = new CBattleSnake_BattleMonitor_LanClient(this);

	//	테스트용
	//	wsprintf(_chatServerIP, L"127.0.0.1");
	_chatServerPort = 11630;
}

CBattleSnake_Battle_MMOServer::~CBattleSnake_Battle_MMOServer()
{
	delete g_pHttp;
	delete _pLanClient;
	delete _pLanServer;
	delete _pMonitorClient;

	for (int i = 0; i < _maxRoom; ++i)
	{
		delete[] _roomArray[i].roomPlayer;
	}

	delete[] _roomArray;
	delete[] _playerArray;
}


void CBattleSnake_Battle_MMOServer::OnError(int errorCode, WCHAR *errorText)
{
	errno_t err;
	FILE *fp;
	time_t timer;
	struct tm t;
	WCHAR buf[256] = { 0, };
	WCHAR logBuf[256] = { 0, };
	wsprintf(logBuf, errorText);

	time(&timer);
	localtime_s(&t, &timer);
	wsprintf(buf, L"GameServer_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}
///-----------------------------------------------------
///	Start Battle Server
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::Start_Battle(void)
{
	DWORD threadID;
	_bExitMonitor = false;
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_Battle_MMOServer::Stop_Battle(void)
{
	_bExitMonitor = true;
	DWORD status = WaitForSingleObject(_hMonitorThread, INFINITE);
	CloseHandle(_hMonitorThread);
}

///-----------------------------------------------------
///	Start Battle LanServer
///-----------------------------------------------------
bool CBattleSnake_Battle_MMOServer::Start_Battle_LanServer(WCHAR *ip, short port, int threadCnt, bool bNagle, int maxUser)
{
	return _pLanServer->start(ip, port, threadCnt, bNagle, maxUser);
}
bool CBattleSnake_Battle_MMOServer::Stop_Battle_LanServer(void)
{
	_pLanServer->stop();
	return true;
}

///-----------------------------------------------------
///	Connect Battle LanClient
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::Set_BattleLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_battleLanClient_IP, ip, sizeof(WCHAR) * 16);
	_battleLanClient_port = port;
	_battleLanClient_threadCnt = threadCnt;
	_battleLanClient_nagle = bNagle;
	_battleLanClient_config = true;
}

bool CBattleSnake_Battle_MMOServer::Connect_Battle_LanClient(void)
{
	if (_battleLanClient_config == false) return false;
	return _pLanClient->Connect(_battleLanClient_IP, _battleLanClient_port, _battleLanClient_threadCnt, _battleLanClient_nagle);
}
bool CBattleSnake_Battle_MMOServer::Disconnect_Battle_LanClient(void)
{
	return _pLanClient->Disconnect();
}

///-----------------------------------------------------
///	Connect Http Client
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::Set_HttpURL(WCHAR *url, int len, int type)
{
	switch (type)
	{
	case 1:
		g_pHttp->Set_SelectAccountURL(url, len);
		break;
	case 2:
		g_pHttp->Set_SelectContentsURL(url, len);
		break;
	case 3:
		g_pHttp->Set_UpdateContentsURL(url, len);
		break;
	};
}

bool CBattleSnake_Battle_MMOServer::Connect_Battle_HttpClient(int threadCnt, bool bNagle, int maxUser)
{
	if (g_pHttp->StartHttpClient(threadCnt, bNagle, maxUser) == false) return false;
	g_pHttp->Start_MessageThread();

	return true;
}

bool CBattleSnake_Battle_MMOServer::Disconnect_Battle_HttpClient(void)
{
	if (g_pHttp->StopHttpClient() == false) return false;
	g_pHttp->Stop_MessageThread();

	return true;
}

void CBattleSnake_Battle_MMOServer::Check_HttpHeartBeat(void)
{
	g_pHttp->Send_Http_Heartbeat();
}

///-----------------------------------------------------
///	Connect Monitor LanClient
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::Set_MonitorLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle)
{
	memcpy(_monitorLanClient_IP, ip, sizeof(WCHAR) * 16);
	_monitorLanClient_port = port;
	_monitorLanClient_threadCnt = threadCnt;
	_monitorLanClient_nagle = bNagle;
	_monitorLanClient_config = true;
}

bool CBattleSnake_Battle_MMOServer::Connect_Monitor_LanClient(void)
{
	if (_monitorLanClient_config == false) return false;
	return _pMonitorClient->Connect(_monitorLanClient_IP, _monitorLanClient_port, _monitorLanClient_threadCnt, _monitorLanClient_nagle);
}
bool CBattleSnake_Battle_MMOServer::Disconnect_Monitor_LanClient(void)
{
	return _pMonitorClient->Disconnect();
}

///-----------------------------------------------------
///	Find Session
///-----------------------------------------------------
CBattleSnake_BattlePlayer_NetSession * CBattleSnake_Battle_MMOServer::Find_PlayerByClientID(UINT64 clientID)
{
	int index = ClientIDtoIndex(clientID);

	if (_playerArray[index]._clientInfo.clientID == clientID)
	{
		return &_playerArray[index];
	}

	return nullptr;
}
CBattleSnake_BattlePlayer_NetSession * CBattleSnake_Battle_MMOServer::Find_PlayerByAccountNo(UINT64 accountNo)
{
	for (int i = 0; i < dfMAX_BATTLE_PLAYER; ++i)
	{
		if (accountNo == _playerArray[i]._accountNo)
		{
			return &_playerArray[i];
		}
	}
	return nullptr;
}


void CBattleSnake_Battle_MMOServer::OnAuth_Update(void)
{
	//	특정 시간마다 
	//	Connect Token 발행 후 마스터 서버에게 알림
	//	Update_ConnectToken();

	//	특정 조건마다
	//	현재 방 개수가 일정 개수 미만이면 방생성 후 마스터 서버에게 알림

	if (_pLanClient->IsLogin() == true && _pLanServer->IsLogin() == true)
	{
		Update_RoomCount();
	}

	//	방 순회
	//	Wait
	//	1. 모든 방을 돌면서 현재 방 인원이 꽉차면 전체 유저에게 준비 패킷 전송.Room_Ready로 변경

	//	Ready
	//	2. 모든 방을 돌면서 Ready 상태로부터 10초 경과시 Start 패킷을 보낸다. 이와함께 해당 방의 유저들의 좌표와 hp를 지정해준다. 

	st_ROOM *pRoom;
	int maxRoom = _maxRoom;

	UINT64 waitRoom = 0;
	for (int i = 0; i < maxRoom; ++i)
	{
		pRoom = &_roomArray[i];
		switch (pRoom->status)
		{
		case Room_Wait:
			Update_Room_Wait(pRoom);
			++waitRoom;
			break;
		case Room_Ready:
			Update_Room_Ready(pRoom);
			break;
		};
	}
	
	_monitor_battleWaitRoom = waitRoom;
}

void CBattleSnake_Battle_MMOServer::OnGame_Update(void)
{
	//	방 순회
	//	Play
	//	3. 모든 방을 돌면서 레드존 좌표와 유저 좌표를 비교하여 닿았다면 체력을 깎는다. 1초마다, 만약 사망시 해당 룸의 전체 유저에게 사망 패킷을 알린다. 
	//	4. 모든 방을 돌면서 새로운 레드존이 활성화 되기 20초 전에 경고를 한다.
	//	5. 모든 방을 돌면서 최종 1명의 플레이어가 살아 남았을때, 유저들에게 결과 패킷 전송, db 업데이트

	//	End
	//	6. 모든 방을 돌면서 배틀이 끝난 방에 대하여 모든 유저가 나갔는지, 또는 5초가 지났는지 확인하여 방 파괴

	st_ROOM *pRoom;

	int maxRoom = _maxRoom;
	UINT64 playRoom = 0;

	for (int i = 0; i < maxRoom; ++i)
	{
		pRoom = &_roomArray[i];
		switch (pRoom->status)
		{
		case Room_Play:
			Update_Room_Play(pRoom);
			++playRoom;
			break;
		case Room_End:
			Update_Room_End(pRoom);
			break;
		};
	}

	_monitor_battlePlayRoom = playRoom;

	//	Always (10초마다 확인하기)
	//	0. 배열 순회 -> Check HeartBeat
	// Update_HeartBeat();
}

void CBattleSnake_Battle_MMOServer::Update_HeartBeat(void)
{
	static int heartPeriod = timeGetTime();
	int curTime = timeGetTime();

	if (curTime - heartPeriod < 10000) return;

	for (int i = 0; i < dfMAX_BATTLE_PLAYER; ++i)
	{
		if (_playerArray[i]._bLogin == false) continue;
		if (curTime - _playerArray[i]._lastRecvTime < 90000) continue;
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Update_HeartBeat : Not Playing User diff[%d] accountNo[%d] roomNo[%d] mode[%d]",
			curTime - _playerArray[i]._lastRecvTime, _playerArray[i]._accountNo, _playerArray[i]._roomNo, _playerArray[i]._mode);

		_playerArray[i].Disconnect();
	}

	heartPeriod = curTime;
}

void CBattleSnake_Battle_MMOServer::Update_ConnectToken(void)
{
	static DWORD tokenPeriod = timeGetTime();
	DWORD curTime = timeGetTime();
	BYTE randNum;

	if (curTime - tokenPeriod < 300000) return;

	for (int i = 0; i < 32; ++i)
	{
		randNum = rand() % 128;
		_connectToken[i] = randNum;
	}

	_pLanServer->Send_ConnectToken(_connectToken);
	_pLanClient->Send_Master_ReissueConnectToken(_connectToken);
	tokenPeriod = curTime;
}

void CBattleSnake_Battle_MMOServer::Update_RoomCount(void)
{
	st_ROOM *pRoom = nullptr;
	
	while (_curOpenRoom < _defaultWaitRoom)
	{
		pRoom = Open_Room();
		if (pRoom == nullptr) break;
		TrackRoom(pRoom, Room_Tracker::Send_Master_CreateRoom);

		_pLanServer->Send_CreateRoom(_battleNo, pRoom->roomNo, pRoom->maxUser, pRoom->enterToken);
		_pLanClient->Send_Master_CreatedRoom(pRoom->roomNo, pRoom->maxUser, pRoom->enterToken);

		++_curOpenRoom;
	}

}

void CBattleSnake_Battle_MMOServer::Update_Room_Wait(st_ROOM *pRoom)
{
	DWORD curTime = timeGetTime();

	if (pRoom->bReady == true && pRoom->bReceived == true)
	{
		if (curTime - pRoom->receiveTime > 1000)
		{
			TrackRoom(pRoom, Room_Tracker::Room_Change_Ready);
			pRoom->status = Room_Ready;
			pRoom->readyTime = timeGetTime();
			Response_Room_ReadySec(pRoom, dfDELAY_READY_COUNT);
		}
	}
}
void CBattleSnake_Battle_MMOServer::Update_Room_Ready(st_ROOM *pRoom)
{
	int curTime = timeGetTime();
	if (curTime - pRoom->readyTime >= 10000)
	{
		CBattleSnake_BattlePlayer_NetSession *pPlayer;
		for (int i = 0; i < _maxRoomPlayer; ++i)
		{
			pPlayer = pRoom->roomPlayer[i];
			if (pPlayer == nullptr) continue;

			//	hp 및 좌표 세팅 & Game Mode
			pPlayer->_moveTarget.moveTargetX = g_Data_Position[i][0];
			pPlayer->_moveTarget.moveTargetZ = g_Data_Position[i][1];
			pPlayer->_hp = g_Data_HP;

			pPlayer->SetMode_GAME();
		}

		Response_Room_Start(pRoom);

		pRoom->aliveUser = pRoom->curUser;
		pRoom->startTime = curTime;
		pRoom->redZone_lastUpdateTime = curTime;
		pRoom->redZone_nextActiveTime = pRoom->startTime + 40000;

		TrackRoom(pRoom, Room_Tracker::Room_Change_Play);
		pRoom->status = Room_Play;
	}
}
void CBattleSnake_Battle_MMOServer::Update_Room_Play(st_ROOM *pRoom)
{
	DWORD curTime = timeGetTime();
	CBattleSnake_BattlePlayer_NetSession *pPlayer = nullptr;

	//	1초마다 체크
	if (curTime - pRoom->redZone_lastUpdateTime > 1000)
	{

		if (pRoom->redZone_nextActiveTime <= curTime && pRoom->redZone_totalActive < 4)
		{
			//	Active Red Zone
			switch (pRoom->redZone[pRoom->redZone_totalActive])
			{
			case RedZone_Left:
				Response_Room_RedZoneActive_Left(pRoom);
				pRoom->map_top = 50;
				break;
			case RedZone_Top:
				Response_Room_RedZoneActive_Top(pRoom);
				pRoom->map_left = 44;
				break;
			case RedZone_Right:
				Response_Room_RedZoneActive_Right(pRoom);
				pRoom->map_bottom = 115;
				break;
			case RedZone_Bottom:
				Response_Room_RedZoneActive_Bottom(pRoom);
				pRoom->map_right = 102;
				break;
			}


			++pRoom->redZone_totalActive;
			pRoom->redZone_nextActiveTime = curTime + 40000;
			pRoom->redZone_bPreAlerted = false;
		}
		else if ((pRoom->redZone_bPreAlerted == false) && (pRoom->redZone_nextActiveTime <= 20000 + curTime))
		{
			//	Alert Red Zone
			switch (pRoom->redZone[pRoom->redZone_totalActive])
			{
			case RedZone_Left:
				Response_Room_RedZoneAlert_Left(pRoom,20);
				break;
			case RedZone_Top:
				Response_Room_RedZoneAlert_Top(pRoom, 20);
				break;
			case RedZone_Right:
				Response_Room_RedZoneAlert_Right(pRoom, 20);
				break;
			case RedZone_Bottom:
				Response_Room_RedZoneAlert_Bottom(pRoom,20);
				break;
			}

			pRoom->redZone_bPreAlerted = true;
		}


		//	Check RedZone Damage
		
		//	Hp 차감, 방인원 전체에게 전송
		//	만약 Die일 경우, Record 갱신 후, (http 전송 안함)
		//	방 인원 전체에게 Die 전송
		for(int i = 0 ; i < _maxRoomPlayer; ++i)
		{
			pPlayer = pRoom->roomPlayer[i];
			if (pPlayer == nullptr) continue;
			if (pPlayer->_isDead == false)
			{
				if (pPlayer->_moveTarget.moveTargetX < pRoom->map_left || pPlayer->_moveTarget.moveTargetX > pRoom->map_right ||
					pPlayer->_moveTarget.moveTargetZ < pRoom->map_top || pPlayer->_moveTarget.moveTargetZ > pRoom->map_bottom)
				{
					pPlayer->_hp = max(0, pPlayer->_hp - 1);

					Response_Room_RedZoneDamage(pRoom, pPlayer->_accountNo, pPlayer->_hp);

					if (pPlayer->_hp == 0)
					{
						Response_Room_Die(pRoom, pPlayer->_accountNo);
						++pPlayer->_curRecord.die;
						pPlayer->_curRecord.playTime += (timeGetTime() - pRoom->startTime);
						--pRoom->aliveUser;
						pPlayer->_isDead = true;
					}
				}
			}
		}


		pRoom->redZone_lastUpdateTime = curTime;
	}

	int curUser = pRoom->curUser;
		//Check Result
	if (pRoom->aliveUser <= 1)
	{
		pRoom->status = Room_End;
		pRoom->endTime = curTime;

		for(int i = 0; i < _maxRoomPlayer; ++i)
		{
			pPlayer = pRoom->roomPlayer[i];
			if (pPlayer == nullptr) continue;

			if (pPlayer->_isDead == true)
			{
				pPlayer->Response_Player_GameOver();
			}
			else if(pPlayer->_isDead == false)
			{
				pPlayer->Response_Player_Winner();
				++pPlayer->_curRecord.win;
				pPlayer->_curRecord.playTime += (timeGetTime() - pRoom->startTime);
			}

			++pRoom->sendResultCount;
			++pPlayer->_curRecord.playCount;
			pPlayer->Response_Player_Record(false);
		}
	}
}
void CBattleSnake_Battle_MMOServer::Update_Room_End(st_ROOM *pRoom)
{
	DWORD curTime = timeGetTime();

	if ((curTime - pRoom->endTime > 5000) || pRoom->curUser == 0)
	{
		TrackRoom(pRoom, Room_Tracker::Room_Change_Close);
		pRoom->status = Room_Close;

		LOG(L"RoomEnd", cSystemLog::LEVEL_SYSTEM, L"#CBattleSnake_Battle_MMOServer - Update_Room_End : [%d]", pRoom->roomNo);
		_pLanServer->Send_CloseRoom(_battleNo, pRoom->roomNo);

		for (int i = 0; i < pRoom->maxUser; ++i)
		{
			if (pRoom->roomPlayer[i] != nullptr)
			{
				pRoom->roomPlayer[i]->Disconnect();
			}
		}

		int index = RoomNoToIndex(pRoom->roomNo);
		pRoom->roomNo = 0;
		_roomIndexQueue.Enqueue(index);
	}
}

///-----------------------------------------------------
///	Request from Session
///-----------------------------------------------------
void	CBattleSnake_Battle_MMOServer::Request_EnterRoom(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	//	로그인 성공 후 매칭 서버에서 가져온 방정보를 요청
	//	roomNo와 EnterToken이 맞다면, 해당 유저를 방정보에 추가시킨다. 
	//	Response_Player_EnterRoom를 호출.
	//	Response_Player_AddUser 호출

	BYTE result;
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		result = Fail_NoExistRoom;
	}
	else if (memcmp(pRoom->enterToken, pPlayer->_enterToken, sizeof(char) * 32) != 0)
	{
		result = Fail_NotMatch_EnterToken;
	}
	else if (pRoom->status != Room_Wait)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_EnterRoom : Not waiting room roomNo[%d] status[%d] curUser[%d]", pRoom->roomNo, pRoom->status, pRoom->curUser);
		result = Fail_NotWaitRoom;
		cCrashDump::Crash();
	}
	else if (pRoom->curUser == pRoom->maxUser)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_EnterRoom : Player full roomNo[%d] status[%d] curUser[%d]", pRoom->roomNo, pRoom->status, pRoom->curUser);
		result = Fail_PlayerFull;
		cCrashDump::Crash();
	}
	else
	{
		result = Success_Enter;
	}

	pPlayer->Response_Player_EnterRoom(_maxRoomPlayer, result);

	if (result == Success_Enter)
	{
		CBattleSnake_BattlePlayer_NetSession *pIter = nullptr;

		cNetPacketPool *sPacket;

		//	입장시 기존 유저 정보를 나에게 전송
		for (int i = 0; i < _maxRoomPlayer; ++i)
		{
			pIter = pRoom->roomPlayer[i];
			if (pIter == nullptr) continue;

			sPacket = cNetPacketPool::Alloc();
			SerializePacket_Room_AddPlayer(sPacket, pRoom->roomNo, pIter->_accountNo, pIter->_nickName, pIter->_curRecord);
			pPlayer->SendPacket(sPacket);
		}

		if (AddPlayer_Room(pRoom, pPlayer) == false)
		{
			cCrashDump::Crash();
		}

		TrackRoom(pRoom, Room_Tracker::Player_EnterRoom);

		//	방 인원 모두에게 내 정보 전송(본인 포함)
		InterlockedIncrement((long*)&pRoom->curUser);
		Response_Room_AddPlayer(pRoom, pPlayer->_accountNo, pPlayer->_nickName, pPlayer->_curRecord);


		if (pRoom->curUser == pRoom->maxUser)
		{
			if (pRoom->bReady == false)
			{
				--_curOpenRoom;
				pRoom->bReady = true;
				TrackRoom(pRoom, Room_Tracker::Send_Master_DeleteRoom, true);
				_pLanClient->Send_Master_CloseRoom(pRoom->roomNo);
			}
		}
	}
}

void	CBattleSnake_Battle_MMOServer::Request_Character_Move(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_Move : Can't find room [%lld]", pPlayer->_accountNo);
		pPlayer->Disconnect();
		return;
	}

	Response_Room_MoveCharacter(pRoom, pPlayer->_accountNo, pPlayer->_moveTarget, pPlayer->_hitPoint);
}
void	CBattleSnake_Battle_MMOServer::Request_Character_CreateCharacter(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	//	Response_Player_Start를 보내고, Game Thread에 진입한 유저.
	//	해당 플레이어가 속해있는 room의 본인을 제외한 다른 유저들에게 
	//	pPlayer->Response_Player_CreateOtherCharacter를 보낸다.
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		cCrashDump::Crash();
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_CreateCharacter : Can't find room [%lld]", pPlayer->_accountNo);
		pPlayer->Disconnect();
		return;
	}

	Response_Room_CreateCharacter(pRoom, pPlayer->_accountNo, pPlayer->_nickName, pPlayer->_moveTarget.moveTargetX, pPlayer->_moveTarget.moveTargetZ, pPlayer->_hp);
}
void	CBattleSnake_Battle_MMOServer::Request_Character_HitPoint(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_HitPoint : Can't find room [%lld]", pPlayer->_accountNo);
		pPlayer->Disconnect();
		return;
	}

	Response_Room_HitPointCharacter(pRoom, pPlayer->_accountNo, pPlayer->_hitPoint);
}
void	CBattleSnake_Battle_MMOServer::Request_Character_Fire(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_Fire : Can't find room [%lld]", pPlayer->_accountNo);
		pPlayer->Disconnect();
		return;
	}


	Response_Room_FireCharacter(pRoom, pPlayer->_accountNo, pPlayer->_hitPoint);
}
void	CBattleSnake_Battle_MMOServer::Request_Character_Reload(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_Reload : Can't find room [%lld]", pPlayer->_accountNo);
		pPlayer->Disconnect();
		return;
	}

	Response_Room_ReloadCharacter(pRoom, pPlayer->_accountNo);
}
void	CBattleSnake_Battle_MMOServer::Request_Character_HitDamage(CBattleSnake_BattlePlayer_NetSession *pPlayer, INT64 targetAccountNo)
{
	//	플레이어의 _targetAccountNo로 속해있는 room에서 플레이어를 찾아서 
	//	피격플레이어와 공격 플레이어간의 x, z 좌표 사이를 계산하여 2~17 사이의 비율을 구해
	//	g_Data_HitDamage에 비율을 적용하여 hp를 차감

	//	해당 플레이어가 속해있는 room의 본인을 포함한 모든 유저에게 
	//	피격 플레이어의 accountNo, hp, 공격 플레이어의 accountNo를 참조하여
	//	pPlayer->Response_Player_HitDamage 전송
	
	float	dX, dZ;
	float	distance, ratio;

	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_HitDamage : Can't find room [%lld]", pPlayer->_accountNo);
		pPlayer->Disconnect();
		return;
	}

	CBattleSnake_BattlePlayer_NetSession *pTarget = nullptr;
	CBattleSnake_BattlePlayer_NetSession *pIter = nullptr;


	//	Target Account 조회 & hp 계산
	for (int i = 0; i < _maxRoomPlayer; ++i)
	{
		pIter = pRoom->roomPlayer[i];
		if (pIter == nullptr) continue;
		if (pIter->_accountNo == targetAccountNo)
		{
			if (pIter->_isDead == true) break;

			pTarget = pIter;

			dX = pTarget->_moveTarget.moveTargetX - pPlayer->_moveTarget.moveTargetX;
			dZ = pTarget->_moveTarget.moveTargetZ - pPlayer->_moveTarget.moveTargetZ;

			distance = sqrtf(dX * dX + dZ * dZ);
			ratio = 1 - (distance - 2) / 15;

			if (ratio > 1) ratio = 1;
			else if (ratio < 0) ratio = 0;

			pTarget->_hp = int(max(0, pTarget->_hp - ratio * g_Data_HitDamage));
			break;
		}
	}

	if (pTarget == nullptr) return;

	//	Hit Damage Packet 전송
	Response_Room_HitDamage(pRoom, pPlayer->_accountNo, pTarget->_accountNo, pTarget->_hp);

	//	만약 Target의 hp가 0이면, Record 갱신 후 Die 전송, MedKit 생성
	if (pTarget->_hp == 0)
	{
		++pPlayer->_curRecord.kill;
		++pTarget->_curRecord.die;
		pPlayer->_curRecord.playTime += (timeGetTime() - pRoom->startTime);

		pTarget->_isDead = true;
		--pRoom->aliveUser;
		
		st_MEDKIT *pMedKit = Create_MedKit(pRoom, pTarget->_moveTarget.moveTargetX, pTarget->_moveTarget.moveTargetZ);

		Response_Room_Die(pRoom, pTarget->_accountNo);
		Response_Room_MedKitCreated(pRoom, pMedKit->medKitID, pMedKit->posX, pMedKit->posY);
	}
}
void	CBattleSnake_Battle_MMOServer::Request_Character_GetMedKit(CBattleSnake_BattlePlayer_NetSession *pPlayer, UINT medKitID)
{
	//	medKitID를 조회하여 해당 메드킷의 좌표와 플레이어의 현재 좌표를 비교하여
	//	오차범위가 -2,2미만이면 g_data_hp의 절반을 회복시키고, 
	//	해당 medKit은 list에서 삭제하고, Free 한다.
	//	해당 room의 모든 플레이어들에게 유저의 accountNo, medKitID, hp를 담아
	//	pPlayer->Response_Player_MedkitGet 응답한다.

	if (pPlayer->_hp == g_Data_HP) return;

	st_MEDKIT *pMed = Find_MedKitbyID(pPlayer->_roomNo, medKitID);
	if (pMed == nullptr) return;

	if (((abs(pPlayer->_moveTarget.moveTargetX - pMed->posX) < 2) &&
		(abs(pPlayer->_moveTarget.moveTargetZ - pMed->posY) < 2)) &&
		 (pPlayer->_isDead == false))
	{

		st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
		if (pRoom == nullptr)
		{
			LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Request_Character_GetMedKit : Can't find room [%lld]", pPlayer->_accountNo);
			pPlayer->Disconnect();
			return;
		}

		Free_MedKit(pPlayer->_roomNo, pMed);
		pPlayer->_hp = min(g_Data_HP, pPlayer->_hp + g_Data_HP / 2);

		Response_Room_MedkitGet(pRoom, pPlayer->_accountNo, medKitID, pPlayer->_hp);
	}
}


void	CBattleSnake_Battle_MMOServer::Request_Character_LeavePlayer(CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	//	해당 플레이어가 속해있는 방에서 유저를 찾아서 지운다.
	//	Master 서버로 _pLanClient->Send_Master_LeavePlayer를 전송
	st_ROOM *pRoom = Find_RoombyRoomNo(pPlayer->_roomNo);
	if (pRoom == nullptr) return;
	if (DeletePlayer_Room(pRoom, pPlayer) == false) return;
	InterlockedDecrement((long*)&pRoom->curUser);

	switch (pRoom->status)
	{
	case Room_Wait:
		TrackRoom(pRoom, Room_Tracker::Player_LeaveWaitRoom);
		Response_Room_LeaveWaitRoom(pRoom, pPlayer->_accountNo);
		if (pRoom->bReady == false)
		{
			TrackRoom(pRoom, Room_Tracker::Send_Master_LeavePlayer);
			_pLanClient->Send_Master_LeavePlayer(pPlayer->_roomNo, pPlayer->_accountNo);
		}
		break;
	case Room_Ready:
		TrackRoom(pRoom, Room_Tracker::Player_LeaveReadyRoom);
		Response_Room_LeaveWaitRoom(pRoom, pPlayer->_accountNo);
		break;
	case Room_Play:
	{
		//	해당 플레이어의 room 안에 있는 본인을 제외한 유저들에게 
		//	pPlayer->Response_Player_LeavePlayRoom 응답한다.
		TrackRoom(pRoom, Room_Tracker::Player_LeavePlayRoom);
		CBattleSnake_BattlePlayer_NetSession *pOther = nullptr;

		//	현재 플레이 중인 방이라면 
		//	해당 유저의 db에 갱신하고 (패킷은 보내지 않음)
		//	방안에 있는 유저들에게 다른 유저가 나갔음을 알린다.
		++pPlayer->_curRecord.playCount;
		pPlayer->_curRecord.playTime += ((timeGetTime() - pRoom->startTime));

		if (pPlayer->_isDead == false)
		{
			--pRoom->aliveUser;
			++pPlayer->_curRecord.die;
			pPlayer->_isDead = true;
			Response_Room_Die(pRoom, pPlayer->_accountNo);
		}

		pPlayer->Response_Player_Record(false);

		Response_Room_LeavePlayRoom(pRoom, pPlayer->_accountNo);
		break;
	}
	}
}
///-----------------------------------------------------
///	Response from master
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::Receive_MasterRequest_Login(void)
{
	// Connect Token 업데이트 
	_pLanClient->Send_Master_ReissueConnectToken(_connectToken);

	// 대기방 업데이트 
	st_ROOM *pRoom;
	for (int i = 0; i < _maxRoom; ++i)
	{
		pRoom = &_roomArray[i];
		if (pRoom->status == Room_Wait)
		{
			_pLanClient->Send_Master_CreatedRoom(pRoom->roomNo, pRoom->maxUser - pRoom->curUser, pRoom->enterToken);

			if (pRoom->curUser == pRoom->maxUser)
			{
				_pLanClient->Send_Master_CloseRoom(pRoom->roomNo);
			}
		}
	}
}

void CBattleSnake_Battle_MMOServer::Receive_MasterRequest_CreateRoom(int roomNo)
{
	//	해당 Room의 상태를 Room_Prepare_Open에서 Wait 상태로 변경
	st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_MasterRequest_CreateRoom : Can't find Room %d", roomNo);
		return;
	}
	if (pRoom->status != Room_Wait)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_MasterRequest_CreateRoom : Not Prepared Room %d", roomNo);
	}

	TrackRoom(pRoom, Room_Tracker::Recv_Master_CreateRoom);
}

void	CBattleSnake_Battle_MMOServer::Receive_MasterRequest_CloseRoom(int roomNo)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_MasterRequest_CloseRoom : Can't find Room %d", roomNo);
		return;
	}

	if (pRoom->status != Room_Wait)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_MasterRequest_CloseRoom : Not Wait Room %d", roomNo);
	}

	pRoom->bReceived = true;
	pRoom->receiveTime = timeGetTime();
	TrackRoom(pRoom, Room_Tracker::Recv_Master_DeleteRoom);
}
void	CBattleSnake_Battle_MMOServer::Receive_MasterRequest_LeavePlayer(int roomNo)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	if (pRoom == nullptr)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_MasterRequest_LeavePlayer : Can't find Room %d", roomNo);
		return;
	}

	TrackRoom(pRoom, Room_Tracker::Recv_Master_LeavePlayer);
}

///-----------------------------------------------------
///	Response from chat
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::Receive_ChatResponse_Login(void)
{
	//	ConnectToken 전송
	_pLanServer->Send_ConnectToken(_connectToken);

	//	대기방 업데이트 
	st_ROOM *pRoom;

	for (int i = 0; i < _maxRoom; ++i)
	{
		pRoom = &_roomArray[i];
		if (pRoom->status >= Room_Wait && pRoom->status <= Room_Play)
		{
			_pLanServer->Send_CreateRoom(_battleNo, pRoom->roomNo, pRoom->maxUser, pRoom->enterToken);
		}
	}

}
void CBattleSnake_Battle_MMOServer::Receive_ChatRequest_CreateRoom(int roomNo)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	if (pRoom == nullptr) return;
	if (pRoom->status != Room_Wait)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_ChatRequest_CreateRoom : Not Prepared Room %d", roomNo);
	}
}

void CBattleSnake_Battle_MMOServer::Receive_ChatRequest_CloseRoom(int roomNo)
{
	//st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	//if (pRoom == nullptr) return;
	//if (pRoom->status != Room_End && pRoom->status != Room_Close)
	//{
	//	LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - Receive_ChatRequest_CloseRoom : Not Closed Room%d", roomNo);
	//}
}

///-----------------------------------------------------
///	MedKit 
///-----------------------------------------------------

CBattleSnake_Battle_MMOServer::st_MEDKIT* CBattleSnake_Battle_MMOServer::Create_MedKit(st_ROOM *pRoom, float posX, float posY)
{
	st_MEDKIT *pMed = _medKitPool.Alloc();
	pMed->medKitID = InterlockedIncrement(&_curMedID);
	pMed->posX = posX;
	pMed->posY = posY;

	pRoom->medKitList.push_back(pMed);

	return pMed;
}

CBattleSnake_Battle_MMOServer::st_MEDKIT*	CBattleSnake_Battle_MMOServer::Find_MedKitbyID(int roomNo, UINT medkitID)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	if (pRoom == nullptr) return nullptr;

	std::list<st_MEDKIT*>::iterator iter;
	std::list<st_MEDKIT*> *pList = &pRoom->medKitList;
	st_MEDKIT *pMed = nullptr;

	iter = pList->begin();

	while (iter != pList->end())
	{
		pMed = *iter;
		if (pMed->medKitID == medkitID)
		{
			return pMed;
		}
		++iter;
	}

	return nullptr;
}

void CBattleSnake_Battle_MMOServer::Free_MedKit(int roomNo, st_MEDKIT* pMed)
{
	st_ROOM *pRoom = Find_RoombyRoomNo(roomNo);
	if (pRoom == nullptr) return;

	std::list<st_MEDKIT*>::iterator iter;
	std::list<st_MEDKIT*> *pList = &pRoom->medKitList;

	iter = pList->begin();

	while (iter != pList->end())
	{
		if (*iter == pMed)
		{
			pList->erase(iter);
			_medKitPool.Free(pMed);
			return;
		}
		++iter;
	}
}

void CBattleSnake_Battle_MMOServer::Clear_RoomMedKit(st_ROOM *pRoom)
{
	std::list<st_MEDKIT*>::iterator iter;
	std::list<st_MEDKIT*> *pList = &pRoom->medKitList;
	st_MEDKIT* pMed;

	iter = pList->begin();

	while (iter != pList->end())
	{
		pMed = *iter;
		iter = pList->erase(iter);
		_medKitPool.Free(pMed);
	}
}

///-----------------------------------------------------
///	Battle Room
///-----------------------------------------------------
void CBattleSnake_Battle_MMOServer::BroadCast_Room(cNetPacketPool *sPacket, st_ROOM *pRoom, UINT64 exceptAccountNo)
{
	CBattleSnake_BattlePlayer_NetSession *pPlayer = nullptr;

	for (int i = 0; i < _maxRoomPlayer; ++i)
	{
		pPlayer = pRoom->roomPlayer[i];
		if (pPlayer == nullptr || pPlayer->_accountNo == exceptAccountNo) continue;

		cNetPacketPool::AddRef(sPacket);
		pPlayer->SendPacket(sPacket);
	}

	cNetPacketPool::Free(sPacket);
}

CBattleSnake_Battle_MMOServer::st_ROOM* CBattleSnake_Battle_MMOServer::Open_Room(void)
{
	int index;
	if (_roomIndexQueue.Dequeue(index) == false) return nullptr;
	
	int roomNo = CreateRoomNo(index);
	InitRoom(&_roomArray[index]);

	TrackRoom(&_roomArray[index], Room_Tracker::Room_Change_Wait);
	_roomArray[index].roomNo = roomNo;
	_roomArray[index].status = Room_Wait;

	return &_roomArray[index];
}

CBattleSnake_Battle_MMOServer::st_ROOM* CBattleSnake_Battle_MMOServer::Find_RoombyRoomNo(int roomNo)
{
	int index = RoomNoToIndex(roomNo);
	st_ROOM *pRoom = &_roomArray[index];

	if (pRoom->roomNo != roomNo) return nullptr;

	return pRoom;
}

bool CBattleSnake_Battle_MMOServer::AddPlayer_Room(st_ROOM *pRoom, CBattleSnake_BattlePlayer_NetSession *pPlayer)
{
	for (int i = 0; i < _maxRoomPlayer; ++i)
	{
		if (pRoom->roomPlayer[i] == nullptr)
		{
			pRoom->roomPlayer[i] = pPlayer;
			return true;
		}
	}

	LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_MMOServer - AddPlayer_Room : Fail to add player[%d] to room[%d]", pPlayer->_accountNo, pRoom->roomNo);
	return false;
}
bool CBattleSnake_Battle_MMOServer::DeletePlayer_Room(st_ROOM *pRoom, CBattleSnake_BattlePlayer_NetSession *pPlayer)
{

	for (int i = 0; i < _maxRoomPlayer; ++i)
	{
		if (pRoom->roomPlayer[i] == pPlayer)
		{
			pRoom->roomPlayer[i] = nullptr;
			return true;
		}
	}

	LOG(L"BattleServer", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Battle_MMOServer - DeletePlayer_Room : Can't delete player %d, %d", pRoom->roomNo, pPlayer->_accountNo);
	return false;
}

int	CBattleSnake_Battle_MMOServer::RoomNoToIndex(int roomNo)
{
	int mask = 0xffff0000;
	return (roomNo & mask) >> 16;
}

int CBattleSnake_Battle_MMOServer::CreateRoomNo(int index)
{
	long ID = InterlockedIncrement(&_curRoomID);
	ID = ID & 0xffff;
	return  (ID  | (index << 16));
}

void CBattleSnake_Battle_MMOServer::InitRoom(st_ROOM *pRoom)
{
	BYTE random;
	pRoom->roomNo = 0;
	pRoom->bReady = false;
	pRoom->bReceived = false;

	//	Create EnterToken Ramdomly
	for (int i = 0; i < 32; ++i)
	{
		random = rand() % 128;
		pRoom->enterToken[i] = random;
	}

	memset(pRoom->roomPlayer, 0, sizeof(CBattleSnake_BattlePlayer_NetSession*)*_maxRoomPlayer);
	pRoom->aliveUser = 0;
	pRoom->curUser = 0;
	pRoom->sendResultCount = 0;
	pRoom->receiveTime = 0;
	pRoom->readyTime = 0;
	pRoom->startTime = 0;
	pRoom->endTime = 0;

	pRoom->redZone_bPreAlerted = false;
	pRoom->redZone_totalActive = 0;
	pRoom->redZone_nextActiveTime = 0;
	pRoom->redZone_lastUpdateTime = 0;

	pRoom->map_left = 0;
	pRoom->map_top = 0;
	pRoom->map_right = 153;
	pRoom->map_bottom = 170;

	//	Queue Redzone Area Info Randomly
	int temp;
	for (int i = 0; i < 4; ++i)
	{
		random = rand() % 4;
		temp = pRoom->redZone[i];
		pRoom->redZone[i] = pRoom->redZone[random];
		pRoom->redZone[random] = (RedZone_Info)temp;
	}

	Clear_RoomMedKit(pRoom);
}

void CBattleSnake_Battle_MMOServer::TrackRoom(st_ROOM *pRoom, Room_Tracker type, bool bReady)
{
	int trackCount = (InterlockedIncrement((long*)&pRoom->trackCount) % 500);
	pRoom->trackArray[trackCount].type = type;
	pRoom->trackArray[trackCount].curUser = pRoom->curUser;

	if (bReady == true)
	{
		WCHAR buffer[256];
		int count = wsprintf(buffer, L"Ready Room[%d] accountNo : ", pRoom->roomNo);

		for (int i = 0; i < pRoom->maxUser; ++i)
		{
			count += wsprintf(buffer + count, L"[%d] ", pRoom->roomPlayer[i]->_accountNo);
		}
		
		buffer[count] = L'\0';
		LOG(L"ReadyRoom", cSystemLog::LEVEL_SYSTEM, buffer);
	}
}

///-----------------------------------------------------
///	Room Packet Proc
///-----------------------------------------------------

void CBattleSnake_Battle_MMOServer::CBattleSnake_Battle_MMOServer::Response_Room_ReadySec(st_ROOM *pRoom, BYTE readySec)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_ReadySec(sPacket, pRoom->roomNo, readySec);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_Start(st_ROOM *pRoom)
{
	cNetPacketPool * sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_Start(sPacket, pRoom->roomNo);
	BroadCast_Room(sPacket, pRoom);
}	
void CBattleSnake_Battle_MMOServer::Response_Room_AddPlayer(st_ROOM *pRoom,INT64 accountNo, WCHAR *nickName, stRecord& record)
{
	cNetPacketPool * sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_AddPlayer(sPacket, pRoom->roomNo, accountNo, nickName, record);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_LeaveWaitRoom(st_ROOM *pRoom,INT64 accountNo)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_LeaveWaitRoom(sPacket, pRoom->roomNo, accountNo);
	BroadCast_Room(sPacket, pRoom);
}																			

void CBattleSnake_Battle_MMOServer::SerializePacket_Room_ReadySec(cNetPacketPool *sPacket, int roomNo, BYTE readySec)
{
	WORD type = en_PACKET_CS_GAME_RES_PLAY_READY;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&readySec, sizeof(BYTE));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_Start(cNetPacketPool* sPacket, int roomNo)
{
	WORD type = en_PACKET_CS_GAME_RES_PLAY_START;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_AddPlayer(cNetPacketPool* sPacket, int roomNo, INT64 accountNo, WCHAR *nickName, stRecord &record)
{
	int playtimeMin = record.playTime / 60000;

	WORD type = en_PACKET_CS_GAME_RES_ADD_USER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)nickName, sizeof(WCHAR) * 20);

	sPacket->Serialize((char*)&record.playCount, sizeof(int));
	sPacket->Serialize((char*)&playtimeMin, sizeof(int));
	sPacket->Serialize((char*)&record.kill, sizeof(int));
	sPacket->Serialize((char*)&record.die, sizeof(int));
	sPacket->Serialize((char*)&record.win, sizeof(int));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_LeaveWaitRoom(cNetPacketPool* sPacket, int roomNo, INT64 accountNo)
{
	WORD type = en_PACKET_CS_GAME_RES_REMOVE_USER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
}

void CBattleSnake_Battle_MMOServer::Response_Room_CreateCharacter(st_ROOM *pRoom,INT64 accountNo, WCHAR *nickName, float posX, float posY, int hp)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_CreateCharacter(sPacket, accountNo, nickName, posX, posY, hp);
	BroadCast_Room(sPacket, pRoom, accountNo);
}
void CBattleSnake_Battle_MMOServer::Response_Room_MoveCharacter(st_ROOM *pRoom, INT64 accountNo, stMoveTarget &move, stHitPoint &hit)
{
	//	해당 플레이어가 속해있는 방의 유저들에게 Move Character 패킷을 보낸다. (본인제외)
	//	해당 플레이어의 moveTarget 

	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_MoveChracter(sPacket, accountNo, move, hit);
	BroadCast_Room(sPacket, pRoom, accountNo);
}

void CBattleSnake_Battle_MMOServer::Response_Room_HitPointCharacter(st_ROOM *pRoom, INT64 accountNo, stHitPoint &hit)
{
	//	해당 플레이어가 속해있는 room의 본인을 제외한 다른 유저에게 보낸다.
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_HitPointCharacter(sPacket, accountNo, hit);
	BroadCast_Room(sPacket, pRoom, accountNo);
}				
void CBattleSnake_Battle_MMOServer::Response_Room_FireCharacter(st_ROOM *pRoom, INT64 accountNo, stHitPoint &hit)
{
	//	해당 플레이어가 속해있는 room의 본인을 제외한 다른 유저에게 보낸다.
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_FireChracter(sPacket, accountNo, hit);
	BroadCast_Room(sPacket, pRoom, accountNo);
}

void CBattleSnake_Battle_MMOServer::Response_Room_ReloadCharacter(st_ROOM *pRoom, INT64 accountNo)
{
	//	해당 플레이어가 속해있는 room의 본인을 제외한 다른 유저에게 보낸다.
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_ReloadCharacter(sPacket, accountNo);
	BroadCast_Room(sPacket, pRoom, accountNo);
}																		
void CBattleSnake_Battle_MMOServer::Response_Room_HitDamage(st_ROOM *pRoom, INT64 attackAccountNo, INT64 targetAccountNo, int targetHP)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_HitDamage(sPacket, attackAccountNo, targetAccountNo, targetHP);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneDamage(st_ROOM *pRoom, INT64 accountNo, int hp)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneDamage(sPacket, accountNo, hp);
	BroadCast_Room(sPacket, pRoom);
}																	
void CBattleSnake_Battle_MMOServer::Response_Room_Die(st_ROOM *pRoom, INT64 accountNo)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_Die(sPacket, accountNo);
	BroadCast_Room(sPacket, pRoom);
}																					
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneAlert_Left(st_ROOM *pRoom, BYTE alertTimeSec)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneAlert_Left(sPacket, alertTimeSec);
	BroadCast_Room(sPacket, pRoom);
}																	
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneAlert_Top(st_ROOM *pRoom, BYTE alertTimeSec)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneAlert_Top(sPacket, alertTimeSec);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneAlert_Right(st_ROOM *pRoom, BYTE alertTimeSec)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneAlert_Right(sPacket, alertTimeSec);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneAlert_Bottom(st_ROOM *pRoom, BYTE alertTimeSec)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneAlert_Bottom(sPacket, alertTimeSec);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneActive_Left(st_ROOM *pRoom)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneActive_Left(sPacket);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneActive_Top(st_ROOM *pRoom)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneActive_Top(sPacket);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneActive_Right(st_ROOM *pRoom)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneActive_Right(sPacket);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_RedZoneActive_Bottom(st_ROOM *pRoom)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_RedZoneActive_Bottom(sPacket);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_MedKitCreated(st_ROOM *pRoom, UINT medKitID, float posX, float posY)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_MedKitCreated(sPacket, medKitID, posX, posY);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_MedkitGet(st_ROOM *pRoom, INT64 accountNo, UINT medKitID, int hp)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_MedKitGet(sPacket, accountNo, medKitID, hp);
	BroadCast_Room(sPacket, pRoom);
}
void CBattleSnake_Battle_MMOServer::Response_Room_LeavePlayRoom(st_ROOM *pRoom, INT64 accountNo)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Room_LeavePlayRoom(sPacket, pRoom->roomNo, accountNo);
	BroadCast_Room(sPacket, pRoom, accountNo);
}

void CBattleSnake_Battle_MMOServer::SerializePacket_Room_CreateCharacter(cNetPacketPool* sPacket, INT64 accountNo, WCHAR* nickname,
																					   float posX, float posY, int hp)
{
	WORD type = en_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)nickname, sizeof(WCHAR) * 20);
	sPacket->Serialize((char*)&posX, sizeof(float));
	sPacket->Serialize((char*)&posY, sizeof(float));
	sPacket->Serialize((char*)&hp, sizeof(int));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_MoveChracter(cNetPacketPool *sPacket, INT64 accountNo, stMoveTarget &move, stHitPoint &hit)
{
	WORD type = en_PACKET_CS_GAME_RES_MOVE_PLAYER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&move, sizeof(stMoveTarget));
	sPacket->Serialize((char*)&hit, sizeof(stHitPoint));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_HitPointCharacter(cNetPacketPool* sPacket, INT64 accountNo, stHitPoint &hit)
{
	WORD type = en_PACKET_CS_GAME_RES_HIT_POINT;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&hit, sizeof(stHitPoint));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_FireChracter(cNetPacketPool* sPacket, INT64 accountNo, stHitPoint &hit)
{
	WORD type = en_PACKET_CS_GAME_RES_FIRE1;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&hit, sizeof(stHitPoint));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_ReloadCharacter(cNetPacketPool* sPacket, INT64 accountNo)
{
	WORD type = en_PACKET_CS_GAME_RES_RELOAD;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_HitDamage(cNetPacketPool* sPacket, INT64 attackAccountNo, INT64 targetAccountNo, int targetHP)
{
	WORD type = en_PACKET_CS_GAME_RES_HIT_DAMAGE;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&attackAccountNo, sizeof(INT64));
	sPacket->Serialize((char*)&targetAccountNo, sizeof(INT64));
	sPacket->Serialize((char*)&targetHP, sizeof(int));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneDamage(cNetPacketPool *sPacket, INT64 accountNo, int hp)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_DAMAGE;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&hp, sizeof(int));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_Die(cNetPacketPool *sPacket, INT64 accountNo)
{
	WORD type = en_PACKET_CS_GAME_RES_DIE;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneAlert_Left(cNetPacketPool* sPacket, BYTE alertTimeSec)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_LEFT;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&alertTimeSec, sizeof(BYTE));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneAlert_Top(cNetPacketPool* sPacket, BYTE alertTimeSec)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_TOP;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&alertTimeSec, sizeof(BYTE));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneAlert_Right(cNetPacketPool* sPacket, BYTE alertTimeSec)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_RIGHT;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&alertTimeSec, sizeof(BYTE));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneAlert_Bottom(cNetPacketPool* sPacket, BYTE alertTimeSec)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ALERT_BOTTOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&alertTimeSec, sizeof(BYTE));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneActive_Left(cNetPacketPool *sPacket)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_LEFT;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneActive_Right(cNetPacketPool *sPacket)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_RIGHT;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneActive_Top(cNetPacketPool *sPacket)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_TOP;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_RedZoneActive_Bottom(cNetPacketPool *sPacket)
{
	WORD type = en_PACKET_CS_GAME_RES_REDZONE_ACTIVE_BOTTOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_MedKitCreated(cNetPacketPool *sPacket, UINT medKitID, float posX, float posY)
{
	WORD type = en_PACKET_CS_GAME_RES_MEDKIT_CREATE;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&medKitID, sizeof(UINT));
	sPacket->Serialize((char*)&posX, sizeof(float));
	sPacket->Serialize((char*)&posY, sizeof(float));
}
void CBattleSnake_Battle_MMOServer::SerializePacket_Room_MedKitGet(cNetPacketPool *sPacket, INT64 accountNo, UINT medKitID, int hp)
{
	WORD type = en_PACKET_CS_GAME_RES_MEDKIT_GET;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&medKitID, sizeof(UINT));
	sPacket->Serialize((char*)&hp, sizeof(int));
}

void CBattleSnake_Battle_MMOServer::SerializePacket_Room_LeavePlayRoom(cNetPacketPool* sPacket, int roomNo, INT64 accountNo)
{
	WORD type = en_PACKET_CS_GAME_RES_LEAVE_USER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
}

unsigned int CBattleSnake_Battle_MMOServer::MonitorThread(void *arg)
{
	CBattleSnake_Battle_MMOServer *pThis = (CBattleSnake_Battle_MMOServer*)arg;
	return pThis->Update_MonitorThread();
}

unsigned int CBattleSnake_Battle_MMOServer::Update_MonitorThread(void)
{
	bool *bExitMonitor = &_bExitMonitor;
	long prevTime = timeGetTime();
	long curTime;
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

		if (IsServerOn())
		{
			if (_pLanClient->isConnected() == false && _pLanServer->IsLogin() == true)
			{
				Connect_Battle_LanClient();
			}

			if (_pMonitorClient->isConnected() == false)
			{
				Connect_Monitor_LanClient();
			}
		}

		_monitor_medKitUsed = _medKitPool.GetUsedBlockCount();
		_monitor_medKitPool = _medKitPool.GetTotalBlockCount();

		prevTime = curTime;
	}

	cLanPacketPool::FreePacketChunk();
	return 0;
}
