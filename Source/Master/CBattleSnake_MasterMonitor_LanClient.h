#pragma once

#define dfBS_MONITOR_MASTER_ID	1
using namespace TUNJI_LIBRARY;
class CBattleSnake_Master_LanServer;
class CBattleSnake_MasterMonitor_LanClient : public CLanClient
{
public:
	CBattleSnake_MasterMonitor_LanClient(CBattleSnake_Master_LanServer *pThis);
	~CBattleSnake_MasterMonitor_LanClient() {}

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

	void Send_ServerStatus(void);
	void Send_ServerProcessCPU(void);
	void Send_ServerUnitCPU(void);
	void Send_ServerMemory(void);
	void Send_ServerPacketPool(void);
	void Send_ServerMatchConnect(void);
	void Send_ServerMatchLogin(void);
	void Send_ServerWaitClient(void);
	void Send_ServerBattleConnect(void);
	void Send_ServerBattleLogin(void);
	void Send_ServerWaitRoom(void);

	void Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerProcessCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerUnitCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerPacketPool(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerMatchConnect(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerMatchLogin(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerWaitClient(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerBattleConnect(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerBattleLogin(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerWaitRoom(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	
private:
	CBattleSnake_Master_LanServer*	_pMasterServer;
	int								_serverNo;

	HANDLE							_hMonitorThread;
	bool							_bExitMonitor;
};