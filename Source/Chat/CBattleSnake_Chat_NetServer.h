#pragma once
#include <map>
#include <list>

#include "cCpuCounter.h"
#include "cPdhCounter.h"

#include "cPacketPool_LanServer.h"
#include "cPacketPool_NetServer.h"
#include "cQueue.h"
#include "cLockFreeQueue.h"
#include "cMemoryPool.h"

//#define dfMAX_ROOM_PLAYER	10

using namespace TUNJI_LIBRARY;

class CBattleSnake_Chat_LanClient;
class CBattleSnake_ChatMonitor_LanClient;
class CBattleSnake_Chat_NetServer : public CNetServer
{
public:
	enum en_MSG_TYPE
	{
		en_MSG_TYPE_PACKET = 1,
		en_MSG_TYPE_JOIN,
		en_MSG_TYPE_LEAVE,
		en_MSG_TYPE_CREATE_ROOM,
		en_MSG_TYPE_DELETE_ROOM,
		en_MSG_TYPE_REISSUE_TOKEN,
		en_MSG_TYPE_DISCONNECTED,
		en_MSG_TYPE_HEARTBEAT,
		en_MSG_TYPE_TERMINATE,
	};

	CBattleSnake_Chat_NetServer(BYTE packetCode, BYTE packetKey1, BYTE packetKey2, WCHAR *accessIP);
	~CBattleSnake_Chat_NetServer();

	void OnClientJoin(UINT64 clientID);
	void OnClientLeave(UINT64 clientID);
	bool OnConnectionRequest(WCHAR *ip, short port);
	void OnRecv(UINT64 clientID, cNetPacketPool *dsPacket);
	void OnSend(UINT64 clientID, int sendSize);
	void OnError(int errorCode, WCHAR *errText);

	///-----------------------------------------------------
	///	Start Chat NetServer
	///-----------------------------------------------------
	void Start_Chat_NetServer(void);
	void Stop_Chat_Server(void);

	///-----------------------------------------------------
	///	Start Chat LanClient
	///-----------------------------------------------------
	void Set_ChatLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool Connect_Chat_LanClient(void);
	bool Disconnect_Chat_LanClient(void);

	///-----------------------------------------------------
	///	Start Monitor LanClient
	///-----------------------------------------------------
	void Set_MonitorLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool Connect_Monitor_LanClient(void);
	bool Disconnect_Monitor_LanClient(void);

	///-----------------------------------------------------
	///	Response from battle server
	///-----------------------------------------------------
	void Receive_BattleRequest_Logout(void);
	void Receive_BattleRequest_ReissueConnectToken(cLanPacketPool *dsPacket);
	void Receive_BattleRequest_CreateRoom(cLanPacketPool *dsPacket);
	void Receive_BattleRequest_DeleteRoom(cLanPacketPool *dsPacket);

private:
	///-----------------------------------------------------
	///	Player Structure
	///-----------------------------------------------------
	struct st_PLAYER
	{
		UINT64	clientID;
		INT64	accountNo;
		bool	bEnter;
		int		roomNo;
		WCHAR	ID[20];
		WCHAR	nick[20];
		int		lastHeartBeat;
	};

	///-----------------------------------------------------
	///	Room Structure
	///-----------------------------------------------------
	struct st_ROOM
	{
		int		roomNo;
		int		maxUser;
		int		curUser;
		char	enterToken[32];

		//st_PLAYER* playerArray[dfMAX_ROOM_PLAYER];
		std::list<st_PLAYER*> playerList;
	};

	///-----------------------------------------------------
	///	Message Structure
	///-----------------------------------------------------
	struct st_MESSAGE
	{
		int		type;
		UINT64	clientID;
		void	*pPacket;
	};

public:
	WCHAR							_accessIP[16];

	///-----------------------------------------------------
	///	HeartBeat
	///-----------------------------------------------------
	void Request_Check_NotPlayingUser(void);
private:
	void Check_NotPlayingUser(void);

private:
	///-----------------------------------------------------
	///	Message Thread
	///-----------------------------------------------------
	static unsigned __stdcall MessageThread(void *arg);
	unsigned int Update_MessageThread(void);

	HANDLE		_hMessageThread;

	///-----------------------------------------------------
	///	Monitor Thread
	///-----------------------------------------------------
	static unsigned __stdcall MonitorThread(void *arg);
	unsigned int Update_MonitorThread(void);

	HANDLE		_hMonitorThread;
	bool		_bExitMonitor;

	///-----------------------------------------------------
	///	Player Allocation
	///-----------------------------------------------------
	void Alloc_Player(UINT64 clientID);
	void Free_Player(st_PLAYER *pPlayer);
	st_PLAYER * Find_PlayerByClientID(UINT64 clientID);
	st_PLAYER * Find_PlayerByAccountNo(INT64 accountNo);
	void Init_Player(st_PLAYER *pPlayer);
	void Clear_PlayerMap(void);

	///-----------------------------------------------------
	///	Room Allocation
	///-----------------------------------------------------
	void AddPlayer_Room(st_PLAYER *pPlayer, st_ROOM *pRoom);
	void DeletePlayer_Room(st_PLAYER *pPlayer, int roomNo);

	void Alloc_Room(int roomNo, int maxUser, char * enterToken);
	void Free_Room(st_ROOM *pRoom);
	bool Free_Room(int roomNo);
	st_ROOM* Find_Room(int roomNo);
	void Clear_Room(st_ROOM *pRoom);
	void Clear_RoomMap(void);

	///-----------------------------------------------------
	///	Packet Proc
	///-----------------------------------------------------
	bool Player_PacketProc(st_PLAYER *pPlayer, cNetPacketPool *dsPacket);

	void Request_Player_Login(st_PLAYER *pPlayer, cNetPacketPool *dsPacket);
	void Request_Player_EnterRoom(st_PLAYER *pPlayer, cNetPacketPool *dsPacket);
	void Request_Player_SendChat(st_PLAYER *pPlayer, cNetPacketPool *dsPacket);
	void Request_Player_HeartBeat(st_PLAYER *pPlayer, cNetPacketPool *dsPacket);
	
	void Request_Server_Disconnected(void);
	void Request_Server_ReissueToken(cLanPacketPool *dsPacket);
	void Request_Server_CreateRoom(cLanPacketPool *dsPacket);
	void Request_Server_DeleteRoom(cLanPacketPool *dsPacket);

	void Response_Player_Login(st_PLAYER *pPlayer, INT64 accountNo, BYTE status);
	void Response_Player_EnterRoom(st_PLAYER *pPlayer, int roomNo, BYTE status);
	void Response_Player_SendChat(st_PLAYER *pPlayer, WCHAR *msg, WORD msgLen);

	void SerializePacket_Player_Login(cNetPacketPool *sPacket, BYTE status, INT64 accountNo);
	void SerializePacket_Player_EnterRoom(cNetPacketPool *sPacket, INT64 accountNo, int roomNo, BYTE status);
	void SerializePacket_Player_SendChat(cNetPacketPool *sPacket, INT64 accountNo, WCHAR *ID, WCHAR *nick, WORD msgLen, WCHAR *msg);

	void Send_RoomCast(st_PLAYER *pPlayer, cNetPacketPool *sPacket);
	
private:
	CBattleSnake_ChatMonitor_LanClient*	_pMonitorClient;
	CBattleSnake_Chat_LanClient*		_pChatClient;

private:
	char							_connectToken[32];

	//	Room
	std::map<int, st_ROOM*>			_roomMap;
	cMemoryPool<st_ROOM, st_ROOM>	_roomPool;

	//	Message 
	SRWLOCK							_threadLock;
	//CRITICAL_SECTION				_threadLock;
	CONDITION_VARIABLE				_cnBufNotEmpty;
	cLockFreeQueue<st_MESSAGE*>		_msgQueue;
	cMemoryPool<st_MESSAGE, st_MESSAGE> _msgPool;

	//	Player
	std::map<UINT64, st_PLAYER*>	_playerMap;
	cMemoryPool<st_PLAYER, st_PLAYER> _playerPool;


	//	Monitor
	int								_updateCounter;
	cPdhCounter						_pdhCounter;
	cCpuCounter						_cpuCounter;


	//	config list
	bool							_chatLanClient_config;
	WCHAR							_chatLanClient_IP[16];
	short							_chatLanClient_port;
	int								_chatLanClient_threadCnt;
	bool							_chatLanClient_nagle;

	bool							_monitorLanClient_config;
	WCHAR							_monitorLanClient_IP[16];
	short							_monitorLanClient_port;
	int								_monitorLanClient_threadCnt;
	bool							_monitorLanClient_nagle;

public:
	UINT64							_monitor_updateFPS;
	UINT64							_monitor_playerPool;
	UINT64							_monitor_playerUsed;
	UINT64							_monitor_roomPool;
	UINT64							_monitor_roomUsed;
	UINT64							_monitor_msgPool;
	UINT64							_monitor_msgUsed;
	UINT64							_monitor_packetPool;
	float							_monitor_processCpuUsage;
	double							_monitor_privateBytes;
};