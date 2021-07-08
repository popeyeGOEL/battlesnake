#include <stdio.h>
#include <tchar.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <WinInet.h>
#include <Mswsock.h>

#include <process.h>
#include "cHttp_Async.h"

#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

namespace TUNJI_LIBRARY
{
	bool cHttp_Async::StartHttpClient(int threadCnt, bool bNagle, int maxUser)
	{
		if (InterlockedExchange8((char*)&_bWorkerThreadOn, 1) == 1) return false;

		// Init Variables
		_bNagle = bNagle;
		_maxUser = maxUser;
		_threadCnt = threadCnt;

		_sessionArray = new st_SESSION[maxUser];
		for (int i = 0; i < maxUser; ++i)
		{
			_indexQueue.Enqueue(i);
			InitializeSession(&_sessionArray[i]);
		}

		_curID				= 0;
		_curSessionCount	= 0;

		_sendCounter		= 0;
		_recvCounter		= 0;
		_connectCounter		= 0;

		WSADATA wsa;
		int ret				= 0;

		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
	
		// Create Iocp Handle 
		_hWorkerThreadComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

		////	Setting Socket 
		//for (int i = 0; i < _maxUser; ++i)
		//{
		//	CreateSocket(&_sessionArray[i]);
		//}

		//	Create Worker Thread
		DWORD threadID;
		for (int i = 0; i < _threadCnt; ++i)
		{
			_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (PVOID)this, 0, (unsigned int*)&threadID);
		}
		
		//	Create Monitor Thread
		_bExitMonitor = false;
		_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&threadID);

		wprintf(L"Initialized Http Client\n");
		return true;
	}

	bool cHttp_Async::StopHttpClient(void)
	{
		if (InterlockedExchange8((char*)&_bWorkerThreadOn, 0) == 0) return false;

		//	Shutdown client socket
		for (int i = 0; i < _maxUser; ++i)
		{
			closesocket(_sessionArray[i].sock);
		}

		//	Terminate WorkerThread
		PostQueuedCompletionStatus(_hWorkerThreadComport, 0, NULL, NULL);
		DWORD status = WaitForMultipleObjects(_threadCnt, _hWorkerThread, TRUE, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"Http thread terminated! \n");
		}

		for (int i = 0; i < _threadCnt; ++i)
		{
			CloseHandle(_hWorkerThread[i]);
		}

		CloseHandle(_hWorkerThreadComport);

		_indexQueue.ClearBuffer();

		//	Terminate Monitor Thread
		_bExitMonitor = true;
		status = WaitForSingleObject(_hMonitorThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"NetServer monitor thread terminated! \n");
		}

		CloseHandle(_hMonitorThread);
		WSACleanup();
		delete[] _sessionArray;

		return true;
	}

	bool cHttp_Async::DirectRequestbyHttp(UINT64 requestID, WCHAR * url, DWORD urlLen, WCHAR *json, DWORD jsonLen, MSG_TYPE type)
	{
		int index = RequestIDtoIndex(requestID);
		st_SESSION* pSession = &_sessionArray[index];

		do {
			if (!Parse_RequestURL(pSession, url, urlLen))
			{
				LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - DirectRequestbyHttp : parse error");
				break;
			}
			pSession->msgLen = CreateHttpMsg(pSession, json, jsonLen, type);
			if (pSession->msgLen == 0) break;

			if (!Conn_Post(pSession))
			{
				LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - DirectRequestbyHttp : connect error");
				break;
			}

			return true;
		} while (0);

		LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - DirectRequestbyHttp : %d, %s, %d, %s, %d, %s", requestID, url, urlLen, json, jsonLen, pSession->requestMsg);
		ReleaseSession(pSession);
		return false;
	}

	bool cHttp_Async::Parse_RequestURL(st_SESSION *pSession, WCHAR *url, DWORD len)
	{
		URL_COMPONENTS	parsed;
		parsed = { 0, };

		parsed.dwStructSize = sizeof(URL_COMPONENTS);
		parsed.lpszHostName = pSession->hostname;
		parsed.lpszUrlPath = pSession->path;
		parsed.dwUrlPathLength = sizeof(WCHAR) * 16;
		parsed.dwHostNameLength = sizeof(WCHAR) * 128;

		if (!InternetCrackUrl(url, len * sizeof(WCHAR), 0, &parsed)) return false;

		pSession->port = parsed.nPort;
		if (pSession->port == 0) pSession->port = 80;

		return true;
	}

	int	 cHttp_Async::CreateHttpMsg(st_SESSION *pSession, WCHAR *json, DWORD jsonLen, MSG_TYPE type)
	{
		//idx += wsprintf(result + idx, szLog);
		int msgIdx = 0;

		if(type == en_TYPE_GET)
		{
			WCHAR getPath[1024];
			int idx = wsprintf(getPath, L"%s?%s", pSession->path, json);
			getPath[idx] = L'\0';

			msgIdx = wsprintf(pSession->requestMsg, L"GET %s HTTP/1.1\r\n", getPath);
			jsonLen = 0;
		}
		else
		{
			msgIdx = wsprintf(pSession->requestMsg, L"POST %s HTTP/1.1\r\n", pSession->path);
		}

		msgIdx += wsprintf(pSession->requestMsg + msgIdx, L"Host: %s\r\n", pSession->hostname);
		msgIdx += wsprintf(pSession->requestMsg + msgIdx, L"Content-Type: application/x-www-form-urlencoded\r\n");
		msgIdx += wsprintf(pSession->requestMsg + msgIdx, L"Content-Length: %d\r\n", jsonLen);
		msgIdx += wsprintf(pSession->requestMsg + msgIdx, L"Connection: Close\r\n\r\n");

		if (jsonLen != 0)
		{
			msgIdx += wsprintf(pSession->requestMsg + msgIdx, L"%s", json);
		}

		int ret = WideCharToMultiByte(CP_UTF8, 0, pSession->requestMsg, wcslen(pSession->requestMsg), pSession->requestMsg_utf8, 1024, 0, 0);
		pSession->requestMsg_utf8[ret] = '\0';

		return ret;
	}

	unsigned int cHttp_Async::WorkerThread(void *arg)
	{
		cHttp_Async *pThis = (cHttp_Async*)arg;
		return pThis->Update_WorkerThread();
	}

	unsigned int cHttp_Async::Update_WorkerThread(void)
	{	
		__int64 ret;
		bool bRet;
		DWORD trans;
		st_SESSION* pSession;
		OVERLAPPED *ol;

		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		while (1)
		{
			pSession	= nullptr;
			ol			= nullptr;
			trans		= 0;

			bRet = GetQueuedCompletionStatus(_hWorkerThreadComport, &trans, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&ol, INFINITE);
			if (bRet == FALSE)
			{
				LOG(L"GQCS_Error", cSystemLog::LEVEL_DEBUG, L"#cHttp_Async - Update_WorkerThread : GetqueuedCompletionStatus Error %d", WSAGetLastError());
				if (pSession != nullptr)
				{
					ReleaseSession(pSession);
				}
				continue;
			}

			if (trans == 0 && pSession == nullptr && ol == nullptr)
			{
				PostQueuedCompletionStatus(_hWorkerThreadComport, 0, (ULONG)0, nullptr);
				break;
			}

			if (&pSession->conn_ol == ol)
			{
				Conn_Complete(pSession);
			}
			else if (&pSession->recv_ol == ol)
			{
				if (trans == 0)
				{
					ReleaseSession(pSession);
				}
				else
				{
					Recv_Complete(pSession, trans);
				}
			}
		};

		return 0;
	}

	bool cHttp_Async::Conn_Post(st_SESSION *pSession)
	{
		if (CreateSocket(pSession) == false) return false;

		pSession->conn_ol = { 0, };
		pSession->bDisconnect = false;
		
		LPFN_CONNECTEX lpfnConnectEx = (LPFN_CONNECTEX)pSession->lpfnConnectEx;
		if (lpfnConnectEx(pSession->sock, (SOCKADDR*)&pSession->addr, sizeof(pSession->addr), NULL, 0, NULL, &pSession->conn_ol) == SOCKET_ERROR &&
			WSAGetLastError() != ERROR_IO_PENDING)
		{
			LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - Conn_Post : %d", WSAGetLastError());
			return false;
		}

		return true;
	}

	bool cHttp_Async::Recv_Post(st_SESSION *pSession)
	{
		__int64 ret;

		WSABUF wsabuf;
		DWORD flag = 0;

		wsabuf.buf = pSession->responseMsg + pSession->recvBytes;
		wsabuf.len = 1024 - pSession->recvBytes;

		pSession->recv_ol = { 0, };

		ret = WSARecv(pSession->sock, &wsabuf, 1, NULL, &flag, &pSession->recv_ol, NULL);
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - Recv_Post : Recv Failed code %d requestID %d", WSAGetLastError(), pSession->requestID);
			return false;
		}


		return true;
	}
	bool cHttp_Async::Send_Post(st_SESSION *pSession)
	{
		__int64 ret;
		//WSABUF wsabuf;

		//wsabuf.buf = pSession->requestMsg_utf8;
		//wsabuf.len = pSession->msgLen;
		
		//ret = WSASend(pSession->sock, &wsabuf, 1, NULL, 0 , 0 , NULL);
		ret = send(pSession->sock, pSession->requestMsg_utf8, pSession->msgLen, 0);
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{
			LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - Send_Post : Send Failed code %d requestID %d length %d", WSAGetLastError(), pSession->requestID, pSession->msgLen);
			return false;
		}

		//if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		//{
		//	LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - Send_Post : Send Failed code %d requestID %d length %d", WSAGetLastError(), pSession->requestID, pSession->msgLen);
		//	return false;
		//}

		return true;
	}
	
	void cHttp_Async::Conn_Complete(st_SESSION *pSession)
	{
		setsockopt(pSession->sock, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

		if (!Recv_Post(pSession))
		{
			ReleaseSession(pSession);
			return;
		}

		if (!Send_Post(pSession))
		{
			shutdown(pSession->sock, SD_BOTH);
			return;
		}

		InterlockedIncrement(&_connectCounter);
		InterlockedIncrement(&_totalConnect);
	}

	void cHttp_Async::Recv_Complete(st_SESSION *pSession, DWORD trans)
	{
		//	Content-Length 길이까지 도착했을 시에만 OnRecv를 호출한다.
		//  데이터를 받은 후 HTTP 헤더에서 완료코드 얻기.
		//	받은 데이터에서 첫번째 0x20 코드를 찾아서 그 다음 0x20 까지가 완료 코드.
		//	HTTP / 1.1 200 OK

		//	데이터를 받은 후 HTTP 헤더에서 Content - Length: 얻기.
		//	받은 데이터에서 첫번째 Content_Length : 문자열을 찾아서, 그 다음 0x0d
		//	까지가 컨텐츠(BODY) 의 길이.
		//	Content - Length : 159
		//	위 숫자를 변환하여 BODY 의 길이로 확인

		//	헤더의 끝 찾기.
		//	\r\n\r\n 을 찾아서 그 아래 부분만을 BODY 로 얻어냄.

		int httpCode		= 0;
		int dataLength		= 0;
		int headerLength	= 0;
		
		// Body 얻기 
		char *pIdx = nullptr;
		char *pIdx2 = nullptr;
		char *pRecvData = pSession->responseMsg;

		pSession->recvBytes += trans;
		InterlockedIncrement(&_recvCounter);

		do
		{
			//	응답 코드
			pIdx = strchr(pRecvData, 0x20);
			if (pIdx == nullptr) return;
			pIdx += 1;
			pIdx2 = strchr(pIdx, 0x20);
			if (pIdx2 == nullptr) return;
			httpCode = atoi(pIdx);
			if (httpCode != 200) break;

			//	Content-Length
			pIdx = strstr(pIdx, "Content-Length:");
			if (pIdx == nullptr) break;
			pIdx += 15;
			pIdx2 = strchr(pIdx, 0x0d);
			if (pIdx2 == nullptr) break;
			dataLength = atoi(pIdx);

			//	헤더 끝
			pIdx = strstr(pIdx, "\r\n\r\n");
			if (pIdx == nullptr) break;
			pIdx += 4;

			headerLength = pIdx - pRecvData;

		} while (0);

		if ((httpCode == 200) && (headerLength + dataLength != pSession->recvBytes))
		{	
			if (!Recv_Post(pSession))
			{
				ReleaseSession(pSession);
			}

			return;
		}

		pIdx[pSession->recvBytes] = L'\0';
		OnRecv(pSession->requestID, pIdx, dataLength,  httpCode);
		ReleaseSession(pSession);
	}

	bool cHttp_Async::CreateSocket(st_SESSION *pSession)
	{
		pSession->addr = { 0, };
		pSession->addr.sin_port = htons(pSession->port);
		pSession->addr.sin_family = AF_INET;
		InetPton(AF_INET, pSession->hostname, &pSession->addr.sin_addr);

		//	Bind Socket 
		SOCKADDR_IN    localAddr;
		localAddr = { 0, };
		localAddr.sin_family = AF_INET;
		localAddr.sin_addr.S_un.S_addr = INADDR_ANY;

		//	Create ConnectEx, DisConnectEx Pointer
		GUID guidConnectEx = WSAID_CONNECTEX;
		GUID guidDisconnectEx = WSAID_DISCONNECTEX;
		DWORD dwBytes;

		pSession->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == pSession->sock) return false;

		if (bind(pSession->sock, (SOCKADDR*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) return false;

		int ret = WSAIoctl(pSession->sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx, sizeof(guidConnectEx),
						   &pSession->lpfnConnectEx, sizeof(LPFN_CONNECTEX), &dwBytes, NULL, NULL);

		/*ret = WSAIoctl(pSession->sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidDisconnectEx, sizeof(guidDisconnectEx),
					   &pSession->lpfnDisconnectEx, sizeof(LPFN_DISCONNECTEX), &dwBytes, NULL, NULL);*/

		if (!_bNagle)
		{
			int option = TRUE;
			setsockopt(pSession->sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&option, sizeof(option));
		}

		//	non block socket
		u_long mode = 1;
		ret = ioctlsocket(pSession->sock, FIONBIO, &mode);

		//	Associate client socket with the completion port
		HANDLE hPort = CreateIoCompletionPort((HANDLE)pSession->sock, _hWorkerThreadComport, (ULONG_PTR)pSession, 0);
		if (hPort != _hWorkerThreadComport) return false;

		return true;
	}

	int	cHttp_Async::RequestIDtoIndex(UINT64 requestID)
	{
		UINT64 mask;
		int idx;
		mask = 0xFFFF;
		idx = int(requestID & mask);

		return idx;
	}

	bool cHttp_Async::Alloc_Session(UINT64 &requestID)
	{
		int index;

		//if (_curSessionCount > _maxUser * 0.1)
		//{
		//	//	Time Wait 때문에 여분 소켓은 남겨둔다. 
		//	//	FIFO Queue
		//	return false;
		//}

		if (!_indexQueue.Dequeue(index))
		{
			return false;
		}

		//	하위 2바이트 = Index 
		//	상위 6바이트 = 고유 키
		requestID = InterlockedIncrement(&_curID);
		requestID = (requestID << 16) | index;

		_sessionArray[index].requestID = requestID;
		InterlockedIncrement(&_curSessionCount);
		return true;
	}

	void cHttp_Async::Free_Session(UINT64 requestID)
	{
		int index = RequestIDtoIndex(requestID);
		st_SESSION *pSession = &_sessionArray[index];
		shutdown(pSession->sock, SD_SEND);

		//if (InterlockedExchange8((char*)&pSession->bDisconnect, 1) == 1) return;

		
		//LPFN_DISCONNECTEX lpfnDisconnectEx = (LPFN_DISCONNECTEX)pSession->lpfnDisconnectEx;
		//if (lpfnDisconnectEx(pSession->sock, NULL, 0 , 0) == false)
		//{
		//	LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - Free_Session : DisconnectEx error %d", WSAGetLastError());
		//}

		//OnLeaveServer(pSession->requestID);
		//InterlockedDecrement(&_curSessionCount);

		//if (CreateSocket(pSession) == false)
		//{	
		//	LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - Free_Session : Create Socket Error");
		//	return;
		//}

		//InitializeSession(pSession);
		//_indexQueue.Enqueue(index);
	}

	void cHttp_Async::ReleaseSession(st_SESSION *pSession)
	{
		if (InterlockedExchange8((char*)&pSession->bDisconnect, 1) == 1) return;

		//LPFN_DISCONNECTEX lpfnDisconnectEx = (LPFN_DISCONNECTEX)pSession->lpfnDisconnectEx;
		//if (lpfnDisconnectEx(pSession->sock, NULL, TF_REUSE_SOCKET, 0) == false)
		//{
		//	LOG(L"Http", cSystemLog::LEVEL_ERROR, L"#cHttp_Async - ReleaseSession : error %d", WSAGetLastError());
		//}

		closesocket(pSession->sock);

		OnLeaveServer(pSession->requestID);
		int index = RequestIDtoIndex(pSession->requestID);
		InitializeSession(pSession);
		_indexQueue.Enqueue(index);
		InterlockedDecrement(&_curSessionCount);
	}

	void cHttp_Async::InitializeSession(st_SESSION* pSession)
	{
		pSession->requestID = 0;
		pSession->recvBytes = 0;
		pSession->msgLen = 0;

		memset(pSession->hostname, 0, sizeof(WCHAR) * 16);
		memset(pSession->path, 0, sizeof(WCHAR) * 128);

		pSession->requestMsg[0] = L'\0';
		pSession->requestMsg_utf8[0] = L'\0';
	}


	unsigned int cHttp_Async::MonitorThread(void *arg)
	{
		cHttp_Async *pThis = (cHttp_Async*)arg;
		return pThis->Update_MonitorThread();
	}

	bool cHttp_Async::Update_MonitorThread(void)
	{
		bool *bExitMonitor = &_bExitMonitor;

		long prevTime	= timeGetTime();
		long curTime	= 0;
		long timeInterval;

		timeBeginPeriod(1);
		while (*bExitMonitor == false)
		{
			curTime = timeGetTime();
			timeInterval = curTime - prevTime;

			if (timeInterval < 1000)
			{
				Sleep(1000);
				continue;
			}

			timeInterval /= 1000;

			_monitor_totalConnect = _totalConnect;
			_monitor_connectTPS = _connectCounter / timeInterval;
			_monitor_sendTPS = _sendCounter / timeInterval;
			_monitor_recvTPS = _recvCounter / timeInterval;

			InterlockedExchange(&_connectCounter, 0);
			InterlockedExchange(&_sendCounter, 0);
			InterlockedExchange(&_recvCounter, 0);
			prevTime = curTime;
		}

		timeEndPeriod(1);
		return true;
	}
};