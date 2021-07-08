#include "CNetServer.h"
#include "CBattleSnake_Chat_NetServer.h"

#include "CLanClient.h"
#include "CBattleSnake_Chat_LanClient.h"

#include <time.h>

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"


CBattleSnake_Chat_LanClient::CBattleSnake_Chat_LanClient(CBattleSnake_Chat_NetServer *pChat)
{
	_pChatServer = pChat;
	_bLogin = false;
}
void CBattleSnake_Chat_LanClient::OnEnterJoinServer(void)
{
	Send_Battle_Login(_pChatServer->_accessIP, _pChatServer->_serverPort);
}
void CBattleSnake_Chat_LanClient::OnLeaveServer(void)
{
	_pChatServer->Receive_BattleRequest_Logout();
	_bLogin = false;
}

void CBattleSnake_Chat_LanClient::OnRecv(cLanPacketPool *dsPacket)
{
	if (PacketProc(dsPacket) == false)
	{
		Disconnect();
	}
}

void CBattleSnake_Chat_LanClient::OnError(int errorCode, WCHAR *errText)
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
	wsprintf(buf, L"ChatLanClient_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;

	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

void CBattleSnake_Chat_LanClient::Send_Battle_Login(WCHAR * chatIP, WORD chatPort)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_Login(sPacket, chatIP, chatPort);
	SendPacket(sPacket);
}


void CBattleSnake_Chat_LanClient::Response_Battle_ReissueConnectToken(UINT req)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_ReissueConnectToken(sPacket, req);
	SendPacket(sPacket);
}
void CBattleSnake_Chat_LanClient::Response_Battle_CreateRoom(int roomNo, UINT req)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_CreateRoom(sPacket, roomNo, req);
	SendPacket(sPacket);
}
void CBattleSnake_Chat_LanClient::Response_Battle_CloseRoom(int roomNo, UINT req)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Battle_DeleteRoom(sPacket, roomNo, req);
	SendPacket(sPacket);
}


void CBattleSnake_Chat_LanClient::SerializePacket_Battle_Login(cLanPacketPool *sPacket, WCHAR *chatIP, WORD chatPort)
{
	WORD type = en_PACKET_CHAT_BAT_REQ_SERVER_ON;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)chatIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&chatPort, sizeof(WORD));
}
void CBattleSnake_Chat_LanClient::SerializePacket_Battle_ReissueConnectToken(cLanPacketPool *sPacket, UINT req)
{
	WORD type = en_PACKET_CHAT_BAT_RES_CONNECT_TOKEN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&req, sizeof(UINT));
}
void CBattleSnake_Chat_LanClient::SerializePacket_Battle_CreateRoom(cLanPacketPool *sPacket, int roomNo, UINT req)
{
	WORD type = en_PACKET_CHAT_BAT_RES_CREATED_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&req, sizeof(UINT));
}
void CBattleSnake_Chat_LanClient::SerializePacket_Battle_DeleteRoom(cLanPacketPool *sPacket, int roomNo, UINT req)
{
	WORD type = en_PACKET_CHAT_BAT_RES_DESTROY_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&req, sizeof(UINT));
}

bool CBattleSnake_Chat_LanClient::PacketProc(cLanPacketPool *dsPacket)
{
	WORD type;
	dsPacket->Deserialize((char*)&type, sizeof(WORD));

	switch (type)
	{
	case en_PACKET_CHAT_BAT_RES_SERVER_ON:
		Response_Battle_Login(dsPacket);
		break;
	case en_PACKET_CHAT_BAT_REQ_CONNECT_TOKEN:
		Request_Battle_ReissueConnectToken(dsPacket);
		break;
	case en_PACKET_CHAT_BAT_REQ_CREATED_ROOM:
		Request_Battle_CreateRoom(dsPacket);
		break;
	case en_PACKET_CHAT_BAT_REQ_DESTROY_ROOM:
		Request_Battle_CloseRoom(dsPacket);
		break;
	default:
		LOG(L"ChatLanClient", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Chat_LanClient - PacketProc : type error");
		return false;
	};

	return true;
}
void CBattleSnake_Chat_LanClient::Response_Battle_Login(cLanPacketPool *dsPacket)
{
	_bLogin = true;
}
void CBattleSnake_Chat_LanClient::Request_Battle_ReissueConnectToken(cLanPacketPool *dsPacket)
{
	_pChatServer->Receive_BattleRequest_ReissueConnectToken(dsPacket);
}
void CBattleSnake_Chat_LanClient::Request_Battle_CreateRoom(cLanPacketPool *dsPacket)
{
	_pChatServer->Receive_BattleRequest_CreateRoom(dsPacket);
}
void CBattleSnake_Chat_LanClient::Request_Battle_CloseRoom(cLanPacketPool *dsPacket)
{
	_pChatServer->Receive_BattleRequest_DeleteRoom(dsPacket);
}