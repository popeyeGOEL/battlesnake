#include <WS2tcpip.h>
#include <mstcpip.h>
#include <Mswsock.h>
#include <process.h>

#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"
#include "CommonProtocol_Old.h"

#include "CNetServer.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Mswsock.lib")

//	서버	시작을 위한 설정 내용입니다.TCP listen port 등을 지정합니다.
namespace TUNJI_LIBRARY
{
	CNetServer::CNetServer(BYTE packetCode, BYTE packetKey1, BYTE packetKey2): _packetCode(packetCode), _packetKey1(packetKey1), _packetKey2(packetKey2)
	{
		InitializeConditionVariable(&_cnIndexQueueIsNotEmpty);
		InitializeSRWLock(&_indexLock);
	}

	bool CNetServer::start(WCHAR *ip, short port, int threadCnt, bool bNagle, int maxUser, int acceptPool)
	{
		WSADATA wsa;
		GUID GuidAcceptEx = WSAID_ACCEPTEX;

		int iRet = 0;
		DWORD dwBytes;
		_lpfnAcceptEx = NULL;

		_curID = 0;
		_curAcceptID = 0;
		_threadCount = threadCnt;
		_bNagle = bNagle;
		_monitor_sendTPS = 0;
		_monitor_recvTPS = 0;
		_monitor_acceptTPS = 0;
		_monitor_totalAccept = 0;

		_monitor_sendCounter = 0;
		_monitor_recvCounter = 0;
		_monitor_acceptCounter = 0;

		_maxUser = maxUser;
		_curUser = 0;
		
		_maxAcceptPool = acceptPool;

		_bExitMonitor = false;

		memcpy(_serverIP, ip, sizeof(WCHAR) * 16);
		_serverPort = port;

		memset(&_serverAddr, 0, sizeof(_serverAddr));
		_serverAddr.sin_family = AF_INET;
		_serverAddr.sin_port = htons(port);
		InetPton(AF_INET, ip, &_serverAddr.sin_addr);


		do
		{
			if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) break;
			_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (INVALID_SOCKET == _listenSock) break;
			if (SOCKET_ERROR == bind(_listenSock, (SOCKADDR*)&_serverAddr, sizeof(_serverAddr))) break;
			if (SOCKET_ERROR == listen(_listenSock, SOMAXCONN)) break;
			if (!_bNagle)
			{
				int option = TRUE;
				setsockopt(_listenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&option, sizeof(option));
			}

			iRet = WSAIoctl(_listenSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&GuidAcceptEx, sizeof(GuidAcceptEx),
				(LPFN_ACCEPTEX)&_lpfnAcceptEx, sizeof(LPFN_ACCEPTEX),
				&dwBytes, NULL, NULL);

			if (iRet == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) break;

			//	IOCP handle 생성
			_hWorkerThreadComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

			//	Associate the listening socket with the completion port
			CreateIoCompletionPort((HANDLE)_listenSock, _hWorkerThreadComport, (u_long)0, 0);

			//	Client Array 동적 할당 
			_clientArray = new st_CLIENT_INFO[_maxUser];


			//CreateThreadProfileID();
			DWORD ID = GetCurrentThreadId();
			g_profiler.CreateThreadID(ID);

			////	accept socket pool 생성
			//int count = 0;
			//for (int i = 0; i < _maxUser; ++i)
			//{
			//	while (_clientArray[i].sock == INVALID_SOCKET)
			//	{
			//		if (++count == 10) break;
			//		_clientArray[i].sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
			//	}

			//	count = 0;
			//	//accp_ol 설정
			//	_clientArray[i].accp_ol.arrayIndex = i;
			//	accept_post(&_clientArray[i]);
			//}

			DWORD dwThreadID; 

			_bServerOn = true;

			//	Worker Thread 생성 
			for (int i = 0; i < threadCnt; ++i)
			{
				_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, workerThread, (PVOID)this, 0, (unsigned int *)&dwThreadID);
			}

			//	Monitor Thread  생성 
			_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&dwThreadID);

			//	Accept Thread 생성
			_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);

			for (int i = 0; i < _maxUser; ++i)
			{
				_indexQueue.Enqueue(i);
				_clientArray[i].accp_ol.arrayIndex = i;
				WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
			}

			wprintf(L"Initialized Network Server\n");
			return true;
		} while (1);

		wprintf(L"#ERROR: Fail to init server %d \n", WSAGetLastError());
		return false;
	}


	//	서버를 종료한다
	void CNetServer::stop(void)
	{
		_bServerOn = false;

		//	1. 리슨 소켓 종료 
		closesocket(_listenSock);

		//	2. 전체 세션 정리
		for (int i = 0; i < _maxUser; ++i)
		{
			if ((_clientArray[i].clientID >> 8) > 0)
			{
				shutdown(_clientArray[i].sock, SD_BOTH);
			}
		}

		//	3. 워커 스레드 종료
		PostQueuedCompletionStatus(_hWorkerThreadComport, 0, NULL, NULL);

		DWORD status = WaitForMultipleObjects(_threadCount, _hWorkerThread, TRUE, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"NetServer thread terminated! \n");
		}

		for (int i = 0; i < _threadCount; ++i)
		{
			CloseHandle(_hWorkerThread[i]);
		}

		CloseHandle(_hWorkerThreadComport);

		// 4. Accept Thread 종료 
		_indexQueue.ClearBuffer();
		_curPostedAcceptEx = 0;
		WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
		status = WaitForSingleObject(_hAcceptThread, INFINITE);
		CloseHandle(_hAcceptThread);

		// 5. Monitor 스레드 종료
		_bExitMonitor = true;
		status = WaitForSingleObject(_hMonitorThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"NetServer Monitor thread terminated! \n");
		}

		CloseHandle(_hMonitorThread);
		WSACleanup();

		delete[] _clientArray;
	}

	//	클라이언트 접속 해제 
	bool CNetServer::disconnect(UINT64 clientID)
	{
		st_CLIENT_INFO* pClient = AcquireSessionLock(clientID);
		if (pClient == nullptr)
		{
			return false;
		}

		UINT64 index = InterlockedIncrement(&pClient->countIndex);
		pClient->countTracker[index % 500] = DISCONNECT;

		shutdown(pClient->sock, SD_BOTH);

		ReleaseSessionLock(pClient);
		return true;
	}

	int CNetServer::ClientIDtoIndex(UINT64 clientID)
	{
		UINT64 mask;
		int idx;
		mask = 0xFFFF;
		idx = int(clientID & mask);

		return idx;
	}

	UINT64 CNetServer::createClientID(int index)
	{
		//	Index와 curID를 조합해서 clientID를 뱉어낸다. 

		UINT64 id = InterlockedIncrement(&_curID);
		return (id << 16) | index;
	}

	void CNetServer::FreeSentPacket(st_CLIENT_INFO* pClient, UINT64 iSize)
	{
		if (InterlockedExchange(&pClient->sendPacketSize, 0) == 0) return;

		for (int i = 0; i < iSize; ++i)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cNetPacketPool::Free(pClient->packetBuffer[i]);
			LeaveProfile(L"DEALLOC_PACKET");
		}
	}

	void CNetServer::FreeSendQueuePacket(st_CLIENT_INFO* pClient)
	{
		cNetPacketPool *pPool = nullptr;
		while (!pClient->sendq.IsEmpty())
		{
			pClient->sendq.Dequeue(pPool);
			if (pPool == nullptr) continue;
			EnterProfile(L"DEALLOC_PACKET");
			cNetPacketPool::Free(pPool);
			LeaveProfile(L"DEALLOC_PACKET");
		};
	}

	void CNetServer::InitializeSession(st_CLIENT_INFO* pClient)
	{
		pClient->clientID = 0;
		//pClient->recvq.ClearBuffer();
		//InterlockedExchange8((char*)&pClient->bSend, 0);
		/*FreeSendQueuePacket(pClient);*/
		//FreeSentPacket(pClient, pClient->sendPacketSize);
		InterlockedDecrement(&_curUser);
	}

	CNetServer::st_CLIENT_INFO* CNetServer::findClientOverlap(OVERLAPPED* ol)
	{
		st_ACCEPT_OVERLAP *lapped_ol = (st_ACCEPT_OVERLAP*)ol;

		int index = lapped_ol->arrayIndex;
		return &_clientArray[index];
	}

	void CNetServer::ReleaseSession(st_CLIENT_INFO* pClient)
	{	
		InterlockedExchange8((char*)&pClient->bReadyToDisconnect, 1);
		if (pClient->ioCount != 0) return;

		FreeSentPacket(pClient, pClient->sendPacketSize);
		FreeSendQueuePacket(pClient);

		if (InterlockedExchange8((char*)&pClient->bDisconnectNow, 1) == 0)
		{
			UINT64 index = InterlockedIncrement(&pClient->countIndex);
			pClient->countTracker[index % 500] = RELEASE;

			closesocket(pClient->sock);
			if (pClient->clientID >> 16 != 0)
			{
				OnClientLeave(pClient->clientID);
			}
			InitializeSession(pClient);
			_indexQueue.Enqueue(pClient->accp_ol.arrayIndex);
			WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
		}
	}

	CNetServer::st_CLIENT_INFO* CNetServer::AcquireSessionLock(UINT64 clientID)
	{
		int idx = ClientIDtoIndex(clientID);
		st_CLIENT_INFO* pClient = &_clientArray[idx];

		do {
			//UINT64 index = InterlockedIncrement(&pClient->countIndex);
			//pClient->countTracker[index % 500] = INC_SESSION_LOCK;

			if (InterlockedIncrement(&pClient->ioCount) == 1) break;
			if (pClient->bReadyToDisconnect) break;
			if (pClient->clientID != clientID) break;

			return pClient;

		} while (1);

		__int64 ret = InterlockedDecrement(&pClient->ioCount);

		if (ret <= 0)
		{
			if (ret < 0)
			{
				cCrashDump::Crash();
			}
			ReleaseSession(pClient);
		}

		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = DEC_SESSION_LOCK;
		return nullptr;
	}

	void CNetServer::ReleaseSessionLock(st_CLIENT_INFO* pClient)
	{
		__int64 ret = InterlockedDecrement(&pClient->ioCount);

		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = DEC_SESSION_UNLOCK;

		if (ret <= 0)
		{
			if (ret < 0)
			{
				cCrashDump::Crash();
			}

			ReleaseSession(pClient);
		}
	}

	void CNetServer::SetIoCompletionPort(st_CLIENT_INFO* pClient)
	{		
		// IOCP Setting하기 
		if (CreateIoCompletionPort((HANDLE)pClient->sock, _hWorkerThreadComport, (ULONG_PTR)pClient, 0) == nullptr)
		{
			LOG(L"GQCS_Error", cSystemLog::LEVEL_DEBUG, L"#CNetServer - SetIoCompletionPort :  Error %d", WSAGetLastError());
			return;
		}

		EnterProfile(L"RECV_POST");
		recv_post(pClient);
		LeaveProfile(L"RECV_POST");
	}

	//	사용자 정의 메시지를 전송합니다. 
	void CNetServer::sendPacket(UINT64 clientID, cNetPacketPool* sPacket)
	{
		st_CLIENT_INFO *pClient = AcquireSessionLock(clientID);
		if (pClient == nullptr)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cNetPacketPool::Free(sPacket);
			LeaveProfile(L"DEALLOC_PACKET");
			return;
		}

		sPacket->Encode(_packetCode, _packetKey1, _packetKey2);
	
		pClient->sendq.Enqueue(sPacket);

		send_post(pClient);
		ReleaseSessionLock(pClient);
	}

	void CNetServer::sendLastPacket(UINT64 clientID, cNetPacketPool *sPacket)
	{
		st_CLIENT_INFO *pClient = AcquireSessionLock(clientID);
		if (pClient == nullptr)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cNetPacketPool::Free(sPacket);
			LeaveProfile(L"DEALLOC_PACKET");
			return;
		}

		sPacket->Encode(_packetCode, _packetKey1, _packetKey2);

		pClient->sendq.Enqueue(sPacket);
		InterlockedExchange8((char*)&pClient->bLastPacket, 1);
		send_post(pClient);

		ReleaseSessionLock(pClient);
	}

	unsigned int CNetServer::workerThread(void *arg)
	{
		CNetServer *pThis = (CNetServer*)arg;
		return pThis->update_workerThread();
	}

	unsigned int CNetServer::update_workerThread(void)
	{
		__int64 ret;
		bool	bRet;
		DWORD trans;
		st_CLIENT_INFO *pClient;
		OVERLAPPED *olOverlap;

		//profiler set 
		//CreateThreadProfileID();
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		//Allocate packet chunk to each thread
		cNetPacketPool::AllocPacketChunk();
		cLanPacketPool::AllocPacketChunk();

		while (1)
		{
			pClient = nullptr;
			trans = 0;

			bRet = GetQueuedCompletionStatus(_hWorkerThreadComport, &trans, (PULONG_PTR)&pClient, (LPOVERLAPPED *)&olOverlap, INFINITE);
			if (bRet == FALSE)
			{
				int error = WSAGetLastError();
				if (error == 121)
				{
					LOG(L"GQCS_Error", cSystemLog::LEVEL_ERROR, L"#CNetServer - update_workerThread : error code [%d] [%d]", error, pClient->accp_ol.arrayIndex);
				}
			}

			if (trans == 0 && pClient == nullptr && olOverlap == nullptr)
			{
				cNetPacketPool::FreePacketChunk();
				cLanPacketPool::FreePacketChunk();
				PostQueuedCompletionStatus(_hWorkerThreadComport, 0, (ULONG)0, nullptr);
				break;
			}

			if (olOverlap != nullptr && pClient == nullptr)
			{
				pClient = findClientOverlap(olOverlap);
				if (pClient == nullptr)
				{
					OnError(Error_Type::enError_FindClientFailure, L"enError_FindClientFailure\n");
					continue;
				}

				if (bRet == true)
				{
					EnterProfile(L"ACCEPT_COMPLETE");
					accp_complete(pClient);
					LeaveProfile(L"ACCEPT_COMPLETE");
				}
			}
			else
			{
				if (trans == 0 && (&pClient->recv_ol == olOverlap))
				{
					UINT64 index = InterlockedIncrement(&pClient->countIndex);
					pClient->countTracker[index % 500] = RECV_ZERO;

					shutdown(pClient->sock, SD_SEND);
				}
				else if (&pClient->send_ol == olOverlap)
				{
					EnterProfile(L"SEND_COMPLETE");
					send_complete(pClient, trans);
					LeaveProfile(L"SEND_COMPLETE");
				}
				else if (&pClient->recv_ol == olOverlap)
				{
					EnterProfile(L"RECV_COMPLETE");
					recv_complete(pClient, trans);
					LeaveProfile(L"RECV_COMPLETE");
				}
			}
			ret = InterlockedDecrement(&pClient->ioCount);
			//UINT64 index = InterlockedIncrement(&pClient->countIndex);
			//pClient->countTracker[index % 500] = DEC_WORKER;

			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}
				
				EnterProfile(L"RELEASE_SESSION");
				ReleaseSession(pClient);
				LeaveProfile(L"RELEASE_SESSION");
			}
		}

		return 0;
	}

	void CNetServer::recv_complete(st_CLIENT_INFO* pClient, DWORD trans)
	{
		cNetPacketPool::st_NET_HEADER header;
		cNetPacketPool *dsPacket;
		int ret;

		pClient->recvq.MoveRearforSave(trans);
		InterlockedIncrement(&_monitor_recvCounter);

		int Count = 0;

		while (1)
		{
			++Count;

			//	네트워크에서는 헤더만 분리
			ret = 0;
			header = { 0, };
			dsPacket = nullptr;

			ret = pClient->recvq.Peek((char*)&header, sizeof(cNetPacketPool::st_NET_HEADER));
			if (ret < sizeof(cNetPacketPool::st_NET_HEADER)) break;

			if (header.len > cNetPacketPool::en_BUFSIZE_DEFALUT - sizeof(cNetPacketPool::st_NET_HEADER))
			{
				shutdown(pClient->sock, SD_BOTH);
				return;
			}

			if (pClient->recvq.GetUsedSize() < header.len + sizeof(cNetPacketPool::st_NET_HEADER)) break;
			pClient->recvq.MoveFrontforDelete(sizeof(cNetPacketPool::st_NET_HEADER));
			EnterProfile(L"ALLOC_PACKET");
			dsPacket = cNetPacketPool::Alloc();
			LeaveProfile(L"ALLOC_PACKET");
			dsPacket->Custom_SetNetHeader(header);
			ret = pClient->recvq.Dequeue(dsPacket->GetWriteptr(), header.len);
			dsPacket->MoveRearforSave(ret);

			try
			{
				dsPacket->Decode(_packetCode, _packetKey1, _packetKey2);
				OnRecv(pClient->clientID, dsPacket);
			}
			catch (cNetPacketPool::packetException &e)
			{
				//ReleaseSession(pClient);
				shutdown(pClient->sock, SD_BOTH);
				cCrashDump::Crash();
				//_LOG(dfLOG_LEVEL_ERROR, L"#PACKET_ERROR : %s [%d]\n", e.errLog, pClient->clientID);
			}

			EnterProfile(L"DEALLOC_PACKET");
			cNetPacketPool::Free(dsPacket);
			LeaveProfile(L"DEALLOC_PACKET");

		}

		EnterProfile(L"RECV_POST");
		recv_post(pClient);
		LeaveProfile(L"RECV_POST");
	}

	void CNetServer::send_complete(st_CLIENT_INFO* pClient, DWORD trans)
	{
		FreeSentPacket(pClient, pClient->sendPacketSize);
		InterlockedExchange8((char*)&pClient->bSend, 0);
		InterlockedIncrement(&_monitor_sendCounter);
		OnSend(pClient->clientID, trans);

		//	이제 끊을 세션인지 아닌지 확인하기.		
		if (pClient->bLastPacket)
		{
			InterlockedExchange8((char*)&pClient->bLastPacket, 0);
			shutdown(pClient->sock, SD_BOTH);
			return;
		}

		EnterProfile(L"SEND_POST");
		send_post(pClient);
		LeaveProfile(L"SEND_POST");
	}

	void CNetServer::accp_complete(st_CLIENT_INFO* pClient)
	{
		WCHAR IP[16];
		USHORT PORT;
		int len;
		int mode;
	
		// Setting Socket Option
		setsockopt(pClient->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&_listenSock, sizeof(SOCKET));
 		//mode = 0;
		//setsockopt(pClient->sock, SOL_SOCKET, SO_SNDBUF, (char*)&mode, sizeof(mode));

		DWORD dwError = 0L;
		tcp_keepalive sKA_Settings = { 0 }, sReturned = { 0 };
		sKA_Settings.onoff = 1;
		sKA_Settings.keepalivetime = 60000;			  // Keep Alive in 60 sec.
		sKA_Settings.keepaliveinterval = 3000;        // Resend if No-Reply

		DWORD dwBytes;
		WSAIoctl(pClient->sock, SIO_KEEPALIVE_VALS, &sKA_Settings, sizeof(sKA_Settings), &sReturned, sizeof(sReturned), &dwBytes, NULL, NULL);

		len = sizeof(pClient->addr);
		getpeername(pClient->sock, (SOCKADDR*)&pClient->addr, &len);
		PORT = ntohs(pClient->addr.sin_port);
		InetNtop(AF_INET, &pClient->addr.sin_addr, IP, wcslen(IP));

		if (OnConnectionRequest(IP, PORT) == false)
		{
			OnError(enError_ServerDeniedConnection, L"enError_ServerDeniedConnection\n");
			//shutdown(pClient->sock, SD_BOTH);
			return;
		}

		if (_curUser == _maxUser)
		{
			OnError(enError_OverMaxUser, L"enError_OverMaxUser\n");
			//shutdown(pClient->sock, SD_BOTH);
			return;
		}

		pClient->clientID = createClientID(pClient->accp_ol.arrayIndex);

		pClient->bReadyToDisconnect = false;
		pClient->bDisconnectNow = false;
		FreeSendQueuePacket(pClient);
		FreeSentPacket(pClient, pClient->sendPacketSize);
		pClient->recvq.ClearBuffer();
		pClient->bSend = false;
		pClient->bLastPacket = false;

		UINT64 index = InterlockedIncrement(&pClient->countIndex);
		pClient->countTracker[index % 500] = ACCEPT;

		OnClientJoin(pClient->clientID);
		SetIoCompletionPort(pClient);


		InterlockedIncrement(&_monitor_totalAccept);
		InterlockedIncrement(&_monitor_acceptCounter);
		InterlockedIncrement(&_curUser);
		InterlockedDecrement(&_curPostedAcceptEx);

		WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
	}

	bool CNetServer::accept_post(st_CLIENT_INFO *pClient)
	{
		BOOL bRet = FALSE;

		memset(&pClient->accp_ol, 0, sizeof(OVERLAPPED));
		
		LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)_lpfnAcceptEx;
		InterlockedIncrement(&pClient->ioCount);
		InterlockedIncrement(&_curPostedAcceptEx);

		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = INC_ACCEPT_POST;
		EnterProfile(L"ACCEPTEX");
		bRet = lpfnAcceptEx(_listenSock, pClient->sock, pClient->recvq.GetWriteptr(), 0
			, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, 0, &pClient->accp_ol);
		LeaveProfile(L"ACCEPTEX");

		if (bRet == FALSE && WSAGetLastError() != ERROR_IO_PENDING)
		{
			__int64 ret = InterlockedDecrement(&pClient->ioCount);
			//index = InterlockedIncrement(&pClient->countIndex);
			//pClient->countTracker[index % 500] = DEC_ACCEPT_POST;
			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}
			}

			closesocket(pClient->sock);
			return false;
		}
		return true;
	}


	void CNetServer::send_post(st_CLIENT_INFO* pClient)
	{
		__int64 ret;
		DWORD trans;
		WSABUF wsabuf[100];

		if (InterlockedExchange8((char*)&pClient->bSend, 1) == 1) return;
		if (pClient->sendq.IsEmpty())
		{
			InterlockedExchange8((char*)&pClient->bSend, 0);
			return;
		}
		if (pClient->bReadyToDisconnect)
		{
			InterlockedExchange8((char*)&pClient->bSend, 0);
			return;
		}

		trans = 0;

		DWORD bufCount = 0;

		while (bufCount < 100)
		{
			if (pClient->sendq.Dequeue(pClient->packetBuffer[bufCount]) == false) break;
			wsabuf[bufCount].buf = pClient->packetBuffer[bufCount]->GetBufptr();
			wsabuf[bufCount].len = pClient->packetBuffer[bufCount]->GetDataSizewithHeader();

			++bufCount;
		}

		InterlockedIncrement(&pClient->ioCount);
		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = INC_SEND_POST;
		pClient->send_ol = { 0, };
		
		InterlockedExchange(&pClient->sendPacketSize, bufCount);

		EnterProfile(L"WSASend");
		ret = WSASend(pClient->sock, wsabuf, bufCount, &trans, 0, &pClient->send_ol, NULL);
		LeaveProfile(L"WSASend");

		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			LOG(L"WSA", cSystemLog::LEVEL_DEBUG, L"Error on WSASend : ClientID [%d] WSAGetLastError [%d] Send Packet Count [%d]", pClient->clientID, WSAGetLastError(), pClient->sendPacketSize);

			ret = InterlockedDecrement(&pClient->ioCount);
			//index = InterlockedIncrement(&pClient->countIndex);
			//pClient->countTracker[index % 500] = DEC_SEND_POST;
			FreeSentPacket(pClient, pClient->sendPacketSize);
			InterlockedExchange8((char*)&pClient->bSend, 0);

			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}
	
				ReleaseSession(pClient);
			}
		}
	}

	void CNetServer::recv_post(st_CLIENT_INFO* pClient)
	{
		__int64 ret;
		char *pWrite = pClient->recvq.GetWriteptr();
		int len = pClient->recvq.GetConstantWriteSize();
		int empty = pClient->recvq.GetEmptySize();
		char *pStart = pClient->recvq.GetStartptr();

		WSABUF wsabuf[2];
		DWORD flag = 0;
		DWORD trans = 0;
		int count = 1;

		wsabuf[0].buf = pWrite;
		wsabuf[0].len = len;

		if (len < empty)
		{
			wsabuf[1].buf = pStart;
			wsabuf[1].len = empty - len;
			count = 2;
		}

		InterlockedIncrement(&pClient->ioCount);
		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = INC_RECV_POST;

		pClient->recv_ol = { 0, };
		EnterProfile(L"WSARecv");
		ret = WSARecv(pClient->sock, wsabuf, count, &trans, &flag, &pClient->recv_ol, NULL);
		LeaveProfile(L"WSARecv");
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			LOG(L"WSA", cSystemLog::LEVEL_DEBUG, L"Error on WSARecv : ClientID [%d] WSAGetLastError [%d]", pClient->clientID, WSAGetLastError());
			ret = InterlockedDecrement(&pClient->ioCount);
			//index = InterlockedIncrement(&pClient->countIndex);
			//pClient->countTracker[index % 500] = DEC_RECV_POST;

			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}
				ReleaseSession(pClient);
			}
		}
	}
	unsigned __stdcall	CNetServer::AcceptThread(void *arg)
	{
		CNetServer *pThis = (CNetServer*)arg;
		return pThis->Update_AcceptThread();
	}

	bool CNetServer::Update_AcceptThread(void)
	{
		//	일어났을때 서버가 꺼져있으면 종료

		int index;
		st_CLIENT_INFO *pSession;
		_curPostedAcceptEx = 0;

		while (1)
		{
			AcquireSRWLockExclusive(&_indexLock);
			while (_indexQueue.IsEmpty() || _curPostedAcceptEx >= _maxAcceptPool)
			{
				SleepConditionVariableSRW(&_cnIndexQueueIsNotEmpty, &_indexLock, INFINITE, 0);
			}
			ReleaseSRWLockExclusive(&_indexLock);

			if (_bServerOn == false) break;
			if (_indexQueue.Dequeue(index) == false) continue;
			pSession = &_clientArray[index];

			pSession->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (accept_post(pSession) == false)
			{
				_indexQueue.Enqueue(index);
			}
		};

		return true;
	}

	unsigned __stdcall CNetServer::MonitorThread(void *arg)
	{
		CNetServer *pThis = (CNetServer*)arg;
		return pThis->Update_MonitorThread();
	}

	bool CNetServer::Update_MonitorThread(void)
	{

		bool *bExitMonitor = &_bExitMonitor;

		long prevTime = timeGetTime();
		long curTime = 0;
		long timeInterval;

		timeBeginPeriod(1);
		while (*bExitMonitor == false)
		{
			curTime = timeGetTime();
			timeInterval = curTime - prevTime;
			
			if (timeInterval < 1000)
			{
				Sleep(100);
				continue;
			}

			timeInterval /= 1000;

			_monitor_acceptTPS = _monitor_acceptCounter / timeInterval;
			_monitor_sendTPS = _monitor_sendCounter / timeInterval;
			_monitor_recvTPS = _monitor_recvCounter / timeInterval;
			_monitor_AcceptSocketPool = _curPostedAcceptEx;
			_monitor_AcceptSocketQueue = _indexQueue.GetTotalBlockCount();
			InterlockedExchange(&_monitor_acceptCounter, 0);
			InterlockedExchange(&_monitor_recvCounter, 0);
			InterlockedExchange(&_monitor_sendCounter, 0);

			prevTime = curTime;
		}
		timeEndPeriod(1);
		return true;
	}
};