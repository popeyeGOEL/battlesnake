#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"
#include "CLanClient.h"
#include "CBattleSnake_Battle_LanClient.h"

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

CBattleSnake_Battle_LanClient::CBattleSnake_Battle_LanClient(CBattleSnake_Battle_MMOServer *pThis, char *masterToken)
{
	_pBattleServer = pThis;
	memcpy(_masterToken, masterToken, sizeof(char) * 32);
	_bLogin = false;
}

void CBattleSnake_Battle_LanClient::OnEnterJoinServer(void) 
{
	Send_Master_Login(_pBattleServer->_accessIP, _pBattleServer->_port, _pBattleServer->_connectToken, _pBattleServer->_chatServerIP, _pBattleServer->_chatServerPort);
}

void CBattleSnake_Battle_LanClient::OnLeaveServer(void) 
{
	_bLogin = false;
}

void CBattleSnake_Battle_LanClient::OnRecv(cLanPacketPool *dsPacket)
{
	if (!PacketProc(dsPacket))
	{
		Disconnect();
		LOG(L"BattleServer_LanClient", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_LanClient - OnRecv : Packet Proc Error");
	}
}
void CBattleSnake_Battle_LanClient::OnSend(int sendSize) {}
void CBattleSnake_Battle_LanClient::OnError(int errorCode, WCHAR *errText)
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
	wsprintf(buf, L"CBattleSnake_Battle_LanClient_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

void CBattleSnake_Battle_LanClient::Send_Master_Login(WCHAR * battleIP, WORD battlePort, char* connectToken, WCHAR *chatIP, WORD chatPort)
{
	cLanPacketPool* sPacket = cLanPacketPool::Alloc();
	SerializePacket_Master_Login(sPacket, battleIP, battlePort, connectToken, _masterToken, chatIP, chatPort);
	SendPacket(sPacket);
}
void CBattleSnake_Battle_LanClient::Send_Master_ReissueConnectToken(char* connectToken)
{
	cLanPacketPool* sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Master_ReissueConnectToken(sPacket, connectToken, req);
	SendPacket(sPacket);
}
void CBattleSnake_Battle_LanClient::Send_Master_CreatedRoom(int roomNo, int maxUser, char* enterToken)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Master_CreateRoom(sPacket, _battleNo, roomNo, maxUser, enterToken, req);
	SendPacket(sPacket);
}

void CBattleSnake_Battle_LanClient::Send_Master_CloseRoom(int roomNo)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Master_CloseRoom(sPacket, roomNo, req);
	SendPacket(sPacket);
}

void CBattleSnake_Battle_LanClient::Send_Master_LeavePlayer(int roomNo, INT64 accountNo)
{
	cLanPacketPool* sPacket = cLanPacketPool::Alloc();
	UINT req = InterlockedIncrement(&_reqSequence);
	SerializePacket_Master_LeavePlayer(sPacket, roomNo, accountNo, req);
	SendPacket(sPacket);
}

void CBattleSnake_Battle_LanClient::SerializePacket_Master_Login(cLanPacketPool* sPacket, WCHAR *battleIP, WORD battlePort, char *connectToken, char* masterToken,
																 WCHAR *chatIP, WORD chatPort)
{
	WORD type = en_PACKET_BAT_MAS_REQ_SERVER_ON;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)battleIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&battlePort, sizeof(WORD));
	sPacket->Serialize(connectToken, sizeof(char) * 32);
	sPacket->Serialize(masterToken, sizeof(char) * 32);
	sPacket->Serialize((char*)chatIP, sizeof(WCHAR) * 16);
	sPacket->Serialize((char*)&chatPort, sizeof(WORD));
}
void CBattleSnake_Battle_LanClient::SerializePacket_Master_ReissueConnectToken(cLanPacketPool* sPacket, char* connectToken, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_REQ_CONNECT_TOKEN;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize(connectToken, sizeof(char) * 32);
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}
void CBattleSnake_Battle_LanClient::SerializePacket_Master_CreateRoom(cLanPacketPool* sPacket, int battleNo, int roomNo, int maxUser, char *enterToken, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_REQ_CREATED_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&battleNo, sizeof(int));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&maxUser, sizeof(int));
	sPacket->Serialize(enterToken, sizeof(char) * 32);
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}
void CBattleSnake_Battle_LanClient::SerializePacket_Master_CloseRoom(cLanPacketPool* sPacket, int roomNo, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_REQ_CLOSED_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}
void CBattleSnake_Battle_LanClient::SerializePacket_Master_LeavePlayer(cLanPacketPool* sPacket, int roomNo, INT64 accountNo, UINT reqSequence)
{
	WORD type = en_PACKET_BAT_MAS_REQ_LEFT_USER;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&accountNo, sizeof(INT64));
	sPacket->Serialize((char*)&reqSequence, sizeof(UINT));
}

bool CBattleSnake_Battle_LanClient::PacketProc(cLanPacketPool* dsPacket)
{
	WORD type;
	dsPacket->Deserialize((char*)&type, sizeof(WORD));

	switch (type)
	{
	case en_PACKET_BAT_MAS_RES_SERVER_ON:
		Response_Master_Login(dsPacket);
		break;
	case en_PACKET_BAT_MAS_RES_CONNECT_TOKEN:
		Response_Master_ReissueConnectToken(dsPacket);
		break;
	case en_PACKET_BAT_MAS_RES_CREATED_ROOM:
		Response_Master_CreateRoom(dsPacket);
		break;
	case en_PACKET_BAT_MAS_RES_CLOSED_ROOM:
		Response_Master_CloseRoom(dsPacket);
		break;
	case en_PACKET_BAT_MAS_RES_LEFT_USER:
		Response_Master_LeavePlayer(dsPacket);
		break;
	default:
		return false;
	};

	return true;
}

void CBattleSnake_Battle_LanClient::Response_Master_Login(cLanPacketPool* dsPacket)
{
	dsPacket->Deserialize((char*)&_battleNo, sizeof(int));
	_pBattleServer->_battleNo = _battleNo;

	//	현재 열려있는 대기방 업데이트 
	_pBattleServer->Receive_MasterRequest_Login();

	_bLogin = true;
}

void CBattleSnake_Battle_LanClient::Response_Master_ReissueConnectToken(cLanPacketPool* dsPacket)
{
	UINT seq;
	dsPacket->Deserialize((char*)&seq, sizeof(UINT));
}

void CBattleSnake_Battle_LanClient::Response_Master_CreateRoom(cLanPacketPool *dsPacket)
{
	int roomNo;
	UINT seq;
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&seq, sizeof(UINT));

	_pBattleServer->Receive_MasterRequest_CreateRoom(roomNo);
}

void CBattleSnake_Battle_LanClient::Response_Master_CloseRoom(cLanPacketPool *dsPacket)
{
	int roomNo;
	UINT seq;
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&seq, sizeof(UINT));

	_pBattleServer->Receive_MasterRequest_CloseRoom(roomNo);
}
void CBattleSnake_Battle_LanClient::Response_Master_LeavePlayer(cLanPacketPool *dsPacket)
{
	int roomNo;
	UINT seq;

	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize((char*)&seq, sizeof(UINT));

	_pBattleServer->Receive_MasterRequest_LeavePlayer(roomNo);
}