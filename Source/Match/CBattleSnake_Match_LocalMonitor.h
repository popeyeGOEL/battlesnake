#pragma once
using namespace TUNJI_LIBRARY;

class CBattleSnake_Match_NetServer;
class CBattleSnake_Match_Http;

class CBattleSnake_Match_LocalMonitor
{
public:
	enum Monitor_Factor
	{
		Factor_Match_Http = 1,
		Factor_Match_NetServer,
	};

	static CBattleSnake_Match_LocalMonitor* GetInstance()
	{
		static CBattleSnake_Match_LocalMonitor cMonitor;
		return &cMonitor;
	}

	virtual ~CBattleSnake_Match_LocalMonitor() {}

	void SetMonitorFactor(void *pThis, Monitor_Factor factor);
	void MonitorMatchServer(void);
	
private:
	CBattleSnake_Match_LocalMonitor() {}
	CBattleSnake_Match_Http*		_pMatchHttpClient;
	CBattleSnake_Match_NetServer*	_pMatchNetServer;
};