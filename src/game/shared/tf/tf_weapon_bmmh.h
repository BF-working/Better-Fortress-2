//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#ifndef TF_WEAPON_COMPOUND_BOW_H
#define TF_WEAPON_COMPOUND_BOW_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"
#include "tf_weapon_pipebomblauncher.h"

// Client specific.
#ifdef CLIENT_DLL
#define CTFBMMH C_TFBMMH
#else
#endif

//=============================================================================
//
// TF Weapon Bow
//
class CTFBMMH : public CTFPipebombLauncher
{
public:

	DECLARE_CLASS( CTFBMMH, CTFPipebombLauncher );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFBMMH();
	~CTFBMMH() {}

	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_PIPEBOMBLAUNCHER; }
	virtual int		GetAmmoPerShot()					{ return RemapValClamped(gpGlobals->curtime - GetInternalChargeBeginTime(), 0, GetChargeMaxTime(), 0, 100 ); }

	CTFBMMH( const CTFBMMH & ) {}
};

#endif // TF_WEAPON_COMPOUND_BOW_H
