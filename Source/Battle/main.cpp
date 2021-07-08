#include <Windows.h>
#include <iostream>
#include <conio.h>

#include "Text_Parser.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"
#include "CBattleSnake_Battle_LocalMonitor.h"

using namespace TUNJI_LIBRARY;
cParse parse;
CBattleSnake_Battle_LocalMonitor *g_pMonitor = CBattleSnake_Battle_LocalMonitor::GetInstance();
cCrashDump g_crash;

bool StartBattleServer(void)
{
	WCHAR	IP[16] = { 0, };
	WCHAR	accessIP[16] = { 0, };
	int		port;
	int		threadCnt;
	int		nagle;
	int		maxUser;
	int		acceptPool;
	int		maxBattleRoom; 
	int		defaultWaitRoom;

	int		authSleepCount;
	int		gameSleepCount;
	int		sendSleepCount;

	int		version;

	WCHAR	connectToken[32] = { 0, };
	char	connectToken_utf8[32] = { 0, };
	WCHAR	masterToken[32] = { 0, };
	char	masterToken_utf8[32] = { 0, };


	int		packetCode;
	int		packetKey1;
	int		packetKey2;

	WCHAR	selectAccountUrl[128] = { 0, };
	WCHAR	selectContentsUrl[128] = { 0, };
	WCHAR	updateContentsUrl[128] = { 0, };

	if (parse.LoadFile(L"BattleSnake_Battle_Config.ini") == false) return false;

	//	MMOServer 
	if (parse.FindSection(L"BattleSnake_Battle_NetServer") == false) return false;
	if (!parse.GetValue(L"BindServerIP", IP)) return false;
	if (!parse.GetValue(L"BindServerPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt)) return false;
	if (!parse.GetValue(L"Nagle", &nagle)) return false;
	if (!parse.GetValue(L"MaxUser", &maxUser)) return false;
	if (!parse.GetValue(L"AcceptPool", &acceptPool)) return false;
	if (!parse.GetValue(L"MaxBattleRoom", &maxBattleRoom)) return false;
	if (!parse.GetValue(L"DefaultWaitRoom", &defaultWaitRoom)) return false;

	if (!parse.GetValue(L"AuthSleepCount", &authSleepCount)) return false;
	if (!parse.GetValue(L"GameSleepCount", &gameSleepCount)) return false;
	if (!parse.GetValue(L"SendSleepCount", &sendSleepCount)) return false;

	if (!parse.GetValue(L"Version", &version)) return false;
	if (!parse.GetValue(L"ConnectToken", connectToken))return false;
	if (!parse.GetValue(L"MasterToken", masterToken)) return false;

	if (!parse.GetValue(L"PacketCode", &packetCode))return false;
	if (!parse.GetValue(L"PacketKey1", &packetKey1)) return false;
	if (!parse.GetValue(L"PacketKey2", &packetKey2)) return false;
	if (!parse.GetValue(L"PublicAccessIP", accessIP)) return false;
	
	WideCharToMultiByte(CP_UTF8, 0, connectToken, 32, connectToken_utf8, 32, 0, 0);
	WideCharToMultiByte(CP_UTF8, 0, masterToken, 32, masterToken_utf8, 32, 0, 0);

	g_pBattle = new CBattleSnake_Battle_MMOServer(maxUser, maxBattleRoom, defaultWaitRoom, authSleepCount, gameSleepCount, sendSleepCount, version,
												  connectToken_utf8, masterToken_utf8, packetCode, packetKey1, packetKey2, accessIP);

	//	Battle Server
	if (g_pBattle->Start(IP, (short)port, threadCnt, (bool)nagle, acceptPool) == false)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Fail to start battle server\n");
		return false;
	}

	// Chat LanServer
	if (parse.FindSection(L"BattleSnake_Battle_LanServer") == false) return false;

	if (!parse.GetValue(L"BindServerIP", IP)) return false;
	if (!parse.GetValue(L"BindServerPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt))return false;
	if (!parse.GetValue(L"Nagle", &nagle))return false;
	if (!parse.GetValue(L"MaxUser", &maxUser)) return false;

	if (g_pBattle->Start_Battle_LanServer(IP, (short)port, threadCnt, (bool)nagle, maxUser) == false)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Fail to start lan server");
		return false;
	}

	//	Master LanClient
	if (parse.FindSection(L"BattleSnake_Battle_LanClient") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	if (!parse.GetValue(L"ConnectIP", IP)) return false;
	if (!parse.GetValue(L"ConnectPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt))return false;
	if (!parse.GetValue(L"Nagle", &nagle))return false;

	if (g_pBattle->Connect_Battle_LanClient(IP, (short)port, threadCnt, (bool)nagle) == false)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Fail to start lan client");
		return false;
	}

	//	Http Client
	if (parse.FindSection(L"BattleSnake_Battle_Http") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt))return false;
	if (!parse.GetValue(L"Nagle", &nagle))return false;
	if (!parse.GetValue(L"MaxUser", &maxUser)) return false;
	if (!parse.GetValue(L"URL_SELECT_ACCOUNT", selectAccountUrl)) return false;
	if (!parse.GetValue(L"URL_SELECT_CONTENTS", selectContentsUrl)) return false;
	if (!parse.GetValue(L"URL_UPDATE_CONTENTS", updateContentsUrl)) return false;

	if (g_pBattle->Connect_Battle_HttpClient(threadCnt, (bool)nagle, maxUser) == false)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Fail to start http client");
		return false;
	}

	g_pBattle->Set_HttpURL(selectAccountUrl, wcslen(selectAccountUrl), 1);
	g_pBattle->Set_HttpURL(selectContentsUrl, wcslen(selectContentsUrl), 2);
	g_pBattle->Set_HttpURL(updateContentsUrl, wcslen(updateContentsUrl), 3);
	

	//	Monitor Client 
	if (parse.FindSection(L"BattleSnake_BattleMonitor_LanClient") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	if (!parse.GetValue(L"ConnectIP", IP)) return false;
	if (!parse.GetValue(L"ConnectPort", &port)) return false;
	if (!parse.GetValue(L"WorkerThreadCount", &threadCnt))return false;
	if (!parse.GetValue(L"Nagle", &nagle))return false;

	if (g_pBattle->Connect_Monitor_LanClient(IP, (short)port, threadCnt, (bool)nagle) == false)
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Fail to start monitor client");
		return false;
	}

	return true;
}

int main(void)
{
	SYSLOG_DIRECTORY(L"BattleSnake_Battle_Log");
	SYSLOG_MODE(dfPRINT_LOG_TEXT);
	SYSLOG_LEVEL(cSystemLog::LEVEL_ERROR);

	cNetPacketPool::ConstructChunkPool();
	cLanPacketPool::ConstructChunkPool();

	if (!StartBattleServer())
	{
		LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Fail to set battle config");
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
				if (g_pBattle->IsServerOn())
				{
					g_pBattle->Stop();
					g_pBattle->Stop_Battle_LanServer();
					g_pBattle->Disconnect_Battle_LanClient();
					g_pBattle->Disconnect_Battle_HttpClient();
					g_pBattle->Disconnect_Monitor_LanClient();
				}
				else
					break;
			}
			else if (key == L's' || key == L'S')
			{
				if (!g_pBattle->IsServerOn())
				{
					if (!StartBattleServer())
					{
						LOG(L"BattleServer", cSystemLog::LEVEL_ERROR, L"#main - StartBattleServer : Restart fail to set battle config");
						break;
					}
				}
			}
		}

		if (g_pBattle->IsServerOn())
		{
			g_pMonitor->MonitorBattleServer();
			g_pBattle->Check_HttpHeartBeat();
		}
		Sleep(100);
	};

	cNetPacketPool::DestructChunkPool();
	cLanPacketPool::DestructChunkPool();
	timeEndPeriod(1);
	return 0;
}