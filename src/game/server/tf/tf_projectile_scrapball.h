//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Rocket Projectile
//
//=============================================================================
#ifndef TF_PROJECTILE_SCRAPBALL_H
#define TF_PROJECTILE_SCRAPBALL_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_rocket.h"
#include "iscorer.h"


//=============================================================================
//
// Generic rocket.
//
class CTFProjectile_ScrapBall : public CTFBaseRocket, public IScorer
{
public:

	DECLARE_CLASS( CTFProjectile_ScrapBall, CTFBaseRocket );
	DECLARE_NETWORKCLASS();

	// Creation.
	static CTFProjectile_ScrapBall *Create( CBaseEntity *pLauncher, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL );	
	virtual void Spawn();
	virtual void Precache();
	virtual void RocketTouch( CBaseEntity *pOther ) OVERRIDE;

	// IScorer interface
	virtual CBasePlayer *GetScorer( void );
	virtual CBasePlayer *GetAssistant( void ) { return NULL; }

	void	SetScorer( CBaseEntity *pScorer );

	void	SetCritical( bool bCritical ) { m_bCritical = bCritical; }
	bool	IsCritical() { return m_bCritical; }
	virtual int		GetDamageType();

	virtual bool	IsDeflectable() { return true; }
	virtual void	Deflected( CBaseEntity *pDeflectedBy, Vector &vecDir );
	// TODO: ADD BMMH
	virtual int		GetWeaponID( void ) const { return TF_WEAPON_BMMH; }

private:
	CBaseHandle m_Scorer;
	CNetworkVar( bool,	m_bCritical );
};

#endif	//TF_PROJECTILE_ROCKET_H
