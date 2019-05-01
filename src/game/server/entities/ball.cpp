
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>

#include <game/collision.h>

#include "character.h"
#include "ball.h"
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <engine/shared/config.h>
#include <stdio.h>

CBall::CBall(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Pos = Pos;
	m_Direction = Dir;
	m_Owner = Owner;
	CCharacter *c = GameServer()->GetPlayerChar(m_Owner);
	if (c) {
		m_Player = c->GetPlayer()->GetCID();
		m_Team = GameServer()->GetPlayerChar(m_Owner)->GetPlayer()->GetTeam();
	} else {
		m_Player = -1;
		m_Team = -1;
	}
	m_StartTick = Server()->Tick();
	m_RespawnTick = Server()->Tick() + g_Config.m_SvBallLifetime * Server()->TickSpeed();
	m_LastOwnerInterTick = Server()->Tick();

	GameWorld()->InsertEntity(this);
}

void CBall::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

vec2 CBall::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(g_Config.m_SvBallType)
	{
		case WEAPON_GRENADE:
			Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = GameServer()->Tuning()->m_GunCurvature;
			Speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CBall::Tick()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 LastPos;
	vec2 ChrPos;
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, NULL, &LastPos, true);
	int Collide_x;
	int Collide_y;
	CCharacter *OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *TargetChr = GameServer()->m_World.IntersectCharacter(PrevPos, CurPos, 6.0f, ChrPos, NULL);
	CPlayer *p;


	if (m_Player != -1) {
		p = GameServer()->m_apPlayers[m_Player];
		if (!p)
			m_Player = -1;
	} else {
		p = NULL;
	}

	if (TargetChr
			&& ((TargetChr == OwnerChar && Server()->Tick() - m_LastOwnerInterTick > 2)
			|| TargetChr != OwnerChar)) {
		TargetChr->GiveWeapon(g_Config.m_SvBallType, 1);
		TargetChr->SetWeapon(g_Config.m_SvBallType);
		if (TargetChr->GetPlayer()->GetCID() != m_Player)
			GameServer()->m_pController->m_LastBallPlayer = m_Player;
		GameServer()->m_World.DestroyEntity(this);
		return;
	} else if (TargetChr && TargetChr == OwnerChar && Server()->Tick() - m_LastOwnerInterTick <= 2) {
		m_LastOwnerInterTick = Server()->Tick();
	}


	if (GameLayerClipped(CurPos) || m_RespawnTick < Server()->Tick() || GameServer()->Collision()->GetCollisionAt(PrevPos.x, PrevPos.y) & CCollision::COLFLAG_SOLID) {
		if (!g_Config.m_SvMultiBall) {
			GameServer()->m_pController->ball_game_state = IGameController::BALL_GAME_RESPAWN;
			GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
		}
		GameServer()->m_World.DestroyEntity(this);
	} else if (Collide) {
		int scol = CCollision::MaskSCollision(Collide);
		if (scol == CCollision::SFLAG_GOAL_TEAM_0 || scol == CCollision::SFLAG_GOAL_TEAM_1) {
			int team_scored;
			if (scol == CCollision::SFLAG_GOAL_TEAM_0) {
				team_scored = 1;
			} else {
				team_scored = 0;
			}
			GameServer()->m_pController->Goal(p, team_scored, m_Team);
			GameServer()->m_World.DestroyEntity(this);
		} else {
			vec2 col_pos;
			col_pos.x = LastPos.x;
			col_pos.y = CurPos.y;
			Collide_y = GameServer()->Collision()->IntersectLine(PrevPos, col_pos, NULL, NULL);
			col_pos.x = CurPos.x;
			col_pos.y = LastPos.y;
			Collide_x = GameServer()->Collision()->IntersectLine(PrevPos, col_pos, NULL, NULL);

			m_Pos = LastPos;
			vec2 vel;
			vel.x = m_Direction.x;
			vel.y = m_Direction.y + 2*GameServer()->Tuning()->m_ShotgunCurvature/10000*Ct*GameServer()->Tuning()->m_ShotgunSpeed;
			if (Collide_x && !Collide_y) {
				m_Direction.x = -vel.x;
				m_Direction.y = vel.y;
			} else if (!Collide_x && Collide_y) {
				m_Direction.x = vel.x;
				m_Direction.y = -vel.y;
			} else {
				m_Direction.x = -vel.x;
				m_Direction.y = -vel.y;
			}
			m_Direction.x *= (100 - g_Config.m_SvBallDecay) / 100.0;
			m_Direction.y *= (100 - g_Config.m_SvBallDecay) / 100.0;
			m_StartTick = Server()->Tick();
			m_LastOwnerInterTick = Server()->Tick()-2;
		}
	}
}

void CBall::TickPaused()
{
	++m_StartTick;
}

void CBall::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = g_Config.m_SvBallType;
}

void CBall::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(pProj)
		FillInfo(pProj);
}
