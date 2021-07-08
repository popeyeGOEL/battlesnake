#pragma once

using namespace TUNJI_LIBRARY;

class CBattleSnake_Match_NetServer;
class CBattleSnake_Match_LanClient : public CLanClient
{
public:
	CBattleSnake_Match_LanClient(CBattleSnake_Match_NetServer *pThis, int serverNo, char *masterToken);
	virtual ~CBattleSnake_Match_LanClient() {}

	virtual void OnEnterJoinServer(void);
	virtual void OnLeaveServer(void);
	virtual void OnRecv(cLanPacketPool *dsPacket);
	virtual void OnSend(int sendSize);
	virtual void OnError(int errorCode, WCHAR *errText);

	void Send_Master_Login(void);
	void Send_Master_Roominfo(UINT64 clientKey, UINT64 accountNo);
	void Send_Master_EnterRoomSuccess(WORD battleServerNo, int roomNo, UINT64 clientKey);
	void Send_Master_EnterRoomFail(UINT64 clientKey);

private:
	void SerializePacket_Master_Login(cLanPacketPool *sPacket, int serverNo, char *masterToken);
	void SerializePacket_Master_RoomInfo(cLanPacketPool *sPacket, UINT64 clientKey, UINT64 accountNo);
	void SerializePacket_Master_EnterRoomSuccess(cLanPacketPool *sPacket, WORD battleServerNo, int roomNo, UINT64 clientKey);
	void SerializePacket_Master_EnterRoomFail(cLanPacketPool *sPacket, UINT64 clientKey);

	bool PacketProc(cLanPacketPool* dsPacket);
	bool Response_Master_Login(cLanPacketPool *dsPacket);
	bool Response_Master_RoomInfo(cLanPacketPool *dsPacket);
	bool Response_Master_RoomEnter(cLanPacketPool *dsPacket);
	
private:
	CBattleSnake_Match_NetServer*	_pMatchNetServer;
	int								_serverNo;
	char							_masterToken[32];
};