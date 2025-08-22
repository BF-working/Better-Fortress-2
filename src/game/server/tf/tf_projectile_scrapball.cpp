
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Rocket
//
//=============================================================================
#include "cbase.h"
#include "tf_weaponbase.h"
#include "tf_projectile_scrapball.h"
#include "tf_player.h"

//=============================================================================
//
// TF Rocket functions (Server specific).
//
#define SCRAPBALL_MODEL "models/weapons/w_models/w_grenade_emp.mdl"

LINK_ENTITY_TO_CLASS( tf_projectile_scrapball, CTFProjectile_ScrapBall );
PRECACHE_REGISTER( tf_projectile_scrapball );

IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ScrapBall, DT_TFProjectile_ScrapBall )

BEGIN_NETWORK_TABLE( CTFProjectile_ScrapBall, DT_TFProjectile_ScrapBall )
	SendPropBool( SENDINFO( m_bCritical ) ),
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFProjectile_ScrapBall *CTFProjectile_ScrapBall::Create( CBaseEntity *pLauncher, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, CBaseEntity *pScorer )
{
	CTFProjectile_ScrapBall *pRocket = static_cast<CTFProjectile_ScrapBall*>( CTFBaseRocket::Create( pLauncher, "tf_projectile_scrapball", vecOrigin, vecAngles, pOwner ) );

	if ( pRocket )
	{
		pRocket->SetScorer( pScorer );
	}

	return pRocket;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Spawn()
{
	SetModel( SCRAPBALL_MODEL );
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	SetGravity( 1.5 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Precache()
{
	int iModel = PrecacheModel( SCRAPBALL_MODEL );
	PrecacheGibsForModel( iModel );
	PrecacheParticleSystem( "critical_rocket_blue" );
	PrecacheParticleSystem( "critical_rocket_red" );
	PrecacheParticleSystem( "eyeboss_projectile" );
	PrecacheParticleSystem( "rockettrail" );
	PrecacheParticleSystem( "rockettrail_RocketJumper" );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::SetScorer( CBaseEntity *pScorer )
{
	m_Scorer = pScorer;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBasePlayer *CTFProjectile_ScrapBall::GetScorer( void )
{
	return dynamic_cast<CBasePlayer *>( m_Scorer.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFProjectile_ScrapBall::GetDamageType() 
{ 
	int iDmgType = BaseClass::GetDamageType();
	if ( m_bCritical )
	{
		iDmgType |= DMG_CRITICAL;
	}

	return iDmgType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::RocketTouch( CBaseEntity *pOther )
{
	BaseClass::RocketTouch( pOther );

	CTFPlayer *pScorer = ToTFPlayer( GetOwnerEntity() );
	Vector vecOrigin = GetAbsOrigin();
	float flRadius = GetRadius(); 

	CUtlVector<CBaseObject*> objVector;
	for ( int i = 0; i < IBaseObjectAutoList::AutoList().Count(); ++i )
	{
		CBaseObject* pObj = static_cast<CBaseObject*>( IBaseObjectAutoList::AutoList()[i] );
		if ( !pObj || pObj->GetTeamNumber() != GetTeamNumber() )
			continue;

		float flDist = (pObj->GetAbsOrigin() - vecOrigin).Length();
		if ( flDist <= flRadius )
		{
			objVector.AddToTail( pObj );
		}
	}

	int iAmmo = 100;
	int iValidObjCount = objVector.Count();
	if ( iValidObjCount > 0 )
	{
		FOR_EACH_VEC( objVector, i )
		{
			CBaseObject *pObj = objVector[i];

			bool bRepairHit = false;
			bool bUpgradeHit = false;

			bRepairHit = ( pObj->Command_Repair( pScorer, 100, 1.f ) > 0 );

			if ( !bRepairHit )
			{
				bUpgradeHit = pObj->CheckUpgradeOnHit( pScorer );
			}
		}
		BaseClass::Destroy();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Rocket was deflected.
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Deflected( CBaseEntity *pDeflectedBy, Vector &vecDir )
{
	CTFPlayer *pTFDeflector = ToTFPlayer( pDeflectedBy );
	if ( !pTFDeflector )
		return;

	ChangeTeam( pTFDeflector->GetTeamNumber() );
	SetLauncher( pTFDeflector->GetActiveWeapon() );

	CTFPlayer* pOldOwner = ToTFPlayer( GetOwnerEntity() );
	SetOwnerEntity( pTFDeflector );

	if ( pOldOwner )
	{
		pOldOwner->SpeakConceptIfAllowed( MP_CONCEPT_DEFLECTED, "projectile:1,victim:1" );
	}

	if ( pTFDeflector->m_Shared.IsCritBoosted() )
	{
		SetCritical( true );
	}

	CTFWeaponBase::SendObjectDeflectedEvent( pTFDeflector, pOldOwner, GetWeaponID(), this );

	IncrementDeflected();
	m_nSkin = ( GetTeamNumber() == TF_TEAM_BLUE ) ? 1 : 0;
}
