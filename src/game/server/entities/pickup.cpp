/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "pickup.h"
#include <game/server/gamecontroller.h>
#include <engine/shared/config.h>

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, int Team)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Type = Type;
	m_Subtype = SubType;
	m_ProximityRadius = PickupPhysSize;

	Reset();

	GameWorld()->InsertEntity(this);
	m_SpawnTickSet = 0;
	m_Goalkeeper = -1;
	m_Team = Team;
}

void CPickup::Reset()
{
	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
	if (m_Type == POWERUP_NINJA) {
		CCharacter *Goalkeeper = GameServer()->GetPlayerChar(m_Goalkeeper);
		if (!Goalkeeper) {
			m_SpawnTick = -1;
			m_Goalkeeper = -1;
		}
	}
	if (m_Type == POWERUP_WEAPON && m_Subtype == g_Config.m_SvBallType) {
		if (g_Config.m_SvMultiBall) {
			switch (GameServer()->m_pController->ball_game_state) {
			case IGameController::BALL_GAME_RESPAWN:
				if (!m_SpawnTickSet) {
					m_SpawnTick = Server()->Tick() + g_Config.m_SvBallRespawn;
					m_SpawnTickSet = 1;
				}
				break;
			case IGameController::BALL_GAME_RUNNING:
				m_SpawnTickSet = 0;
				break;
			default:
				break;
			}
		} else {
			switch (GameServer()->m_pController->ball_game_state) {
			case IGameController::BALL_GAME_RESPAWN:
				if (!m_SpawnTickSet) {
					m_SpawnTick = Server()->Tick() + g_Config.m_SvBallRespawn;
					m_SpawnTickSet = 1;
				}
				break;
			case IGameController::BALL_GAME_RUNNING:
				m_SpawnTick = Server()->Tick() + 1;
				m_SpawnTickSet = 0;
				break;
			default:
				break;
			}
		}
	}
	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == POWERUP_WEAPON)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}
	// Check if a player intersected us
	CCharacter *pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 20.0f, 0);
	if(pChr && pChr->IsAlive() && (m_Team == -1 || m_Team == pChr->GetPlayer()->GetTeam()))
	{
		// player picked us up, is someone was hooking us, let them go
		int RespawnTime = -1;
		switch (m_Type)
		{
			case POWERUP_HEALTH:
				if(pChr->IncreaseHealth(1))
				{
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_ARMOR:
				if(pChr->IncreaseArmor(1))
				{
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_WEAPON:
				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS)
				{
					bool ret;
					if (m_Subtype == g_Config.m_SvBallType) {
						ret = pChr->GiveWeapon(g_Config.m_SvBallType, 1);
						pChr->SetWeapon(g_Config.m_SvBallType);
						GameServer()->m_pController->ball_game_state = IGameController::BALL_GAME_RUNNING;
						if (g_Config.m_SvMultiBall) {
							RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
						}
					} else {
						ret = pChr->GiveWeapon(m_Subtype, 10);
					}
					if (ret)
					{
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

						if(m_Subtype == WEAPON_GRENADE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
						else if(m_Subtype == WEAPON_SHOTGUN)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
						else if(m_Subtype == WEAPON_RIFLE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);

						if(pChr->GetPlayer())
							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
					}
				}
				break;

			case POWERUP_NINJA:
				{
					// activate ninja on target player
					pChr->GiveNinja();
					m_Goalkeeper = pChr->GetPlayer()->GetCID();
					RespawnTime = Server()->Tick() + 1;
					break;
				}

			default:
				break;
		};

		if(RespawnTime >= 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
				pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type, m_Subtype);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
			m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
		}
	}
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	pP->m_Subtype = m_Subtype;
}
