#include "CNetServer.h"
#include "CBattleSnake_Match_NetServer.h"
#include "CLanClient.h"
#include "CBattleSnake_Match_LanClient.h"

#include "CommonProtocol.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

CBattleSnake_Match_LanClient::CBattleSnake_Match_LanClient(CBattleSnake_Match_NetServer *pThis, int serverNo, char *masterToken)
{
	_pMatchNetServer = pThis;
	_serverNo = serverNo;
	memcpy(_masterToken, masterToken, sizeof(char) * 32);
}

void CBattleSnake_Match_LanClient::OnEnterJoinServer(void) 
{
	Send_Master_Login();
}

void CBattleSnake_Match_LanClient::OnLeaveServer(void)
{
	// 사용안함 
}

void CBattleSnake_Match_LanClient::OnRecv(cLanPacketPool *dsPacket)
{
	if (!PacketProc(dsPacket))
	{
		Disconnect();
		LOG(L"MatchServer_Lan", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_LanClient - OnRecv : Packet Proc Error");
	}
}

void CBattleSnake_Match_LanClient::OnSend(int sendSize)
{
	//	사용 안함 
}
void CBattleSnake_Match_LanClient::OnError(int errorCode, WCHAR *errText)
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
	wsprintf(buf, L"CBattleSnake_Match_LanClient_Log_%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	err = _wfopen_s(&fp, buf, L"at");
	if (err != 0) return;
	fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	fwprintf_s(fp, logBuf);
	fclose(fp);
}

void CBattleSnake_Match_LanClient::Send_Master_Login(void)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Master_Login(sPacket, _serverNo, _masterToken);
	SendPacket(sPacket);
}

void CBattleSnake_Match_LanClient::Send_Master_Roominfo(UINT64 clientKey, UINT64 accountNo)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Master_RoomInfo(sPacket, clientKey, accountNo);
	SendPacket(sPacket);
}
void CBattleSnake_Match_LanClient::Send_Master_EnterRoomSuccess(WORD battleServerNo, int roomNo, UINT64 clientKey)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Master_EnterRoomSuccess(sPacket, battleServerNo, roomNo, clientKey);
	SendPacket(sPacket);
}

void CBattleSnake_Match_LanClient::Send_Master_EnterRoomFail(UINT64 clientKey)
{
	cLanPacketPool *sPacket = cLanPacketPool::Alloc();
	SerializePacket_Master_EnterRoomFail(sPacket, clientKey);
	SendPacket(sPacket);
}


void CBattleSnake_Match_LanClient::SerializePacket_Master_Login(cLanPacketPool *sPacket, int serverNo, char *masterToken)
{
	WORD type = en_PACKET_MAT_MAS_REQ_SERVER_ON;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&serverNo, sizeof(int));
	sPacket->Serialize(masterToken, sizeof(char) * 32);
}
void CBattleSnake_Match_LanClient::SerializePacket_Master_RoomInfo(cLanPacketPool *sPacket, UINT64 clientKey, UINT64 accountNo)
{
	WORD type = en_PACKET_MAT_MAS_REQ_GAME_ROOM;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&clientKey, sizeof(UINT64));
	sPacket->Serialize((char*)&accountNo, sizeof(UINT64));
}
void CBattleSnake_Match_LanClient::SerializePacket_Master_EnterRoomSuccess(cLanPacketPool *sPacket, WORD battleServerNo, int roomNo, UINT64 clientKey)
{
	WORD type = en_PACKET_MAT_MAS_REQ_ROOM_ENTER_SUCCESS;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&battleServerNo, sizeof(WORD));
	sPacket->Serialize((char*)&roomNo, sizeof(int));
	sPacket->Serialize((char*)&clientKey, sizeof(UINT64));
}

void CBattleSnake_Match_LanClient::SerializePacket_Master_EnterRoomFail(cLanPacketPool *sPacket, UINT64 clientKey)
{
	WORD type = en_PACKET_MAT_MAS_REQ_ROOM_ENTER_FAIL;
	sPacket->Serialize((char*)&type, sizeof(WORD));
	sPacket->Serialize((char*)&clientKey, sizeof(UINT64));
}

bool CBattleSnake_Match_LanClient::PacketProc(cLanPacketPool* dsPacket)
{
	WORD type;

	dsPacket->Deserialize((char*)&type, sizeof(WORD));

	switch (type)
	{
	case en_PACKET_MAT_MAS_RES_SERVER_ON:
		return Response_Master_Login(dsPacket);
	case en_PACKET_MAT_MAS_RES_GAME_ROOM:
		return Response_Master_RoomInfo(dsPacket);
	case en_PACKET_MAT_MAS_RES_ROOM_ENTER_SUCCESS:
		return Response_Master_RoomEnter(dsPacket);
	default:
		return false;
	};
}

bool CBattleSnake_Match_LanClient::Response_Master_Login(cLanPacketPool *dsPacket)
{
	int serverNo;
	dsPacket->Deserialize((char*)&serverNo, sizeof(int));

	return _pMatchNetServer->Recieve_MasterRequest_Login(serverNo);
}

bool CBattleSnake_Match_LanClient::Response_Master_RoomInfo(cLanPacketPool *dsPacket)
{
	UINT64	clientKey;
	BYTE	status;
	WORD	battleServerNo;
	WCHAR	battleIP[16];
	WORD	battlePort;
	int		roomNo;
	char	connectToken[32];
	char	enterToken[32];

	WCHAR	chatIP[16];
	WORD	chatPort;

	dsPacket->Deserialize((char*)&clientKey, sizeof(UINT64));
	dsPacket->Deserialize((char*)&status, sizeof(BYTE));
	dsPacket->Deserialize((char*)&battleServerNo, sizeof(WORD));
	dsPacket->Deserialize((char*)battleIP, sizeof(WCHAR) * 16);
	dsPacket->Deserialize((char*)&battlePort, sizeof(WORD));
	dsPacket->Deserialize((char*)&roomNo, sizeof(int));
	dsPacket->Deserialize(connectToken, sizeof(char) * 32);
	dsPacket->Deserialize(enterToken, sizeof(char) * 32);
	dsPacket->Deserialize((char*)&chatIP, sizeof(WCHAR) * 16);
	dsPacket->Deserialize((char*)&chatPort, sizeof(WORD));

	_pMatchNetServer->Recieve_MasterRequest_RoomInfo(clientKey, status, battleServerNo, battleIP, battlePort, roomNo, connectToken, enterToken, chatIP, chatPort);

	return true;
}

bool CBattleSnake_Match_LanClient::Response_Master_RoomEnter(cLanPacketPool *dsPacket)
{
	UINT64 clientKey;
	dsPacket->Deserialize((char*)&clientKey, sizeof(UINT64));

	_pMatchNetServer->Receive_MasterRequest_RoomEnter(clientKey);
	return true;
}