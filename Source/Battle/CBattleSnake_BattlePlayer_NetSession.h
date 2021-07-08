#pragma once
#include "PlayerData.h"
using namespace TUNJI_LIBRARY;
using namespace PLAYER_DATA;

class CBattleSnake_BattlePlayer_NetSession : public CNetSession
{
	friend class CBattleSnake_Battle_MMOServer;
public:

	void InitPlayer(void);
	void OnAuth_ClientJoin(void);
	void OnAuth_ClientLeave(void);
	void OnAuth_Packet(cNetPacketPool *dsPacket);
	
	void OnGame_ClientJoin(void);
	void OnGame_ClientLeave(void);
	void OnGame_Packet(cNetPacketPool *dsPacket);
	void OnGame_ClientRelease(void);

	void Fail_HttpRequest(UINT64 clientID, int type);
	void Receive_HttpRequest_SearchAccount(int result, WCHAR* nick, char* sessionKey);
	void Receive_HttpRequest_SearchContents(int result, stRecord &record);
	void Receive_HttpRequest_UpdateContents(int result);


private:
	enum Player_Error
	{
		Success = 1,
		Error_UserInfo = 2,
		Error_NotMatch_SessionKey = 3,
		Error_Etc = 4,
		Error_Version = 5,
		Erorr_AlreadyLogin = 6,
	};

private:
	BYTE Check_HttpRequest_Result(int result);
	///-----------------------------------------------------
	///	Auth Packet Proc
	///-----------------------------------------------------
	bool Auth_PacketProc(cNetPacketPool *dsPacket);
	bool Request_Player_Login(cNetPacketPool *dsPacket);
	bool Request_Player_EnterRoom(cNetPacketPool *dsPacket);

	void Response_Player_Login(BYTE result);																			//	본인
	void Response_Player_EnterRoom(BYTE maxUser, BYTE result);															//	본인

	void SerializePacket_Player_Login(cNetPacketPool *sPacket, INT64 accountNo, BYTE result);
	void SerializePacket_Player_EnterRoom(cNetPacketPool *sPacket, INT64 accountNo, int roomNo, BYTE maxUser, BYTE result);

	///-----------------------------------------------------
	///	Game Packet Proc
	///-----------------------------------------------------
	bool Game_PacketProc(cNetPacketPool *dsPacket);
	void Request_Player_Move(cNetPacketPool* dsPacket);
	void Request_Player_HitPoint(cNetPacketPool *dsPacket);
	void Request_Player_Fire(cNetPacketPool* dsPacket);
	void Request_Player_Reload(cNetPacketPool* dsPacket);
	void Request_Player_HitDamage(cNetPacketPool* dsPacket);
	void Request_Player_MedKitGet(cNetPacketPool* dsPacket);
	void Request_Player_Heartbeat(cNetPacketPool* dsPacket);

	void Response_Player_CreateMyCharacter(void);																		//	본인
	void Response_Player_Winner(void);																					//	본인
	void Response_Player_GameOver(void);																				//	본인
	void Response_Player_Record(bool bSend = true);

	void SerializePacket_Player_CreateMyCharacter(cNetPacketPool* sPacket, float posX, float posY, int hp);
	void SerializePacket_Player_Winner(cNetPacketPool* sPacket);
	void SerializePacket_Player_GameOver(cNetPacketPool *sPacket);
	void SerializePacket_Player_Record(cNetPacketPool *sPacket, stRecord& record);


private:
	INT64			_accountNo;
	char			_sessionKey[64];
	WCHAR			_nickName[20];
	char			_enterToken[32];

	bool			_bSendLoginResult;
	long			_httpSelectCount;
	long			_httpFailCount;

	bool			_bLogin;
	int				_roomNo;
	bool			_isDead;

	stMoveTarget	_moveTarget;
	stHitPoint		_hitPoint;
	stRecord		_curRecord;

	int				_hp;

	int				_lastRecvTime;
};