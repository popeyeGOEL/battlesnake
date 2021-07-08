#pragma once

using namespace TUNJI_LIBRARY;

class CBattleSnake_Battle_MMOServer;
class CBattleSnake_Battle_LanClient : public CLanClient
{
public:
	CBattleSnake_Battle_LanClient(CBattleSnake_Battle_MMOServer *pThis, char *masterToken);
	virtual ~CBattleSnake_Battle_LanClient() {}

	virtual void OnEnterJoinServer(void);
	virtual void OnLeaveServer(void);
	virtual void OnRecv(cLanPacketPool *dsPacket);
	virtual void OnSend(int sendSize);
	virtual void OnError(int errorCode, WCHAR *errText);

	void Send_Master_Login(WCHAR * battleIP, WORD battlePort, char* connectToken, WCHAR *chatIP, WORD chatPort);
	void Send_Master_ReissueConnectToken(char* connectToken);
	void Send_Master_CreatedRoom(int roomNo, int maxUser, char* enterToken);
	void Send_Master_CloseRoom(int roomNo);
	void Send_Master_LeavePlayer(int roomNo, INT64 accountNo);

	bool IsLogin(void) {return _bLogin;}

private:
	void SerializePacket_Master_Login(cLanPacketPool* sPacket, WCHAR *battleIP, WORD battlePort, char *connectToken, char* masterToken,
									  WCHAR *chatIP, WORD chatPort);
	void SerializePacket_Master_ReissueConnectToken(cLanPacketPool* sPacket, char* connectToken, UINT reqSequence);
	void SerializePacket_Master_CreateRoom(cLanPacketPool* sPacket, int battleNo, int roomNo, int maxUser, char *enterToken, UINT reqSequence);
	void SerializePacket_Master_CloseRoom(cLanPacketPool* sPacket, int roomNo, UINT reqSequence);
	void SerializePacket_Master_LeavePlayer(cLanPacketPool* sPacket, int roomNo, INT64 accountNo, UINT reqSequence);

	bool PacketProc(cLanPacketPool* dsPacket);
	void Response_Master_Login(cLanPacketPool* dsPacket);
	void Response_Master_ReissueConnectToken(cLanPacketPool* dsPacket);
	void Response_Master_CreateRoom(cLanPacketPool *dsPacket);
	void Response_Master_CloseRoom(cLanPacketPool *dsPacket);
	void Response_Master_LeavePlayer(cLanPacketPool *dsPacket);

private:
	bool							_bLogin;
	CBattleSnake_Battle_MMOServer*	_pBattleServer;
	char							_masterToken[32];
	int								_battleNo;
	UINT							_reqSequence;
};