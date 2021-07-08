#pragma once

using namespace TUNJI_LIBRARY;

class CBattleSnake_Master_LanServer;
class CBattleSnake_Master_LocalMonitor
{
public:
	static CBattleSnake_Master_LocalMonitor* GetInstance()
	{
		static CBattleSnake_Master_LocalMonitor cMonitor;
		return &cMonitor;
	}

	virtual ~CBattleSnake_Master_LocalMonitor() {}
	void SetMonitorFactor(CBattleSnake_Master_LanServer *pThis);
	void MonitorMasterServer(void);

private:
	CBattleSnake_Master_LocalMonitor() {}
	CBattleSnake_Master_LanServer*	_pMasterLanServer;
};