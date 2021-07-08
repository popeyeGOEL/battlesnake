#pragma once

using namespace TUNJI_LIBRARY;
class CBattleSnake_Match_StatusDB : public cDBConnector
{
public:
	struct Match_Status
	{
		int		serverNo;
		WCHAR	*serverIP;
		int		serverPort;
		int		connectUser;
	};

	bool	DBWrite(Match_Status *status);
};