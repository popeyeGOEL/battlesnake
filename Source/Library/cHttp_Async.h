#pragma once
#include <string>
#include <list>
#include "cLockFreeQueue.h"
#include "cRingBuffer.h"

#include "rapidjson\document.h"
#include "rapidjson\writer.h"
#include "rapidjson\stringbuffer.h"

#pragma comment(lib, "Wininet.lib")
#pragma comment(lib,"ws2_32")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Mswsock.lib")

using namespace rapidjson;

namespace TUNJI_LIBRARY
{
	class cHttp_Async
	{
	public:
		enum MSG_TYPE
		{
			en_TYPE_POST = 1,
			en_TYPE_GET,
		};

		cHttp_Async() {}
		virtual ~cHttp_Async() {}
		bool StartHttpClient(int threadCnt, bool bNagle, int maxUser);
		bool StopHttpClient(void);

		bool IsExecutedThread(void) {return _bWorkerThreadOn; }
		UINT64 GetSessionCount(void) { return _curSessionCount; }

		bool DirectRequestbyHttp(UINT64 requestID, WCHAR * url, DWORD urlLen, WCHAR *json, DWORD jsonLen, MSG_TYPE type);

		virtual void OnLeaveServer(UINT64 requestID) = 0;
		virtual void OnRecv(UINT64 requestID, char *recvData, int dataLen, int httpCode = 200) = 0;

	protected:
		bool Alloc_Session(UINT64 &requestID);
		void Free_Session(UINT64 requestID);

	private:
		///-----------------------------------------------------
		///	Data
		///-----------------------------------------------------
		enum
		{
			WORKER_THREAD_MAX = 10,
		};

		struct st_SESSION
		{
			bool			bDisconnect;
			UINT64			requestID;
			SOCKET			sock;
			SOCKADDR_IN		addr;
			PVOID			lpfnConnectEx;

			OVERLAPPED		conn_ol;
			OVERLAPPED		recv_ol;

			WCHAR			hostname[16];
			WCHAR			path[128];
			short			port;
			int				msgLen;
			WCHAR			requestMsg[1024];
			char			requestMsg_utf8[1024];

			int				recvBytes;
			char			responseMsg[1024];
		};

	private:
		///-----------------------------------------------------
		///	Http
		///-----------------------------------------------------
		bool Parse_RequestURL(st_SESSION *pSession, WCHAR *url, DWORD len);
		int	 CreateHttpMsg(st_SESSION *pSession, WCHAR *json, DWORD jsonLen, MSG_TYPE type);

		///-----------------------------------------------------
		///	Worker Thread
		///-----------------------------------------------------
		static unsigned __stdcall WorkerThread(void *arg);
		unsigned int Update_WorkerThread(void);

		bool Conn_Post(st_SESSION *pSession);
		bool Recv_Post(st_SESSION *pSession);
		bool Send_Post(st_SESSION *pSession);
		void Recv_Complete(st_SESSION *pSession, DWORD trans);
		void Conn_Complete(st_SESSION *pSession);

		HANDLE		_hWorkerThreadComport;
		HANDLE		_hWorkerThread[WORKER_THREAD_MAX];

		///-----------------------------------------------------
		///	Monitor Thread
		///-----------------------------------------------------
		static unsigned __stdcall MonitorThread(void *arg);
		bool Update_MonitorThread(void);

		HANDLE		_hMonitorThread;
		bool		_bExitMonitor;

		///-----------------------------------------------------
		///	Session
		///-----------------------------------------------------
		bool	CreateSocket(st_SESSION *pSession);
		int		RequestIDtoIndex(UINT64 requestID);
		void	ReleaseSession(st_SESSION *pSession);
		void	InitializeSession(st_SESSION* pSession);

	private:
		///-----------------------------------------------------
		///	Variables 
		///-----------------------------------------------------
		st_SESSION*	_sessionArray;
		UINT64		_curID;
		UINT64		_curSessionCount;
		cLockFreeQueue<int> _indexQueue;			// 현재 비어있는 소켓풀 인덱스

		bool		_bWorkerThreadOn;
		bool		_bNagle;
		int			_maxUser;
		int			_threadCnt;

		UINT64		_sendCounter;
		UINT64		_recvCounter;
		UINT64		_connectCounter;
		UINT64		_totalConnect;

	public:
		///-----------------------------------------------------
		///	Variables 
		///-----------------------------------------------------
		UINT64		_monitor_sendTPS;
		UINT64		_monitor_recvTPS;
		UINT64		_monitor_connectTPS;
		UINT64		_monitor_totalConnect;
	};
};