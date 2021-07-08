#include <Windows.h>
#include <iostream>
#include <conio.h>
#include "Text_Parser.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

#include "CNetServer.h"

#include "CBattleSnake_Match_LocalMonitor.h"
#include "CBattleSnake_Match_NetServer.h"

using namespace TUNJI_LIBRARY;
cParse parse;
CBattleSnake_Match_NetServer *pMatchServer;
CBattleSnake_Match_LocalMonitor *g_pMonitor = CBattleSnake_Match_LocalMonitor::GetInstance();
cCrashDump g_crash;

bool StartMatchServer(void)
{
	WCHAR	IP[16] = { 0, };
	WCHAR	publicIP[16] = { 0 , };
	int		port;
	int		threadCnt;
	int		nagle;
	int		maxUser;
	int		acceptPool;
	int		version;
	int		packetCode;
	int		packetKey1;
	int		packetKey2;
	int		serverNo;
	WCHAR	token[32];
	char	token_utf8[32];

	WCHAR	dbUserName[64] = { 0, };
	WCHAR	dbPassword[64] = { 0, };
	WCHAR	dbName[64] = { 0, };

	WCHAR	selectAccountUrl[128] = { 0, };

	if (parse.LoadFile(L"BattleSnake_Match_Config.ini") == false) return false;

	//	NetServer
	if (parse.FindSection(L"BattleSnake_Match_NetServer") == false) return false;
	parse.GetValue(L"BindServerIP", IP);
	parse.GetValue(L"BindServerPort", &port);
	parse.GetValue(L"WorkerThreadCount", &threadCnt);
	parse.GetValue(L"Nagle", &nagle);
	parse.GetValue(L"MaxUser", &maxUser);
	parse.GetValue(L"AcceptPool", &acceptPool);
	parse.GetValue(L"Version", &version);
	parse.GetValue(L"PacketCode", &packetCode);
	parse.GetValue(L"PacketKey1", &packetKey1);
	parse.GetValue(L"PacketKey2", &packetKey2);
	parse.GetValue(L"Match_No", &serverNo);
	parse.GetValue(L"Token", token);
	parse.GetValue(L"PublicAccessIP", publicIP);

	WideCharToMultiByte(CP_UTF8, 0, token, 32, token_utf8, 32, 0, 0);

	pMatchServer = new CBattleSnake_Match_NetServer(version, (BYTE)packetCode, (BYTE)packetKey1, (BYTE)packetKey2, serverNo, token_utf8, publicIP);
	if (pMatchServer->start(IP, (short)port, threadCnt, (bool)nagle, maxUser, acceptPool) == false)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : Fail to start match server");
		return false;
	}

	//	DB
	memset(IP, 0, sizeof(WCHAR) * 16);
	if (parse.FindSection(L"BattleSnake_Match_DB") == false) return false;
	parse.GetValue(L"DB_IP", IP);
	parse.GetValue(L"DB_Port", &port);
	parse.GetValue(L"DB_UserName", dbUserName);
	parse.GetValue(L"DB_Password", dbPassword);
	parse.GetValue(L"DB_Name", dbName);

	if (pMatchServer->Connect_MatchStatusDB(IP, dbUserName, dbPassword, dbName, port) == false)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : Fail to connect db server");
		return false;
	}

	//	LanClient
	if (parse.FindSection(L"BattleSnake_Match_LanClient") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	parse.GetValue(L"ConnectIP", IP);
	parse.GetValue(L"ConnectPort", &port);
	parse.GetValue(L"WorkerThreadCount", &threadCnt);
	parse.GetValue(L"Nagle", &nagle);

	if (pMatchServer->ConnectLanClient_Match(IP, (short)port, threadCnt, (bool)nagle) == false)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : Fail to connect master lan client");
		return false;
	}

	//	HttpClient
	if (parse.FindSection(L"BattleSnake_Match_Http") == false) return false;
	parse.GetValue(L"WorkerThreadCount", &threadCnt);
	parse.GetValue(L"Nagle", &nagle);
	parse.GetValue(L"MaxUser", &maxUser);
	parse.GetValue(L"URL_SELECT_ACCOUNT", selectAccountUrl);

	if (pMatchServer->Start_HttpClient(threadCnt, (bool)nagle, maxUser) == false) 
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : Fail to connect http client");
		return false;
	}

	pMatchServer->Set_HttpURL(selectAccountUrl, wcslen(selectAccountUrl));

	//	MonitorClient
	if (parse.FindSection(L"BattleSnake_Monitor_MatchLanClient") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	parse.GetValue(L"ConnectIP", IP);
	parse.GetValue(L"ConnectPort", &port);
	parse.GetValue(L"WorkerThreadCount", &threadCnt);
	parse.GetValue(L"Nagle", &nagle);

	if (pMatchServer->ConnectLanClient_Monitor(IP, (short)port, threadCnt, (bool)nagle) == false)
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : Fail to monitor client");
		return false;
	}
	parse.CloseFile();
	return true;
}

int main()
{
	SYSLOG_DIRECTORY(L"BattleSnake_Match_Log");
	SYSLOG_MODE(dfPRINT_LOG_TEXT);
	SYSLOG_LEVEL(cSystemLog::LEVEL_ERROR);

	cNetPacketPool::ConstructChunkPool();
	cLanPacketPool::ConstructChunkPool();

	if (!StartMatchServer())
	{
		LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : FAIL TO SET MATCH CONFIG");
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
				if (pMatchServer->IsServerOn())
				{
					pMatchServer->DisconnectLanClient_Monitor();
					pMatchServer->Disconnect_MatchStatusDB();
					pMatchServer->Stop_HttpClient();
					pMatchServer->stop();
					pMatchServer->DisconnectLanClient_Match();
				}
				else break;
			}
			else if (key == L's' || key == L'S')
			{
				if (!pMatchServer->IsServerOn())
				{
					if (!StartMatchServer())
					{
						LOG(L"MatchServer", cSystemLog::LEVEL_ERROR, L"#StartMatchServer : FAIL TO SET MATCH CONFIG");
						break;
					}
				}
			}
		}

		if (pMatchServer->IsServerOn())
		{
			g_pMonitor->MonitorMatchServer();
			
		}
		Sleep(100);
	};

	cNetPacketPool::DestructChunkPool();
	cLanPacketPool::DestructChunkPool();
	timeEndPeriod(1);
	return 0;
}
