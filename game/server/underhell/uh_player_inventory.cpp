//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell inventory — server-side CHL2_Player implementation.
//
// Reconstructed 1:1 from the original server.dll:
//   * UH_ItemAction             — hexrays sub_102E05F0 (vtable index 411)
//   * UH_UpdateInventory        — hexrays sub_102DDDE0
//   * UH_HandleInventoryCommand — hexrays sub_102DDBF0 (switch/dropitem/useitem)
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "props.h"
#include "hl2/weapon_flaregun.h"

#include "underhell/uh_inventory.h"
#include "underhell/uh_items.h"

//-----------------------------------------------------------------------------
// Console commands.
//
// NOTE: "switch", "dropitem", "useitem" and "UpdateInventory" are
// intentionally NOT registered as ConCommands — exactly like the original,
// they arrive through the engine's client-command route (unknown commands are
// forwarded to the server and dispatched by CHL2_Player::ClientCommand).
// "emit" stays in the vanilla base handler.
//-----------------------------------------------------------------------------
static CHL2_Player *UH_GetCommandPlayer( void )
{
	return dynamic_cast<CHL2_Player *>( UTIL_GetCommandClient() );
}

//-----------------------------------------------------------------------------
// Dev/testing helper — not part of the original. Lets the inventory UI and
// use/drop flows be exercised without the (still stubbed) pickup logic.
//-----------------------------------------------------------------------------
CON_COMMAND_F( uh_give_item, "Adds an item to your inventory (dev/testing).", FCVAR_CHEAT )
{
	CHL2_Player *pPlayer = UH_GetCommandPlayer();
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg( "usage: uh_give_item <item id 1..%d>\n", UH_ITEM_MAX - 1 );
		return;
	}

	int iItem = atoi( args[1] );
	if ( !UH_IsValidInventoryItem( iItem ) )
	{
		Msg( "uh_give_item: invalid item id %d\n", iItem );
		return;
	}

	pPlayer->UH_AddInventoryItem( iItem );
	engine->ClientCommand( pPlayer->edict(), "UpdateInventory" );
}

//-----------------------------------------------------------------------------
// Item ids that get a body group when dropped into the world as an item model.
// Original mapping (hexrays sub_102E05F0 switch): apple/soda colours.
//-----------------------------------------------------------------------------
static int UH_GetDropBodyGroup( int iItem )
{
	switch ( iItem )
	{
	case UH_ITEM_APPLE_RED:			// Apple (Red)
	case UH_ITEM_SODA_FIRST:		// Soda flavour 1
		return 0;
	case UH_ITEM_APPLE_GREEN:		// Apple (Green)
	case UH_ITEM_SODA_FIRST + 1:	// Soda flavour 2
		return 1;
	case UH_ITEM_SODA_FIRST + 2:
		return 2;
	case UH_ITEM_SODA_FIRST + 3:
		return 3;
	case UH_ITEM_SODA_FIRST + 4:
		return 4;
	case UH_ITEM_SODA_FIRST + 5:
		return 5;
	default:
		return -1;	// no body group change
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initial inventory state. The original constructor zeroes the array
// and forces m_bInventoryEnabled to 1 (hexrays sub_102E0350).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_InitializeInventory( void )
{
	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		m_iInventory.Set( i, UH_ITEM_NONE );
	}

	m_bShoulderFlashlight = false;
	m_iUHBatteryCount = 0;
	m_flUHBatteryCharge = 0.0f;
	m_iUHHermitCardsCount = 0;
	m_iUHHermitCurrentQuestCount = 0;
	m_iUHHermitTotalQuestCount = 0;
	m_bDisplayHermitCard = false;
	m_bFlashlightOn = false;
	m_bInventoryEnabled = true;
	m_hActiveGlowStick = NULL;
	m_hActiveGlowStickLight = NULL;
	m_bIronSighted = false;
	m_fIronsightedTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the world entity for an inventory item in front of the
// player (eye position + forward*56 + up*64, angled to (0, eye yaw - 90, 0)),
// styled per id. Shared by UH_GiveItem (vtable [410], drop-on-full) and
// UH_ItemAction (vtable [411], dropitem) — the original has the same
// per-id switch in both.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_SpawnItemInWorld( int iItem )
{
	// Original: origin = eye + forward*56 + up*64 (sub_102DE310).
	Vector vecForward;
	EyeVectors( &vecForward );

	Vector vecOrigin = EyePosition() + vecForward * 56.0f + Vector( 0, 0, 64.0f );
	QAngle angItem( 0, EyeAngles().y - 90.0f, 0 );

	// Original drop switch uses the individual stick model and m_nSkin
	// 0/2/4/6/8. Lit inventory ids release the tracked flare prop.
	if ( UH_IsGlowstick( iItem ) || UH_IsLitGlowstick( iItem ) )
	{
		CBaseEntity *pGlow = UH_IsLitGlowstick( iItem ) ? UH_GetActiveGlowStick() : NULL;
		if ( pGlow )
		{
			// The carried prop was spawned hidden/non-solid. Rebuilding the
			// dropped prop is safer than attempting to turn that entity back into
			// VPhysics after it has been parented to the player.
			CBaseEntity *pLight = UH_GetActiveGlowStickLight();
			if ( pLight ) UTIL_Remove( pLight );
			UTIL_Remove( pGlow );
			UH_SetActiveGlowStick( NULL );
			UH_SetActiveGlowStickLight( NULL );
			pGlow = NULL;
		}

		pGlow = CreateEntityByName( "prop_physics" );
		if ( !pGlow ) return;
		pGlow->SetModel( "models/PG_props/pg_obj/pg_glow_stick.mdl" );
		pGlow->SetAbsOrigin( vecOrigin );
		pGlow->SetAbsAngles( angItem );
		DispatchSpawn( pGlow );
		pGlow->GetBaseAnimating()->m_nSkin = UH_GetGlowstickBodyGroup( iItem ) + ( UH_IsLitGlowstick( iItem ) ? 1 : 0 );
		if ( UH_IsLitGlowstick( iItem ) )
		{
			CBaseAnimating *pGlowAnim = pGlow->GetBaseAnimating();
			int attachment = pGlowAnim ? pGlowAnim->LookupAttachment( "fuse" ) : 0;
			Vector org = pGlow->GetAbsOrigin(); QAngle ang = pGlow->GetAbsAngles();
			if ( attachment > 0 ) pGlowAnim->GetAttachment( attachment, org, ang );
			CBaseEntity *pLight = CreateEntityByName( "light_dynamic" );
			if ( pLight )
			{
				Color color = UH_GetGlowstickColor( iItem );
				pLight->KeyValue( "_light", UTIL_VarArgs( "%d %d %d 255", color.r(), color.g(), color.b() ) );
				pLight->KeyValue( "distance", "256" );
				pLight->KeyValue( "brightness", "2" );
				pLight->SetAbsOrigin( org );
				pLight->SetAbsAngles( ang );
				DispatchSpawn( pLight );
				pLight->SetOwnerEntity( pGlow );
				pLight->SetParent( pGlow, attachment );
			}
			pGlow->AddEffects( EF_NOSHADOW );
		}
		return;
	}

	const char *pszClass = UH_GetInventoryItemClass( iItem );
	CBaseEntity *pItem = CreateEntityByName( pszClass );
	if ( !pItem )
	{
		Warning( "Unable to create entity?!\n" );
		return;
	}

	// Per-id styling (original switch, sub_102E05F0 / sub_102DE310).
	switch ( iItem )
	{
	case UH_ITEM_FLARE_PACK:
		// Flare packs drop a lit flare prop instead of the pack model.
		UTIL_Remove( pItem );
		pItem = CreateEntityByName( "prop_physics" );
		pItem->SetModel( "models/PG_props/pg_obj/pg_flare.mdl" );
		break;

	default:
		break;
	}

	pItem->SetAbsOrigin( vecOrigin );
	pItem->SetAbsAngles( angItem );
	pItem->Spawn();

	// The original dispatches Spawn first, then overwrites m_nSkin. This is
	// significant for apple/soda skin 0 because both Spawn functions randomize
	// their default skin.
	int iSkin = UH_GetDropBodyGroup( iItem );
	if ( iSkin >= 0 && pItem->GetBaseAnimating() )
		pItem->GetBaseAnimating()->m_nSkin = iSkin;
}

//-----------------------------------------------------------------------------
// Purpose: Add an item to the inventory (original: CHL2_Player vtable [410],
// sub_102DE310). When every slot is taken the item is spawned back into the
// world in front of the player instead of vanishing.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_GiveItem( int iItem )
{
	if ( !UH_IsValidInventoryItem( iItem ) )
		return;

	int iFreeSlot = UH_FindFreeSlot();
	if ( iFreeSlot < 0 )
	{
		// Inventory full — the original drops the item to the world.
		UH_SpawnItemInWorld( iItem );
		return;
	}

	m_iInventory.Set( iFreeSlot, iItem );
	engine->ClientCommand( edict(), "UpdateInventory" );
}

//-----------------------------------------------------------------------------
// Purpose: First free inventory slot (0-based), or -1 when full.
// Original helper sub_10171D30 returned a 1-based index with 28 as the
// "full" sentinel; the clean equivalent is used internally here.
//-----------------------------------------------------------------------------
int CHL2_Player::UH_FindFreeSlot( void ) const
{
	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		if ( m_iInventory[i] == UH_ITEM_NONE )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Add an item to the first free inventory slot.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_AddInventoryItem( int iItem )
{
	UH_GiveItem( iItem );
}

void CHL2_Player::UH_RemoveInventoryItem( int iSlot )
{
	if ( iSlot >= 0 && iSlot < UH_INVENTORY_SLOTS )
	{
		m_iInventory.Set( iSlot, UH_ITEM_NONE );
	}
}

int CHL2_Player::UH_FindInventoryItem( int iItem ) const
{
	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		if ( m_iInventory[i] == iItem )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Use or drop the item in a slot (original: vtable index 411,
// called with (slot, 0) from "dropitem" and (slot, 1) from "useitem").
//
// Behaviour per id, decoded from hexrays sub_102E05F0:
//   * spawns the world entity named by the item's classname at the eye
//     position, angled to (0, eye yaw - 90, 0), then either Use()s it
//     (bUse) or styles it as a dropped prop (body groups / flare / glowstick)
//   * lit glowsticks (19..23) have no world entity — the slot is consumed
//     and the lit prop attached to the player is removed
//   * using an unlit glowstick (14..18) lights it: slot becomes id + 5
//   * the slot is cleared and the client is told to resync afterwards
//-----------------------------------------------------------------------------
bool CHL2_Player::UH_ItemAction( int iSlot, bool bUse )
{
	// ClientCommand arguments are untrusted. Validate before indexing the
	// network array so malformed `useitem`/`dropitem` commands cannot read
	// outside m_iInventory.
	if ( iSlot < 0 || iSlot >= UH_INVENTORY_SLOTS )
	{
		Warning( "Invalid inventory slot %d\n", iSlot );
		return false;
	}

	int iItem = m_iInventory[iSlot];
	if ( !iItem )
	{
		Warning( "No Entity\n" );
		return false;
	}

	// Flare packs equip a flare in the left hand instead of spawning a world
	// item (original: holding a flare lets "Throw_Nade" toss a flare).
	if ( bUse && iItem == UH_ITEM_FLARE_PACK )
	{
		UH_EquipFlare();
		UH_RemoveInventoryItem( iSlot );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return true;
	}

	// FM radio / radio cracker – spawn active radio prop that attracts infected (sub_10173790/ sub_101737E0)
	if ( bUse && ( iItem == UH_ITEM_FM_RADIO || iItem == UH_ITEM_RADIO_CRACKER ) )
	{
		Vector vecForward;
		EyeVectors( &vecForward );
		Vector vecOrigin = EyePosition() + vecForward * 56.0f + Vector( 0, 0, 64.0f );
		QAngle angItem( 0, EyeAngles().y - 90.0f, 0 );
		CBaseEntity *pRadio = CreateEntityByName( "uh_radio" );
		if ( !pRadio )
		{
			EmitSound( "HL2Player.UseDeny" );
			engine->ClientCommand( edict(), "UpdateInventory" );
			return false;
		}

		pRadio->SetAbsOrigin( vecOrigin );
		pRadio->SetAbsAngles( angItem );
		CUHRadio *pUHRadio = dynamic_cast<CUHRadio *>( pRadio );
		if ( pUHRadio )
			pUHRadio->SetIsCracker( iItem == UH_ITEM_RADIO_CRACKER );
		pRadio->Spawn();
		pRadio->Use( this, this, USE_ON, 0 );
		UH_RemoveInventoryItem( iSlot );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return true;
	}

	if ( !bUse )
	{
		// Drop path — spawn the styled world item in front of the player
		// (shared with UH_GiveItem, like the original's duplicated switch).
		UH_SpawnItemInWorld( iItem );

		UH_RemoveInventoryItem( iSlot );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return true;
	}

	// The original item virtual returns success before the slot is cleared. The
	// SDK CItem::Use signature is void, so reproduce its refusal gates here.
	const bool bFoodOrDrink =
		( iItem >= UH_ITEM_APPLE_RED && iItem <= UH_ITEM_SODA_LAST ) ||
		iItem == UH_ITEM_CHOCOBAR || iItem == UH_ITEM_ORANGE;
	if ( UH_IsGasMaskOn() && ( bFoodOrDrink || iItem == UH_ITEM_PAINKILLERS ) )
	{
		EmitSound( "HL2Player.UseDeny" );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return false;
	}

	if ( iItem == UH_ITEM_BANDAGES && GetHealth() >= GetMaxHealth() && UH_GetBleedCounter() <= 0 )
	{
		EmitSound( "HL2Player.UseDeny" );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return false;
	}

	if ( ( iItem == UH_ITEM_PAINKILLERS || iItem == UH_ITEM_SYRINGE ||
		   iItem == UH_ITEM_HEALTHKIT || iItem == UH_ITEM_HEALTH_VIAL ) &&
		 GetHealth() >= GetMaxHealth() )
	{
		EmitSound( "HL2Player.UseDeny" );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return false;
	}

	// Use path (original sub_102E05F0): create the item entity, place it at
	// eye position + forward*56 + up*64, angled to (0, eye yaw - 90, 0),
	// then Use() it with the item id as the value.
	const char *pszClass = UH_GetInventoryItemClass( iItem );
	CBaseEntity *pItem = CreateEntityByName( pszClass );

	if ( !pItem )
	{
		if ( UH_IsLitGlowstick( iItem ) )
		{
			// Lit glowsticks have no world entity ("nothing"): using the slot
			// again removes the lit prop strapped to the player.
			UH_RemoveInventoryItem( iSlot );

			CBaseEntity *pGlow = UH_GetActiveGlowStick();
			CBaseEntity *pGlowLight = UH_GetActiveGlowStickLight();
			if ( pGlowLight ) UTIL_Remove( pGlowLight );
			UH_SetActiveGlowStickLight( NULL );
			if ( pGlow )
			{
				UTIL_Remove( pGlow );
				UH_SetActiveGlowStick( NULL );
			}
			else
			{
				Warning( "Attempted to remove Lit glowstick, but did not find one on player! \n" );
			}
		}
		else
		{
			Warning( "Unable to create entity?!\n" );
		}

		engine->ClientCommand( edict(), "UpdateInventory" );
		return false;
	}

	Vector vecForward;
	EyeVectors( &vecForward );

	Vector vecOrigin = EyePosition() + vecForward * 56.0f + Vector( 0, 0, 64.0f );
	QAngle angItem( 0, EyeAngles().y - 90.0f, 0 );

	pItem->SetAbsOrigin( vecOrigin );
	pItem->SetAbsAngles( angItem );
	pItem->Spawn();

	// NOTE: the SDK's CItem::Use() returns void, but the original binary
	// returned a success bool from vtable index 64 (played
	// "HL2Player.UseDeny" + removed the item on failure). Assume success
	// until the per-item use logic is implemented.
	// TODO: recreate the original success/failure semantics.
	pItem->Use( this, this, USE_ON, (float)iItem );

	if ( UH_IsGlowstick( iItem ) )
	{
		// Using an unlit glowstick lights it in place of dropping it.
		m_iInventory.Set( iSlot, UH_GetLitGlowstickItem( iItem ) );
		engine->ClientCommand( edict(), "UpdateInventory" );
		return true;
	}

	UH_RemoveInventoryItem( iSlot );
	engine->ClientCommand( edict(), "UpdateInventory" );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Pure inventory command dispatcher. Only inventory commands here —
// objective / signaling commands live in uh_player_objectives.cpp for
// code quality / portability (see CHL2_Player::UH_HandleObjectiveCommand).
// Original: hexrays sub_102DDBF0 for switch/dropitem/useitem.
//-----------------------------------------------------------------------------
bool CHL2_Player::UH_HandleInventoryCommand( const CCommand &args )
{
	const char *pszCommand = args[0];

	if ( !Q_stricmp( pszCommand, "switch" ) )
	{
		if ( args.ArgC() > 2 )
		{
			int iSlotA = atoi( args[1] );
			int iSlotB = atoi( args[2] );

			if ( iSlotA >= 0 && iSlotA < UH_INVENTORY_SLOTS &&
				 iSlotB >= 0 && iSlotB < UH_INVENTORY_SLOTS )
			{
				int iTemp = m_iInventory[iSlotA];
				m_iInventory.Set( iSlotA, m_iInventory[iSlotB] );
				m_iInventory.Set( iSlotB, iTemp );

				engine->ClientCommand( edict(), "UpdateInventory" );
			}
		}
		return true;
	}

	if ( !Q_stricmp( pszCommand, "dropitem" ) )
	{
		if ( args.ArgC() > 1 )
		{
			UH_ItemAction( atoi( args[1] ), false );
		}
		return true;
	}

	if ( !Q_stricmp( pszCommand, "useitem" ) )
	{
		if ( args.ArgC() > 1 )
		{
			UH_ItemAction( atoi( args[1] ), true );
		}
		return true;
	}

	if ( !Q_stricmp( pszCommand, "UpdateInventory" ) )
	{
		UH_UpdateInventory();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: "UpdateInventory" server command (FCVAR_CLIENTCMD_CAN_EXECUTE).
// Original (hexrays sub_102DDDE0):
//   * marks every non-empty slot as network-changed so the owning client
//     receives the current contents,
//   * removes the item the player is currently holding in hands,
//   * tells the client to run "UpdateInventory" locally (refreshes the UI).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_UpdateInventory( void )
{
	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		if ( m_iInventory[i] != UH_ITEM_NONE )
		{
			// GetForModify() forces a network state change for the element
			// (original called StateChanged( 4928 + 4*i ) directly).
			m_iInventory.GetForModify( i );
		}
	}

	// TODO: remove the held item entity (original stored an EHANDLE at
	// player offset 2164, called a virtual on it, then removed the entity
	// and reset the handle). Needs the CBasePlayer held-item member.

	engine->ClientCommand( edict(), "UpdateInventory" );
}
