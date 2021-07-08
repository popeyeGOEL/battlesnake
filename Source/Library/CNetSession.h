#pragma once
#include "cRingBuffer.h"
#include "cLockFreeQueue.h"
#include "cQueue.h"
#include "cPacketPool_NetServer.h"

namespace TUNJI_LIBRARY
{
	class CNetSession
	{
	public:
		enum en_SESSION_MODE
		{
			MODE_NONE,				//	세션 미사용 상태 
			MODE_AUTH,				//	연결 후 인증 모드 상태 
			MODE_AUTH_TO_GAME,		//	인증 모드에서 게임 모드로 전환 
			MODE_GAME,				//	인증 후 게임 모드 상태 
			MODE_LOGOUT_IN_AUTH,	//	인증 모드에서 종료 요청 상태 -> MODE_WAIT_LOGOUT 으로 변경 
			MODE_LOGOUT_IN_GAME,	//	게임 모드에서 종료 요청 상태 -> MODE_WAIT_LOGOUT 으로 변경 
			MODE_WAIT_LOGOUT		//	최종 종료 요청 상태 -> MODE_NONE 으로 변경 
		};

		enum IO_COUNT_TYPE
		{
			DEC_WORKER = 1,
			INC_ACCEPT_POST, DEC_ACCEPT_POST,
			INC_SEND_POST, DEC_SEND_POST,
			INC_RECV_POST, DEC_RECV_POST,
			INC_SESSION_LOCK, DEC_SESSION_LOCK, DEC_SESSION_UNLOCK,
			INC_TRANSMIT, DEC_TRANSMIT,
			RECV_ZERO, DISCONNECT, RELEASE
		};

		enum OVERLAPED_TYPE
		{
			NONE = 0,
			SEND,
			RECV,
			ACCEPT,
		};

		struct st_IO_INFO
		{
			IO_COUNT_TYPE type;
			LONG64	ioCount;
			OVERLAPED_TYPE overlapType;
			DWORD	curTime;
		};

		struct st_MODE_INFO
		{
			en_SESSION_MODE	mode;
			DWORD	curTime;
		};

	public:
		CNetSession();
		virtual ~CNetSession();


		virtual void OnAuth_ClientJoin(void) = 0;
		virtual void OnAuth_ClientLeave(void) = 0;
		virtual void OnAuth_Packet(cNetPacketPool* dsPacket) = 0;

		virtual void OnGame_ClientJoin(void) = 0;
		virtual void OnGame_ClientLeave(void) = 0;
		virtual void OnGame_Packet(cNetPacketPool * dsPacket) = 0;
		virtual void OnGame_ClientRelease(void) = 0;


		void SetPacketCode(BYTE packetCode, BYTE packetKey1, BYTE packetKey2);
		void SetMode_GAME(void);
		void SendPacket(cNetPacketPool *sPacket);
		void SendLastPacket(cNetPacketPool *sPacket);
		void Disconnect(void);

		void InitSession(void);
		void InitBuffer(void);
		void FreeSentPacket(void);
		void FreeSendQueuePacket(void);
		void FreeCompletePacket(void);

	public:

		void SetIoTracker(IO_COUNT_TYPE type, LONG64 count, OVERLAPPED *ol);
		void SetModeTracker(en_SESSION_MODE mode);

		struct st_ACCEPT_OVERLAP : public OVERLAPPED
		{
			int		arrayIndex;							//	세션 배열의 자기 인덱스 
		};

		struct st_CLIENT_INFO
		{
			UINT64		clientID;
			SOCKET		sock;
			SOCKADDR_IN	addr;
			WCHAR		IP[16];
			int			port;
		};

		BYTE							_packetCode;
		BYTE							_packetKey1;
		BYTE							_packetKey2;

		en_SESSION_MODE					_mode;			//	세션의 상태 모드 
		st_MODE_INFO					_modeTracker[100];
		st_IO_INFO						_ioTracker[200];
		UINT64							_modeTrackerCount;
		UINT64							_ioTrackerCount;

		st_CLIENT_INFO					_clientInfo;
		
		cRingBuffer						_recvq;
		cQueue<cNetPacketPool*>			_completePacket;

		cLockFreeQueue<cNetPacketPool*>	_sendq;
		cNetPacketPool*					_packetBuffer[100];
		UINT64							_sendPacketSize;

		st_ACCEPT_OVERLAP				_acceptOl;
		OVERLAPPED						_recvOl;
		OVERLAPPED						_sendOl;

		LONG							_sendIO;
		LONG							_ioCount;

		bool							_bDisconnect;
		bool							_bLastPacket;
		bool							_bLogoutFlag;
		bool							_bShutdown;
		bool							_bAuthToGameFlag;

	};

};