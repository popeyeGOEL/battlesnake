#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"

#include "cHttp_Async.h"
#include "CBattleSnake_Battle_Http.h"

#include "CNetSession.h"
#include "CBattleSnake_BattlePlayer_NetSession.h"

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

void CBattleSnake_BattlePlayer_NetSession::InitPlayer(void)
{
	_accountNo = 0;
	_roomNo = 0;
	memset(_nickName, 0, sizeof(WCHAR) * 20);
	_isDead = false;
	_bSendLoginResult = false;
	_httpSelectCount = 0;
	_httpFailCount = 0;
}

void CBattleSnake_BattlePlayer_NetSession::OnAuth_ClientJoin(void)
{
	_lastRecvTime = timeGetTime();
}

void CBattleSnake_BattlePlayer_NetSession::OnAuth_ClientLeave(void) 
{
	g_pBattle->Request_Character_LeavePlayer(this);
}

void CBattleSnake_BattlePlayer_NetSession::OnAuth_Packet(cNetPacketPool *dsPacket)
{
	if (Auth_PacketProc(dsPacket) == false)
	{
		Disconnect();
	}
}

void CBattleSnake_BattlePlayer_NetSession::OnGame_ClientJoin(void)
{
	//	Create My Character
	Response_Player_CreateMyCharacter();

	//	Request Create Character
	g_pBattle->Request_Character_CreateCharacter(this);

}
void CBattleSnake_BattlePlayer_NetSession::OnGame_ClientLeave(void) 
{
	g_pBattle->Request_Character_LeavePlayer(this);
}
void CBattleSnake_BattlePlayer_NetSession::OnGame_Packet(cNetPacketPool *dsPacket)
{
	if (Game_PacketProc(dsPacket) == false)
	{
		Disconnect();
	}
}
void CBattleSnake_BattlePlayer_NetSession::OnGame_ClientRelease(void) 
{
	if (_bLogin)
	{
		InterlockedDecrement(&g_pBattle->_monitor_loginPlayer);
		_bLogin = false;
	}

	InitPlayer();
}

BYTE CBattleSnake_BattlePlayer_NetSession::Check_HttpRequest_Result(int result)
{
	BYTE status;
	switch (result)
	{
	case 1:
		status = Player_Error::Success;
		break;
	case -10:
	case -11:
	case -12:
		status = Player_Error::Error_UserInfo;
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - Check_HttpRequest_Result : No exists accountno [user: %d]", _accountNo);
		break;
	default:
		status = Player_Error::Error_Etc;
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - Check_HttpRequest_Result : shdb error %d", result);
		break;
	}

	return status;
}

void CBattleSnake_BattlePlayer_NetSession::Fail_HttpRequest(UINT64 clientID, int type)
{
	//	재전송
	if (_clientInfo.clientID != clientID) return;
	if (InterlockedIncrement(&_httpFailCount) > 20)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - Fail_HttpRequest : Message Type: %d clientID %lld", type, clientID);
		Disconnect();
		return;
	}

	switch (type)
	{
	case 1:
		g_pHttp->Send_Select_Account(_clientInfo.clientID, _accountNo);
		break;
	case 2:
		g_pHttp->Send_Select_Contents(_clientInfo.clientID, _accountNo);
		break;
	case 3:
		g_pHttp->Send_Update_Contents(_clientInfo.clientID, _accountNo, _curRecord);
		break;
	}
}

void CBattleSnake_BattlePlayer_NetSession::Receive_HttpRequest_SearchAccount(int result, WCHAR* nick, char* sessionKey)
{
	BYTE status = Check_HttpRequest_Result(result);
	if (status != Player_Error::Success)
	{
		Response_Player_Login(status);
		return;
	}

	//if (strcmp(_sessionKey, sessionKey) != 0)
	if(memcmp(_sessionKey, sessionKey, sizeof(char)*64) != 0)
	{
		status = Player_Error::Error_NotMatch_SessionKey;
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - Receive_HttpRequest_SearchAccount : NOT MATCH SESSIONKEY [result: %s, user: %s]", sessionKey, _sessionKey);
		Response_Player_Login(status);
		return;
	}

	wcscpy(_nickName, nick);

	if (InterlockedIncrement(&_httpSelectCount) == 2)
	{
		Response_Player_Login(status);
	}

}
void CBattleSnake_BattlePlayer_NetSession::Receive_HttpRequest_SearchContents(int result, stRecord &record)
{
	BYTE status = Check_HttpRequest_Result(result);
	if (status != Player_Error::Success)
	{
		Response_Player_Login(status);
		return;
	}

	_curRecord = record;

	if (InterlockedIncrement(&_httpSelectCount) == 2)
	{
		Response_Player_Login(status);
	}
}
void CBattleSnake_BattlePlayer_NetSession::Receive_HttpRequest_UpdateContents(int result)
{
	BYTE status = Check_HttpRequest_Result(result);
	if (status != Player_Error::Success)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - Receive_HttpRequest_UpdateContents : Update Record Failed [%d], [%d,%d,%d,%d,%d]", 
			_accountNo, _curRecord.playTime, _curRecord.playCount, _curRecord.kill, _curRecord.die, _curRecord.win);
	}
}



///-----------------------------------------------------
///	Auth Packet Proc
///-----------------------------------------------------

bool CBattleSnake_BattlePlayer_NetSession::Auth_PacketProc(cNetPacketPool *dsPacket)
{
	WORD type;

	try
	{
		dsPacket->Deserialize((char*)&type, sizeof(WORD));
		switch (type)
		{
		case en_PACKET_CS_GAME_REQ_LOGIN:
			return Request_Player_Login(dsPacket);
		case en_PACKET_CS_GAME_REQ_ENTER_ROOM:
			return Request_Player_EnterRoom(dsPacket);
		case en_PACKET_CS_GAME_REQ_HEARTBEAT:
			Request_Player_Heartbeat(dsPacket);
			return true;
		default:
			LOG(L"Packet", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - OnAuth_Packet : Packet Type Error [ID %lld] [TYPE %d]", _accountNo, type);
			return false;
		};
	}
	catch (cNetPacketPool::packetException &e)
	{
		LOG(L"Packet", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - OnAuth_Packet : Error Exception %s", e.errLog);
		return false;
	}
}
bool CBattleSnake_BattlePlayer_NetSession::Request_Player_Login(cNetPacketPool *dsPacket)
{
	INT64	accountNo;
	char	connectToken[32];
	int		verCode;

	dsPacket->Deserialize((char*)&accountNo, sizeof(INT64));

	CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByAccountNo(accountNo);
	if (pPlayer != nullptr && pPlayer->_bDisconnect == false)
	{
		//	중복 로그인시 해당 플레이어들 모두 로그아웃처리
		LOG(L"BattleServer", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_BattlePlayer_NetSession - Request_Player_Login : Already Login User %lld, %lld", pPlayer->_accountNo, accountNo);
		pPlayer->Disconnect();
		//Response_Player_Login(Player_Error::Erorr_AlreadyLogin);
		//return true;
	}

	_accountNo = accountNo;
	dsPacket->Deserialize(_sessionKey, sizeof(char) * 64);
	dsPacket->Deserialize(connectToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)&verCode, sizeof(UINT));

	
	if (g_pBattle->_verCode != verCode)
	{
		Response_Player_Login(Player_Error::Error_Version);
		return true;
	}

	if (memcmp(g_pBattle->_connectToken, connectToken, sizeof(char) * 32) != 0)
	{
		Response_Player_Login(Player_Error::Error_UserInfo);
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - Request_Player_Login : Not match connectToken");
		return true;
	}

	_lastRecvTime = timeGetTime();
	g_pHttp->Send_Select_Account(_clientInfo.clientID, _accountNo);
	g_pHttp->Send_Select_Contents(_clientInfo.clientID, _accountNo);

	return true;
}

bool CBattleSnake_BattlePlayer_NetSession::Request_Player_EnterRoom(cNetPacketPool *dsPacket)
{
	INT64 accountNo;
	dsPacket->Deserialize((char*)&accountNo, sizeof(INT64));

	if (accountNo != _accountNo) return false;
	dsPacket->Deserialize((char*)&_roomNo, sizeof(int));
	dsPacket->Deserialize(_enterToken, sizeof(char) * 32);

	_lastRecvTime = timeGetTime();
	g_pBattle->Request_EnterRoom(this);
	return true;
}

void CBattleSnake_BattlePlayer_NetSession::Response_Player_Login(BYTE result)
{
	if (InterlockedExchange8((char*)&_bSendLoginResult, 1) == 1) return;

	if (result == Success)
	{
		_bLogin = true;
		InterlockedIncrement(&g_pBattle->_monitor_loginPlayer);
	}
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_Login(sPacket, _accountNo, result);
	SendPacket(sPacket);
}
void CBattleSnake_BattlePlayer_NetSession::Response_Player_EnterRoom(BYTE maxUser, BYTE result)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_EnterRoom(sPacket, _accountNo, _roomNo, maxUser, result);
	SendPacket(sPacket);
}


void CBattleSnake_BattlePlayer_NetSession::SerializePacket_Player_Login(cNetPacketPool *sPacket, INT64 accountNo, BYTE result)
{
	WORD type = en_PACKET_CS_GAME_RES_LOGIN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&result, sizeof(BYTE));
}
void CBattleSnake_BattlePlayer_NetSession::SerializePacket_Player_EnterRoom(cNetPacketPool *sPacket, INT64 accountNo, int roomNo, BYTE maxUser, BYTE result)
{
	WORD type = en_PACKET_CS_GAME_RES_ENTER_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&maxUser, sizeof(BYTE));
	sPacket->Serialize((char*)&result, sizeof(BYTE));
}


///-----------------------------------------------------
///	Game Packet Proc
///-----------------------------------------------------
bool CBattleSnake_BattlePlayer_NetSession::Game_PacketProc(cNetPacketPool *dsPacket)
{
	WORD type;

	try
	{
		dsPacket->Deserialize((char*)&type, sizeof(WORD));
		switch (type)
		{
		case en_PACKET_CS_GAME_REQ_MOVE_PLAYER:
			Request_Player_Move(dsPacket);
			break;
		case en_PACKET_CS_GAME_REQ_HIT_POINT:
			Request_Player_HitPoint(dsPacket);
			break;
		case en_PACKET_CS_GAME_REQ_FIRE1:
			Request_Player_Fire(dsPacket);
			break;
		case en_PACKET_CS_GAME_REQ_RELOAD:
			Request_Player_Reload(dsPacket);
			break;
		case en_PACKET_CS_GAME_REQ_HIT_DAMAGE:
			Request_Player_HitDamage(dsPacket);
			break;
		case en_PACKET_CS_GAME_REQ_MEDKIT_GET:
			Request_Player_MedKitGet(dsPacket);
			break;
		case en_PACKET_CS_GAME_REQ_HEARTBEAT:
			Request_Player_Heartbeat(dsPacket);
			break;
		default:
			LOG(L"Packet", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - OnGame_Packet : Packet Type Error [ID %lld] [TYPE %d]", _accountNo, type);
			return false;
		};

		return true;
	}
	catch (cNetPacketPool::packetException &e)
	{
		LOG(L"Packet", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_BattlePlayer_NetSession - OnGame_Packet : Error Exception %s", e.errLog);
		return false;
	}
}

void CBattleSnake_BattlePlayer_NetSession::Request_Player_Move(cNetPacketPool* dsPacket)
{
	dsPacket->Deserialize((char*)&_moveTarget, sizeof(stMoveTarget));
	dsPacket->Deserialize((char*)&_hitPoint, sizeof(stHitPoint));

	_lastRecvTime = timeGetTime();
	g_pBattle->Request_Character_Move(this);
}
void CBattleSnake_BattlePlayer_NetSession::Request_Player_HitPoint(cNetPacketPool *dsPacket)
{
	dsPacket->Deserialize((char*)&_hitPoint, sizeof(stHitPoint));

	_lastRecvTime = timeGetTime();
	g_pBattle->Request_Character_HitPoint(this);
}
void CBattleSnake_BattlePlayer_NetSession::Request_Player_Fire(cNetPacketPool* dsPacket)
{
	dsPacket->Deserialize((char*)&_hitPoint, sizeof(stHitPoint));

	_lastRecvTime = timeGetTime();
	g_pBattle->Request_Character_Fire(this);
}
void CBattleSnake_BattlePlayer_NetSession::Request_Player_Reload(cNetPacketPool* dsPacket)
{
	_lastRecvTime = timeGetTime();
	g_pBattle->Request_Character_Reload(this);
}
void CBattleSnake_BattlePlayer_NetSession::Request_Player_HitDamage(cNetPacketPool* dsPacket)
{
	INT64 targetAccountNo;
	dsPacket->Deserialize((char*)&targetAccountNo, sizeof(INT64));
	_lastRecvTime = timeGetTime();
	g_pBattle->Request_Character_HitDamage(this, targetAccountNo);
}
void CBattleSnake_BattlePlayer_NetSession::Request_Player_MedKitGet(cNetPacketPool* dsPacket)
{
	UINT medkitID;

	dsPacket->Deserialize((char*)&medkitID, sizeof(UINT));
	_lastRecvTime = timeGetTime();
	g_pBattle->Request_Character_GetMedKit(this, medkitID);
}
void CBattleSnake_BattlePlayer_NetSession::Request_Player_Heartbeat(cNetPacketPool* dsPacket)
{
	_lastRecvTime = timeGetTime();
}

void CBattleSnake_BattlePlayer_NetSession::Response_Player_CreateMyCharacter(void)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_CreateMyCharacter(sPacket, _moveTarget.moveTargetX, _moveTarget.moveTargetZ, _hp);
	SendPacket(sPacket);
}

void CBattleSnake_BattlePlayer_NetSession::Response_Player_Winner(void)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_Winner(sPacket);
	SendPacket(sPacket);
}
void CBattleSnake_BattlePlayer_NetSession::Response_Player_GameOver(void)
{
	cNetPacketPool *sPacket = cNetPacketPool::Alloc();
	SerializePacket_Player_GameOver(sPacket);
	SendPacket(sPacket);
}
void CBattleSnake_BattlePlayer_NetSession::Response_Player_Record(bool bSend)
{
	if (bSend == true)
	{
		cNetPacketPool *sPacket = cNetPacketPool::Alloc();
		SerializePacket_Player_Record(sPacket, _curRecord);
		SendPacket(sPacket);
	}

	_httpFailCount = 0;
	g_pHttp->Send_Update_Contents(_clientInfo.clientID, _accountNo, _curRecord);
}

void CBattleSnake_BattlePlayer_NetSession::SerializePacket_Player_CreateMyCharacter(cNetPacketPool* sPacket, float posX, float posY, int hp)
{
	WORD type = en_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&posX, sizeof(float));
	sPacket->Serialize((char*)&posY, sizeof(float));
	sPacket->Serialize((char*)&hp, sizeof(int));
}

void CBattleSnake_BattlePlayer_NetSession::SerializePacket_Player_Winner(cNetPacketPool* sPacket)
{
	WORD type = en_PACKET_CS_GAME_RES_WINNER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_BattlePlayer_NetSession::SerializePacket_Player_GameOver(cNetPacketPool *sPacket)
{
	WORD type = en_PACKET_CS_GAME_RES_GAMEOVER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_BattlePlayer_NetSession::SerializePacket_Player_Record(cNetPacketPool *sPacket, stRecord &record)
{
	int playtimeMin = record.playTime / 60000;

	WORD type = en_PACKET_CS_GAME_RES_RECORD;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&record.playCount, sizeof(int));
	sPacket->Serialize((char*)&playtimeMin, sizeof(int));
	sPacket->Serialize((char*)&record.kill, sizeof(int));
	sPacket->Serialize((char*)&record.die, sizeof(int));
	sPacket->Serialize((char*)&record.win, sizeof(int));
}