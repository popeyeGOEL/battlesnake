#include <Windows.h>
#include <iostream>
#include <conio.h>

#include "Text_Parser.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

#include "CNetServer.h"
#include "CBattleSnake_Chat_NetServer.h"
#include "CBattleSnake_Chat_LocalMonitor.h"

using namespace TUNJI_LIBRARY;
cParse parse;
CBattleSnake_Chat_NetServer *pChatServer;
CBattleSnake_Chat_LocalMonitor *g_pMonitor = CBattleSnake_Chat_LocalMonitor::GetInstance();
cCrashDump g_crash;

bool StartChatServer(void)
{
	WCHAR IP[16] = { 0, };
	WCHAR accessIP[16] = { 0, };
	int port;
	int threadCnt;
	int nagle;
	int maxUser;
	int acceptPool;
	int packetCode;
	int packetKey1;
	int packetKey2;

	if (parse.LoadFile(L"BattleSnake_Chat_Config.ini") == false) return false;

	//	Chat NetServer
	if (parse.FindSection(L"BattleSnake_Chat_NetServer") == false) return false;
	if (!parse.GetValue(L"BindServerIP", IP)) return false;
	if (!parse.GetValue(L"BindServerPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt)) return false;
	if (!parse.GetValue(L"Nagle", &nagle)) return false;
	if (!parse.GetValue(L"MaxUser", &maxUser)) return false;
	if (!parse.GetValue(L"AcceptPool", &acceptPool)) return false;

	if (!parse.GetValue(L"PacketCode", &packetCode))return false;
	if (!parse.GetValue(L"PacketKey1", &packetKey1)) return false;
	if (!parse.GetValue(L"PacketKey2", &packetKey2)) return false;
	if (!parse.GetValue(L"PublicAccessIP", accessIP)) return false;

	pChatServer = new CBattleSnake_Chat_NetServer((BYTE)packetCode, (BYTE)packetKey1, (BYTE)packetKey2, accessIP);

	if (pChatServer->start(IP, (short)port, threadCnt, (bool)nagle, maxUser, acceptPool) == false)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#main - StartChatServer : Fail to start chat server\n");
		return false;
	}

	pChatServer->Start_Chat_NetServer();

	//	Chat LanClient
	if (parse.FindSection(L"BattleSnake_Chat_LanClient") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	if (!parse.GetValue(L"ConnectIP", IP)) return false;
	if (!parse.GetValue(L"ConnectPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt))return false;
	if (!parse.GetValue(L"Nagle", &nagle))return false;

	if (pChatServer->Connect_Chat_LanClient(IP, (short)port, threadCnt, (bool)nagle) == false)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#main - StartChatServer : Fail to start lan client");
		return false;
	}

	//	Monitor LanClient

	if (parse.FindSection(L"BattleSnake_ChatMonitor_LanClient") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	if (!parse.GetValue(L"ConnectIP", IP)) return false;
	if (!parse.GetValue(L"ConnectPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt))return false;
	if (!parse.GetValue(L"Nagle", &nagle))return false;

	if (pChatServer->Connect_Monitor_LanClient(IP, (short)port, threadCnt, (bool)nagle) == false)
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#main - StartChatServer : Fail to start monitor client");
		return false;
	}

	parse.CloseFile();
	return true;
}

int main(void)
{
	SYSLOG_DIRECTORY(L"BattleSnake_Chat_Log");
	SYSLOG_MODE(dfPRINT_LOG_TEXT);
	SYSLOG_LEVEL(cSystemLog::LEVEL_ERROR);

	cNetPacketPool::ConstructChunkPool();
	cLanPacketPool::ConstructChunkPool();

	if (!StartChatServer())
	{
		LOG(L"ChatServer", cSystemLog::LEVEL_ERROR, L"#main - StartChatServer : Fail to set chat config");
		return -1;
	}

	WCHAR key;
	timeBeginPeriod(1);
	while (1)
	{
		if (_kbhit())
		{
			key = _getch();
			g_profiler.printKey(key);
			if (key == L'q' || key == L'Q')
			{
				if (pChatServer->IsServerOn())
				{
					pChatServer->Stop_Chat_Server();
					pChatServer->stop();
					pChatServer->Disconnect_Chat_LanClient();
					pChatServer->Disconnect_Monitor_LanClient();
				}
				else
					break;
			}
			else if (key == L's' || key == L'S')
			{
				if (!pChatServer->IsServerOn())
				{
					if (!StartChatServer())
					{
						LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartChatServer : Restart fail to set chat config");
						break;
					}
				}
			}
		}

		if (pChatServer->IsServerOn())
		{
			g_pMonitor->MonitorChatServer();
			//pChatServer->Request_Check_NotPlayingUser();
		}
		Sleep(100);
	}

	cNetPacketPool::DestructChunkPool();
	cLanPacketPool::DestructChunkPool();
	timeEndPeriod(1);
	return 0;
}