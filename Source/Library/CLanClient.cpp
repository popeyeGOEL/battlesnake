#include <WS2tcpip.h>
#include <Mswsock.h>
#include <WinSock2.h>
#include <process.h>

#include "CLanClient.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Mswsock.lib")

namespace TUNJI_LIBRARY
{
	bool CLanClient::Connect(WCHAR *ip, short port, int threadCnt, bool bNagle)
	{
		if (InterlockedExchange8((char*)&_bClientOn, 1) == 1)
		{
			return false;
		}

		_bNagle = bNagle;
		_threadCount = threadCnt;
		_sendTPS = 0;
		_recvTPS = 0;

		memcpy(_serverIP, ip, sizeof(WCHAR) * 16);
		_serverPort = port;

		_session.addr.sin_family = AF_INET;
		_session.addr.sin_port = htons(port);
		InetPton(AF_INET, ip, &_session.addr.sin_addr);

		SetUpLanClient();

		_session.sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		InitializeSession();

		//	Connect Socket
		if (WSAConnect(_session.sock, (SOCKADDR*)&_session.addr, sizeof(_session.addr), NULL, NULL, NULL, NULL) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{
			int error = WSAGetLastError();
			InterlockedExchange8((char*)&_bClientOn, 0);
			closesocket(_session.sock);
			return false;
		}

		InterlockedExchange8((char*)&_session.bDisconnectNow, 0);

		//	Associate the client socket with the completion port
		_hPort = CreateIoCompletionPort((HANDLE)_session.sock, _hWorkerThreadComport, (ULONG_PTR)&_session, 0);

		u_long mode = 0;
		//setsockopt(pClient->sock, SOL_SOCKET, SO_SNDBUF, (char*)&mode, sizeof(mode));

		if (!_bNagle)
		{
			int option = TRUE;
			setsockopt(_session.sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&option, sizeof(option));
		}

		recv_post();
		OnEnterJoinServer();
		return true;
	}

	void CLanClient::SetUpLanClient(void)
	{
		if (InterlockedExchange8((char*)&_bSetUp, 1) == 1) return;

		WSADATA wsa;
		int iRet = 0;

		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		{
			InterlockedExchange8((char*)&_bSetUp, 0);
			return;
		}
		
		//	Create Thread Profiler
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		//	IOCP handle 생성
		_hWorkerThreadComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

		DWORD dwThreadID;
		//	Create Thread Pool 
		for (int i = 0; i < _threadCount; ++i)
		{
			//_workerThreadList.push_back(std::thread{ WorkerThread, this });
			_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (PVOID)this, 0, (unsigned int*)&dwThreadID);
		}

	}

	bool CLanClient::Disconnect(void)
	{
		CleanUpLanClient();

		if (_session.bDisconnectNow == true) {
			return false;
		}

		OnLeaveServer();
		return true;
	}

	void CLanClient::CleanUpLanClient(void)
	{
		st_CLIENT_INFO *pClient = &_session;
		PostQueuedCompletionStatus(_hWorkerThreadComport, 0, NULL, NULL);

		DWORD status = WaitForMultipleObjects(_threadCount, _hWorkerThread, TRUE, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			//wprintf(L"LanClient thread terminated! \n");
		}

		closesocket(pClient->sock);
		CloseHandle(_hWorkerThreadComport);

		for (int i = 0; i < _threadCount; ++i)
		{
			CloseHandle(_hWorkerThread[i]);
		}

		WSACleanup();

		//cLanPacketPool::FreePacketChunk();

		memset(_serverIP, 0, sizeof(WCHAR) * 16);
		InterlockedExchange8((char*)&_bSetUp, 0);
	}

	void CLanClient::ReleaseSession(void)
	{
		st_CLIENT_INFO *pClient = &_session;

		if (InterlockedExchange8((char*)&pClient->bDisconnectNow, 1) == 0)
		{
			closesocket(pClient->sock);
			OnLeaveServer();
			InitializeSession();
			InterlockedExchange8((char*)&_bClientOn, 0);
		}
	}

	void CLanClient::SendPacket(cLanPacketPool *sPacket)
	{
		if (_bClientOn == false)
		{
			cLanPacketPool::Free(sPacket);
			return;
		}

		if(_session.bDisconnectNow)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cLanPacketPool::Free(sPacket);
			LeaveProfile(L"DEALLOC_PACKET");
			return;
		}

		sPacket->Custom_SetLanHeader(sPacket->GetDataSize());
		_session.sendq.Enqueue(sPacket);

		send_post();
	}

	unsigned int CLanClient::WorkerThread(void *arg)
	{
		CLanClient *pThis = (CLanClient*)arg;
		return pThis->Update_WorkerThread();
	}

	unsigned int CLanClient::Update_WorkerThread(void)
	{
		bool bRet;
		__int64 ret;
		DWORD trans;
		OVERLAPPED *olOverlap;

		//profiler set 
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		//Allocate packet chunk to each thread
		cLanPacketPool::AllocPacketChunk();
		cNetPacketPool::AllocPacketChunk();
		st_CLIENT_INFO *pClient;

		while (1)
		{
			olOverlap = nullptr;
			trans = 0;
			pClient = nullptr;
			bRet = GetQueuedCompletionStatus(_hWorkerThreadComport, &trans, (PULONG_PTR)&pClient, (LPOVERLAPPED*)&olOverlap, INFINITE);
			if (bRet == FALSE)
			{
				LOG(L"GQCS_Error", cSystemLog::LEVEL_ERROR, L"#CLanClient - Update_WorkerThread : GetqueuedCompletionStatus Error %d", WSAGetLastError());
			}

			if (trans == 0 && olOverlap == nullptr && pClient == nullptr)
			{
				cLanPacketPool::FreePacketChunk();
				cNetPacketPool::FreePacketChunk();
				PostQueuedCompletionStatus(_hWorkerThreadComport, 0, (ULONG)0, nullptr);
				break;
			}

			if (trans == 0)
			{
				shutdown(pClient->sock, SD_SEND);
			}
			else if (&pClient->send_ol == olOverlap)
			{
				send_complete(trans);
			}
			else if (&pClient->recv_ol == olOverlap)
			{
				recv_complete(trans);
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
				ReleaseSession();
				LeaveProfile(L"RELEASE_SESSION");
			}
		};

		return 0;
	}

	void CLanClient::recv_post(void)
	{
		st_CLIENT_INFO *pClient = &_session;

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
				ReleaseSession();
			}
		}
	}
	void CLanClient::send_post(void)
	{
		__int64 ret;
		DWORD trans;
		WSABUF wsabuf[100];

		st_CLIENT_INFO *pClient = &_session;

		if (pClient->sendq.IsEmpty()) return;
		if (InterlockedExchange8((char*)&pClient->bSend, 1) == 1) return;
		if (pClient->sendq.IsEmpty())
		{
			InterlockedExchange8((char*)&pClient->bSend, 0);
			return;
		}
		if (pClient->bDisconnectNow)
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

		for (int i = 0; i < Count; ++i)
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

			FreeSentPacket(pClient->sendPacketSize);
			InterlockedExchange8((char*)&pClient->bSend, 0);

			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}

				ReleaseSession();
			}
		}
	}

	void CLanClient::recv_complete(DWORD trans)
	{
		cLanPacketPool::st_LAN_HEADER header;
		cLanPacketPool *dsPacket;
		int ret;

		st_CLIENT_INFO *pClient = &_session;
		pClient->recvq.MoveRearforSave(trans);
		InterlockedIncrement(&_recvTPS);

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

			OnRecv(dsPacket);
			cLanPacketPool::Free(dsPacket);
		}

		//if (pClient->bDisconnectNow) return;

		EnterProfile(L"RECV_POST");
		recv_post();
		LeaveProfile(L"RECV_POST");
	}

	void CLanClient::send_complete(DWORD trans)
	{
		st_CLIENT_INFO *pClient = &_session;

		FreeSentPacket(pClient->sendPacketSize);
		InterlockedExchange8((char*)&pClient->bSend, 0);
		InterlockedIncrement(&_sendTPS);
		OnSend(trans);

		//	이제 끊을 세션인지 아닌지 확인하기.		
		if (pClient->bLastPacket)
		{
			InterlockedExchange8((char*)&pClient->bLastPacket, 0);
			shutdown(pClient->sock, SD_BOTH);
			return;
		}

		EnterProfile(L"SEND_POST");
		send_post();
		LeaveProfile(L"SEND_POST");
	}

	void CLanClient::FreeSentPacket(UINT64 iSize)
	{
		st_CLIENT_INFO *pClient = &_session;

		if (pClient->bSend == false) return;
		if (InterlockedExchange(&pClient->sendPacketSize, 0) == 0) return;

		for (int i = 0; i < iSize; ++i)
		{
			EnterProfile(L"DEALLOC_PACKET");
			cLanPacketPool::Free(pClient->packetBuffer[i]);
			LeaveProfile(L"DEALLOC_PACKET");
		}
	}

	void CLanClient::FreeSendQueuePacket(void)
	{
		st_CLIENT_INFO *pClient = &_session;
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

	void CLanClient::InitializeSession(void)
	{
		FreeSendQueuePacket();
		FreeSentPacket(_session.sendPacketSize);
		_session.bSend = false;
		_session.bLastPacket = false;
		_session.ioCount = 0;
	}
};