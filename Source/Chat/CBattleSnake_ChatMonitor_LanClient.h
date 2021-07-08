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

	//dfMONITOR_DATA_TYPE_CHAT_SERVER_ON,                         // ä�ü��� ON
	//	dfMONITOR_DATA_TYPE_CHAT_CPU,                               // ä�ü��� CPU ���� (Ŀ�� + ����)
	//	dfMONITOR_DATA_TYPE_CHAT_MEMORY_COMMIT,                     // ä�ü��� �޸� ���� Ŀ�� ��뷮 (Private) MByte
	//	dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL,                       // ä�ü��� ��ŶǮ ��뷮
	//	dfMONITOR_DATA_TYPE_CHAT_SESSION,                           // ä�ü��� ���� ������ü
	//	dfMONITOR_DATA_TYPE_CHAT_PLAYER,                            // ä�ü��� �α����� ������ ��ü �ο�
	//	dfMONITOR_DATA_TYPE_CHAT_ROOM,                               // ��Ʋ���� �� ��
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