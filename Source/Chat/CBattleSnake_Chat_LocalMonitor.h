#pragma once
#pragma once
using namespace TUNJI_LIBRARY;

class CBattleSnake_Chat_NetServer;
class CBattleSnake_Chat_LocalMonitor
{
public:
	static CBattleSnake_Chat_LocalMonitor* GetInstance()
	{
		static CBattleSnake_Chat_LocalMonitor cMonitor;
		return &cMonitor;
	}

	virtual ~CBattleSnake_Chat_LocalMonitor() {}

	void SetMonitorFactor(void *pThis);
	void MonitorChatServer(void);

private:
	CBattleSnake_Chat_LocalMonitor() {}
	CBattleSnake_Chat_NetServer	*_pChatServer;
};