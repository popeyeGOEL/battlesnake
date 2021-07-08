#include <Windows.h>
#include <iostream>
#include <conio.h>
#include "Text_Parser.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

#include "CLanClient.h"
#include "CLanServer.h"
#include "CBattleSnake_Master_LanServer.h"
#include "CBattleSnake_Master_LocalMonitor.h"


using namespace TUNJI_LIBRARY;
cParse parse;
CBattleSnake_Master_LanServer *pMasterServer;
CBattleSnake_Master_LocalMonitor *g_pMonitor = CBattleSnake_Master_LocalMonitor::GetInstance();
cCrashDump g_crash;

bool StartMasterServer(void)
{
	WCHAR IP[16] = { 0, };
	int port = 0;
	int threadCnt = 0;
	int bNagle = 1;
	int maxUser = 0;
	WCHAR token[32] = { 0, };
	char token_utf8[32] = { 0, };

	if (parse.LoadFile(L"BattleSnake_Master_Config.ini") == false) return -1;

	if (parse.FindSection(L"BattleSnake_Master_Info") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	parse.GetValue(L"BindServerIP", IP);
	parse.GetValue(L"BindServerPort", &port);
	parse.GetValue(L"WorkerThreadCount", &threadCnt);
	parse.GetValue(L"Nagle", &bNagle);
	parse.GetValue(L"MaxUser", &maxUser);
	parse.GetValue(L"Token", token);

	WideCharToMultiByte(CP_UTF8, 0, token, 32, token_utf8, 32, 0, 0);
	
	pMasterServer = new CBattleSnake_Master_LanServer(token_utf8);
	if (pMasterServer->start(IP, (short)port, threadCnt, (bool)bNagle, maxUser) == false) return false;

	if (parse.FindSection(L"BattleSnake_Monitor_Info") == false) return false;
	memset(IP, 0, sizeof(WCHAR) * 16);
	parse.GetValue(L"ConnectIP", IP);
	parse.GetValue(L"ConnectPort", &port);
	parse.GetValue(L"WorkerThreadCount", &threadCnt);
	parse.GetValue(L"Nagle", &bNagle);

	if (pMasterServer->ConnectMonitorClient(IP, (short)port, threadCnt, (bool)bNagle) == false) return false;

	parse.CloseFile();
	return true;
}

int main(void)
{
	SYSLOG_DIRECTORY(L"BattleSnake_Master_Log");
	SYSLOG_MODE(dfPRINT_LOG_TEXT);
	SYSLOG_LEVEL(cSystemLog::LEVEL_ERROR);
	//SYSLOG_LEVEL(cSystemLog::LEVEL_DEBUG);
	cNetPacketPool::ConstructChunkPool();
	cLanPacketPool::ConstructChunkPool();

	if (!StartMasterServer())
	{
		LOG(L"MasterServer", cSystemLog::LEVEL_ERROR, L"#StartMastserServer : SET MASTER SERVER CONFIG FAILED");
		return -1;
	}

	g_pMonitor->SetMonitorFactor(pMasterServer);

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
				if (pMasterServer->IsServerOn())
				{
					pMasterServer->stop();
					pMasterServer->DisconnectMonitorClient();
				}
				else break;
			}
			else if (key == L's' || key == L'S')
			{
				if (!pMasterServer->IsServerOn())
				{
					if (!StartMasterServer()) break;
				}
			}
		}

		if (pMasterServer->IsServerOn())
		{
			g_pMonitor->MonitorMasterServer();
		}
		Sleep(100);
	};

	cNetPacketPool::DestructChunkPool();
	cLanPacketPool::DestructChunkPool();
	timeEndPeriod(1);
	return 0;
};