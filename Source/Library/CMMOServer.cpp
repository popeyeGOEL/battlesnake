#include <WS2tcpip.h>
#include <mstcpip.h>
#include <MSWSock.h>
#include <process.h>

#include "CMMOServer.h"

#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"
#include "CommonProtocol.h"

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Mswsock.lib")

namespace TUNJI_LIBRARY
{
	CMMOServer::CMMOServer(int maxSession,int authSleepCount, int gameSleepCount, int sendSleepCount, BYTE packetCode, BYTE packetKey1, BYTE packetKey2)
	{
		_listenSock = INVALID_SOCKET;
		memset(_bindIP, 0, sizeof(WCHAR) * 16);
		_pSessionArray = new CNetSession*[maxSession];
		_maxUser = maxSession;
		_curID = 0;
		_authSleepCount = authSleepCount;
		_gameSleepCount = gameSleepCount;
		_sendSleepCount = sendSleepCount;

		_packetCode = packetCode;
		_packetKey1 = packetKey1;
		_packetKey1 = packetKey2;

		InitializeConditionVariable(&_cnIndexQueueIsNotEmpty);
		InitializeSRWLock(&_indexLock);
	}

	CMMOServer::~CMMOServer(void)
	{
		delete[] _pSessionArray;
	}

	bool CMMOServer::Start(WCHAR *bindIP, int port, int threadCnt, bool bNagle, int maxAcceptPool)
	{
		WSADATA wsa;
		u_long mode = 1;

		GUID GuidAcceptEx = WSAID_ACCEPTEX;

		int iRet;
		DWORD dwBytes;

		_lpfnAcceptEx = NULL;

		memcpy(_bindIP, bindIP, sizeof(WCHAR) * 16);
		_port = port;

		if (threadCnt > WORKER_THREAD_MAX)
		{
			threadCnt = WORKER_THREAD_MAX;
		}

		_threadCnt = threadCnt;
		_bNagle = bNagle;
		_maxAcceptPool = maxAcceptPool;
		_curUser = 0;

		_bExitAuth = false;
		_bExitGame = false;
		_bExitSend = false;
		_bExitMonitor = false;

		_monitor_AcceptSocket = 0;
		_monitor_sessionMode_Auth = 0;
		_monitor_sessionMode_Game = 0;
		_monitor_AuthUpdateFPS = 0;
		_monitor_GameUpdateFPS = 0;
		_monitor_AcceptTPS = 0;
		_monitor_GamePacketProcTPS = 0;
		_monitor_AuthPacketProcTPS = 0;
		_monitor_PacketSendTPS = 0;

		_monitor_AuthUpdateCounter = 0;
		_monitor_GameUpdateCounter = 0;
		_monitor_AcceptCounter = 0;
		_monitor_GamePacketProcCounter = 0;
		_monitor_AuthPacketProcCounter = 0;
		_monitor_PacketSendCounter = 0;

		memset(&_addr, 0, sizeof(_addr));
		_addr.sin_family = AF_INET;
		_addr.sin_port = htons(port);
		InetPton(AF_INET, bindIP, &_addr.sin_addr);

		do
		{
			if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) break;
			_listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (INVALID_SOCKET == _listenSock) break;
			if (SOCKET_ERROR == bind(_listenSock, (SOCKADDR*)&_addr, sizeof(_addr)))break;
			if (SOCKET_ERROR == listen(_listenSock, SOMAXCONN)) break;

			if (!bNagle)
			{
				int option = TRUE; 
				setsockopt(_listenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&option, sizeof(option));
			}

			iRet = WSAIoctl(_listenSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&GuidAcceptEx, sizeof(GuidAcceptEx),
				(LPFN_ACCEPTEX)&_lpfnAcceptEx, sizeof(LPFN_ACCEPTEX),
				&dwBytes, NULL, NULL);

			if (iRet == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) break;

			//	Create IOCP Handle
			_hWorkerThreadComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);

			//	Associate the listening socket with the completion port
			CreateIoCompletionPort((HANDLE)_listenSock, _hWorkerThreadComport, (ULONG_PTR)0, 0);

			//	Create Thread Profile ID
			DWORD ID = GetCurrentThreadId();
			g_profiler.CreateThreadID(ID);

			DWORD dwThreadID;

			//	Worker Thread 생성 
			for (int i = 0; i < threadCnt; ++i)
			{
				_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);
			}

			//	Auth Thread 생성 
			_hAuthThread = (HANDLE)_beginthreadex(NULL, 0, AuthThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);
	
			//	Game Update Thread 생성
			_hGameUpdateThread = (HANDLE)_beginthreadex(NULL, 0, GameUpdateThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);

			//	Send Thread 생성 
			_hSendThread = (HANDLE)_beginthreadex(NULL, 0, SendThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);

			//	Monitor Thread 생성
			_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);

			//	Accept Pool Thread 생성
			_bServerOn = true;

			_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, 0, (unsigned int*)&dwThreadID);

			for (int i = 0; i < _maxUser; ++i)
			{
				_indexQueue.Enqueue(i);
				WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
			}

			_pdhCounter.InitProcessCounter(L"BattleSnake_Battle");
			_pdhCounter.InitEthernetCounter();
			_pdhCounter.InitMemoryCounter();

			wprintf(L"INIT GAME SERVER!\n");
			return true;
		} while (1);

		wprintf(L"FAIL TO INIT GAME SERVER: %d\n", WSAGetLastError());
		return false;
	}

	bool CMMOServer::Stop(void)
	{
		_bServerOn = false;

		//	전체 세션 정리 
		for (int i = 0; i < _maxUser; ++i)
		{
			if (_pSessionArray[i]->_clientInfo.clientID != 0)
			{
				shutdown(_pSessionArray[i]->_clientInfo.sock, SD_BOTH);
			}
		}

		//	리슨 소켓 닫기 
		closesocket(_listenSock);

		//	Worker	Thread 종료 
		PostQueuedCompletionStatus(_hWorkerThreadComport, 0, NULL, NULL);

		DWORD status = WaitForMultipleObjects(_threadCnt, _hWorkerThread, TRUE, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"EXIT WORKER THREAD! \n");
		}

		for (int i = 0; i < _threadCnt; ++i)
		{
			CloseHandle(_hWorkerThread[i]);
		}

		// Accept Thread 종료 
		_indexQueue.ClearBuffer();
		_curPostedAcceptEx = 0;
		WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
		status = WaitForSingleObject(_hAcceptThread, INFINITE);
		_indexQueue.ClearBuffer();
		CloseHandle(_hAcceptThread);

		//	Send Thread , Game Thread, Auth Thread 종료 
		_bExitSend = true;
		status = WaitForSingleObject(_hSendThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"EXIT SEND THREAD!\n");
		}

		CloseHandle(_hSendThread);

		_bExitGame = true; 
		status = WaitForSingleObject(_hGameUpdateThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"EXIT GAME THREAD!\n");
		}
		CloseHandle(_hGameUpdateThread);

		_bExitAuth = true;
		status = WaitForSingleObject(_hAuthThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"EXIT AUTH THREAD!\n");
		}
		CloseHandle(_hAuthThread);

		_bExitMonitor = true;
		status = WaitForSingleObject(_hMonitorThread, INFINITE);
		if (status == WAIT_OBJECT_0)
		{
			wprintf(L"EXIT MONITOR THREAD!\n");
		}
		CloseHandle(_hMonitorThread);

		_pdhCounter.TerminatePdhCounter();

		return true;
	}

	void CMMOServer::SetSessionArray(CNetSession *pSession, int index)
	{
		_pSessionArray[index] = pSession;
		LOG(L"SetSessionArray", cSystemLog::LEVEL_DEBUG, L"CMMOServer ID : [%d] Index : [%d]", pSession->_clientInfo.clientID, index);
	}

	int	CMMOServer::ClientIDtoIndex(UINT64 clientID)
	{
		UINT64	mask;
		int idx;
		mask = 0xFFFF;
		idx = int(clientID & mask);

		return idx;
	}

	CNetSession* CMMOServer::FindClientbyOverlap(OVERLAPPED * acceptOl)
	{
		CNetSession::st_ACCEPT_OVERLAP *ol = (CNetSession::st_ACCEPT_OVERLAP*)acceptOl;
		int index = ol->arrayIndex;

		return _pSessionArray[index];
	}

	UINT64	CMMOServer::CreateClientID(int index)
	{
		UINT64 ID = InterlockedIncrement(&_curID);
		return (ID << 16) | index;
	}

	void CMMOServer::ReleaseSession(CNetSession *pSession)
	{	
		if (pSession->_ioCount != 0) return;
		if (InterlockedExchange8((char*)&pSession->_bDisconnect, 1) == 1) return;

		pSession->SetIoTracker(CNetSession::IO_COUNT_TYPE::RELEASE, pSession->_ioCount, nullptr);
		//closesocket(pSession->_clientInfo.sock);

		pSession->InitSession();
		InterlockedDecrement(&_curUser);

		_indexQueue.Enqueue(pSession->_acceptOl.arrayIndex);
		WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
	}

	bool CMMOServer::Accept_Post(CNetSession *pSession)
	{
		BOOL bRet = FALSE;
		memset(&pSession->_acceptOl, 0, sizeof(OVERLAPPED));
		
		LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)_lpfnAcceptEx;
		int ret = InterlockedIncrement(&pSession->_ioCount);
		pSession->SetIoTracker(CNetSession::INC_ACCEPT_POST, ret , nullptr);

		//EnterProfile(L"AcceptEx");
		InterlockedIncrement(&_curPostedAcceptEx);

		bRet = lpfnAcceptEx(_listenSock, pSession->_clientInfo.sock, pSession->_recvq.GetWriteptr(), 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, 0, &pSession->_acceptOl);
		//LeaveProfile(L"AcceptEx");

		if (bRet == FALSE && WSAGetLastError() != ERROR_IO_PENDING)
		{
			LOG(L"WSA", cSystemLog::LEVEL_ERROR, L"Error on AcceptEx : [%d] [%d]", pSession->_clientInfo.clientID >> 16, WSAGetLastError());
			ret = InterlockedDecrement(&pSession->_ioCount);
			pSession->SetIoTracker(CNetSession::DEC_ACCEPT_POST, ret, nullptr);
			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}
			}
			closesocket(pSession->_clientInfo.sock);
			return false;
		}
		return true;
	}

	void CMMOServer::Recv_Post(CNetSession *pSession)
	{
		int ret;
		char *pWrite = pSession->_recvq.GetWriteptr();
		int len = pSession->_recvq.GetConstantWriteSize();
		int empty = pSession->_recvq.GetEmptySize();
		char *pStart = pSession->_recvq.GetStartptr();

		WSABUF wsabuf[2];
		DWORD flag = 0;
		DWORD trans = 0;
		int count = 1;

		wsabuf[0].buf = pWrite;
		wsabuf[0].len = len;

		if (len < empty )
		{
			wsabuf[1].buf = pStart;
			wsabuf[1].len = empty - len;
			count = 2;
		}

		ret = InterlockedIncrement(&pSession->_ioCount);
		pSession->SetIoTracker(CNetSession::INC_RECV_POST, ret, nullptr);

		pSession->_recvOl = { 0, };
		//EnterProfile(L"WSARecv");
		ret = WSARecv(pSession->_clientInfo.sock, wsabuf, count, &trans, &flag, &pSession->_recvOl, NULL);
		//LeaveProfile(L"WSARecv");
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			LOG(L"WSA", cSystemLog::LEVEL_DEBUG, L"Error on WSARecv : ClientID [%d] WSAGetLastError [%d] Client Mode [%d]", pSession->_clientInfo.clientID >> 16, WSAGetLastError(), pSession->_mode);

			ret = InterlockedDecrement(&pSession->_ioCount);
			pSession->SetIoTracker(CNetSession::DEC_RECV_POST, ret, nullptr);
			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}

				pSession->_bLogoutFlag = true;
			}
		}
	}

	void CMMOServer::Send_Post(CNetSession *pSession)
	{
		int ret;
		DWORD trans;
		WSABUF wsabuf[100];
		UINT64 bufCount = 0;
		trans = 0;

		while (bufCount < 100)
		{
			if (pSession->_sendq.Dequeue(pSession->_packetBuffer[bufCount]) == false) break;
			wsabuf[bufCount].buf = pSession->_packetBuffer[bufCount]->GetBufptr();
			wsabuf[bufCount].len = pSession->_packetBuffer[bufCount]->GetDataSizewithHeader();

			++bufCount;
		}

		ret = InterlockedIncrement(&pSession->_ioCount);
		pSession->SetIoTracker(CNetSession::INC_SEND_POST, ret, nullptr);
		InterlockedExchange(&pSession->_sendPacketSize, bufCount);

		pSession->_sendOl = { 0, };
		//EnterProfile(L"WSASend");

		ret = WSASend(pSession->_clientInfo.sock, wsabuf, bufCount, &trans, 0, &pSession->_sendOl, NULL);
		//LeaveProfile(L"WSASend");
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			LOG(L"WSA", cSystemLog::LEVEL_DEBUG, L"Error on WSASend : ClientID [%d] WSAGetLastError [%d] Client Mode [%d] Send Packet Count [%d]", pSession->_clientInfo.clientID >> 16, WSAGetLastError(), pSession->_mode, bufCount);

			ret = InterlockedDecrement(&pSession->_ioCount);
			pSession->SetIoTracker(CNetSession::DEC_SEND_POST, ret, nullptr);
			pSession->FreeSentPacket();
			InterlockedExchange(&pSession->_sendIO, 0);

			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}

				pSession->_bLogoutFlag = true;
			}
		}

	}

	void CMMOServer::Accept_Complete(CNetSession *pSession)
	{
		int len; 
		int mode;
		DWORD dwError;
		DWORD dwBytes;

		mode = 0;
		setsockopt(pSession->_clientInfo.sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&_listenSock, sizeof(SOCKET));
		//setsockopt(pSession->_clientInfo.sock, SOL_SOCKET, SO_SNDBUF, (char*)&mode, sizeof(mode));

		dwError = 0L;
		tcp_keepalive sKA_Settings = { 0, }, sReturned = { 0, };
		sKA_Settings.onoff = 1;
		sKA_Settings.keepalivetime = 60000;
		sKA_Settings.keepaliveinterval = 3000;

		WSAIoctl(pSession->_clientInfo.sock, SIO_KEEPALIVE_VALS, &sKA_Settings, sizeof(sKA_Settings), &sReturned, sizeof(sReturned), &dwBytes, NULL, NULL);

		len = sizeof(pSession->_clientInfo.addr);
		getpeername(pSession->_clientInfo.sock, (SOCKADDR*)&pSession->_clientInfo.addr, &len);
		pSession->_clientInfo.port = ntohs(pSession->_clientInfo.addr.sin_port);
		InetNtop(AF_INET, &pSession->_clientInfo.addr.sin_addr, pSession->_clientInfo.IP, wcslen(pSession->_clientInfo.IP));

		pSession->_clientInfo.clientID = CreateClientID(pSession->_acceptOl.arrayIndex);
		
		//	Clear Session Queue
		pSession->InitBuffer();
		InterlockedExchange(&pSession->_sendIO, 0);
		pSession->_bAuthToGameFlag = false;
		pSession->_bLogoutFlag = false;
		pSession->_bShutdown = false;
		pSession->_bLastPacket = false;
		pSession->_bDisconnect = false;

		//	AUTH 상태 변환 
		pSession->_mode = CNetSession::MODE_AUTH;
		pSession->SetModeTracker(CNetSession::MODE_AUTH);
		pSession->OnAuth_ClientJoin();
		SetIoCompletionPort(pSession);

		InterlockedIncrement((UINT64*)&_monitor_sessionMode_Auth);
		InterlockedIncrement((UINT64*)&_monitor_AcceptSocket);
		InterlockedIncrement((UINT64*)&_monitor_AcceptCounter);
		InterlockedDecrement(&_curPostedAcceptEx);
		InterlockedIncrement(&_curUser);

		WakeConditionVariable(&_cnIndexQueueIsNotEmpty);
	}

	void CMMOServer::Recv_Complete(CNetSession *pSession, DWORD trans)
	{
		cNetPacketPool::st_NET_HEADER header;
		cNetPacketPool *dsPacket;
		int ret;

		pSession->_recvq.MoveRearforSave(trans);

		while (1)
		{
			ret = 0;
			header = { 0, };
			dsPacket = nullptr;

			ret = pSession->_recvq.Peek((char*)&header, sizeof(cNetPacketPool::st_NET_HEADER));
			if (ret < sizeof(cNetPacketPool::st_NET_HEADER)) break;

			if (header.len > cNetPacketPool::en_BUFSIZE_DEFALUT - sizeof(cNetPacketPool::st_NET_HEADER))
			{
				LOG(L"PacketError", cSystemLog::LEVEL_ERROR, L"Header Length Error : [%d]", pSession->_clientInfo.clientID >> 16);
				return;
			}

			if (pSession->_recvq.GetUsedSize() < header.len + sizeof(cNetPacketPool::st_NET_HEADER)) break;
			pSession->_recvq.MoveFrontforDelete(sizeof(cNetPacketPool::st_NET_HEADER));

			EnterProfile(L"AllocPacket");
			dsPacket = cNetPacketPool::Alloc();
			LeaveProfile(L"AllocPacket");
			dsPacket->Custom_SetNetHeader(header);
			ret = pSession->_recvq.Dequeue(dsPacket->GetWriteptr(), header.len);

			dsPacket->MoveRearforSave(ret);

			try
			{
				dsPacket->Decode(_packetCode, _packetKey1, _packetKey2);
			}
			catch(cNetPacketPool::packetException &e)
			{
				cNetPacketPool::Free(dsPacket);
				LOG(L"PacketError", cSystemLog::LEVEL_ERROR, L"Decode Error : [%d]", pSession->_clientInfo.clientID >> 16);
				return;
			}

			//	Enqueue Packet to Complete Queue
			EnterProfile(L"Complete_Enqueue");
			bool bRet = pSession->_completePacket.Enqueue(dsPacket);
			LeaveProfile(L"Complete_Enqueue");

			if (bRet == false)
			{
				cNetPacketPool::Free(dsPacket);
				LOG(L"PacketError", cSystemLog::LEVEL_ERROR, L"Complete Queue is Full : [%d]", pSession->_clientInfo.clientID >> 16);
				return;
			}
		}

		Recv_Post(pSession);
	}

	void CMMOServer::Send_Complete(CNetSession *pSession, DWORD trans)
	{
		//LOG(L"Send Complete", cSystemLog::LEVEL_DEBUG, L"Trans Size = %d , Send Packet Size = %d\n", trans, pSession->_sendPacketSize);
		InterlockedAdd64(&_monitor_PacketSendCounter, pSession->_sendPacketSize);
		pSession->FreeSentPacket();
		InterlockedExchange(&pSession->_sendIO, 0);

		//InterlockedDecrement(&pSession->_sendIO);

		if (pSession->_bLastPacket == true && pSession->_sendIO == 0)
		{
			shutdown(pSession->_clientInfo.sock, SD_BOTH);
		}
	}

	void CMMOServer::SetIoCompletionPort(CNetSession *pSession)
	{
		if (CreateIoCompletionPort((HANDLE)pSession->_clientInfo.sock, _hWorkerThreadComport, (ULONG_PTR)pSession, 0) == nullptr)
		{
			LOG(L"GQCS_Error", cSystemLog::LEVEL_DEBUG, L"#CMMOServer - SetIoCompletionPort :  Error %d", WSAGetLastError());
			return;
		}
		Recv_Post(pSession);
	}

	void CMMOServer::SendPacket_AllLoginUser(cNetPacketPool *sPacket, UINT64 execptID)
	{
		
	}

	void CMMOServer::SendPacket(cNetPacketPool *sPacket, UINT64 clientID)
	{
		int idx = ClientIDtoIndex(clientID);
		CNetSession *pSession = _pSessionArray[idx];
		sPacket->Encode(_packetCode, _packetKey1, _packetKey2);
		pSession->_sendq.Enqueue(sPacket);
	}

	void CMMOServer::Error(int errorCode, WCHAR *formatStr, ...)
	{

	}

	void CMMOServer::Auth_PacketProc(void)
	{
		UINT64 count;
		int i;
		UINT64 maxUser = _maxUser;
		CNetSession **pSessionArray = _pSessionArray;
		cNetPacketPool *dsPacket; 

		for (count = 0; count < maxUser; ++count)
		{
			if (pSessionArray[count]->_mode == CNetSession::MODE_AUTH)
			{
				if (pSessionArray[count]->_bAuthToGameFlag == false &&
					pSessionArray[count]->_completePacket.isEmpty() == false && 
					pSessionArray[count]->_bLogoutFlag == false)
				{
					for (i = 0; i < AUTH_PACKET_LIMIT; ++i)							//	Auth Packet 일정량 만큼만 처리하기	
					{
						EnterProfile(L"Complete_Dequeue");
						pSessionArray[count]->_completePacket.Dequeue(dsPacket);
						LeaveProfile(L"Complete_Dequeue");

						if (dsPacket == nullptr) break;

						EnterProfile(L"OnAuth_Packet");
						pSessionArray[count]->OnAuth_Packet(dsPacket);
						LeaveProfile(L"OnAuth_Packet");

						EnterProfile(L"FreePacket");
						cNetPacketPool::Free(dsPacket);
						LeaveProfile(L"FreePacket");

						++_monitor_AuthPacketProcCounter;

						if (pSessionArray[count]->_bAuthToGameFlag) break;			//	Game Thread 용 패킷 인지 확인 할 것
																		
					}
				}
				else if (pSessionArray[count]->_bLogoutFlag == true)
				{
					pSessionArray[count]->_mode = CNetSession::MODE_LOGOUT_IN_AUTH;
					pSessionArray[count]->SetModeTracker(CNetSession::MODE_LOGOUT_IN_AUTH);
					InterlockedDecrement((UINT64*)&_monitor_sessionMode_Auth);
				}
			}
		}
	}

	void CMMOServer::Auth_LeaveProc(void)
	{
		UINT64 count;
		UINT64 maxUser = _maxUser;
		CNetSession **pSessionArray = _pSessionArray;

		for (count = 0; count < maxUser; ++count)
		{
			if (pSessionArray[count]->_bAuthToGameFlag == true && 
				pSessionArray[count]->_mode == CNetSession::MODE_AUTH)
			{
				pSessionArray[count]->_mode = CNetSession::MODE_AUTH_TO_GAME;
				pSessionArray[count]->SetModeTracker(CNetSession::MODE_AUTH_TO_GAME);
				InterlockedDecrement((UINT64*)&_monitor_sessionMode_Auth);
			}
		}
	}

	void CMMOServer::Auth_LogoutProc(void)
	{
		UINT64 count;
		UINT64 maxUser = _maxUser;
		CNetSession **pSessionArray = _pSessionArray;

		static UINT64 curIndex = 0;
		int limit = 0;
		UINT64 index;

		for (count = 0; count < maxUser; ++count)
		{
			index = (count + curIndex) % maxUser;
			if (limit > LOGOUT_SESSION_LIMIT)
			{
				curIndex = index;
				break;
			}
			if (pSessionArray[index]->_mode == CNetSession::MODE_LOGOUT_IN_AUTH && pSessionArray[index]->_sendIO == 0)
			{
				pSessionArray[index]->_mode = CNetSession::MODE_WAIT_LOGOUT;
				pSessionArray[index]->SetModeTracker(CNetSession::MODE_WAIT_LOGOUT);
				pSessionArray[index]->OnAuth_ClientLeave();
				++limit;
			}
		}
	}

	void CMMOServer::Game_AuthToGameProc(void)
	{
		UINT64 count;
		UINT64 maxUser = _maxUser;
		CNetSession **pSessionArray = _pSessionArray;

		static UINT64 curIndex = 0;
		int limit = 0;
		UINT64 index;
		for(count = 0 ; count < maxUser; ++count)
		{
			index = (count + curIndex) % maxUser;
			if (limit > AUTH_TO_GAME_LIMIT)
			{
				curIndex = index;
				break;
			}
			if (pSessionArray[index]->_mode == CNetSession::MODE_AUTH_TO_GAME)
			{
				pSessionArray[index]->_mode = CNetSession::MODE_GAME;
				pSessionArray[index]->SetModeTracker(CNetSession::MODE_GAME);
				pSessionArray[index]->OnGame_ClientJoin();
				++_monitor_sessionMode_Game;
				++limit;
			}
		}
	}

	void CMMOServer::Game_PacketProc(void)
	{
		UINT64 count;
		int i;
		UINT64 maxUser = _maxUser;

		CNetSession **pSessionArray = _pSessionArray;
		cNetPacketPool *dsPacket;

		for (count = 0; count < maxUser; ++count)
		{
			if (pSessionArray[count]->_mode == CNetSession::MODE_GAME)
			{
				if (pSessionArray[count]->_completePacket.isEmpty() == false && 
					pSessionArray[count]->_bLogoutFlag == false)
				{
					for (i = 0; i < GAME_PACKET_LIMIT; ++i)
					{
						EnterProfile(L"Complete_Dequeue");
						pSessionArray[count]->_completePacket.Dequeue(dsPacket);
						LeaveProfile(L"Complete_Dequeue");

						if (dsPacket == nullptr) break;

						EnterProfile(L"OnGame_Packet");
						pSessionArray[count]->OnGame_Packet(dsPacket);
						LeaveProfile(L"OnGame_Packet");

						EnterProfile(L"FreePacket");
						cNetPacketPool::Free(dsPacket);
						LeaveProfile(L"FreePacket");

						++_monitor_GamePacketProcCounter;
					}
				}
				else if (pSessionArray[count]->_bLogoutFlag == true)
				{
					pSessionArray[count]->_mode = CNetSession::MODE_LOGOUT_IN_GAME;
					pSessionArray[count]->SetModeTracker(CNetSession::MODE_LOGOUT_IN_GAME);
					--_monitor_sessionMode_Game;
				}
			}
		}
	}

	void CMMOServer::Game_LeaveProc(void)
	{
		UINT64 count;
		UINT64 maxUser = _maxUser;
		CNetSession **pSessionArray = _pSessionArray;

		static UINT64 curIndex = 0;
		int limit = 0;
		UINT64 index;
		for (count = 0; count < maxUser; ++count)
		{
			index = (count + curIndex) % maxUser;
			if (limit > LOGOUT_SESSION_LIMIT)
			{
				curIndex = index;
				break;
			}
			if (pSessionArray[index]->_mode == CNetSession::MODE_LOGOUT_IN_GAME && pSessionArray[index]->_sendIO == 0)
			{
				pSessionArray[index]->_mode = CNetSession::MODE_WAIT_LOGOUT;
				pSessionArray[index]->SetModeTracker(CNetSession::MODE_WAIT_LOGOUT);
				pSessionArray[index]->OnGame_ClientLeave();
				++limit;
			}
		}
	}

	void CMMOServer::Game_ReleaseProc(void)
	{
		UINT64 count;
		UINT64 maxUser = _maxUser;
		CNetSession **pSessionArray = _pSessionArray;

		static UINT64 curIndex = 0;
		int limit = 0;
		UINT64 index;
		for (count = 0; count < maxUser; ++count)
		{
			index = (count + curIndex) % maxUser;
			if (limit > RELEASE_SESSION_LIMIT)
			{
				curIndex = index;
				break;
			}

			if (pSessionArray[index]->_mode == CNetSession::MODE_WAIT_LOGOUT && pSessionArray[index]->_sendIO == 0)
			{
				pSessionArray[index]->OnGame_ClientRelease();
				ReleaseSession(pSessionArray[index]);
				++limit;
			}
		}
	}

	unsigned __stdcall WINAPI CMMOServer::AuthThread(void *arg)
	{
		CMMOServer *pThis = (CMMOServer*)arg; 
		return pThis->Update_AuthThread();
	}

	bool CMMOServer::Update_AuthThread(void)
	{
		bool *bExitAuth = &_bExitAuth;
		int authSleepCount = _authSleepCount;
		
		//	Create Thread Profile ID
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		timeBeginPeriod(1);
		cNetPacketPool::AllocPacketChunk();
		cLanPacketPool::AllocPacketChunk();
		while (*bExitAuth == false)
		{
			//EnterProfile(L"Auth_PacketProc");
			Auth_PacketProc();
			//LeaveProfile(L"Auth_PacketProc");

			//EnterProfile(L"Auth_Update");
			OnAuth_Update();
			//LeaveProfile(L"Auth_Update");

			//EnterProfile(L"Auth_LogoutProc");
			Auth_LogoutProc();
			//LeaveProfile(L"Auth_LogoutProc");

			//EnterProfile(L"Auth_LeaveProc");
			Auth_LeaveProc();
			//LeaveProfile(L"Auth_LeaveProc");

			++_monitor_AuthUpdateCounter;
			Sleep(authSleepCount);
		}
		timeEndPeriod(1);
		cNetPacketPool::FreePacketChunk();
		cLanPacketPool::FreePacketChunk();
		return true;
	}

	unsigned __stdcall WINAPI CMMOServer::GameUpdateThread(void *arg)
	{
		CMMOServer *pThis = (CMMOServer*)arg;
		return pThis->Update_GameUpdateThread();
	}

	bool CMMOServer::Update_GameUpdateThread(void)
	{
		bool *bExitGame = &_bExitGame;
		int gameSleepCount = _gameSleepCount;

		//	Create Thread Profile ID
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		timeBeginPeriod(1);
		cNetPacketPool::AllocPacketChunk();
		cLanPacketPool::AllocPacketChunk();
		while (*bExitGame == false)
		{
			//EnterProfile(L"Game_AuthToGameProc");
			Game_AuthToGameProc();
			//LeaveProfile(L"Game_AuthToGameProc");

			//EnterProfile(L"Game_PacketProc");
			Game_PacketProc();
			//LeaveProfile(L"Game_PacketProc");

			//EnterProfile(L"Game_Update");
			OnGame_Update();
			//LeaveProfile(L"Game_Update");

			//EnterProfile(L"Game_LeaveProc");
			Game_LeaveProc();
			//LeaveProfile(L"Game_LeaveProc");

			//EnterProfile(L"Game_ReleaseProc");
			Game_ReleaseProc();
			//LeaveProfile(L"Game_ReleaseProc");

			//InterlockedIncrement((UINT64*)&_monitor_GameUpdateCounter);
			++_monitor_GameUpdateCounter;
			Sleep(gameSleepCount);
		}
		cNetPacketPool::AllocPacketChunk();
		cLanPacketPool::FreePacketChunk();
		timeEndPeriod(1);
		return true;
	}

	unsigned __stdcall WINAPI CMMOServer::WorkerThread(void *arg)
	{
		CMMOServer *pThis = (CMMOServer*)arg;
		return pThis->Update_WorkerThread();
	}

	bool CMMOServer::Update_WorkerThread(void)
	{
		int ret;
		int error;
		bool bRet;
		DWORD trans; 
		CNetSession *pSession; 
		OVERLAPPED *ol;

		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		cNetPacketPool::AllocPacketChunk();

		while (1)
		{
			pSession = NULL;
			ol = NULL;
			trans = 0;

			bRet = GetQueuedCompletionStatus(_hWorkerThreadComport, &trans, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&ol, INFINITE);
			if (bRet == FALSE)
			{
				int error = WSAGetLastError();
				if (error == ERROR_SEM_TIMEOUT)
				{
					LOG(L"GQCS_Error", cSystemLog::LEVEL_ERROR, L"#CMMOServer - Update_WorkerThread : GetQueuedCompletion Error %d clientID %d", WSAGetLastError(), pSession->_acceptOl.arrayIndex);
				}
			}

			if (trans == 0 && pSession == NULL && ol == NULL)
			{
				wprintf(L"[%4d] Thread Terminated!\n", ID);
				cNetPacketPool::FreePacketChunk();
				PostQueuedCompletionStatus(_hWorkerThreadComport, 0, (ULONG)0, NULL);
				break;
			}

			if (ol != NULL && pSession == NULL)
			{
				pSession = FindClientbyOverlap(ol);
				if (pSession == NULL)
				{
					OnError(Error_Type::enError_FindClientFailure, L"enError_FindClientFailure\n");
					continue;
				}

				if (bRet == true)
				{
					//EnterProfile(L"Accept_Complete");
					Accept_Complete(pSession);
					//LeaveProfile(L"Accept_Complete");
				}
			}
			else
			{
				if (trans == 0 && (&pSession->_recvOl == ol))
				{
					pSession->SetIoTracker(CNetSession::IO_COUNT_TYPE::RECV_ZERO, pSession->_ioCount, ol);
					shutdown(pSession->_clientInfo.sock, SD_SEND);
				}
				else if (&pSession->_sendOl == ol)
				{
					//EnterProfile(L"Send_Complete");
					Send_Complete(pSession, trans);
					//LeaveProfile(L"Send_Complete");

				}
				else if (&pSession->_recvOl == ol)
				{
					//EnterProfile(L"Recv_Complete");
					Recv_Complete(pSession, trans);
					//LeaveProfile(L"Recv_Complete");
				}
			}

			ret = InterlockedDecrement(&pSession->_ioCount);
			pSession->SetIoTracker(CNetSession::DEC_WORKER, ret, ol);
			if (ret <= 0)
			{
				if (ret < 0)
				{
					cCrashDump::Crash();
				}

				LOG(L"Logout_Flag", cSystemLog::LEVEL_DEBUG, L"Logout flag on Worker Thread : [%d]", pSession->_clientInfo.clientID >> 16);
				pSession->_bLogoutFlag = true;
				pSession->FreeSentPacket();
				InterlockedExchange(&pSession->_sendIO, 0);
			}
		};

		return true;
	}

	unsigned __stdcall	CMMOServer::AcceptThread(void *arg)
	{
		CMMOServer *pThis = (CMMOServer*)arg;
		return pThis->Update_AcceptThread();
	}

	bool CMMOServer::Update_AcceptThread(void)
	{
		//	일어났을때 서버가 꺼져있으면 종료
		int index;
		CNetSession *pSession;
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
			//if (_curPostedAcceptEx >= _maxAcceptPool) continue;
			if (_indexQueue.Dequeue(index) == false) continue;
			pSession = _pSessionArray[index];
			pSession->_clientInfo.sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (Accept_Post(pSession) == false)
			{
				_indexQueue.Enqueue(index);
			}
		};

		return true;
	}

	unsigned __stdcall WINAPI CMMOServer::SendThread(void *arg)
	{
		CMMOServer *pThis = (CMMOServer*)arg;
		return pThis->Update_SendThread();
	}

	bool CMMOServer::Update_SendThread(void)
	{
		UINT64 count;
		UINT64 maxUser = _maxUser;
		bool * bExitSend = &_bExitSend;
		int sendSleepCount = _sendSleepCount;
		CNetSession **pSessionArray = _pSessionArray;
		__int64 ioReturn;

		//	Create Thread Profile ID
		DWORD ID = GetCurrentThreadId();
		g_profiler.CreateThreadID(ID);

		timeBeginPeriod(1);
		while (*bExitSend == false)
		{
			//EnterProfile(L"Send_Proc");
			for (count = 0; count < maxUser; ++count)
			{
				//	1. 해당 세션이 현재 Send 작업 중인지 
				//	2. 해당 세션의 모드가 AUTH / GAME 모드 인지
				//	3. Send Queue에 보낼 패킷이 있는지 

				if (InterlockedCompareExchange(&_pSessionArray[count]->_sendIO, 1, 0) == 1) continue;
				//if (InterlockedExchange(&_pSessionArray[count]->_sendIO, 1) == 1) continue;

				if (pSessionArray[count]->_mode != CNetSession::MODE_AUTH &&
					pSessionArray[count]->_mode != CNetSession::MODE_GAME && 
					pSessionArray[count]->_bLogoutFlag == true)
				{
					InterlockedExchange(&_pSessionArray[count]->_sendIO, 0);
					continue;
				}

				if (pSessionArray[count]->_sendq.IsEmpty())
				{
					InterlockedExchange(&_pSessionArray[count]->_sendIO, 0);
					continue;
				}

				Send_Post(pSessionArray[count]);
			}
			//LeaveProfile(L"Send_Proc");
			Sleep(sendSleepCount);
		}
		timeEndPeriod(1);
		return true;
	}

	unsigned __stdcall CMMOServer::MonitorThread(void *arg)
	{
		CMMOServer *pThis = (CMMOServer*)arg;
		return pThis->Update_MonitorThread();

	}

	bool CMMOServer::Update_MonitorThread(void)
	{
		bool *bExitMonitor = &_bExitMonitor;
		long prevTime = GetTickCount();
		long curTime = 0;

		LONG64 timeInterval;

		double bytes = 0;
		double availableBytes, nonpagedBytes;
		double recv, send;
		timeBeginPeriod(1);
		while (*bExitMonitor == false)
		{
			curTime = GetTickCount();
			timeInterval = curTime - prevTime;

			if (timeInterval < 1000)
			{
				Sleep(100);
				continue;
			}

			timeInterval /= 1000;

			
			_pdhCounter.UpdatePdhQueryData();

			//	process commit memory
			_pdhCounter.GetProcessStatus(bytes);
			_monitor_PrivateBytes = bytes / 1048576;

			//	Ethernet Recv, Send
			_pdhCounter.GetNetworkStatus(recv, send);
			_monitor_HardwareRecvKBytes = recv / 1024;
			_monitor_HardwareSendKBytes = send / 1024;

			//	cpu usage
			_cpuCounter.UpdateCpuTime();
			_monitor_ProcessCpuUsage = _cpuCounter.GetProcessTotalUsage();
			_monitor_HardwareCpuUsage = _cpuCounter.GetUnitTotalUsage();

			//	Nonpaged memory
			//	Available memory
			_pdhCounter.GetMemoryStatus(_monitor_HardwareAvailableMBytes, _monitor_HardwareNonpagedMBytes);

			_monitor_packetPool = cNetPacketPool::GetUsedPoolSize() + cLanPacketPool::GetUsedPoolSize();
			_monitor_AuthUpdateFPS = _monitor_AuthUpdateCounter / timeInterval;
			_monitor_GameUpdateFPS = _monitor_GameUpdateCounter / timeInterval;
			_monitor_AcceptTPS = _monitor_AcceptCounter / timeInterval;
			_monitor_GamePacketProcTPS = _monitor_GamePacketProcCounter / timeInterval;
			_monitor_AuthPacketProcTPS = _monitor_AuthPacketProcCounter / timeInterval;
			_monitor_PacketSendTPS = _monitor_PacketSendCounter / timeInterval;
			_monitor_AcceptSocketPool = _curPostedAcceptEx;
			_monitor_AcceptSocketQueue = _indexQueue.GetTotalBlockCount();

			InterlockedExchange((UINT64*)&_monitor_AuthUpdateCounter, 0);
			InterlockedExchange((UINT64*)&_monitor_GameUpdateCounter, 0);
			InterlockedExchange((UINT64*)&_monitor_AcceptCounter, 0);
			InterlockedExchange((UINT64*)&_monitor_GamePacketProcCounter, 0);
			InterlockedExchange((UINT64*)&_monitor_AuthPacketProcCounter, 0);
			InterlockedExchange((UINT64*)&_monitor_PacketSendCounter, 0);

			prevTime = curTime;
		};
		timeEndPeriod(1);
		return true;
	}

};