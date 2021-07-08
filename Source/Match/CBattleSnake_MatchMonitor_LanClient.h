#pragma once

#define dfBS_MONTIOR_MATCH_ID	2
using namespace TUNJI_LIBRARY;

class CBattleSnake_Match_NetServer;
class CBattleSnake_MatchMonitor_LanClient : public CLanClient
{
public:
	CBattleSnake_MatchMonitor_LanClient(CBattleSnake_Match_NetServer* pThis);
	~CBattleSnake_MatchMonitor_LanClient() {}

	virtual void OnEnterJoinServer(void);
	virtual void OnLeaveServer(void);
	virtual void OnRecv(cLanPacketPool *dsPacket);
	virtual void OnSend(int sendSize);
	virtual void OnError(int errorCode, WCHAR *errText);

	void StartMonitorClient(void);
	void StopMonitorClient(void);

private:
	static unsigned __stdcall MonitorThread(void *arg);
	unsigned int Update_MonitorThread(void);

	void RequestLogin(void);

	//void Send_HardwareCPU(void);
	//void Send_HardwareAvailableMemory(void);
	//void Send_HardwareRecvBytes(void);
	//void Send_HardwareSendBytes(void);
	//void Send_HardwareNonpagedMemory(void);

	void Send_ServerStatus(void);
	void Send_ServerProcessCPU(void);
	void Send_ServerMemory(void);
	void Send_ServerPacketPool(void);
	void Send_ServerSession(void);
	void Send_ServerPlayer(void);
	void Send_ServerMatchTPS(void);

	//void Packet_HardwareCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	//void Packet_HardwareAvailableMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	//void Packet_HardwareRecvBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	//void Packet_HardwareSendBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	//void Packet_HardwareNonpagedMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp);

	void Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerProcessCPU(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerMemory(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerSession(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerPlayer(cLanPacketPool* sPacket, int dataValue, int timeStamp);
	void Packet_ServerMatchTPS(cLanPacketPool *sPacket, int dataValue, int timeStamp);

private:
	CBattleSnake_Match_NetServer*		_pMatchNetServer;
	int									_serverNo;
	HANDLE								_hMonitorThread;
	bool								_bExitMonitor;
};