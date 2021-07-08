#pragma once
using namespace TUNJI_LIBRARY;
#include <list>
#include "cPdhCounter.h"
#include "cCpuCounter.h"
#include "cMemoryPool.h"

class CBattleSnake_Match_Http;
class CBattleSnake_Match_LanClient;
class CBattleSnake_MatchMonitor_LanClient;
class CBattleSnake_Match_NetServer : public CNetServer
{
public:
	CBattleSnake_Match_NetServer(UINT verCode, BYTE packetCode, BYTE packetKey1, BYTE packetKey2, int serverNo, char* masterToken, WCHAR *accessIP);
	virtual ~CBattleSnake_Match_NetServer();

	virtual void OnClientJoin(UINT64 clientID);
	virtual void OnClientLeave(UINT64 clientID);
	virtual bool OnConnectionRequest(WCHAR *ip, short port);
	virtual void OnRecv(UINT64 clientID, cNetPacketPool *dsPacket);
	virtual void OnSend(UINT64 clientID, int sendSize);
	virtual void OnError(int errorCode, WCHAR*);

	///-----------------------------------------------------
	///	Match Server
	///-----------------------------------------------------
	void Start_Match(void);
	void Stop_Match(void);

	///-----------------------------------------------------
	///	Match Status DB
	///-----------------------------------------------------
	bool Connect_MatchStatusDB(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, short dbPort);
	bool Disconnect_MatchStatusDB(void);

	///-----------------------------------------------------
	///	Match Lan Client
	///-----------------------------------------------------
	void Set_MatchLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool ConnectLanClient_Match(void);
	bool DisconnectLanClient_Match(void);

	bool Recieve_MasterRequest_Login(int serverNo);
	void Recieve_MasterRequest_RoomInfo(UINT64 clientKey, BYTE status, WORD battleServerNo, WCHAR *battleIP, WORD battlePort,
										int roomNo, char* connectToken, char* enterToken,
										WCHAR *chatIP, WORD chatPort);
	void Receive_MasterRequest_RoomEnter(UINT64 clientKey);

	///-----------------------------------------------------
	///	Monitor Lan Client
	///-----------------------------------------------------
	void Set_MonitorLanClient_Config(WCHAR *ip, short port, int threadCnt, bool bNagle);
	bool ConnectLanClient_Monitor(void);
	bool DisconnectLanClient_Monitor(void);

	///-----------------------------------------------------
	///	Http Net Client
	///-----------------------------------------------------
	bool Start_HttpClient(int threadCnt, bool bNagle, int maxUser);
	bool Stop_HttpClient(void);
	void Set_HttpURL(WCHAR *url, int len);

	void Fail_HttpRequest(UINT64 clientKey);	// 서버 응답 실패
	bool Receive_HttpRequest_SessionKey(UINT64 clientKey, int result, char *sessionKey);

private:
	///-----------------------------------------------------
	///	Data
	///-----------------------------------------------------
	enum Request_Status
	{
		none = 0,
		Status_Request_Login = 1,
		Status_Success_Login = 2,
		Status_Request_RoomInfo = 3,
		Status_Success_RoomInfo = 4,
		Status_Request_RoomEnter = 5,
		Status_Success_RoomEnter = 6
	};

	enum Login_Result
	{
		SUCCESS = 1,
		ERROR_NOTMATCH_SESSIONKEY = 2,
		ERROR_NOEXIST_ACCOUNTNO = 3,
		ERROR_ETC = 4,
		ERROR_VERSION = 5,
	};

	struct st_SESSION
	{
		UINT64	clientID;
		UINT64	clientKey;
		Request_Status curStatus;
		long	httpFailCount;
		UINT64	accountNo;
		char	sessionKey[64];
		UINT	verCode;

		WORD	battleServerNo;
		int		roomNo;
	};

private:
	///-----------------------------------------------------
	///	Packet Proc
	///-----------------------------------------------------
	bool PacketProc(st_SESSION *pSession, cNetPacketPool *dsPacket);

	bool Request_Session_Login(st_SESSION *pSession, cNetPacketPool *dsPacket);
	bool Request_Session_RoomInfo(st_SESSION *pSession, cNetPacketPool *dsPacket);
	bool Request_Session_RoomEnter(st_SESSION *pSession, cNetPacketPool *dsPacket);

	void Response_Session_Login(st_SESSION *pSession, BYTE status);
	void Response_Session_RoomInfo(st_SESSION *pSession, BYTE status,
								   WORD battleServerNo, WCHAR *battleIP, WORD battlePort, int roomNo, char *connectToken, char* enterToken,
								   WCHAR *chatIP, WORD chatPort);
	void Response_Session_RoomEnter(st_SESSION *pSession);

	void SerializePacket_Session_Login(cNetPacketPool *sPacket, BYTE status);
	void SerializePacket_Session_RoomInfo(cNetPacketPool *sPacket, BYTE status,
										  WORD battleServerNo, WCHAR *battleIP, WORD battlePort, int roomNo, char *connectToken, char* enterToken,
										  WCHAR *chatIP, WORD chatPort);
	void SerializePacket_Session_RoomEnter(cNetPacketPool *sPacket);

	///-----------------------------------------------------
	///	Session
	///-----------------------------------------------------
	void Alloc_Session(UINT64 clientID);
	void Free_Session(st_SESSION *pSession);
	st_SESSION* Find_SessionByClientID(UINT64 clientID);
	st_SESSION* Find_SessionByClientKey(UINT64 clientKey);
	void Init_Session(st_SESSION *pSession, UINT64 clientID, UINT64 clientKey);
	UINT64 Create_ClientKey(UINT64 clientID);

	CRITICAL_SECTION						_sessionLock;
	std::list<st_SESSION*>					_sessionList;
	cMemoryPool<st_SESSION, st_SESSION>		_sessionPool;

private:
	UINT		_verCode;
	int			_serverNo;
	UINT64		_curID;
	UINT64		_curLogin;

	WCHAR		_publicIP[16];

	CBattleSnake_Match_LanClient*			_pMatchClient;
	CBattleSnake_MatchMonitor_LanClient*	_pMonitorClient;
	CBattleSnake_Match_Http*				_pHttpClient;

	cCpuCounter								_cpuCounter;
	cPdhCounter								_pdhCounter;
	UINT64									_matchCounter;


	bool									_matchLanClient_config;
	WCHAR									_matchLanClient_IP[16];
	short									_matchLanClient_port;
	int										_matchLanClient_threadCnt;
	bool									_matchLanCliebt_nagle;

	bool									_monitorLanClient_config;
	WCHAR									_monitorLanClient_IP[16];
	short									_monitorLanClient_port;
	int										_monitorLanClient_threadCnt;
	bool									_monitorLanCliebt_nagle;
private:
	///-----------------------------------------------------
	///	Monitoring Thread
	///-----------------------------------------------------
	static unsigned __stdcall MonitorThread(void *arg);
	unsigned int Update_MonitorThread(void);
	HANDLE		_hMonitorThread;
	bool		_bExitMonitor;


public:
	UINT64		_monitor_packetPool;
	UINT64		_monitor_session;
	UINT64		_monitor_login;
	UINT64		_monitor_matchTPS;

	float		_monitor_process_cpu;
	double		_monitor_commit_MBytes;

	//float		_monitor_HardwareCpuUsage;
	//double		_monitor_HardwareRecvKBytes;
	//double		_monitor_HardwareSendKBytes;
	//double		_monitor_HardwareAvailableMBytes;
	//double		_monitor_HardwareNonpagedMBytes;
};