#include <WS2tcpip.h>
#include <mstcpip.h>
#include <Mswsock.h>
#include <process.h>

#include "CLanServer.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Mswsock.lib")

//	서버	시작을 위한 설정 내용입니다.TCP listen port 등을 지정합니다.
namespace TUNJI_LIBRARY
{
	bool CLanServer::start(WCHAR *ip, short port, int threadCnt, bool bNagle, int maxUser)
	{
		WSADATA wsa;
		u_long mode = 1;
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

		_bExitMonitor = false;


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

			DWORD dwThreadID;

			//	Worker Thread 생성 
			for (int i = 0; i < threadCnt; ++i)
			{
				_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, workerThread, (PVOID)this, 0, (unsigned int *)&dwThreadID);
			}

			//	Monitor Thread  생성 
			_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (PVOID)this, 0, (unsigned int*)&dwThreadID);

			//	accept socket pool 생성
			int count = 0;
			for (int i = 0; i < _maxUser; ++i)
			{
				while (_clientArray[i].sock == INVALID_SOCKET)
				{
					if (++count == 10) break;
					_clientArray[i].sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
				}

				count = 0;
				//accp_ol 설정
				_clientArray[i].accp_ol.arrayIndex = i;
				accept_post(&_clientArray[i]);
			}

			_bServerOn = true;
			//wprintf(L"Initialized Lan Server\n");
			return true;
		} while (1);

		//wprintf(L"#ERROR: Fail to init server %d \n", WSAGetLastError());
		return false;
	}

	//	서버를 종료한다
	void CLanServer::stop(void)
	{
		_bServerOn = false;
		//	1. 리슨 소켓 종료 
		closesocket(_listenSock);

		//	2. 전체 세션 정리
		for (int i = 0; i < _maxUser; ++i)
		{
			if ((_clientArray[i].clientID != 0))
			{
				//ReleaseSession(&_clientArray[i]);
				//shutdown(_clientArray[i].sock, SD_BOTH);
				closesocket(_clientArray[i].sock);
			}
		}

		//	3. 워커 스레드 종료
		PostQueuedCompletionStatus(_hWorkerThreadComport, 0, NULL, NULL);

		DWORD status = WaitForMultipleObjects(_threadCount, _hWorkerThread, TRUE, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"LanServer thread terminated! \n");
		}

		for (int i = 0; i < _threadCount; ++i)
		{
			CloseHandle(_hWorkerThread[i]);
		}

		CloseHandle(_hWorkerThreadComport);

		// 4. Monitor 스레드 종료
		_bExitMonitor = true;
		status = WaitForSingleObject(_hMonitorThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"LanServer Monitor thread terminated! \n");
		}

		CloseHandle(_hMonitorThread);

		WSACleanup();

		delete[] _clientArray;
	}

	//	클라이언트 접속 해제 
	bool CLanServer::disconnect(UINT64 clientID)
	{
		st_CLIENT_INFO* pClient = AcquireSessionLock(clientID);
		if (pClient == nullptr)
		{
			return false;
		}

		shutdown(pClient->sock, SD_BOTH);

		ReleaseSessionLock(pClient);
		return true;
	}

	int CLanServer::ClientIDtoIndex(UINT64 clientID)
	{
		UINT64 mask;
		int idx;
		mask = 0xFFFF;
		idx = int(clientID & mask);

		return idx;
	}

	UINT64 CLanServer::createClientID(int index)
	{
		//	Index와 curID를 조합해서 clientID를 뱉어낸다. 

		UINT64 id = InterlockedIncrement(&_curID);
		return (id << 16) | index;
	}

	void CLanServer::FreeSentPacket(st_CLIENT_INFO* pClient, UINT64 iSize)
	{
		if (pClient->bSend == false) return;
		if (InterlockedExchange(&pClient->sendPacketSize, 0) == 0) return;

		for (int i = 0; i < iSize; ++i)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cLanPacketPool::Free(pClient->packetBuffer[i]);
			LeaveProfile(L"DEALLOC_PACKET");
		}
	}

	void CLanServer::FreeSendQueuePacket(st_CLIENT_INFO* pClient)
	{
		cLanPacketPool *pPool = nullptr;
		while (!pClient->sendq.IsEmpty())
		{
			pClient->sendq.Dequeue(pPool);
			if (pPool == nullptr) continue;
			EnterProfile(L"DEALLOC_PACKET");
			cLanPacketPool::Free(pPool);
			LeaveProfile(L"DEALLOC_PACKET");
		};
	}


	void CLanServer::InitializeSession(st_CLIENT_INFO* pClient)
	{
		pClient->clientID = 0;
		//pClient->recvq.ClearBuffer();
		//InterlockedExchange8((char*)&pClient->bSend, 0);
		//FreeSendQueuePacket(pClient);
		//FreeSentPacket(pClient, pClient->sendPacketSize);
		InterlockedDecrement(&_curUser);
	}

	CLanServer::st_CLIENT_INFO* CLanServer::findClientOverlap(OVERLAPPED* ol)
	{
		st_ACCEPT_OVERLAP *lapped_ol = (st_ACCEPT_OVERLAP*)ol;

		int index = lapped_ol->arrayIndex;
		return &_clientArray[index];
	}

	void CLanServer::ReleaseSession(st_CLIENT_INFO* pClient)
	{
		InterlockedExchange8((char*)&pClient->bReadytoDisconnect, 0);
		if (pClient->ioCount != 0)return;

		if (InterlockedExchange8((char*)&pClient->bDisconnectNow, 1) == 0)
		{
			closesocket(pClient->sock);
			OnClientLeave(pClient->clientID);
			InitializeSession(pClient);

			if (_bServerOn)
			{
				pClient->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
				accept_post(pClient);
			}
		}
	}


	CLanServer::st_CLIENT_INFO* CLanServer::AcquireSessionLock(UINT64 clientID)
	{
		int idx = ClientIDtoIndex(clientID);
		st_CLIENT_INFO* pClient = &_clientArray[idx];

		do {
			//UINT64 index = InterlockedIncrement(&pClient->countIndex);
			//pClient->countTracker[index % 500] = INC_SESSION_LOCK;

			if (InterlockedIncrement(&pClient->ioCount) == 1) break;
			if (pClient->bReadytoDisconnect) break;
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

		//InterlockedDecrement(&pClient->ioCount);

		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = DEC_SESSION_LOCK;
		return nullptr;
	}

	void CLanServer::ReleaseSessionLock(st_CLIENT_INFO* pClient)
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

	void CLanServer::SetIoCompletionPort(st_CLIENT_INFO* pClient)
	{
		// IOCP Setting하기 
		CreateIoCompletionPort((HANDLE)pClient->sock, _hWorkerThreadComport, (ULONG_PTR)pClient, 0);

		EnterProfile(L"RECV_POST");
		recv_post(pClient);
		LeaveProfile(L"RECV_POST");
	}

	//	사용자 정의 메시지를 전송합니다. 
	void CLanServer::sendPacket(UINT64 clientID, cLanPacketPool* sPacket)
	{
		st_CLIENT_INFO *pClient = AcquireSessionLock(clientID);
		if (pClient == nullptr)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cLanPacketPool::Free(sPacket);
			LeaveProfile(L"DEALLOC_PACKET");
			return;
		}

		sPacket->Custom_SetLanHeader(sPacket->GetDataSize());

		pClient->sendq.Enqueue(sPacket);

		send_post(pClient);
		ReleaseSessionLock(pClient);
	}

	void CLanServer::sendLastPacket(UINT64 clientID, cLanPacketPool *sPacket)
	{
		st_CLIENT_INFO *pClient = AcquireSessionLock(clientID);
		if (pClient == nullptr)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cLanPacketPool::Free(sPacket);
			LeaveProfile(L"DEALLOC_PACKET");
			return;
		}

		sPacket->Custom_SetLanHeader(sPacket->GetDataSize());

		pClient->sendq.Enqueue(sPacket);
		InterlockedExchange8((char*)&pClient->bLastPacket, 1);
		send_post(pClient);

		ReleaseSessionLock(pClient);
	}
	unsigned int CLanServer::workerThread(void *arg)
	{
		CLanServer *pThis = (CLanServer*)arg;
		return pThis->update_workerThread();
	}

	unsigned int CLanServer::update_workerThread(void)
	{
		__int64 ret;
		DWORD trans;
		st_CLIENT_INFO *pClient;
		OVERLAPPED *olOverlap;

		//profiler set 
		//CreateThreadProfileID();
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		//Allocate packet chunk to each thread
		cLanPacketPool::AllocPacketChunk();
		cNetPacketPool::AllocPacketChunk();

		while (1)
		{
			pClient = nullptr;
			olOverlap = nullptr;
			trans = 0;

			ret = GetQueuedCompletionStatus(_hWorkerThreadComport, &trans, (PULONG_PTR)&pClient, (LPOVERLAPPED *)&olOverlap, INFINITE);
			if (ret == FALSE)
			{
				if (pClient == nullptr) continue;
				LOG(L"GQCS_Error", cSystemLog::LEVEL_DEBUG, L"#CLanServer - update_workerThread : GetqueuedCompletionStatus Error %d", WSAGetLastError());
			}

			if (trans == 0 && pClient == nullptr && olOverlap == nullptr)
			{
				cLanPacketPool::FreePacketChunk();
				cNetPacketPool::FreePacketChunk();
				PostQueuedCompletionStatus(_hWorkerThreadComport, 0, (ULONG)0, nullptr);
				break;
			}

			if (olOverlap != nullptr && pClient == nullptr)
			{
				pClient = findClientOverlap(olOverlap);
				if (pClient == nullptr)
				{
					//wprintf(L"can't find accept client!\n");
					continue;
				}
				else
				{
					//	overlap이 널이 아니며, GQCS가 0이라면, AcceptEx 걸어놓은 소켓이 중단된것이므로 
					//	ReleaseSession으로 들어가면 된다. 
					if (ret == true)
					{
						EnterProfile(L"ACCEPT_COMPLETE");
						accp_complete(pClient);
						LeaveProfile(L"ACCEPT_COMPLETE");
					}
				}
			}
			else
			{
				if (trans == 0)
				{
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

	void CLanServer::recv_complete(st_CLIENT_INFO* pClient, DWORD trans)
	{
		cLanPacketPool::st_LAN_HEADER header;
		cLanPacketPool *dsPacket;
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

			ret = pClient->recvq.Peek((char*)&header, cLanPacketPool::en_HEADERSIZE_DEFAULT);
			if (ret < cLanPacketPool::en_HEADERSIZE_DEFAULT) break;

			if (header.payload > cLanPacketPool::en_BUFSIZE_DEFALUT - cLanPacketPool::en_HEADERSIZE_DEFAULT) return;

			if (pClient->recvq.GetUsedSize() < header.payload + cLanPacketPool::en_HEADERSIZE_DEFAULT) break;
			pClient->recvq.MoveFrontforDelete(cLanPacketPool::en_HEADERSIZE_DEFAULT);
			EnterProfile(L"ALLOC_PACKET");
			dsPacket = cLanPacketPool::Alloc();
			LeaveProfile(L"ALLOC_PACKET");
			dsPacket->Custom_SetLanHeader(header.payload);
			ret = pClient->recvq.Dequeue(dsPacket->GetWriteptr(), header.payload);
			dsPacket->MoveRearforSave(ret);

			OnRecv(pClient->clientID, dsPacket);
			cLanPacketPool::Free(dsPacket);
		}

		//if (pClient->bDisconnectNow) return;

		EnterProfile(L"RECV_POST");
		recv_post(pClient);
		LeaveProfile(L"RECV_POST");
	}

	void CLanServer::send_complete(st_CLIENT_INFO* pClient, DWORD trans)
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

	void CLanServer::accp_complete(st_CLIENT_INFO* pClient)
	{
		WCHAR IP[16];
		USHORT PORT;
		int len;
		//int mode;

		setsockopt(pClient->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&_listenSock, sizeof(SOCKET));
		//mode = 0;
		//setsockopt(pClient->sock, SOL_SOCKET, SO_SNDBUF, (char*)&mode, sizeof(mode));

		//DWORD dwError = 0L;
		//tcp_keepalive sKA_Settings = { 0 }, sReturned = { 0 };
		//sKA_Settings.onoff = 1;
		//sKA_Settings.keepalivetime = 60000;			  // Keep Alive in 5.5 sec.
		//sKA_Settings.keepaliveinterval = 3000;        // Resend if No-Reply

		//DWORD dwBytes;
		//WSAIoctl(pClient->sock, SIO_KEEPALIVE_VALS, &sKA_Settings, sizeof(sKA_Settings), &sReturned, sizeof(sReturned), &dwBytes, NULL, NULL);

		len = sizeof(pClient->addr);
		getpeername(pClient->sock, (SOCKADDR*)&pClient->addr, &len);
		PORT = ntohs(pClient->addr.sin_port);
		InetNtop(AF_INET, &pClient->addr.sin_addr, IP, wcslen(IP));

		if (OnConnectionRequest(IP, PORT) == false)
		{
			OnError(enError_ServerDeniedConnection, L"enError_ServerDeniedConnection\n");
			//ReleaseSession(pClient);
			return;
		}

		if (_curUser == _maxUser)
		{
			OnError(enError_OverMaxUser, L"enError_OverMaxUser\n");
			//ReleaseSession(pClient);
			return;
		}

		pClient->clientID = createClientID(pClient->accp_ol.arrayIndex);

		pClient->bReadytoDisconnect = false;
		pClient->bDisconnectNow = false;
		FreeSendQueuePacket(pClient);
		FreeSentPacket(pClient, pClient->sendPacketSize);
		pClient->recvq.ClearBuffer();
		pClient->bSend = false;

		OnClientJoin(pClient->clientID);
		SetIoCompletionPort(pClient);

		InterlockedIncrement(&_monitor_totalAccept);
		InterlockedIncrement(&_monitor_acceptTPS);
		InterlockedIncrement(&_curUser);
	}

	void CLanServer::accept_post(st_CLIENT_INFO *pClient)
	{
		BOOL bRet = FALSE;

		memset(&pClient->accp_ol, 0, sizeof(OVERLAPPED));

		LPFN_ACCEPTEX lpfnAccpetEx = (LPFN_ACCEPTEX)_lpfnAcceptEx;
		InterlockedIncrement(&pClient->ioCount);
		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = INC_ACCEPT_POST;
		EnterProfile(L"ACCEPTEX");
		bRet = lpfnAccpetEx(_listenSock, pClient->sock, pClient->recvq.GetWriteptr(), 0
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

				ReleaseSession(pClient);
			}

			OnError(Error_Type::enError_AcceptExFailure, L"enError_AcceptExFailure\n");
			return;
		}
	}

	void CLanServer::send_post(st_CLIENT_INFO* pClient)
	{
		__int64 ret;
		DWORD trans;
		WSABUF wsabuf[100];

		if (pClient->sendq.IsEmpty()) return;
		if (pClient->sendPacketSize != 0) return;
		if (InterlockedExchange8((char*)&pClient->bSend, 1) == 1) return;
		if (pClient->sendq.IsEmpty())
		{
			InterlockedExchange8((char*)&pClient->bSend, 0);
			return;
		}
		if (pClient->bReadytoDisconnect)
		{
			InterlockedExchange8((char*)&pClient->bSend, 0);
			return;
		}

		trans = 0;

		DWORD Count = pClient->sendq.GetUsedBlockCount();
		if (Count > 100)
		{
			Count = 100;
		}

		for (UINT i = 0; i < Count; ++i)
		{
			pClient->sendq.Dequeue(pClient->packetBuffer[i]);		//뽑아놓고 보관해놓을 곳이 필요함.
			if (pClient->packetBuffer[i] == nullptr)
			{
				//	다른 스레드에서 Enqueue하는 과정에 Tail이 옮겨지지 않고, BlockCount만 1이 증가되었을 수도 있음.
				Count = i;
				break;
			}
			wsabuf[i].buf = pClient->packetBuffer[i]->GetBufptr();
			wsabuf[i].len = pClient->packetBuffer[i]->GetDataSizewithHeader();
		}


		InterlockedIncrement(&pClient->ioCount);
		//UINT64 index = InterlockedIncrement(&pClient->countIndex);
		//pClient->countTracker[index % 500] = INC_SEND_POST;
		pClient->send_ol = { 0, };

		InterlockedExchange(&pClient->sendPacketSize, Count);

		EnterProfile(L"WSASend");
		ret = WSASend(pClient->sock, wsabuf, Count, &trans, 0, &pClient->send_ol, NULL);
		LeaveProfile(L"WSASend");

		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
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

	void CLanServer::recv_post(st_CLIENT_INFO* pClient)
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

	unsigned __stdcall CLanServer::MonitorThread(void *arg)
	{
		CLanServer *pThis = (CLanServer*)arg;
		return pThis->Update_MonitorThread();
	}

	bool CLanServer::Update_MonitorThread(void)
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

			_monitor_sendTPS = _monitor_sendCounter / timeInterval;
			_monitor_recvTPS = _monitor_recvCounter / timeInterval;
			_monitor_acceptTPS = _monitor_acceptCounter / timeInterval;

			_monitor_sendCounter = 0;
			_monitor_recvCounter = 0;
			_monitor_acceptCounter = 0;

			prevTime = curTime;
		};

		timeEndPeriod(1);
		return true;
	}
};