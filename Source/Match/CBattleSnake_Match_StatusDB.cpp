#include "cDBConnector.h"
#include "CBattleSnake_Match_StatusDB.h"

bool CBattleSnake_Match_StatusDB::DBWrite(Match_Status *status)
{
	return Query(L"insert into `matchmaking_status`.`server`(`serverno`,`ip`,`port`,`connectuser`, `heartbeat`) values (%d, '%s', %d,  %d, NOW()) on duplicate key update `serverno` = %d, `ip` = '%s', `port` = %d, `connectuser` = %d, `heartbeat` = now()",
		status->serverNo, status->serverIP, status->serverPort, status->connectUser, status->serverNo, status->serverIP, status->serverPort, status->connectUser);				
};