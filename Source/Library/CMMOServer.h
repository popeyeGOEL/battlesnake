#pragma once
#include "cRingBuffer.h"
#include "cLockFreeQueue.h"
#include "cQueue.h"
#include "cPacketPool_NetServer.h"
#include "cPacketPool_LanServer.h"
#include "CNetSession.h"
#include "cPdhCounter.h"
#include "cCpuCounter.h"

namespace TUNJI_LIBRARY
{
	class CMMOServer
	{
	public:
		CMMOServer(int maxSession, int authSleepCount, int gameSleepCount, int sendSleepCount,
					BYTE packetCode, BYTE packetKey1, BYTE packetKey2);
		virtual ~CMMOServer();

		bool Start(WCHAR *bindIP, int port, int threadCnt, bool bNagle, int maxAcceptPool);
		bool Stop(void);

		///	외부에서 세션 객체 연결
		void SetSessionArray(CNetSession *pSession, int index);

		///	전체 유저 패킷 보내기 
		void SendPacket_AllLoginUser(cNetPacketPool *sPacket, UINT64 execptID = 0);

		///	네트워크 서버 단에서 보내는 패킷
		void SendPacket(cNetPacketPool *sPacket, UINT64 clientID);

		bool IsServerOn(void) { return _bServerOn; }
		UINT64 getSessionCount(void) {return _curUser;}
		int GetUsedPacketPool(void) {return cNetPacketPool::GetUsedPoolSize();}

		int	ClientIDtoIndex(UINT64 clientID);

	private:
		CNetSession* FindClientbyOverlap(OVERLAPPED * acceptOl);
		UINT64	CreateClientID(int index);
		void ReleaseSession(CNetSession *pSession);

		bool Accept_Post(CNetSession *pSession);
		void Recv_Post(CNetSession *pSession);
		void Send_Post(CNetSession *pSession);
		void Accept_Complete(CNetSession *pSession);
		void Recv_Complete(CNetSession *pSession, DWORD trans);
		void Send_Complete(CNetSession *pSession, DWORD trans);

		void SetIoCompletionPort(CNetSession *pSession);

	private:
		enum
		{
			WORKER_THREAD_MAX = 1000,
			AUTH_PACKET_LIMIT = 1,
			GAME_PACKET_LIMIT = 3,
			AUTH_TO_GAME_LIMIT = 200,
			LOGOUT_SESSION_LIMIT = 200,
			RELEASE_SESSION_LIMIT = 200,
		};

		enum Error_Type
		{
			enError_AlreadyConnected, enError_TCPConnectFail, enError_InvalidSessionKey,
			enError_ServerDeniedConnection, enError_ServerNotReady, enError_ServerPortListenFailure,
			enError_DisconnectFailure, enError_FindClientFailure, enError_OverMaxUser,
			enError_SendQueueisFull, enError_TransmitFileFailure, enError_AcceptExFailure
		};

		void Error(int errorCode, WCHAR * formatStr, ...);

		///	Auth, Game 스레드 처리 함수 
		void Auth_PacketProc(void);
		void Auth_LeaveProc(void);
		void Auth_LogoutProc(void);

		void Game_AuthToGameProc(void);
		void Game_PacketProc(void);
		void Game_LeaveProc(void);
		void Game_ReleaseProc(void);

	protected:
		///	Auth 모드 업데이트 이벤트 로직 처리 
		virtual void OnAuth_Update(void) = 0;
		
		///	Game 모드 업데이트 이벤트 로직 처리 
		virtual void OnGame_Update(void) = 0;

		virtual void OnError(int errorCode, WCHAR *errorText) = 0;

	public:
		WCHAR				_bindIP[16];
		int					_port;

	private:
		///	---------------------------------------------------
		/// listen Socket 
		///	---------------------------------------------------
		SOCKET				_listenSock;
		SOCKADDR_IN			_addr;
		bool				_bNagle;
		int					_threadCnt;

		BYTE				_packetCode;
		BYTE				_packetKey1;
		BYTE				_packetKey2;

		bool				_bServerOn;
		UINT64				_curID;
		UINT64				_curUser;
		UINT64				_maxUser;
		int					_maxAcceptPool;

		///	---------------------------------------------------
		/// Accept
		///	---------------------------------------------------
		PVOID				_lpfnAcceptEx;
		HANDLE				_hAcceptThread;
		CONDITION_VARIABLE	_cnIndexQueueIsNotEmpty;
		cLockFreeQueue<int>	_indexQueue;
		SRWLOCK				_indexLock;
		UINT64				_curPostedAcceptEx;

		///	---------------------------------------------------
		/// Auth
		///	---------------------------------------------------
		HANDLE				_hAuthThread;
		bool				_bExitAuth;
		int					_authSleepCount;

		///	---------------------------------------------------
		/// Game Update
		///	---------------------------------------------------
		HANDLE				_hGameUpdateThread;
		bool				_bExitGame;
		int					_gameSleepCount;

		///	---------------------------------------------------
		/// IOCP
		///	---------------------------------------------------
		HANDLE				_hWorkerThread[WORKER_THREAD_MAX];
		HANDLE				_hWorkerThreadComport;

		CNetSession**		_pSessionArray;

		///	---------------------------------------------------
		/// Send
		///	---------------------------------------------------
		HANDLE				_hSendThread;
		bool				_bExitSend;
		int					_sendSleepCount;

		///	---------------------------------------------------
		/// Monitor
		///	---------------------------------------------------
		HANDLE				_hMonitorThread;
		bool				_bExitMonitor;

		///	---------------------------------------------------
		/// 스레드 함수 
		///	---------------------------------------------------
		static unsigned __stdcall	AcceptThread(void *arg);
		bool				Update_AcceptThread(void);

		static unsigned __stdcall	AuthThread(void *arg);
		bool				Update_AuthThread(void);

		static unsigned __stdcall GameUpdateThread(void *arg);
		bool				Update_GameUpdateThread(void);

		static unsigned __stdcall WorkerThread(void *arg);
		bool				Update_WorkerThread(void);

		static unsigned __stdcall SendThread(void *arg);
		bool				Update_SendThread(void);

		static unsigned __stdcall MonitorThread(void *arg);
		bool				Update_MonitorThread(void);

		LONG64				_monitor_AuthUpdateCounter;
		LONG64				_monitor_GameUpdateCounter;
		LONG64				_monitor_AcceptCounter;
		LONG64				_monitor_GamePacketProcCounter;
		LONG64				_monitor_AuthPacketProcCounter;
		LONG64				_monitor_PacketSendCounter;

		cPdhCounter			_pdhCounter;
		cCpuCounter			_cpuCounter;

	public:
		///	---------------------------------------------------
		/// 모니터링용 멤버 변수
		///	---------------------------------------------------
		LONG64				_monitor_AcceptSocketPool;
		LONG64				_monitor_AcceptSocketQueue;
		LONG64				_monitor_AcceptSocket;
		LONG64				_monitor_sessionMode_Auth;
		LONG64				_monitor_sessionMode_Game;
		LONG64				_monitor_packetPool;
		LONG64				_monitor_AuthUpdateFPS;
		LONG64				_monitor_GameUpdateFPS;
		LONG64				_monitor_AcceptTPS;
		LONG64				_monitor_GamePacketProcTPS;
		LONG64				_monitor_AuthPacketProcTPS;
		LONG64				_monitor_PacketSendTPS;

		float				_monitor_ProcessCpuUsage;
		double				_monitor_PrivateBytes;

		float				_monitor_HardwareCpuUsage;
		double				_monitor_HardwareRecvKBytes;
		double				_monitor_HardwareSendKBytes;
		double				_monitor_HardwareAvailableMBytes;
		double				_monitor_HardwareNonpagedMBytes;
	};

};