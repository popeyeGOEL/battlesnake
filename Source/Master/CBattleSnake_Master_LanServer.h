#pragma once
#include <list>
#include <map>
#include "cPdhCounter.h"
#include "cCpuCounter.h"
#include "cMemoryPool.h"

using namespace TUNJI_LIBRARY;
class CBattleSnake_MasterMonitor_LanClient;
class CBattleSnake_Master_LanServer: public CLanServer
{
public:
	CBattleSnake_Master_LanServer(char *masterToken);
	~CBattleSnake_Master_LanServer();

	void Start_Master(void);
	void Stop_Master(void);
	virtual void OnClientJoin(UINT64 clientID);
	virtual void OnClientLeave(UINT64 clientID);
	virtual bool OnConnectionRequest(WCHAR *ip, short port);
	virtual void OnRecv(UINT64 clientID, cLanPacketPool *);
	virtual void OnSend(UINT64 clientID, int sendSize);
	virtual void OnError(int errorCode, WCHAR*);

private:
	enum SERVER_TYPE
	{
		NONE = 0,
		BATTLE_SERVER = 1,
		MATCH_SERVER = 2,
	};

	struct st_SERVER
	{
		UINT64	clientID;
		int		serverNo;
		SERVER_TYPE serverType;
		bool	bLogin;
	};
	
	enum Room_Tracker
	{
		Match_RoomInfo,
		Match_EnterRoomFail,
		Match_EnterRoomSuccess,
		Battle_RoomCreated,
		Battle_RoomClosed,
		Battle_LeaveRoom,
	};

	struct st_TRACKER
	{
		Room_Tracker	type;
		int				available;
		UINT64			accountNo;
	};

	struct st_WAIT_ROOM
	{
		st_WAIT_ROOM()
		{
			memset(tracker, 0, sizeof(st_TRACKER) * 500);
			trackCount = 0;
		};

		int		serverNo;
		int		roomNo;
		int		openTime;
		int		available;		//	가용 인원 
		int		maxUser;		//	최대 인원 
		char	enterToken[32];	//	입장 토큰 

		st_TRACKER	tracker[500];
		int			trackCount;
	};

	struct st_BATTLE_INFO
	{
		int		serverNo;
		char	connectionToken[32];	//	연결 토큰 
		WCHAR	battleServerIP[16];
		WCHAR	chatServerIP[16];
		short	battleServerPort;
		short	chatServerPort;
	};

	struct st_SESSION
	{
		bool	bLogOut;
		UINT64	clientKey;
		UINT64	accountNo;
		int		serverNo;
		int		roomNo;
	};

	//struct RoomList_Sort
	//{
	//	bool operator()(const st_WAIT_ROOM* pRoom1, const st_WAIT_ROOM* pRoom2) const
	//	{
	//		//	최소 배분
	//		if ((pRoom2->available > pRoom1->available && pRoom1->available > 0) ||
	//			(pRoom2->available == 0 && pRoom1->available != 0))
	//		{
	//			//	A-B (pRoom2 - pRoom1)
	//			//	뒤로 넘길 조건
	//			return true;
	//		}
	//		return false;
	//		
	//		////	균등 배분
	//		//if(pRoom2->available < pRoom1->available)
	//		//{
	//		//	//	A-B (pRoom2 - pRoom1)
	//		//	//	뒤로 넘길 조건
	//		//	return true;
	//		//}
	//		//return false;
	//	}
	//};

private:
	void PacketProc(st_SERVER *pServer, cLanPacketPool *dsPacket);

	///-----------------------------------------------------
	///	Match Server Packet Proc
	///-----------------------------------------------------
	void Request_Match_Login(st_SERVER *pServer, cLanPacketPool *dsPacket);
	void Request_Match_RoomInfo(st_SERVER *pServer, cLanPacketPool *dsPacket);
	void Request_Match_UserEnterSuccess(st_SERVER *pServer, cLanPacketPool *dsPacket);
	void Request_Match_UserEnterFail(st_SERVER *pServer, cLanPacketPool *dsPacket);

	void Response_Match_Login(st_SERVER *pServer);
	void Response_Match_RoomInfo(st_SERVER *pServer, st_SESSION *pSession);
	void Response_Match_RoomEnter(st_SERVER *pServer, UINT64 clientKey);

	///-----------------------------------------------------
	///	Battle Server Packet Proc
	///-----------------------------------------------------
	void Request_Battle_Login(st_SERVER* pServer, cLanPacketPool *dsPacket);
	void Request_Battle_ReissueToken(st_SERVER* pServer, cLanPacketPool *dsPacket);
	void Request_Battle_RoomCreated(st_SERVER* pServer, cLanPacketPool *dsPacket);
	void Request_Battle_RoomClosed(st_SERVER* pServer, cLanPacketPool *dsPacket);
	void Request_Battle_UserLogout(st_SERVER *pServer, cLanPacketPool *dsPacket);

	void Response_Battle_Login(st_SERVER *pServer);
	void Response_Battle_ReissueToken(st_SERVER* pServer, st_BATTLE_INFO *pBattle, UINT req);
	void Response_Battle_RoomCreated(st_SERVER* pServer, int roomNo, UINT req);
	void Response_Battle_RoomClosed(st_SERVER *pServer, int roomNo, UINT req);
	void Response_Battle_UserLogout(st_SERVER *pServer, int roomNo, UINT req);
	
	///-----------------------------------------------------
	///	Response Packet Serialize
	///-----------------------------------------------------
	void SerializePacket_Match_Login(cLanPacketPool *sPacket, int serverNo);
	void SerializePacket_Match_RoomInfo(cLanPacketPool *sPacket, UINT64 clientKey, BYTE status,
										WORD battleServerNo = 0 , WCHAR *battleIP = 0, WORD battlePort = 0, int roomNo = 0,
										char *connectToken = 0, char *enterToken = 0,
										WCHAR *chatIP = 0, WORD chatPort = 0);

	void SerializePacket_Match_RoomEnter(cLanPacketPool *sPacket, UINT64 clientKey);

	void SerializePacket_Battle_Login(cLanPacketPool *sPacket, int serverNo);
	void SerializePacket_Battle_ReissueToken(cLanPacketPool *sPacket, char *connectToken, UINT reqSequence);
	void SerializePacket_Battle_RoomCreated(cLanPacketPool *sPacket, int roomNo, UINT reqSequence);
	void SerializePacket_Battle_RoomClosed(cLanPacketPool *sPacket, int roomNo, UINT reqSequence);
	void SerializePacket_Battle_UserLogout(cLanPacketPool *sPacket, int roomNo, UINT reqSequence);

private:
	///-----------------------------------------------------
	///	Allocation
	///-----------------------------------------------------

	void Alloc_Server(UINT64 clientID);
	void Free_Server(st_SERVER * pServer);
	st_SERVER* Find_ServerByClientID(UINT64 clientID);
	st_SERVER* Find_ServerByServerNo(int serverNo);
	void Init_Server(st_SERVER *pServer, UINT64 clientID);

	int Create_BattleServerNo(void);
	void Alloc_BattleInfo(int serverNo, char *connectionToken, WCHAR *battleIP, short battlePort, WCHAR *chatIP, short chatPort);
	void Free_BattleInfo(st_BATTLE_INFO *pBattle);
	void Init_BattleInfo(st_BATTLE_INFO *pBattle, int serverNo, char* connectionToken, WCHAR *battleIP, short battlePort, WCHAR *chatIP, short chatPort);
	st_BATTLE_INFO* Find_BattleInfo(int serverNo);
	
	void Track_WaitRoom(st_WAIT_ROOM *pRoom, Room_Tracker type, int curUser, UINT64 clientKey);
	void Alloc_WaitRoom(int serverNo, int roomNo, int maxUser, char* enterToken);
	void Free_WaitRoom(int serverNo, int roomNo);
	void Init_WaitRoom(st_WAIT_ROOM *pRoom, int serverNo, int roomNo, int maxUser, char* enterToken);
	st_WAIT_ROOM* Find_WaitRoom(int serverNo, int roomNo);
	void Increment_WaitRoom(int serverNo, int roomNo, Room_Tracker type, UINT64 clientKey);
	st_WAIT_ROOM* Find_HighPriorityWaitRoom(UINT64 clientKey);
	void Clear_WaitRoom(int serverNo, int roomNo);

	st_SESSION* Alloc_Session(UINT64 clientKey, UINT64 accountNo);
	void Free_Session(UINT64 clientKey);
	st_SESSION* Find_SessionByClientKey(UINT64 clientKey);
	st_SESSION* Find_SessionByAccountNo(UINT64 accountNo, int roomNo);
	void Init_Session(st_SESSION *pSession, UINT64 clientKey, UINT64 accountNo);

private:
	///-----------------------------------------------------
	///	서버 목록, 방 목록, 유저 관리 리스트
	///-----------------------------------------------------
	std::list<st_SERVER*>				_serverList;
	std::list<st_BATTLE_INFO*>			_battleInfoList;
	std::list<st_WAIT_ROOM*>			_roomList;
	std::map<UINT64, st_SESSION*>		_sessionMap;

	///-----------------------------------------------------
	///	동기화 객체
	///-----------------------------------------------------
	SRWLOCK	_serverLock;
	SRWLOCK _battleInfoLock;
	CRITICAL_SECTION _waitRoomLock;
	//SRWLOCK _waitRoomLock;
	CRITICAL_SECTION _sessionLock;
	//SRWLOCK _sessionLock;

	///-----------------------------------------------------
	///	생성 및 파괴가 빈번한 방과 유저는 풀로 관리한다.
	///-----------------------------------------------------

	cMemoryPool<st_WAIT_ROOM, st_WAIT_ROOM> _roomPool;
	cMemoryPool<st_SESSION, st_SESSION> _sessionPool;

	///-----------------------------------------------------
	///	기타 변수 
	///-----------------------------------------------------
	char			_masterToken[32];	//	연결확인용 마스터 토큰 
	int				_battleMaskID;		//	배틀서버 아이디 생성용 마스크
	int				_curBattleID;

	///-----------------------------------------------------
	///	Monitor Handle
	///-----------------------------------------------------
	HANDLE			_hMonitorThread;
	bool			_bExitMonitor;

	///-----------------------------------------------------
	///	Monitor Variables
	///-----------------------------------------------------	
	CBattleSnake_MasterMonitor_LanClient*	_pMonitorClient;
	int				_battleLoginServer;
	int				_matchLoginServer;
	int				_battleConnectServer;
	int				_matchConnectServer;
	
	bool			_bMonitorConfigSet;
	WCHAR			_monitorIP[16];
	short			_monitorPort;
	int				_monitorThreadCnt;
	bool			_monitorbNagle;

	cCpuCounter		_cpuCounter;
	cPdhCounter		_pdhCounter;

private:
	///-----------------------------------------------------
	///	Monitoring Thread
	///-----------------------------------------------------
	static unsigned __stdcall MonitorThread(void *arg);
	unsigned int Update_MonitorThread(void);

public:
	///-----------------------------------------------------
	///	Monitoring LanClient
	///-----------------------------------------------------
	void SetMonitorClientConfig(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool ConnectMonitorClient(void);
	void DisconnectMonitorClient(void);

	///-----------------------------------------------------
	///	모니터링 요소
	///-----------------------------------------------------
	UINT64	_monitor_battleServerConnection;
	UINT64	_monitor_battleServerLogin;
	UINT64	_monitor_matchServerConnection;
	UINT64	_monitor_matchServerLogin;

	UINT64	_monitor_packetPool;
	UINT64	_monitor_waitSessionUsed;
	UINT64	_monitor_waitSessionPool;
	UINT64	_monitor_waitRoomUsed;
	UINT64	_monitor_waitRoomPool;

	float	_monitor_process_cpu;
	float	_monitor_unit_cpu;
	double	_monitor_privateBytes;

};