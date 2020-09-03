/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>
#include <stdio.h>

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;

		if(Index > 128)
			continue;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_BALL_SOLID;
			break;
		case TILE_NOHOOK:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_NOHOOK|COLFLAG_BALL_SOLID;
			break;
		case TILE_GOAL_TEAM_0:
			m_pTiles[i].m_Index = SFLAG_GOAL_TEAM_0;
			break;
		case TILE_GOAL_TEAM_1:
			m_pTiles[i].m_Index = SFLAG_GOAL_TEAM_1;
			break;
		case TILE_LIMIT_TEAM_0:
			m_pTiles[i].m_Index = SFLAG_LIMIT_TEAM_0;
			break;
		case TILE_LIMIT_TEAM_1:
			m_pTiles[i].m_Index = SFLAG_LIMIT_TEAM_1;
			break;
		case TILE_GOALIE_LIMIT_0:
			m_pTiles[i].m_Index = SFLAG_GOALIE_LIMIT_0;
			break;
		case TILE_GOALIE_LIMIT_1:
			m_pTiles[i].m_Index = SFLAG_GOALIE_LIMIT_1;
			break;
		case TILE_GOAL_DEATH_TEAM_O:
			m_pTiles[i].m_Index = SFLAG_GOAL_TEAM_0 | COLFLAG_DEATH;
			break;
		case TILE_GOAL_DEATH_TEAM_1:
			m_pTiles[i].m_Index = SFLAG_GOAL_TEAM_1 | COLFLAG_DEATH;
			break;
		case TILE_BALL_SOLID:
			m_pTiles[i].m_Index = COLFLAG_BALL_SOLID;
			break;
		case TILE_DEATH_BALL_SOLID:
			m_pTiles[i].m_Index = COLFLAG_BALL_SOLID | COLFLAG_DEATH;
			break;
		case TILE_DEATH_NON_GOALKEEPER:
			m_pTiles[i].m_Index = SFLAG_LIMIT_NON_GOALIES;
			break;
		default:
			m_pTiles[i].m_Index = 0;
		}
	}
}

int CCollision::GetTile(int x, int y)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	//return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
	if(!m_pTiles || Ny < 0 || Nx < 0 || m_pTiles[Ny*m_Width+Nx].m_Index > 128)
		return 0;

	if(m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_SOLID
		|| m_pTiles[Ny*m_Width+Nx].m_Index == (COLFLAG_SOLID|COLFLAG_NOHOOK)
		|| m_pTiles[Ny*m_Width+Nx].m_Index == COLFLAG_DEATH)
		return m_pTiles[Ny*m_Width+Nx].m_Index;
	return 0;
}

bool CCollision::IsTileSolid(int x, int y)
{
	return GetTile(x, y)&COLFLAG_SOLID;
}

bool CCollision::IsTileBallEvent(int x, int y)
{
	int tile = GetTile(x, y);
	int stile = MaskSCollision(tile);
	if (stile == SFLAG_GOAL_TEAM_0 || stile == SFLAG_GOAL_TEAM_1)
		return true;
	return GetTile(x, y)&COLFLAG_BALL_SOLID;
}

// Calculate next tile in the given direction
vec2 CCollision::NextTile(vec2 Pos, vec2 Dir)
{
	vec2 Next;

	// Calculate the next border point in X and Y direction
	//
	// Teeworlds works with rounded position values. This shifts the correct
	// tile order by 0.5. So it is not possible to use the exact correct tile
	// borders. Instead we also shift it by 0.5 or 0.51, depending on the
	// side of the tile we want.
	//
	// Notice: the unsigned int conversion is necessary for correct NextTile
	// behaviour around 0 coordinates. At 0 or below, the /32 arithmetic
	// works the other way round. By using unsigned int arithmetics we wrap
	// the positive number space around, which allows correct positive only
	// calculations
	if (Dir.x > 0)
	{
		Next.x = (int)(((unsigned int)round(Pos.x)/32 + 1) * 32) - 0.5;
	}
	else
	{
		Next.x = (int)(((unsigned int)round(Pos.x)/32) * 32) - 0.51;
	}
	if (Dir.y > 0)
	{
		Next.y = (int)(((unsigned int)round(Pos.y)/32 + 1) * 32) - 0.5;
	}
	else
	{
		Next.y = (int)(((unsigned int)round(Pos.y)/32) * 32) - 0.51;
	}

	// Calculate remaining distances and which intersection point
	// (X or Y) is closer on the Pos0->Pos1 line.
	vec2 RemainDist;
	RemainDist.x = (Next.x - Pos.x) / Dir.x;
	RemainDist.y = (Next.y - Pos.y) / Dir.y;

	/* Create the the next position to check */
	if (RemainDist.x < RemainDist.y)
	{
		Pos.x += RemainDist.x * Dir.x;
		Pos.y += RemainDist.x * Dir.y;
	}
	else
	{
		Pos.x += RemainDist.y * Dir.x;
		Pos.y += RemainDist.y * Dir.y;
	}
	return Pos;
}

int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, bool ball)
{
	vec2 Cur = Pos0;
	vec2 Dir = Pos1 - Pos0;

	// Move along the line Pos0 -> Pos1 by jumping between tile border points
	while ((Pos1.x - Cur.x) * Dir.x >= 0 && (Pos1.y - Cur.y) * Dir.y >= 0)
	{
		if((ball && BallCheckPoint(Cur.x, Cur.y)) || (!ball && CheckPoint(Cur.x, Cur.y)))
		{
			if(pOutCollision)
				*pOutCollision = Cur;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = NextTile(Cur, vec2(-Dir.x, -Dir.y));
			return GetCollisionAt(Cur.x, Cur.y);
		}

		Cur = NextTile(Cur, Dir);
	}

	// Also check the Pos1 tile
	if((ball && BallCheckPoint(Pos1.x, Pos1.y)) || (!ball && CheckPoint(Pos1.x, Pos1.y)))
	{
		if(pOutCollision)
			*pOutCollision = Pos1;
		if(pOutBeforeCollision)
			*pOutBeforeCollision = NextTile(Pos1, vec2(-Dir.x, -Dir.y));
		return GetCollisionAt(Pos1.x, Pos1.y);
	}

	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces)
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size)
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity)
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}
