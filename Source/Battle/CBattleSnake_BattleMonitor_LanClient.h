#pragma once

#define dfBS_MONITOR_BATTLE_ID	3
using namespace TUNJI_LIBRARY;

class CBattleSnake_Battle_MMOServer;
class CBattleSnake_BattleMonitor_LanClient : public CLanClient
{
public:
	CBattleSnake_BattleMonitor_LanClient(CBattleSnake_Battle_MMOServer* pThis);
	~CBattleSnake_BattleMonitor_LanClient() {}

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

	void Send_HardwareCPU(void);
	void Send_HardwareAvailableMemory(void);
	void Send_HardwareRecvBytes(void);
	void Send_HardwareSendBytes(void);
	void Send_HardwareNonpagedMemory(void);

	void Send_ServerStatus(void);
	void Send_ServerCPU(void);
	void Send_ServerMemory(void);
	void Send_ServerPacketPool(void);
	void Send_ServerAuthFPS(void);
	void Send_ServerGameFPS(void);
	void Send_ServerSessionAll(void);
	void Send_ServerSessionAuth(void);
	void Send_ServerSessionGame(void);
	void Send_ServerWaitRoom(void);
	void Send_ServerPlayRoom(void);

	void Packet_HardwareCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_HardwareAvailableMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_HardwareRecvBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_HardwareSendBytes(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_HardwareNonpagedMemory(cLanPacketPool *sPacket, int dataValue, int timeStamp);

	void Packet_ServerStatus(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerCPU(cLanPacketPool *sPacket, int dataValue, int timeStamp);
	void Packet_ServerMemory(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerPacketPool(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerAuthFPS(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerGameFPS(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerSessionAll(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerSessionAuth(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerSessionGame(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerWaitRoom(cLanPacketPool * sPacket, int dataValue, int timeStamp);
	void Packet_ServerPlayRoom(cLanPacketPool * sPacket, int dataValue, int timeStamp);

private:
	CBattleSnake_Battle_MMOServer*		_pBattleServer;
	int									_serverNo;
	HANDLE								_hMonitorThread;
	bool								_bExitMonitor;
};