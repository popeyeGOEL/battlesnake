#pragma once
using namespace TUNJI_LIBRARY;

class CBattleSnake_Battle_MMOServer;
class CBattleSnake_Battle_Http;
class CBattleSnake_Battle_LanServer;

class CBattleSnake_Battle_LocalMonitor
{
public:
	enum Monitor_Factor
	{
		Factor_Battle_Http,
		Factor_Battle_MMOServer,
		Factor_Battle_LanServer,
	};

	static CBattleSnake_Battle_LocalMonitor* GetInstance()
	{
		static CBattleSnake_Battle_LocalMonitor cMonitor;
		return &cMonitor;
	}

	virtual ~CBattleSnake_Battle_LocalMonitor() {}
	
	void SetMonitorFactor(void *pThis, Monitor_Factor factor);
	void MonitorBattleServer(void);

private:
	CBattleSnake_Battle_LocalMonitor() {}
	CBattleSnake_Battle_MMOServer	*_pBattleServer;
	CBattleSnake_Battle_Http		*_pHttpClient;
	CBattleSnake_Battle_LanServer	*_pChatLanServer;
};