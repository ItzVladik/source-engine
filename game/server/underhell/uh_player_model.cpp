//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell player model / skin / viewmodel-skin / kick-model inputs.
//          Decoded from the original CHL2_Player datamap (server sub_101F2D30)
//          and the input handlers (sub_101EEE40 = "ViewModelSkin"). These are
//          fired at "!player" from the maps, e.g. Uh_House_1_d.vmf OnMapSpawn:
//            setplayermodel models/player/jake_casual.mdl
//            setplayerkickmodel models/weapons/v_kick_jake_casual.mdl
//            setplayerskin 1 / viewmodelskin 0
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "baseviewmodel_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Set the player model (third person / reflection). Also flags the
// model mirror-only (m_bIsMirrorOnly) so it renders in func_reflective_glass
// but never in the normal first-person world view — the "player in mirrors"
// system from the Player Model & Mirror Reflections tutorial.
//-----------------------------------------------------------------------------
void CHL2_Player::InputSetPlayerModel( inputdata_t &inputdata )
{
	const char *pszModel = inputdata.value.String();
	if ( !pszModel || !*pszModel )
		return;

	PrecacheModel( pszModel );
	SetModel( pszModel );

	// SetModel alone preserves the previous model's sequence number.  Rebind
	// that number against the new studio header immediately.  Do not call
	// SetAnimation( PLAYER_IDLE ) here: with no weapon deployed yet the stock
	// player code looks for the incomplete name "ref_aim_" and falls back to
	// sequence 0, which is the bind/T pose in Jake's models.  PostThink will
	// subsequently choose idle/walk/attack every frame.
	int iIdle = SelectWeightedSequence( ACT_IDLE );
	if ( iIdle >= 0 )
	{
		SetActivity( ACT_IDLE );
		ResetSequence( iIdle );
		SetCycle( 0.0f );
		ResetSequenceInfo();
	}

	// Only render this body in mirrors/monitors.
	SetMirrorOnly( true );
}

//-----------------------------------------------------------------------------
// Purpose: Set the player skin (integer, "SetPlayerSkin").
//-----------------------------------------------------------------------------
void CHL2_Player::InputSetPlayerSkin( inputdata_t &inputdata )
{
	m_nSkin = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: Set the skin of both viewmodels ("ViewModelSkin" integer). This is
// the hand/glove texture switch (see sub_101EEE40, which sets m_nSkin @848 on
// each of the player's two viewmodels).
//-----------------------------------------------------------------------------
void CHL2_Player::InputViewModelSkin( inputdata_t &inputdata )
{
	int nSkin = inputdata.value.Int();

	for ( int i = 0; i < MAX_VIEWMODELS; ++i )
	{
		CBaseViewModel *pVM = GetViewModel( i );
		if ( pVM )
			pVM->m_nSkin = nSkin;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the kick-attack viewmodel ("SetPlayerKickModel", e.g.
// models/weapons/v_kick_jake_casual.mdl). Precaches the model, remembers it and
// applies it to the kick viewmodel (index 2) immediately.
//-----------------------------------------------------------------------------
void CHL2_Player::InputSetPlayerKickModel( inputdata_t &inputdata )
{
	const char *pszModel = inputdata.value.String();
	if ( !pszModel || !*pszModel )
		return;

	UH_SetKickViewModel( pszModel );
}

//-----------------------------------------------------------------------------
// Purpose: "Give" input — mirrors the vanilla "give" ConCommand (client.cpp).
// item_suit is special-cased so it doesn't play the pickup sound.
//-----------------------------------------------------------------------------
void CHL2_Player::InputGive( inputdata_t &inputdata )
{
	const char *psz = inputdata.value.String();
	if ( !psz || !*psz )
		return;

	if ( !Q_stricmp( psz, "item_suit" ) )
	{
		EquipSuit( false );
		return;
	}

	GiveNamedItem( psz );
}

//-----------------------------------------------------------------------------
// Purpose: "GiveInv" input — hands the named weapon/item to the player. The
// maps use it for both weapons (weapon_bfg_minigun) and items (item_heavyarmor);
// GiveNamedItem routes weapons through Use -> BumpWeapon (one-weapon-per-slot
// replace) and Underhell items through Use -> MyTouch (pickup into inventory).
//-----------------------------------------------------------------------------
void CHL2_Player::InputGiveInv( inputdata_t &inputdata )
{
	const char *psz = inputdata.value.String();
	if ( !psz || !*psz )
		return;

	GiveNamedItem( psz );
}
