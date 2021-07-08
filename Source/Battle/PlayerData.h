#pragma once
namespace PLAYER_DATA
{
	struct stRecord
	{
		int		playTime;
		int		playCount;
		int		kill;
		int		die;
		int		win;
	};

	struct stMoveTarget
	{
		float	moveTargetX;
		float	moveTargetY;
		float	moveTargetZ;
	};

	struct stHitPoint
	{
		float	hitPointX;
		float	hitPointY;
		float	hitPointZ;
	};
}