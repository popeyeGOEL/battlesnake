#pragma once
#include <list>
#include "cLockFreeQueue.h"
#include "PlayerData.h"

#define dfMAX_BATTLE_PLAYER 10000
#define dfDELAY_READY_COUNT 10
//#define dfMAX_ROOM_PLAYER	10

using namespace TUNJI_LIBRARY;
using namespace PLAYER_DATA;

class CBattleSnake_Battle_LanClient;
class CBattleSnake_BattlePlayer_NetSession;
class CBattleSnake_Battle_LanServer;
class CBattleSnake_BattleMonitor_LanClient;
class CBattleSnake_Battle_MMOServer : public CMMOServer
{
public:

	~CBattleSnake_Battle_MMOServer();
	
	CBattleSnake_Battle_MMOServer(int maxSession, int maxBattleRoom, int defaultWaitRoom, int maxRoomPlayer, int authSleepCount, int gameSleepCount, int sendSleepCount,
							   UINT verCode, char * connectToken, char* masterToken, BYTE packetCode, BYTE packetKey1, BYTE packetKey2, WCHAR * accessIP);

	void OnAuth_Update(void);
	void OnGame_Update(void);
	void OnError(int errorCode, WCHAR *errorText);

	///-----------------------------------------------------
	///	Start Battle Server
	///-----------------------------------------------------
	void Start_Battle(void);
	void Stop_Battle(void);
	///-----------------------------------------------------
	///	Start Battle LanServer
	///-----------------------------------------------------
	bool Start_Battle_LanServer(WCHAR *ip, short port, int threadCnt, bool bNagle, int maxUser);
	bool Stop_Battle_LanServer(void);

	///-----------------------------------------------------
	///	Connect Battle LanClient
	///-----------------------------------------------------
	void Set_BattleLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool Connect_Battle_LanClient(void);
	bool Disconnect_Battle_LanClient(void);

	///-----------------------------------------------------
	///	Connect Http Client
	///-----------------------------------------------------
	void Set_HttpURL(WCHAR *url, int len, int type);	//	select_account = 1, select_contents = 2, update_contents = 3
	bool Connect_Battle_HttpClient(int threadCnt, bool bNagle, int maxUser);
	bool Disconnect_Battle_HttpClient(void);
	void Check_HttpHeartBeat(void);
	CBattleSnake_BattlePlayer_NetSession * Find_PlayerByClientID(UINT64 clientID);
	CBattleSnake_BattlePlayer_NetSession * Find_PlayerByAccountNo(UINT64 accountNo);

	///-----------------------------------------------------
	///	Connect Monitor LanClient
	///-----------------------------------------------------
	void Set_MonitorLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool Connect_Monitor_LanClient(void);
	bool Disconnect_Monitor_LanClient(void);

	///-----------------------------------------------------
	///	Request from Session
	///-----------------------------------------------------
	void	Request_EnterRoom(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	//void	Request_Character_LeaveWaitRoom(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_Move(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_CreateCharacter(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_HitPoint(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_Fire(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_Reload(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_HitDamage(CBattleSnake_BattlePlayer_NetSession *pPlayer, INT64 targetAccountNo);
	void	Request_Character_GetMedKit(CBattleSnake_BattlePlayer_NetSession *pPlayer, UINT medKitID);
	//void	Request_Character_LeavePlayRoom(CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void	Request_Character_LeavePlayer(CBattleSnake_BattlePlayer_NetSession *pPlayer);

	///-----------------------------------------------------
	///	Response from master
	///-----------------------------------------------------
	void	Receive_MasterRequest_Login(void);
	void	Receive_MasterRequest_CreateRoom(int roomNo);
	void	Receive_MasterRequest_CloseRoom(int roomNo);
	void	Receive_MasterRequest_LeavePlayer(int roomNo);

	///-----------------------------------------------------
	///	Response from chat
	///-----------------------------------------------------
	void	Receive_ChatResponse_Login();
	void	Receive_ChatRequest_CreateRoom(int roomNo);
	void	Receive_ChatRequest_CloseRoom(int roomNo);

public:
	int				_battleNo;
	char			_connectToken[32];
	UINT			_verCode;

	WCHAR			_accessIP[16];
	WCHAR			_chatServerIP[16];
	short			_chatServerPort;

private:

	///-----------------------------------------------------
	///	Battle Room
	///-----------------------------------------------------
	enum Room_Status
	{
		Room_Close,
		Room_Wait,
		Room_Ready,
		Room_Play,
		Room_End,
	};

	enum Request_EnterRoom_Result
	{
		Success_Enter = 1,
		Fail_NotMatch_EnterToken,
		Fail_NotWaitRoom,
		Fail_NoExistRoom,
		Fail_PlayerFull,
	};

	enum RedZone_Info
	{
		RedZone_Left,
		RedZone_Top,
		RedZone_Right,
		RedZone_Bottom,
	};

	struct st_MEDKIT
	{
		UINT	medKitID;
		float	posX;		//	MoveTargetX
		float	posY;		//	MoveTargetY
	};

	enum Room_Tracker
	{
		Send_Master_CreateRoom,
		Recv_Master_CreateRoom,
		Player_EnterRoom,
		Player_LeaveWaitRoom,
		Player_LeaveReadyRoom,
		Player_LeavePlayRoom,
		Send_Master_DeleteRoom,
		Recv_Master_DeleteRoom,
		Send_Master_LeavePlayer,
		Recv_Master_LeavePlayer,
		Room_Change_Wait,
		Room_Change_Ready,
		Room_Change_Play,
		Room_Change_Close,
	};

	struct st_TRACKER
	{
		Room_Tracker	type;
		int				curUser;
	};

	struct st_ROOM
	{
		st_ROOM()
		{
			for (int i = 0; i < 4; ++i)
			{
				redZone[i] = (RedZone_Info)i;
			}

			memset(trackArray, 0, sizeof(st_TRACKER) * 500);
			trackCount = 0;
		};

		Room_Status		status;
		int				roomNo;
		bool			bReady;
		bool			bReceived;
		char			enterToken[32];
		int				maxUser;
		long			curUser;
		long			aliveUser;
		long			sendResultCount;
		
		int				receiveTime;
		int				readyTime;
		int				startTime;
		int				endTime;

		int				map_left;
		int				map_top;
		int				map_right;
		int				map_bottom;

		bool			redZone_bPreAlerted;
		int				redZone_nextActiveTime;
		int				redZone_totalActive;
		int				redZone_lastUpdateTime;
		RedZone_Info	redZone[4];

		//std::list<CBattleSnake_BattlePlayer_NetSession*>	playerList;
		//CBattleSnake_BattlePlayer_NetSession* roomPlayer[dfMAX_ROOM_PLAYER];
		CBattleSnake_BattlePlayer_NetSession** roomPlayer;
		std::list<st_MEDKIT*> medKitList;

		st_TRACKER		trackArray[500];
		int				trackCount;
	};

	st_MEDKIT*	Create_MedKit(st_ROOM *pRoom, float posX, float posY);
	st_MEDKIT*	Find_MedKitbyID(int roomNo, UINT medkitID);
	void		Free_MedKit(int roomNo, st_MEDKIT* pMed);
	void		Clear_RoomMedKit(st_ROOM *pRoom);

	UINT									_curMedID;
	cMemoryPool<st_MEDKIT, void>			_medKitPool;

	int	RoomNoToIndex(int roomNo);		
	int CreateRoomNo(int index);	//	curRoomID와 Index를 조합해서 roomNo를 만든다.
	void InitRoom(st_ROOM *pRoom);
	void TrackRoom(st_ROOM *pRoom, Room_Tracker type, bool bReady = false);

	st_ROOM*	Open_Room(void);
	st_ROOM*	Find_RoombyRoomNo(int roomNo);

	bool		DeletePlayer_Room(st_ROOM *pRoom, CBattleSnake_BattlePlayer_NetSession *pPlayer);
	bool		AddPlayer_Room(st_ROOM *pRoom, CBattleSnake_BattlePlayer_NetSession *pPlayer);
	void		BroadCast_Room(cNetPacketPool *sPacket, st_ROOM *pRoom, UINT64 accountNo = 0);

	///-----------------------------------------------------
	///	Room Packet Proc
	///-----------------------------------------------------

	void Response_Room_ReadySec(st_ROOM *pRoom, BYTE readySec);																					//	방전체
	void Response_Room_Start(st_ROOM *pRoom);																								//	방전체
	void Response_Room_AddPlayer(st_ROOM *pRoom,INT64 accountNo, WCHAR *nickName, stRecord& record);		//	본인 제외
	void Response_Room_LeaveWaitRoom(st_ROOM *pRoom,INT64 accountNo);																			//	본인 제외

	void SerializePacket_Room_ReadySec(cNetPacketPool *sPacket, int roomNo, BYTE readySec);
	void SerializePacket_Room_Start(cNetPacketPool* sPacket, int roomNo);
	void SerializePacket_Room_AddPlayer(cNetPacketPool* sPacket, int roomNo, INT64 accountNo, WCHAR *nickName, stRecord &record);
	void SerializePacket_Room_LeaveWaitRoom(cNetPacketPool* sPacket, int roomNo, INT64 accountNo);

	void Response_Room_CreateCharacter(st_ROOM *pRoom, INT64 accountNo, WCHAR *nickName, float posX, float posY, int hp);
	void Response_Room_MoveCharacter(st_ROOM *pRoom, INT64 accountNo, stMoveTarget &move, stHitPoint &hit);						//	본인 제외
	void Response_Room_HitPointCharacter(st_ROOM *pRoom, INT64 accountNo, stHitPoint &hit);				//	본인 제외 
	void Response_Room_FireCharacter(st_ROOM *pRoom,INT64 accountNo,stHitPoint &hit);					//	본인 제외 
	void Response_Room_ReloadCharacter(st_ROOM *pRoom, INT64 accountNo);																		//	본인 제외
	void Response_Room_HitDamage(st_ROOM *pRoom,INT64 attackAccountNo, INT64 targetAccountNo, int targetHP);									//	방전체
	void Response_Room_RedZoneDamage(st_ROOM *pRoom,INT64 accountNo, int hp);																	//	방전체 
	void Response_Room_Die(st_ROOM *pRoom,INT64 accountNo);																					//	방전체
	void Response_Room_RedZoneAlert_Left(st_ROOM *pRoom,BYTE alertTimeSec);																	//	방전체
	void Response_Room_RedZoneAlert_Top(st_ROOM *pRoom,BYTE alertTimeSec);																		//	방전체
	void Response_Room_RedZoneAlert_Right(st_ROOM *pRoom,BYTE alertTimeSec);																	//	방전체
	void Response_Room_RedZoneAlert_Bottom(st_ROOM *pRoom,BYTE alertTimeSec);																	//	방전체
	void Response_Room_RedZoneActive_Left(st_ROOM *pRoom);																				//	방전체
	void Response_Room_RedZoneActive_Top(st_ROOM *pRoom);																					//	방전체	
	void Response_Room_RedZoneActive_Right(st_ROOM *pRoom);																				//	방전체
	void Response_Room_RedZoneActive_Bottom(st_ROOM *pRoom);																				//	방전체
	void Response_Room_MedKitCreated(st_ROOM *pRoom,UINT medKitID, float posX, float posY);													//	방전체
	void Response_Room_MedkitGet(st_ROOM *pRoom,INT64 accountNo, UINT medKitID, int hp);														//	방전체
	void Response_Room_LeavePlayRoom(st_ROOM *pRoom,INT64 accountNo);

	void SerializePacket_Room_CreateCharacter(cNetPacketPool* sPacket, INT64 accountNo, WCHAR* nickname,
													 float posX, float posY, int hp);
	void SerializePacket_Room_MoveChracter(cNetPacketPool *sPacket, INT64 accountNo, stMoveTarget &move, stHitPoint &hit);
	void SerializePacket_Room_HitPointCharacter(cNetPacketPool* sPacket, INT64 accountNo, stHitPoint &hit);
	void SerializePacket_Room_FireChracter(cNetPacketPool* sPacket, INT64 accountNo, stHitPoint &hit);
	void SerializePacket_Room_ReloadCharacter(cNetPacketPool* sPacket, INT64 accountNo);
	void SerializePacket_Room_HitDamage(cNetPacketPool* sPacket, INT64 attackAccountNo, INT64 targetAccountNo, int targetHP);
	void SerializePacket_Room_RedZoneDamage(cNetPacketPool *sPacket, INT64 accountNo, int hp);
	void SerializePacket_Room_Die(cNetPacketPool *sPacket, INT64 accountNO);
	void SerializePacket_Room_RedZoneAlert_Left(cNetPacketPool* sPacket, BYTE alertTimeSec);
	void SerializePacket_Room_RedZoneAlert_Top(cNetPacketPool* sPacket, BYTE alertTimeSec);
	void SerializePacket_Room_RedZoneAlert_Right(cNetPacketPool* sPacket, BYTE alertTimeSec);
	void SerializePacket_Room_RedZoneAlert_Bottom(cNetPacketPool* sPacket, BYTE alertTimeSec);
	void SerializePacket_Room_RedZoneActive_Left(cNetPacketPool *sPacket);
	void SerializePacket_Room_RedZoneActive_Right(cNetPacketPool *sPacket);
	void SerializePacket_Room_RedZoneActive_Top(cNetPacketPool *sPacket);
	void SerializePacket_Room_RedZoneActive_Bottom(cNetPacketPool *sPacket);
	void SerializePacket_Room_MedKitCreated(cNetPacketPool *sPacket, UINT medKitID, float posX, float posY);
	void SerializePacket_Room_MedKitGet(cNetPacketPool *sPacket, INT64 accountNo, UINT medKitID, int hp);
	void SerializePacket_Room_LeavePlayRoom(cNetPacketPool* sPacket, int roomNo, INT64 accountNo);


	long				_curRoomID;
	cLockFreeQueue<int> _roomIndexQueue;
private:
	///-----------------------------------------------------
	///	Update 
	///-----------------------------------------------------
	void		Update_HeartBeat(void);
	void		Update_ConnectToken(void);
	void		Update_RoomCount(void);
	void		Update_Room_Wait(st_ROOM *pRoom);
	void		Update_Room_Ready(st_ROOM *pRoom);
	void		Update_Room_Play(st_ROOM *pRoom);
	void		Update_Room_End(st_ROOM *pRoom);

private:	
	CBattleSnake_BattlePlayer_NetSession	*_playerArray;
	int				_maxRoom;
	int				_curOpenRoom;
	int				_defaultWaitRoom;
	int				_maxRoomPlayer;

	st_ROOM*		_roomArray;

	CBattleSnake_Battle_LanClient *_pLanClient;				//	마스터 통신용 랜클라이언트
	CBattleSnake_Battle_LanServer *_pLanServer;				//	채팅 통신용 랜 서버
	CBattleSnake_BattleMonitor_LanClient *_pMonitorClient;	//	모니터링 랜클라이언트


	///-----------------------------------------------------
	///	Monitoring Thread
	///-----------------------------------------------------
	static unsigned __stdcall MonitorThread(void *arg);
	unsigned int Update_MonitorThread(void);
	bool			_bExitMonitor;
	HANDLE			_hMonitorThread;


	///	Lan Client Config List
	bool			_battleLanClient_config;
	WCHAR			_battleLanClient_IP[16];
	short			_battleLanClient_port;
	int				_battleLanClient_threadCnt;
	bool			_battleLanClient_nagle;

	bool			_monitorLanClient_config;
	WCHAR			_monitorLanClient_IP[16];
	short			_monitorLanClient_port;
	int				_monitorLanClient_threadCnt;
	bool			_monitorLanClient_nagle;
	
public:
	UINT64			_monitor_loginPlayer;
	UINT64			_monitor_battleWaitRoom;
	UINT64			_monitor_battlePlayRoom;

	UINT64			_monitor_medKitUsed;
	UINT64			_monitor_medKitPool;
};


extern CBattleSnake_Battle_MMOServer *g_pBattle;