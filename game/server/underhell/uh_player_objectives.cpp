//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell objectives & map signaling — server-side CHL2_Player.
//
// Reconstructed 1:1 from the original server.dll:
//   * DispObj    — hexrays sub_101F11D0: find "Display_Objective" relay and trigger it
//   * GiveSign   — same func: find "GiveSignal" and trigger
//   * SkipScene  — same func: find "Relay_SkipScene" and trigger
//
// VMF evidence (uh_prologue_2_d.vmf, added by user):
//   Display_Objective (logic_relay)
//     OnTrigger -> MainObjective,ShowMessage
//   MainObjective (env_message)
//     messagesound "Underhell.NewObjective"
//     message set at runtime via logic_auto:
//       OnMapSpawn MainObjective,SetMessagePriority1,@titles_Prologue.txt_Prologue_2_Objective_A
//   So the text was not drawn because vanilla CMessage lacks SetMessagePriority1
//   input — only sound played. Fixed in envmessage.cpp by adding those inputs.
//
// Original CLogicRelay trigger path (sub_10180EC0):
//   if (!m_bDisabled@848 && !m_bWaitForRefire@849)
//     m_OnTrigger@800.FireOutput(activator, this);
//     ...
//
// This file is intentionally separate from uh_player_inventory.cpp for
// code quality / portability. Inventory stays pure inventory.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "globalstate.h"
#include "underhell/uh_inventory.h"

//-----------------------------------------------------------------------------
// Purpose: Generic helper — find entity by targetname and fire its Trigger
// input with this player as activator/caller. Mirrors the exact pattern
// used for DispObj/GiveSign/SkipScene in sub_101F11D0.
//
// Original used:
//   sub_1012BF20(&gEntList, 0, name, 0,0,0,0)  -> FindEntityByName
//   sub_10180EC0(entity, player)               -> InputTrigger
// AcceptInput("Trigger") performs the same disabled / wait checks.
// For robustness we also try Display (game_text) and ShowMessage (env_message)
// because Display_Objective could be overridden in custom maps.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_TriggerMapEntity( const char *pszTargetName )
{
	if ( !pszTargetName || !*pszTargetName )
		return;

	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, pszTargetName );
	if ( pEnt )
	{
		variant_t emptyVariant;
		// Original passed player in v53[0] as activator.
		pEnt->AcceptInput( "Trigger", this, this, emptyVariant, 0 );
		// Fallbacks for direct game_text / env_message usage:
		pEnt->AcceptInput( "Display", this, this, emptyVariant, 0 );
		pEnt->AcceptInput( "ShowMessage", this, this, emptyVariant, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: DispObj — display current objective (chapter task).
// 1:1 from sub_101F11D0 + VMF fix:
//   v10 = gEntList.FindEntityByName(0, "Display_Objective")
//   if (v10) InputTrigger(v10, player) -> MainObjective,ShowMessage
// If Display_Objective relay is missing (custom map), also try MainObjective
// directly, then fallback to direct HudMessage via UTIL_ShowMessage.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_DisplayObjective( void )
{
	// Primary path: original relay
	UH_TriggerMapEntity( "Display_Objective" );

	// Secondary: direct MainObjective env_message (as in uh_prologue_2_d.vmf)
	// This ensures text draws even if relay output was broken.
	CBaseEntity *pMain = gEntList.FindEntityByName( NULL, "MainObjective" );
	if ( pMain )
	{
		variant_t emptyVariant;
		pMain->AcceptInput( "ShowMessage", this, this, emptyVariant, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dispatcher for objective / signaling client commands.
// Called from CHL2_Player::ClientCommand, exactly like the original engine
// forwarded unknown client commands to CBasePlayer::ClientCommand.
//-----------------------------------------------------------------------------
bool CHL2_Player::UH_HandleObjectiveCommand( const CCommand &args )
{
	const char *pszCommand = args[0];

	if ( !Q_stricmp( pszCommand, "DispObj" ) )
	{
		UH_DisplayObjective();
		return true;
	}

	if ( !Q_stricmp( pszCommand, "GiveSign" ) )
	{
		UH_TriggerMapEntity( "GiveSignal" );
		return true;
	}

	if ( !Q_stricmp( pszCommand, "SkipScene" ) )
	{
		UH_TriggerMapEntity( "Relay_SkipScene" );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Dev/testing helper — not part of the original. Directly overrides the
// networked Hermit-card counters. Usage:
// uh_give_hermit_card [cards] [questcur] [questtotal]
//-----------------------------------------------------------------------------
CON_COMMAND_F( uh_give_hermit_card, "Sets hermit card / quest counters (dev/testing).", FCVAR_CHEAT )
{
	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player *>( UTIL_GetCommandClient() );
	if ( !pPlayer )
		return;

	int iCards = ( args.ArgC() > 1 ) ? atoi( args[1] ) : ( pPlayer->UH_GetHermitCardsCount() + 1 );
	int iQuestCur = ( args.ArgC() > 2 ) ? atoi( args[2] ) : 0;
	int iQuestTotal = ( args.ArgC() > 3 ) ? atoi( args[3] ) : 0;

	pPlayer->UH_SetHermitCards( iCards, iQuestCur, iQuestTotal );
}

//-----------------------------------------------------------------------------
// Chapter map inputs. DisplayHermitCards is sub_102E1B60: refresh the three
// networked values from persistent env_global counters, then toggle the HUD.
//-----------------------------------------------------------------------------
void CHL2_Player::InputDisplayHermitCards( inputdata_t &inputdata )
{
	const int cards = GlobalEntity_IsInTable( "GC_HermitCards" ) ?
		GlobalEntity_GetCounter( "GC_HermitCards" ) : 0;
	const int questTotal = GlobalEntity_IsInTable( "GC_HermitQuest_Total" ) ?
		GlobalEntity_GetCounter( "GC_HermitQuest_Total" ) : 0;
	const int questCurrent = GlobalEntity_IsInTable( "GC_HermitQuest_Current" ) ?
		GlobalEntity_GetCounter( "GC_HermitQuest_Current" ) : 0;

	m_iUHHermitCardsCount = cards;
	m_iUHHermitTotalQuestCount = questTotal;
	m_iUHHermitCurrentQuestCount = questCurrent;
	m_bDisplayHermitCard = !m_bDisplayHermitCard;
}

void CHL2_Player::InputDisableInventory( inputdata_t &inputdata )
{
	m_bInventoryEnabled = false;
}

void CHL2_Player::InputEnableInventory( inputdata_t &inputdata )
{
	m_bInventoryEnabled = true;
}

void CHL2_Player::InputRemoveEndurance( inputdata_t &inputdata )
{
	m_iEndurance = max( 0, m_iEndurance - max( 0, inputdata.value.Int() ) );
}

void CHL2_Player::InputBleedPlayer( inputdata_t &inputdata )
{
	// Chapter 1 passes 0 while entering cinematics/dialogue: this closes the
	// current wound. A true input opens a minimum wound for mapper use.
	if ( inputdata.value.Bool() )
		m_iBleedCounter = max( m_iBleedCounter, 10 );
	else
		m_iBleedCounter = 0;
	m_flLastBleedTime = gpGlobals->curtime;
}

void CHL2_Player::InputRemoveLitGlowstick( inputdata_t &inputdata )
{
	CBaseEntity *pGlowLight = UH_GetActiveGlowStickLight();
	if ( pGlowLight )
		UTIL_Remove( pGlowLight );
	UH_SetActiveGlowStickLight( NULL );

	CBaseEntity *pGlow = UH_GetActiveGlowStick();
	if ( pGlow )
		UTIL_Remove( pGlow );
	UH_SetActiveGlowStick( NULL );

	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		if ( UH_IsLitGlowstick( m_iInventory[i] ) )
			m_iInventory.Set( i, UH_ITEM_NONE );
	}
	engine->ClientCommand( edict(), "UpdateInventory" );
}

void CHL2_Player::InputSetStatusVisibility( inputdata_t &inputdata )
{
	// sub_101EEAC0 clears these custom/status bits first. Value 0 then hides
	// the complete status presentation; value 1 leaves it visible.
	const int statusMask = 0x0000F109; // 1|8|0x100|0x1000|0x2000|0x4000|0x8000
	m_Local.m_iHideHUD &= ~statusMask;

	const int mode = inputdata.value.Int();
	if ( mode == 0 )
	{
		if ( m_bIronSighted )
			UH_ToggleIronsight();
		if ( m_bGasMaskOn )
			UH_ToggleGasMask();
		if ( m_bNightVisionOn )
			UH_ToggleNightVision();
		m_Local.m_iHideHUD |= statusMask;
	}
	else if ( mode == 1 )
	{
		m_bNightVisionEnabled = true;
		m_bGasMaskEnabled = true;
	}
	else if ( mode == 2 )
	{
		if ( m_bGasMaskOn )
			UH_ToggleGasMask();
		if ( m_bNightVisionOn )
			UH_ToggleNightVision();
		m_Local.m_iHideHUD |= ( 1 | 8 | 0x100 | 0x4000 );
	}
}

//-----------------------------------------------------------------------------
// Temporary interaction outline (original sub_102E16B0). Every 0.15 seconds
// trace 600 units through the camera center; usable items within 96 units glow
// unless a mapper-owned hard glow is already controlling the entity.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_UpdateLookGlow( void )
{
	if ( gpGlobals->curtime < m_flNextUHLookGlowTime )
		return;
	m_flNextUHLookGlowTime = gpGlobals->curtime + 0.15f;

	CBaseEntity *oldTarget = m_hUHLookGlowTarget.Get();
	if ( oldTarget )
		oldTarget->SetUHAutomaticGlow( false );
	m_hUHLookGlowTarget = NULL;
	if ( !IsAlive() )
		return;

	Vector forward;
	EyeVectors( &forward );
	trace_t tr;
	UTIL_TraceLine( EyePosition(), EyePosition() + forward * 600.0f,
		0x46004003, this, COLLISION_GROUP_NONE, &tr );

	CBaseEntity *target = tr.m_pEnt;
	if ( !target || tr.DidHitWorld() || target == this || EyePosition().DistTo( tr.endpos ) > 96.0f )
		return;

	const char *classname = target->GetClassname();
	const bool itemClass = classname &&
		( !Q_strnicmp( classname, "item_", 5 ) || !Q_strnicmp( classname, "weapon_", 7 ) );

	// Automatic crosshair glow is only for real item/weapon entities. Quest
	// props, doors, vehicles and NPCs are controlled by the mapper's Glow input;
	// m_bHardGlow keeps that state independent of this temporary target.
	if ( !itemClass || !( target->ObjectCaps() & FCAP_IMPULSE_USE ) )
		return;

	target->SetUHAutomaticGlow( true );
	m_hUHLookGlowTarget = target;
}
