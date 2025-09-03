//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"

#include "c_tf_tank_boss.h"

#include "teamplayroundbased_gamerules.h"


IMPLEMENT_CLIENTCLASS_DT(C_TFTankBoss, DT_TFTankBoss, CTFTankBoss)
	//RecvPropVector(RECVINFO(m_shadowDirection)),
	RecvPropString( RECVINFO( m_iszClassIcon ) ),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( tank_boss, C_TFTankBoss );


C_TFTankBoss::C_TFTankBoss()
{
}

void C_TFTankBoss::GetGlowEffectColor( float *r, float *g, float *b )
{
	TeamplayRoundBasedRules()->GetTeamGlowColor( GetTeamNumber(), *r, *g, *b );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* C_TFTankBoss::GetBossProgressImageName() const
{
	if ( m_iszClassIcon )
	{
		return GetClassIconName();
	}

	return "tank";
}
