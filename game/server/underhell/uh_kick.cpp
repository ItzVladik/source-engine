//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell kick attack ("uh_jake_kick"). Decoded from the original
//          CHL2_Player::ClientCommand dispatch (sub_101F11D0) and the kick
//          think/impact handlers (sub_101F2990 / sub_101F0050 / sub_101E5A60).
//
// Flow (matches the original):
//   1. command "uh_jake_kick": gate checks -> drain 20 suit power, raise the
//      kick viewmodel (index 2, v_kick_jake_*.mdl), viewpunch + rumble + swing
//      sound, schedule UH_KickThink +0.35s.
//   2. UH_KickThink (first pass): do the forward trace + damage + force +
//      impact sound + OnKicked output, schedule +0.4s.
//   3. UH_KickThink (second pass): holster the kick viewmodel, clear the flags.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "baseviewmodel_shared.h"
#include "entityoutput.h"
#include "soundent.h"
#include "basepropdoor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Kick trace mask (decode sub_101F0050: 100679691 = 0x600400B).
#define UH_KICK_MASK (CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_GRATE|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_DEBRIS)

#define UH_KICK_REACH		72.0f		// forward reach of the kick (units)
#define UH_KICK_COST		20.0f		// suit power drained per kick
#define UH_KICK_WIND_DELAY	0.35f		// command -> strike
#define UH_KICK_RECOVER		0.40f		// strike -> holster

static ConVar uh_kick_damage( "uh_kick_damage", "21", FCVAR_ARCHIVE, "Damage dealt by the player kick." );
static ConVar uh_kick_forcemult( "uh_kick_forcemult", "2", FCVAR_ARCHIVE, "Force multiplier applied to the player kick." );
static ConVar uh_kick_enabled( "uh_kick_enabled", "1", FCVAR_CHEAT, "Enable the player kick." );

//-----------------------------------------------------------------------------
// Purpose: gate checks for the kick.
//-----------------------------------------------------------------------------
bool CHL2_Player::UH_CanKick( void )
{
	if ( !uh_kick_enabled.GetBool() )
		return false;
	if ( m_bKickDisabled )
		return false;
	if ( m_bKickActive )
		return false;
	if ( !IsAlive() )
		return false;
	if ( GetVehicle() )
		return false;
	if ( IsSprinting() )
		return false;
	// Not enough sprint stamina to kick.
	if ( m_HL2Local.m_flSuitPower < UH_KICK_COST )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: set + precache the kick viewmodel (index 2) model.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_SetKickViewModel( const char *pszModel )
{
	if ( !pszModel || !*pszModel )
		return;

	PrecacheModel( pszModel );
	m_iszKickViewModel = AllocPooledString( pszModel );

	CBaseViewModel *vm = GetViewModel( 2 );
	if ( vm )
	{
		vm->SetWeaponModel( pszModel, NULL );
		vm->SendViewModelMatchingSequence( 1 );	// holstered / idle pose
	}
}

//-----------------------------------------------------------------------------
// Purpose: perform the forward kick trace + damage + force + impact sound and
// fire OnKicked on whatever was hit.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_DoKickStrike( void )
{
	// Original sub_101F0050 calls GetAutoaimVector( 1.0, 72.0 ), so the leg
	// follows the same bounded free-aim ray as firearms instead of raw eye
	// pitch.  Using EyeVectors here allowed upward kicks into ceilings.
	Vector vecForward = static_cast<CBasePlayer *>( this )->GetAutoaimVector( 1.0f, UH_KICK_REACH );
	// Match the index-2 viewmodel clamp in Cliento sub_10014D80: the kick may
	// angle down, but never above the player's horizontal facing plane.
	if ( vecForward.z > 0.0f )
	{
		vecForward.z = 0.0f;
		VectorNormalize( vecForward );
	}

	Vector vecOrigin = EyePosition();
	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + vecForward * UH_KICK_REACH, UH_KICK_MASK, this, COLLISION_GROUP_NONE, &tr );

	// sub_101F0050 follows the 72-unit line with a short hull pass when the
	// line misses. This makes close doors/props and broad NPC bodies register
	// reliably without allowing a hit behind the player.
	if ( tr.fraction == 1.0f )
	{
		trace_t hullTrace;
		Vector vecHullEnd = vecOrigin + vecForward * ( UH_KICK_REACH - 55.424f );
		UTIL_TraceHull( vecOrigin, vecHullEnd, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ),
			UH_KICK_MASK, this, COLLISION_GROUP_NONE, &hullTrace );
		if ( hullTrace.fraction < 1.0f && hullTrace.m_pEnt )
		{
			Vector vecToTarget = hullTrace.m_pEnt->WorldSpaceCenter() - vecOrigin;
			VectorNormalize( vecToTarget );
			if ( DotProduct( vecToTarget, vecForward ) >= 0.70721f )
				tr = hullTrace;
		}
	}

	// Register the kick as combat noise so nearby NPCs react.
	CSoundEnt::InsertSound( SOUND_COMBAT, tr.endpos, 400, 0.2f, this );

	// Small forward view punch (decode sub_101F0050: (-2, 0, 0)).
	ViewPunch( QAngle( -2.0f, 0.0f, 0.0f ) );

	// Contact rumble in sub_101F0050 is (9, 0, 4); a second lighter (4, 0, 4)
	// pulse is emitted below only on an actual entity hit.
	RumbleEffect( 9, 0, 4 );

	if ( tr.fraction >= 1.0f || !tr.m_pEnt )
	{
		EmitSound( "HL2Player.kick_fire" );
		return;
	}

	CBaseEntity *pHit = tr.m_pEnt;

	// Impact sound: body vs world.
	if ( pHit->MyCombatCharacterPointer() )
		EmitSound( "HL2Player.kick_body" );
	else
		EmitSound( "HL2Player.kick_wall" );

	// Damage (DMG_CLUB melee; attacker = player).
	CTakeDamageInfo info( this, this, uh_kick_damage.GetFloat(), DMG_CLUB );
	info.SetDamagePosition( tr.endpos );

	// Use the engine's melee-force curve then apply the original configurable
	// multiplier. A constant 300-unit push made doors and heavy props react
	// very differently from the binary.
	CalculateMeleeDamageForce( &info, vecForward, tr.endpos, uh_kick_forcemult.GetFloat() );

	// The impact path adds its own, lighter rumble pulse after a real hit.
	RumbleEffect( 4, 0, 4 );

	// sub_101E5A60 calls the dedicated CBasePropDoor breach virtual before the
	// generic OnKicked output and damage-force path.
	CBasePropDoor *pPropDoor = dynamic_cast<CBasePropDoor *>( pHit );
	if ( pPropDoor && pHit->IsUHKickableDoor() && !pPropDoor->IsDoorLocked() )
		pPropDoor->UHBreachDoor( this, this, false, tr.endpos );

	// The VMF's breakaway doors use OnKicked -> EnableMotion. Fire it before
	// applying damage so the subsequent impulse reaches an enabled physbox.
	pHit->FireOnKicked( this );
	pHit->TakeDamage( info );

}

//-----------------------------------------------------------------------------
// Purpose: think context for the kick. First pass = strike; second pass =
// holster + reset.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_KickThink( void )
{
	if ( m_bKickMarker )
	{
		// Strike already resolved: holster the kick viewmodel and finish.
		m_bKickMarker = false;

		CBaseViewModel *vm = GetViewModel( 2 );
		if ( vm )
			vm->SendViewModelMatchingSequence( 1 );

		m_bKickActive = false;
		return;
	}

	m_bKickMarker = true;
	UH_DoKickStrike();

	SetContextThink( &CHL2_Player::UH_KickThink, gpGlobals->curtime + UH_KICK_RECOVER, "KickContext" );
}

//-----------------------------------------------------------------------------
// Purpose: "uh_jake_kick" command handler (dispatch sub_101F11D0).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_Kick( void )
{
	if ( m_bKickDisabled )
	{
		m_OnDisabledKickAttempted.FireOutput( this, this );
		return;
	}

	if ( !UH_CanKick() )
		return;

	// Kick cancels ironsight.
	if ( m_bIronSighted )
		UH_ToggleIronsight();

	// Consume sprint stamina.
	SuitPower_Drain( UH_KICK_COST );

	m_bKickActive = true;

	// Raise the kick viewmodel (index 2).
	CBaseViewModel *vm = GetViewModel( 2 );
	if ( vm )
		vm->SendViewModelMatchingSequence( 0 );

	// Forward view punch (decode sub_101F11D0: (-2, 0, 0)).
	ViewPunch( QAngle( -2.0f, 0.0f, 0.0f ) );

	// Swing sound: airborne = "fly" variant.
	if ( GetFlags() & FL_ONGROUND )
		EmitSound( "HL2Player.kick_fire" );
	else
		EmitSound( "HL2Player.kick_fire_fly" );

	// Exertion voice.
	if ( m_HL2Local.m_flSuitPower < 35.0f )
		EmitSound( "Player.Voice.Kick.Exhausted" );
	else
		EmitSound( "Player.Voice.Kick" );

	SetContextThink( &CHL2_Player::UH_KickThink, gpGlobals->curtime + UH_KICK_WIND_DELAY, "KickContext" );
}

//-----------------------------------------------------------------------------
// Purpose: DisableKick / EnableKick inputs (fired at !player from the maps).
//-----------------------------------------------------------------------------
void CHL2_Player::InputDisableKick( inputdata_t &inputdata )
{
	m_bKickDisabled = true;
}

void CHL2_Player::InputEnableKick( inputdata_t &inputdata )
{
	m_bKickDisabled = false;
}
