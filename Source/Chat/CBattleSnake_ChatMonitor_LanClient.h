#pragma once

#define dfBS_MONITOR_CHAT_ID 4
using namespace TUNJI_LIBRARY;

class CBattleSnake_Chat_NetServer;
class CBattleSnake_ChatMonitor_LanClient : public CLanClient
{
public:
	CBattleSnake_ChatMonitor_LanClient(CBattleSnake_Chat_NetServer* pThis);
	~CBattleSnake_ChatMonitor_LanClient() {}

	virtual void OnEnterJoinServer(void);
	virtual void OnLeaveServer(void);
	virtual void OnRecv(cLanPacketPool *dsPacket) {}
	virtual void OnSend(int sendSize) {}
	virtual void OnError(int errorCode, WCHAR *errText) {}

	void StartMonitorClient(void);
	void StopMonitorClient(void);

	//dfMONITOR_DATA_TYPE_CHAT_SERVER_ON,                         // 채팅서버 ON
	//	dfMONITOR_DATA_TYPE_CHAT_CPU,                               // 채팅서버 CPU 사용률 (커널 + 유저)
	//	dfMONITOR_DATA_TYPE_CHAT_MEMORY_COMMIT,                     // 채팅서버 메모리 유저 커밋 사용량 (Private) MByte
	//	dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL,                       // 채팅서버 패킷풀 사용량
	//	dfMONITOR_DATA_TYPE_CHAT_SESSION,                           // 채팅서버 접속 세션전체
	//	dfMONITOR_DATA_TYPE_CHAT_PLAYER,                            // 채팅서버 로그인을 성공한 전체 인원
	//	dfMONITOR_DATA_TYPE_CHAT_ROOM,                               // 배틀서버 방 수
private:
	static unsigned __stdcall MonitorThread(void *arg);
	unsigned int Update_MonitorThread(void);

	void RequestLogin(void);

	void Send_ServerStatus(void);
	void Send_ServerCPU(void);
	void Send_ServerMemory(void);
	void Send_ServerPacketPool(void);
	void Send_ServerSession(void);
	void Send_ServerPlayer(void);
	void Send_ServerChatRoom(void);

	void Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerProcessCPU(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerMemory(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerSession(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerPlayer(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerChatRoom(cLanPacketPool* sPacket, int dataValue, int timeStamp);

private:
	CBattleSnake_Chat_NetServer*	_pChatServer;
	int								_serverNo;
	HANDLE							_hMonitorThread;
	bool							_bExitMonitor;
};