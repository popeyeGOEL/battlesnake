#pragma once

using namespace TUNJI_LIBRARY;

class CBattleSnake_Battle_MMOServer;
class CBattleSnake_Battle_LanServer : public CLanServer
{
public:
	CBattleSnake_Battle_LanServer(CBattleSnake_Battle_MMOServer *pBattle);
	virtual void OnClientJoin(UINT64 clientID);
	virtual void OnClientLeave(UINT64 clientID);
	virtual bool OnConnectionRequest(WCHAR *ip, short port);
	virtual void OnRecv(UINT64 clientID, cLanPacketPool* dsPacket);
	virtual void OnSend(UINT64 clientID, int sendsize);
	virtual void OnError(int errorcode, WCHAR *errText);
	
public:
	///-----------------------------------------------------
	///	Request from Battle Server
	///-----------------------------------------------------
	void Send_ConnectToken(char *connectToken);
	void Send_CreateRoom(int battleServerNo, int roomNo, int maxUser, char* enterToken);
	void Send_CloseRoom(int battleServerNo, int roomNo);

private:
	struct st_CHAT
	{
		bool	bLogin;
		UINT64	clientID;
		WCHAR	IP[16];
		short	port;
	};

private:
	///-----------------------------------------------------
	///	Packet Proc
	///-----------------------------------------------------
	void PacketProc(cLanPacketPool *dsPacket);
	void Request_Chat_Login(cLanPacketPool *dsPacket);

	void Response_Chat_Login(void);
	void Response_Chat_ConnectToken(cLanPacketPool *dsPacket);
	void Response_Chat_CreateRoom(cLanPacketPool *dsPacket);
	void Response_Chat_CloseRoom(cLanPacketPool *dsPacket);

	void SerializePacket_Chat_Login(cLanPacketPool *sPacket);
	void SerializePacket_Chat_ConnectToken(cLanPacketPool * sPacket, char* connectToken, UINT req);
	void SerializePacket_Chat_CreateRoom(cLanPacketPool *sPacket, int battleServerNo, int roomNo, int maxUser, char* enterToken, UINT req);
	void SerializePacket_Chat_CloseRoom(cLanPacketPool *sPacket, int battleServerNo, int roomNo, UINT req);

private:
	CBattleSnake_Battle_MMOServer *_pBattle;
	st_CHAT	_chatServer;
	UINT	_reqSequence;

public:
	bool	IsLogin(void) {return _chatServer.bLogin;}
	int		_curOpenRoom;;
};