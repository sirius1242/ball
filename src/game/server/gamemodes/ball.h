#ifndef GAME_SERVER_GAMEMODES_BALL_H
#define GAME_SERVER_GAMEMODES_BALL_H
#include <game/server/gamecontroller.h>

class CGameControllerBALL : public IGameController
{
public:
	CGameControllerBALL(class CGameContext *pGameServer);

	// event
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual void Snap(int SnappingClient);
	virtual void Goal(CPlayer *p, int team_scored, int start_team, int death_goal = 0);
	virtual void Tick();
};

#endif
