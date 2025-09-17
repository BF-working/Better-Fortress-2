//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_bmmh.h"
#include "tf_fx_shared.h"
#include "tf_gamerules.h"
#include "in_buttons.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_tf_gamestats.h"
#include "prediction.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#include "tf_projectile_scrapball.h"
#endif

#define TF_WEAPON_BMMH_MODEL		"models/weapons/w_models/w_stickybomb_d.mdl"

#define TF_WEAPON_BMMH_CHARGE_SOUND	"Weapon_StickyBombLauncher.ChargeUp"

//=============================================================================
//
// Weapon tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFBMMH, DT_WeaponBMMH )

BEGIN_NETWORK_TABLE( CTFBMMH, DT_WeaponBMMH )
#ifdef CLIENT_DLL
	
#else
	
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFBMMH )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flChargeBeginTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_bmmh, CTFBMMH );
PRECACHE_WEAPON_REGISTER( tf_weapon_bmmh );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFBMMH )
END_DATADESC()
#endif

//=============================================================================
//
// Weapon functions.
//
CTFBMMH::CTFBMMH()
{
	m_bReloadsSingly = false;
}