#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "ball.h"
#include <string.h>

CGameControllerBALL::CGameControllerBALL(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "BALL";
	m_GameFlags = GAMEFLAG_TEAMS;
	ball_game_state = IGameController::BALL_GAME_RESPAWN;
	m_LastBallPlayer = -1;
}

// event
int CGameControllerBALL::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	while (pVictim->m_aWeapons[g_Config.m_SvBallType].m_Ammo) {
		pVictim->FireWeapon(true);
	}

	pVictim->GetPlayer()->m_RespawnTick = max(pVictim->GetPlayer()->m_RespawnTick, Server()->Tick()+Server()->TickSpeed()*g_Config.m_SvRespawnDelayTDM);

	return 0;
}

void CGameControllerBALL::Tick()
{
	IGameController::Tick();
	if (m_LastBallPlayer != -1) {
		CPlayer *p = GameServer()->m_apPlayers[m_LastBallPlayer];
		if (p == NULL)
			m_LastBallPlayer = -1;
	}
}

void CGameControllerBALL::Goal(CPlayer *p, int team_scored, int start_team, int death_goal)
{
	CPlayer *pass_p = NULL;
	if (m_LastBallPlayer != -1) {
		pass_p = GameServer()->m_apPlayers[m_LastBallPlayer];
	}
	int passed = 0;
	if (pass_p != NULL && !g_Config.m_SvMultiBall)
		passed = pass_p->GetTeam() == team_scored && pass_p->GetTeam() == p->GetTeam();
	ball_game_state = BALL_GAME_RESPAWN;
	char aBuf[512];
	GameServer()->m_pController->m_aTeamscore[team_scored] += 2 + death_goal + passed;
	if (p && start_team == p->GetTeam()) {
		if (team_scored == p->GetTeam())
			p->m_Score += 2 + death_goal;
		else
			--p->m_Score;
		if (passed)
			pass_p->m_Score += 1;
		char dunk[64] = "";
		char pass[512] = "";
		if (death_goal)
			strcpy(dunk, " (slam dunk)");
		if (passed) {
			strcpy(pass, " with a pass from ");
			strcat(pass, Server()->ClientName(pass_p->GetCID()));
		}
		if (team_scored == 0) {
			str_format(aBuf, sizeof(aBuf), "%s scored for team red%s%s", Server()->ClientName(p->GetCID()), dunk, pass);
		} else {
			str_format(aBuf, sizeof(aBuf), "%s scored for team blue%s%s", Server()->ClientName(p->GetCID()), dunk, pass);
		}
	} else {
		if (team_scored == 0) {
			str_format(aBuf, sizeof(aBuf), "Team red scored");
		} else {
			str_format(aBuf, sizeof(aBuf), "Team blue scored");
		}
	}
	GameServer()->SendChat(-1, -2, aBuf);
	GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
	if (g_Config.m_SvGoalRespawn) {
		for (int i = 0; i != MAX_CLIENTS; ++i) {
			CPlayer *p = GameServer()->m_apPlayers[i];
			if (p)
				p->KillCharacter();
		}
	}
	m_LastBallPlayer = -1;
}
