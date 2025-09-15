//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"

#include "NextBot.h"
#include "tf_tank_boss.h"
#include "tf_tank_boss_body.h"

ConVar bf_tank_boss_collisiontype("bf_tank_boss_collisiontype", "1", FCVAR_NONE, "Collision type for the tank \n 0 - CONTENTS_SOLID \n 1 - MASK_SOLID ");

//-------------------------------------------------------------------------------------------
CTFTankBossBody::CTFTankBossBody( INextBot *bot ) : IBody( bot )
{
}


//-------------------------------------------------------------------------------------------
bool CTFTankBossBody::StartSequence( const char *name )
{
	CTFTankBoss *me = (CTFTankBoss *)GetBot()->GetEntity();

	int animSequence = me->LookupSequence( name );

	if ( animSequence )
	{
		me->SetSequence( animSequence );
		me->SetPlaybackRate( 1.0f );
		me->SetCycle( 0 );
		me->ResetSequenceInfo();

		return true;
	}

	return false;
}

void CTFTankBossBody::SetSkin( int nSkin )
{
	CTFTankBoss *me = (CTFTankBoss *)GetBot()->GetEntity();
	me->m_nSkin = nSkin;
}


//-------------------------------------------------------------------------------------------
void CTFTankBossBody::Update( void )
{
	CTFTankBoss *me = (CTFTankBoss *)GetBot()->GetEntity();

	// move the animation ahead in time	
	me->StudioFrameAdvance();
	me->DispatchAnimEvents( me );
}


//---------------------------------------------------------------------------------------------
// return the bot's collision mask (hack until we get a general hull trace abstraction here or in the locomotion interface)
unsigned int CTFTankBossBody::GetSolidMask( void ) const
{
	//Convar for legacy maps support.
	if ( bf_tank_boss_collisiontype.GetInt() )
		return MASK_SOLID;
	else
		return CONTENTS_SOLID;
}
