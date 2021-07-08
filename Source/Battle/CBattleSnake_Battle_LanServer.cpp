#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"

#include "CLanServer.h"
#include "CBattleSnake_Battle_LanServer.h"

#include "CBattleSnake_Battle_LocalMonitor.h"

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

CBattleSnake_Battle_LanServer::CBattleSnake_Battle_LanServer(CBattleSnake_Battle_MMOServer *pBattle)
{
	_curOpenRoom = 0;
	_reqSequence = 0;
	_pBattle = pBattle;

	_chatServer = { 0, };

	CBattleSnake_Battle_LocalMonitor *g_pMonitor = CBattleSnake_Battle_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this, CBattleSnake_Battle_LocalMonitor::Factor_Battle_LanServer);
}

void CBattleSnake_Battle_LanServer::OnClientJoin(UINT64 clientID)
{
	_chatServer.clientID = clientID;
}

void CBattleSnake_Battle_LanServer::OnClientLeave(UINT64 clientID) 
{
	if (clientID == _chatServer.clientID)
	{
		_chatServer = { 0, };
		_curOpenRoom = 0;
	}
}
bool CBattleSnake_Battle_LanServer::OnConnectionRequest(WCHAR *ip, short port){return true;}

void CBattleSnake_Battle_LanServer::OnRecv(UINT64 clientID, cLanPacketPool* dsPacket)
{
	if (clientID != _chatServer.clientID)
	{
		LOG(L"BattleServer_LanServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_LanServer - OnRecv: Error ClientID %lld", clientID);
		disconnect(clientID);
		return;
	}

	PacketProc(dsPacket);
}
void CBattleSnake_Battle_LanServer::OnSend(UINT64 clientID, int sendsize) {}
void CBattleSnake_Battle_LanServer::OnError(int errorcode, WCHAR *errText)
{
	errno_t err;
	FILE *fp;
	time_t timer;
	struct tm t;
	WCHAR buf[256] = { 0, };
	WCHAR logBuf[256] = { 0, };
	wsprintf(logBuf, errText);

	time(&timer);
	localtime_s(&t, &timer);
	wsprintf(buf, L"BattleLanServer_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}


///-----------------------------------------------------
///	Request from Battle Server
///-----------------------------------------------------
void CBattleSnake_Battle_LanServer::Send_ConnectToken(char *connectToken)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Chat_ConnectToken(sPacket, connectToken, req);
	sendPacket(_chatServer.clientID, sPacket);
}
void CBattleSnake_Battle_LanServer::Send_CreateRoom(int battleServerNo, int roomNo, int maxUser, char* enterToken)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Chat_CreateRoom(sPacket, battleServerNo, roomNo, maxUser, enterToken, req);
	sendPacket(_chatServer.clientID, sPacket);
}

void CBattleSnake_Battle_LanServer::Send_CloseRoom(int battleServerNo, int roomNo)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Chat_CloseRoom(sPacket, battleServerNo, roomNo, req);
	sendPacket(_chatServer.clientID, sPacket);
}

///-----------------------------------------------------
///	Packet Proc
///-----------------------------------------------------
void CBattleSnake_Battle_LanServer::PacketProc(cLanPacketPool *dsPacket)
{
	WORD type;
	
	dsPacket->Deserialize((char*)&type, sizeof(WORD));
	
	switch (type)
	{
	case en_PACKET_CHAT_BAT_REQ_SERVER_ON:
		Request_Chat_Login(dsPacket);
		break;
	case en_PACKET_CHAT_BAT_RES_CONNECT_TOKEN:
		Response_Chat_ConnectToken(dsPacket);
		break;
	case en_PACKET_CHAT_BAT_RES_CREATED_ROOM:
		Response_Chat_CreateRoom(dsPacket);
		break;
	case en_PACKET_CHAT_BAT_RES_DESTROY_ROOM:
		Response_Chat_CloseRoom(dsPacket);
		break;
	default:
		LOG(L"BattleServer_LanServer", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_LanServer - PacketProc: Error packet type");
		disconnect(_chatServer.clientID);
		break;
	};
}

void CBattleSnake_Battle_LanServer::Request_Chat_Login(cLanPacketPool *dsPacket)
{
	dsPacket->Deserialize((char*)_chatServer.IP, sizeof(WCHAR) * 16);
	dsPacket->Deserialize((char*)&_chatServer.port, sizeof(WORD));

	memcpy(_pBattle->_chatServerIP, _chatServer.IP, sizeof(WCHAR) * 16);
	_pBattle->_chatServerPort = _chatServer.port;

	Response_Chat_Login();

	//	현재 열려있는 대기방 업데이트 
	_pBattle->Receive_ChatResponse_Login();

	_chatServer.bLogin = true;
}


void CBattleSnake_Battle_LanServer::Response_Chat_ConnectToken(cLanPacketPool *dsPacket)
{
	UINT req;
	dsPacket->Deserialize((char*)&req, sizeof(UINT));
}

void CBattleSnake_Battle_LanServer::Response_Chat_CreateRoom(cLanPacketPool *dsPacket)
{
	UINT	req;
	int		roomNo;

	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&req, sizeof(UINT));
	InterlockedIncrement((long*)&_curOpenRoom);

	_pBattle->Receive_ChatRequest_CreateRoom(roomNo);
}
void CBattleSnake_Battle_LanServer::Response_Chat_CloseRoom(cLanPacketPool *dsPacket)
{
	UINT	req;
	int		roomNo;

	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&req, sizeof(UINT));
	InterlockedDecrement((long*)&_curOpenRoom);

	_pBattle->Receive_ChatRequest_CloseRoom(roomNo);
}

void CBattleSnake_Battle_LanServer::Response_Chat_Login(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Chat_Login(sPacket);
	sendPacket(_chatServer.clientID, sPacket);
}

void CBattleSnake_Battle_LanServer::SerializePacket_Chat_Login(cLanPacketPool *sPacket)
{
	WORD type = en_PACKET_CHAT_BAT_RES_SERVER_ON;
	sPacket->Serialize((char*)&type, sizeof(WORD));
}
void CBattleSnake_Battle_LanServer::SerializePacket_Chat_ConnectToken(cLanPacketPool * sPacket, char* connectToken, UINT req)
{
	WORD type = en_PACKET_CHAT_BAT_REQ_CONNECT_TOKEN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize(connectToken, sizeof(char) * 32);
	sPacket->Serialize((char*)&req, sizeof(UINT));
}
void CBattleSnake_Battle_LanServer::SerializePacket_Chat_CreateRoom(cLanPacketPool *sPacket, int battleServerNo, int roomNo, int maxUser, char* enterToken, UINT req)
{
	WORD type = en_PACKET_CHAT_BAT_REQ_CREATED_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&battleServerNo, sizeof(int));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&maxUser, sizeof(int));
	sPacket->Serialize(enterToken, sizeof(char) * 32);
	sPacket->Serialize((char*)&req, sizeof(UINT));
}
void CBattleSnake_Battle_LanServer::SerializePacket_Chat_CloseRoom(cLanPacketPool *sPacket, int battleServerNo, int roomNo, UINT req)
{
	WORD type = en_PACKET_CHAT_BAT_REQ_DESTROY_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&battleServerNo, sizeof(int));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&req, sizeof(UINT));
}
