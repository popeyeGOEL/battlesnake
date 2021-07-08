#ifndef __TUNJI_LIBRARY_NETSERVER__
#define __TUNJI_LIBRARY_NETSERVER__
#include "cRingBuffer.h"
#include "cLockFreeQueue.h"
#include "cPacketPool_NetServer.h"
#include "cPacketPool_LanServer.h"

namespace TUNJI_LIBRARY
{
	class CNetServer
	{
	public:
		CNetServer(BYTE packetCode, BYTE packetKey1, BYTE packetKey2);

		bool IsServerOn(void) {return _bServerOn;}
		//	서버	시작을 위한 설정 내용입니다.TCP listen port 등을 지정합니다.

		bool start(WCHAR *ip, short port, int threadCnt, bool bNagle, int maxUser, int acceptPool);

		//	서버를 종료한다
		void stop(void);

		//	클라이언트 접속 해제 
		bool disconnect(UINT64 clientID);

		//	접속한 클라이언트 갯수를 얻는다.
		int getSessionCount(void) { return int(_curUser);}
		int GetUsedPacketPool(void) { return cNetPacketPool::GetUsedPoolSize();}

		//	사용자 정의 메시지를 전송합니다. 
		void sendPacket(UINT64 clientID, cNetPacketPool* sPacket);
		void sendLastPacket(UINT64 clientID, cNetPacketPool *sPacket);

		//	클라이언트 연결 이벤트, OnConnectionRequest에서 진입 허가를 한 클라이언트에 대해서만 콜백된다.
		//	콘텐츠 서버에게 공지
		virtual void OnClientJoin(UINT64 clientID) = 0;

		//	클라이언트 해제 이벤트
		//	콘텐츠 서버에게 공지
		virtual void OnClientLeave(UINT64 clientID) = 0;

		//	클라이언트가 서버로 처음 연결을 시도했을 때 콜백되는 이벤트 
		//	이 이벤트에서는 서버가 클라의 연결 시도를 수용할 것인지 거절할 것인지를 결정한다. 
		//	만약 거절할 경우 클라이언트는 서버와의 연결이 실패하며 클라이언트는 ErrorType_NotifyServerDeniedConnection 를 받게 된다. 
		//	만약 수용할 경우 클라이언트는 서버와의 연결이 성공하게 되며 클라이언트는 새 HostID 를 할당받게 된다. 이때 서버는 OnClientJoin 이벤트가 콜백된다.
		virtual bool OnConnectionRequest(WCHAR *ip, short port) = 0;

		//	패킷 수신 완료 후 콜백되는 이벤트
		virtual void OnRecv(UINT64 clientID, cNetPacketPool *) = 0;

		//	패킷 송신 완료 후 콜백되는 이벤트
		virtual void OnSend(UINT64 clientID, int sendSize) = 0;

		//virtual void OnWorkerThreadBegin() = 0;
		//virtual void OnWorkerThreadend() = 0;
		virtual void OnError(int errorCode, WCHAR*) = 0;

	protected:
		enum
		{
			WORKER_THREAD_MAX = 2000
		};

		enum Error_Type
		{
			enError_AlreadyConnected, enError_TCPConnectFail, enError_InvalidSessionKey,
			enError_ServerDeniedConnection, enError_ServerNotReady, enError_ServerPortListenFailure,
			enError_DisconnectFailure, enError_FindClientFailure, enError_OverMaxUser,
			enError_SendQueueisFull, enError_TransmitFileFailure, enError_AcceptExFailure
		};

		enum IO_COUNT_TYPE
		{
			DEC_WORKER = 1,
			INC_ACCEPT_POST, DEC_ACCEPT_POST,
			INC_SEND_POST, DEC_SEND_POST,
			INC_RECV_POST, DEC_RECV_POST,
			INC_SESSION_LOCK, DEC_SESSION_LOCK, DEC_SESSION_UNLOCK, 
			RECV_ZERO ,ACCEPT, DISCONNECT, RELEASE, 
		};

	public:
		WCHAR	_serverIP[16];
		short	_serverPort;

		//	Monitor Factor
		UINT64						_monitor_sendTPS;
		UINT64						_monitor_recvTPS;
		UINT64						_monitor_acceptTPS;
		UINT64						_monitor_totalAccept;
		UINT64						_monitor_AcceptSocketPool;
		UINT64						_monitor_AcceptSocketQueue;

	private:
		struct st_ACCEPT_OVERLAP : public OVERLAPPED
		{
			int		arrayIndex;
		};

		struct st_CLIENT_INFO
		{
			st_CLIENT_INFO()
			{
				memset(&addr, 0, sizeof(addr));
				//memset(countTracker, 0, sizeof(IO_COUNT_TYPE) * 500);
				memset(&accp_ol, 0, sizeof(accp_ol));
				sock = INVALID_SOCKET;
				clientID = 0;
				recv_ol = { 0, };
				send_ol = { 0, };
				ioCount = 0;
				bSend = false;
				bReadyToDisconnect = false;
				bDisconnectNow = false;
				sendq.ClearBuffer();
				recvq.ClearBuffer();
				countIndex = 0;
				sendPacketSize = 0;
				bLastPacket = 0;
			};

			UINT64							clientID;
			st_ACCEPT_OVERLAP				accp_ol;
			OVERLAPPED						send_ol;
			OVERLAPPED						recv_ol;
			bool							bSend;
			bool							bReadyToDisconnect;
			bool							bDisconnectNow;
			bool							bLastPacket;
			UINT64							sendPacketSize;
			UINT64							ioCount;
			UINT64							countIndex;
			cLockFreeQueue<cNetPacketPool*>	sendq;
			cRingBuffer						recvq;
			IO_COUNT_TYPE					countTracker[500];
			cNetPacketPool*					packetBuffer[100];
			SOCKET							sock;
			SOCKADDR_IN						addr;
		};

		static unsigned __stdcall	AcceptThread(void *arg);
		bool				Update_AcceptThread(void);

		static unsigned __stdcall workerThread(void *arg);
		unsigned int update_workerThread(void);

		static unsigned __stdcall MonitorThread(void *arg);
		bool Update_MonitorThread(void);

		int ClientIDtoIndex(UINT64);
		st_CLIENT_INFO* findClientOverlap(OVERLAPPED*);
		UINT64 createClientID(int index);
		void ReleaseSession(st_CLIENT_INFO*);
		
		st_CLIENT_INFO* AcquireSessionLock(UINT64 clientID);
		void ReleaseSessionLock(st_CLIENT_INFO* pClient);

		bool accept_post(st_CLIENT_INFO*);
		void recv_post(st_CLIENT_INFO*);
		void send_post(st_CLIENT_INFO*);
		void recv_complete(st_CLIENT_INFO*, DWORD);
		void send_complete(st_CLIENT_INFO*, DWORD);
		void accp_complete(st_CLIENT_INFO *);
		void InitializeSession(st_CLIENT_INFO* pClient);
		void FreeSentPacket(st_CLIENT_INFO* pClient, UINT64 iSize);
		void FreeSendQueuePacket(st_CLIENT_INFO* pClient);
		void SetIoCompletionPort(st_CLIENT_INFO* pClient);

		BYTE						_packetCode;
		BYTE						_packetKey1;
		BYTE						_packetKey2;

		//LPFN_ACCEPTEX				_lpfnAcceptEx;
		PVOID						_lpfnAcceptEx;
		HANDLE						_hAcceptThread;
		CONDITION_VARIABLE			_cnIndexQueueIsNotEmpty;
		cLockFreeQueue<int>			_indexQueue;
		SRWLOCK						_indexLock;
		UINT64						_curPostedAcceptEx;
		int							_maxAcceptPool;

		UINT64						_curID;
		UINT64						_curAcceptID;
		HANDLE						_hWorkerThreadComport;
		SOCKET						_listenSock;
		SOCKADDR_IN					_serverAddr;

		int							_maxUser;
		UINT64						_curUser;
		bool						_bNagle;
		int							_threadCount;
		bool						_bServerOn;

		UINT64						_monitor_sendCounter;
		UINT64						_monitor_recvCounter;
		UINT64						_monitor_acceptCounter;

		HANDLE						_hWorkerThread[WORKER_THREAD_MAX];

		HANDLE						_hMonitorThread;
		bool						_bExitMonitor;

		st_CLIENT_INFO*				_clientArray;
	};
}
#endif