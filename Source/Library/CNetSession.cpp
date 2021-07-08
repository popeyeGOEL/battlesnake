#include <WS2tcpip.h>
#include "CNetSession.h"
#include "cCrashDump.h"

namespace TUNJI_LIBRARY
{
	CNetSession::CNetSession() {};

	CNetSession::~CNetSession()
	{
		InitBuffer();
	};

	void CNetSession::SetPacketCode(BYTE packetCode, BYTE packetKey1, BYTE packetKey2)
	{
		_packetCode = packetCode;
		_packetKey1 = packetKey1;
		_packetKey2 = packetKey2;
	}

	void CNetSession::SetMode_GAME(void)
	{
		InterlockedExchange8((char*)&_bAuthToGameFlag, 1);
	}

	void CNetSession::SendPacket(cNetPacketPool *sPacket)
	{
		do
		{
			if (InterlockedIncrement(&_ioCount) == 1) break;
			if (_bLogoutFlag == true) break;
			if (_bShutdown == true) break;

			//	해당 세션에게 패킷을 보낸다
			sPacket->Encode(_packetCode, _packetKey1, _packetKey2);
			_sendq.Enqueue(sPacket);

			if (InterlockedDecrement(&_ioCount) == 0)
			{
				_bLogoutFlag = true;
			}

			return;
		} while (0);

		cNetPacketPool::Free(sPacket);
		
		if (InterlockedDecrement(&_ioCount) == 0)
		{
			_bLogoutFlag = true;
		}
	}

	void CNetSession::SendLastPacket(cNetPacketPool *sPacket)
	{
		do
		{
			if (InterlockedIncrement(&_ioCount) == 1) break;
			if (_bLogoutFlag == true) break;
			if (_bShutdown == true) break;

			//	해당 세션에게 패킷을 보낸다
			InterlockedExchange8((char*)&_bLastPacket, 1);
			sPacket->Encode(_packetCode, _packetKey1, _packetKey2);
			_sendq.Enqueue(sPacket);

			if (InterlockedDecrement(&_ioCount) == 0)
			{
				_bLogoutFlag = true;
			}

			return;
		} while (0);

		cNetPacketPool::Free(sPacket);

		if (InterlockedDecrement(&_ioCount) == 0)
		{
			_bLogoutFlag = true;
		}
	}

	void CNetSession::Disconnect(void)
	{
		//	해당 세션을 끊는다
		do
		{
			if (InterlockedIncrement(&_ioCount) == 1) break;
			if (_bLogoutFlag == true) break;

			SetIoTracker(CNetSession::IO_COUNT_TYPE::DISCONNECT, _ioCount, nullptr);
			_bShutdown = true;
			shutdown(_clientInfo.sock, SD_BOTH);

			if (InterlockedDecrement(&_ioCount) == 0)
			{
				_bLogoutFlag = true;
			}

			return;
		} while (0);

		if (InterlockedDecrement(&_ioCount) == 0)
		{
			_bLogoutFlag = true;
		}
	}

	void CNetSession::InitSession(void)
	{
		_mode = MODE_NONE;
		SetModeTracker(MODE_NONE);

		_clientInfo.clientID = 0;
	
		InitBuffer();
	}

	void CNetSession::InitBuffer(void)
	{
		FreeCompletePacket();
		FreeSendQueuePacket();
		FreeSentPacket();
		_recvq.ClearBuffer();
	}

	void CNetSession::FreeSentPacket(void)
	{
		int sendSize = InterlockedExchange(&_sendPacketSize, 0);

		if (sendSize == 0) return;

		for (int i = 0; i < sendSize; ++i)
		{
			cNetPacketPool::Free(_packetBuffer[i]);
		}
	}

	void CNetSession::FreeSendQueuePacket(void)
	{
		cNetPacketPool *pPacket = nullptr;

		while (!_sendq.IsEmpty())
		{
			_sendq.Dequeue(pPacket);
			if (pPacket == nullptr) continue;
			cNetPacketPool::Free(pPacket);
		}
	}

	void CNetSession::FreeCompletePacket(void)
	{
		cNetPacketPool *pPacket = nullptr;

		while (!_completePacket.isEmpty())
		{
			_completePacket.Dequeue(pPacket);
			if (pPacket == nullptr) continue;
			cNetPacketPool::Free(pPacket);
		}
	}

	void CNetSession::SetModeTracker(en_SESSION_MODE mode)
	{
		UINT64 ret = InterlockedIncrement(&_modeTrackerCount) % 100;
		_modeTracker[ret].mode = mode;
		_modeTracker[ret].curTime = timeGetTime();
	}

	void CNetSession::SetIoTracker(IO_COUNT_TYPE type, LONG64 count, OVERLAPPED * ol)
	{
		UINT64 ret = InterlockedIncrement(&_ioTrackerCount) % 200;
		_ioTracker[ret].ioCount = count;
		_ioTracker[ret].type = type;
		_ioTracker[ret].curTime = timeGetTime();

		if (ol == nullptr)
		{
			_ioTracker[ret].overlapType = CNetSession::NONE;
		}
		else if (ol == &_sendOl)
		{
			_ioTracker[ret].overlapType = CNetSession::SEND;
		}
		else if (ol == &_recvOl)
		{
			_ioTracker[ret].overlapType = CNetSession::RECV;
		}
		else
		{
			_ioTracker[ret].overlapType = CNetSession::ACCEPT;
		}
	}
};