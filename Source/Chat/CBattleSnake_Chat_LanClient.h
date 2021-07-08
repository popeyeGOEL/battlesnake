#pragma once
#include <map>
#include "cPacketPool_LanServer.h"

using namespace TUNJI_LIBRARY;

class CBattleSnake_Chat_NetServer;
class CBattleSnake_Chat_LanClient : public CLanClient
{
public:
	CBattleSnake_Chat_LanClient(CBattleSnake_Chat_NetServer *pChat);
	~CBattleSnake_Chat_LanClient() {}

	virtual void OnEnterJoinServer(void);
	virtual void OnLeaveServer(void);
	virtual void OnRecv(cLanPacketPool *dsPacket);
	virtual void OnSend(int sendSize) {}
	virtual void OnError(int errorCode, WCHAR *errText);

	bool IsLogin(void) {return _bLogin;}
	void Send_Battle_Login(WCHAR * chatIP, WORD chatPort);
	
	void Response_Battle_ReissueConnectToken(UINT req);
	void Response_Battle_CreateRoom(int roomNo, UINT req);
	void Response_Battle_CloseRoom(int roomNo, UINT req);

private:
	bool PacketProc(cLanPacketPool *dsPacket);
	void Response_Battle_Login(cLanPacketPool *dsPacket);
	void Request_Battle_ReissueConnectToken(cLanPacketPool *dsPacket);
	void Request_Battle_CreateRoom(cLanPacketPool *dsPacket);
	void Request_Battle_CloseRoom(cLanPacketPool *dsPacket);

	void SerializePacket_Battle_Login(cLanPacketPool *sPacket, WCHAR *chatIP, WORD chatPort);
	void SerializePacket_Battle_ReissueConnectToken(cLanPacketPool *sPacket, UINT req);
	void SerializePacket_Battle_CreateRoom(cLanPacketPool *sPacket, int roomNo, UINT req);
	void SerializePacket_Battle_DeleteRoom(cLanPacketPool *sPacket, int roomNo, UINT req);

private:
	bool							_bLogin;
	CBattleSnake_Chat_NetServer*	_pChatServer;
};