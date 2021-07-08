#include <list>
#include "cLockFreeQueue.h"
#include "cRingBuffer.h"
#include "cPacketPool_LanServer.h"
#include "cPacketPool_NetServer.h"

namespace TUNJI_LIBRARY
{
	class CLanClient
	{
	public:
		bool isConnected(void) {return _bClientOn;	}
		bool Connect(WCHAR *ip, short port, int threadCnt, bool bNagle);
		bool Disconnect(void);
		void SendPacket(cLanPacketPool *);

		void SetUpLanClient(void);
		void CleanUpLanClient(void);

		virtual void OnEnterJoinServer() = 0;
		virtual void OnLeaveServer() = 0;
		virtual void OnRecv(cLanPacketPool *) = 0;
		virtual void OnSend(int sendSize) = 0;

		virtual void OnError(int errorCode, WCHAR *) = 0;

	public:
		enum
		{
			WORKER_THREAD_MAX = 10
		};

		enum IO_COUNT_TYPE
		{
			DEC_WORKER = 1,
			INC_CONN_POST, DEC_CONN_POST,
			INC_SEND_POST, DEC_SEND_POST,
			INC_RECV_POST, DEC_RECV_POST,
			INC_DICN_POST, DEC_DICN_POST
		};

		enum Error_Type
		{
			eError_CompeltionPortError,
		};

	private:
		UINT64		_sendTPS;
		UINT64		_recvTPS;
		int			_threadCount;
		bool		_bNagle;

	private:

		struct st_CLIENT_INFO
		{
			st_CLIENT_INFO()
			{
				memset(&addr, 0, sizeof(addr));
				//memset(countTracker, 0, sizeof(IO_COUNT_TYPE) * 500);
				sock = INVALID_SOCKET;
				recv_ol = { 0, };
				send_ol = { 0, };
				ioCount = 0;
				bSend = false;
				bDisconnectNow = false;
				bLastPacket = false;
				sendq.ClearBuffer();
				recvq.ClearBuffer();
				//countIndex = 0;
				sendPacketSize = 0;
			};

			OVERLAPPED						send_ol;
			OVERLAPPED						recv_ol;
			bool							bSend;
			bool							bDisconnectNow;
			bool							bLastPacket;
			UINT64							sendPacketSize;
			UINT64							ioCount;
			UINT64							countIndex;
			cLockFreeQueue<cLanPacketPool*>	sendq;
			cRingBuffer						recvq;
			//IO_COUNT_TYPE					countTracker[500];
			cLanPacketPool*					packetBuffer[100];
			SOCKET							sock;
			SOCKADDR_IN						addr;
		};



#pragma pack(push,1)
		struct st_PACKET_HEADER
		{
			WORD wPayload;
		};
#pragma pack(pop)
		static unsigned __stdcall WorkerThread(void *arg);
		unsigned int Update_WorkerThread(void);

		void ReleaseSession(void);

		void recv_post(void);
		void send_post(void);

		void recv_complete(DWORD);
		void send_complete(DWORD);

		void InitializeSession(void);
		void FreeSentPacket(UINT64 iSize);
		void FreeSendQueuePacket(void);

		st_CLIENT_INFO			_session;

		HANDLE					_hWorkerThreadComport;
		HANDLE					_hPort;

		HANDLE					_hWorkerThread[WORKER_THREAD_MAX];

		bool					_bClientOn;
		bool					_bSetUp;
		WCHAR					_serverIP[16];
		short					_serverPort;
	};
};