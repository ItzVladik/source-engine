//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handling for the suit batteries.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "basecombatweapon.h"
#include "gamerules.h"
#include "items.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CItemBattery : public CItem
{
public:
	DECLARE_CLASS( CItemBattery, CItem );

	void Spawn( void )
	{
		Precache();
		SetModel( "models/PG_props/pg_obj/pg_battery.mdl" );
		BaseClass::Spawn();
		SetTouch( NULL );
	}
	void Precache( void )
	{
		PrecacheModel( "models/PG_props/pg_obj/pg_battery.mdl" );
		PrecacheScriptSound( "ItemBattery.Touch" );
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( pPlayer )
			MyTouch( pPlayer );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player *>( pPlayer );
		if ( !pHL2Player )
			return false;
		if ( pHL2Player->UH_GetBatteryCount() >= 20 )
			return false;
		FirePlayerPickupOutput( pHL2Player );
		pHL2Player->UH_AddBattery( 1 );
		CPASAttenuationFilter filter( pPlayer, "ItemBattery.Touch" );
		EmitSound( filter, pPlayer->entindex(), "ItemBattery.Touch" );
		UTIL_Remove( this );
		return true;
	}
};

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);
PRECACHE_REGISTER(item_battery);

