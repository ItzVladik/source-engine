//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell ironsight — server-side CHL2_Player.
//
// "ironsight_toggle" is a client command (not a ConCommand): the engine
// forwards it into CHL2_Player::ClientCommand, which toggles the ironsight.
// Decoded from sub_101ECF40 (toggle) + sub_101F11D0 (ClientCommand dispatch).
//
// State, all networked (mirrors the original binary):
//   - CBaseViewModel::m_bExpSighted  — viewmodel slide target (client reads it
//     in CalcViewModelView and interpolates m_expFactor). Server sendtable
//     offset 1120 / client recv offset 1960 (hexrays sub_100F8EA0/10015160).
//   - CHL2_Player::m_bIronSighted     — authoritative flag (accuracy + HUD).
//   - CHL2_Player::m_fIronsightedTime — last toggle time (debounce).
//
// Ironsight FOV uses uh_ironsight_zoom (default 0.9). The separate +zoom focus
// subtracts uh_ironsight_zoom_focus (40) from default FOV. Ironsight movement
// switches to hl2_walkspeed and restores hl2_normspeed.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "baseviewmodel_shared.h"
#include "uh_weapons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The sighted FOV is GetDefaultFOV() * uh_ironsight_zoom (original convar
// "uh_ironsight_zoom", default 0.9 — a mild zoom; the real sighting effect is
// the viewmodel sliding to the eye). Not FCVAR_ARCHIVE in the original.
static ConVar uh_ironsight_zoom( "uh_ironsight_zoom", "0.9", 0,
	"FOV multiplier when ironsighted (sighted FOV = defaultFOV * this)." );

// Suit +zoom subtracts this value from the default FOV (sub_102DEE20).
// Ironsight itself uses hl2_walkspeed/hl2_normspeed for movement.
ConVar uh_ironsight_zoom_focus( "uh_ironsight_zoom_focus", "40", FCVAR_ARCHIVE,
	"Amount subtracted from default FOV by the +zoom focus control." );

// FOV transition duration in seconds (original passes a data-section constant,
// flt_1063C554, that Hex-Rays could not resolve — reconstructed to match the
// ~0.1 s viewmodel slide, VDC "Adding Ironsights" gMoveTime).
#define UH_IRONSIGHT_FOV_TIME 0.1f

// Minimum time between ironsight toggles (original sub_101ECF40 debounce).
#define UH_IRONSIGHT_TOGGLE_DELAY 0.1f

extern ConVar hl2_walkspeed;
extern ConVar hl2_normspeed;

// Enter slightly earlier than we leave to prevent the animation oscillating
// when the muzzle sits exactly on a wall plane.
#define UH_WEAPON_OBSTRUCTION_ENTER 36.0f
#define UH_WEAPON_OBSTRUCTION_LEAVE 44.0f

//-----------------------------------------------------------------------------
// Purpose: Drive the weapon's authored raised/sideways pose when its barrel is
// close to solid geometry. The original has a distinct raised state using
// activities 204/205/206; the friendly/scripted lowered state (201/202/203) is
// not the wall-obstruction pose.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_UpdateWeaponObstruction( void )
{
	CBaseHLCombatWeapon *pWeapon = dynamic_cast<CBaseHLCombatWeapon *>( GetActiveWeapon() );
	if ( !IsAlive() || !pWeapon || pWeapon->GetWpnData().m_bMeleeWeapon ||
		 pWeapon->SelectWeightedSequence( ACT_VM_IDLE_RAISED ) == ACTIVITY_NOT_AVAILABLE )
	{
		if ( m_bUHWeaponObstructed )
		{
			m_bUHWeaponObstructed = false;
			if ( pWeapon )
				pWeapon->SendWeaponAnim( ACT_VM_RAISED_TO_IDLE );
		}
		return;
	}

	const float flDistance = m_bUHWeaponObstructed ?
		UH_WEAPON_OBSTRUCTION_LEAVE : UH_WEAPON_OBSTRUCTION_ENTER;
	Vector vecDirection = static_cast<CBasePlayer *>( this )->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT, flDistance );
	Vector vecStart = Weapon_ShootPosition();

	trace_t tr;
	UTIL_TraceHull( vecStart, vecStart + vecDirection * flDistance,
		Vector( -2.0f, -2.0f, -2.0f ), Vector( 2.0f, 2.0f, 2.0f ),
		MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	const bool bObstructed = tr.startsolid || tr.fraction < 1.0f;
	if ( bObstructed == m_bUHWeaponObstructed )
		return;

	m_bUHWeaponObstructed = bObstructed;
	if ( bObstructed )
	{
		UH_DisableIronsight();
		if ( pWeapon->SelectWeightedSequence( ACT_VM_IDLE_TO_RAISED ) != ACTIVITY_NOT_AVAILABLE )
			pWeapon->SendWeaponAnim( ACT_VM_IDLE_TO_RAISED );
		else
			pWeapon->SendWeaponAnim( ACT_VM_IDLE_RAISED );
	}
	else
	{
		if ( pWeapon->SelectWeightedSequence( ACT_VM_RAISED_TO_IDLE ) != ACTIVITY_NOT_AVAILABLE )
			pWeapon->SendWeaponAnim( ACT_VM_RAISED_TO_IDLE );
		else
			pWeapon->SendWeaponAnim( ACT_VM_IDLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Toggle ironsight (called from the "ironsight_toggle" client
// command). Mirrors sub_101ECF40: debounce, drop sprint, toggle the networked
// flags, zoom the FOV, slow to aim-walk speed, and play the enter/leave sound.
//
// The FOV is set directly (m_iFOV + m_flFOVTime + m_iFOVStart + m_flFOVRate),
// NOT through CBasePlayer::SetFOV. SetFOV claims m_hZoomOwner (which the
// original's custom setter sub_100F8040 also does, passing the player itself);
// going through it makes IsZooming() return true while only ironsighted and
// collides with the vanilla suit zoom (CheckSuitZoom / Weapon_Switch call
// StopZooming on IN_ZOOM). A direct set produces the same GetFOV() result
// without touching m_hZoomOwner — intentional, documented divergence.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ToggleIronsight( void )
{
	// Debounce.
	if ( gpGlobals->curtime - m_fIronsightedTime < UH_IRONSIGHT_TOGGLE_DELAY )
		return;

	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( !pWeapon )
		return;

	// Preconditions (sub_101ECF40):
	//  - not a melee weapon (weapon script "MeleeWeapon" flag → info.m_bMeleeWeapon,
	//    hexrays sub_100D0E00 reads weapon-info offset 1832). No escape clause:
	//    a melee weapon can never sight.
	//  - lowered/hard-lowered blocks entering sight, with an escape allowing an
	//    already sighted player to leave it.
	if ( pWeapon->GetWpnData().m_bMeleeWeapon )
		return;
	if ( m_bUHWeaponObstructed && !m_bIronSighted )
		return;

	// Can't sight while sprinting — drop sprint first.
	if ( IsSprinting() )
	{
		StopSprinting();
	}

	CBaseViewModel *pVm = GetViewModel();
	if ( !pVm )
		return;

	// Hide the crosshair while sighted (original toggles bit 256 =
	// HIDEHUD_CROSSHAIR regardless of direction).
	m_Local.m_iHideHUD ^= HIDEHUD_CROSSHAIR;

	// Toggle the viewmodel's networked flag (original @1120). It is the source
	// the client reads to slide the gun up to the eye.
	bool bSighted = !pVm->IsExpSighted();
	pVm->SetExpSighted( bSighted );

	if ( bSighted )
	{
		// Entering ironsight.
		ClearUseEntity();
		EmitSound( "HL2Player.Ironsighton" );

		// Sighted FOV = defaultFOV * uh_ironsight_zoom (original multiplies).
		int iFOV = (int)( (float)GetDefaultFOV() * uh_ironsight_zoom.GetFloat() );

		m_iFOVStart = GetFOV();
		m_flFOVTime = gpGlobals->curtime;
		m_iFOV = iFOV;
		m_Local.m_flFOVRate = UH_IRONSIGHT_FOV_TIME;

		m_bIronSighted = true;

		// sub_101ECF40 enters aim movement at hl2_walkspeed (150).
		if ( hl2_walkspeed.GetFloat() < MaxSpeed() )
			SetMaxSpeed( hl2_walkspeed.GetFloat() );
	}
	else
	{
		// Leaving ironsight.
		EmitSound( "HL2Player.Ironsightoff" );

		m_bIronSighted = false;

		// FOV 0 = "use default" in GetFOV().
		m_iFOVStart = GetFOV();
		m_flFOVTime = gpGlobals->curtime;
		m_iFOV = 0;
		m_Local.m_flFOVRate = UH_IRONSIGHT_FOV_TIME;

		// Leaving ironsight restores hl2_normspeed (190).
		if ( hl2_normspeed.GetFloat() > MaxSpeed() )
			SetMaxSpeed( hl2_normspeed.GetFloat() );
	}

	m_fIronsightedTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Force ironsight off without the debounce/sound (used when the
// weapon changes or is dropped so the FOV can't stay zoomed). Restores every
// piece of state UH_ToggleIronsight changed.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_DisableIronsight( void )
{
	if ( !m_bIronSighted )
		return;

	m_bIronSighted = false;

	CBaseViewModel *pVm = GetViewModel();
	if ( pVm )
		pVm->SetExpSighted( false );

	// Restore the crosshair (clear the bit the toggle set).
	m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;

	m_iFOVStart = GetFOV();
	m_flFOVTime = gpGlobals->curtime;
	m_iFOV = 0;
	m_Local.m_flFOVRate = UH_IRONSIGHT_FOV_TIME;

	if ( hl2_normspeed.GetFloat() > MaxSpeed() )
		SetMaxSpeed( hl2_normspeed.GetFloat() );

	m_fIronsightedTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the active weapon's silencer (called from the
// "silencer_toggle" client command). Decoded from sub_101E2F50:
//   - pistols (GetWeaponType() == 1) require m_bHavePistolSilencer,
//   - rifles (GetWeaponType() == 4) require m_bHaveRifleSilencer,
//   - everything else (SMG / shotgun / BFG) toggles freely.
// TODO: the original also skips when the weapon is lowered/holstered
// (weapon flags @1144/@1145).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ToggleSilencer( void )
{
	CUHGunWeapon *pWeapon = dynamic_cast<CUHGunWeapon *>( GetActiveWeapon() );
	if ( !pWeapon )
		return;

	int iType = pWeapon->GetWeaponType();
	if ( ( iType == 1 && !m_bHavePistolSilencer ) ||
		 ( iType == 4 && !m_bHaveRifleSilencer ) )
	{
		return;
	}

	bool bSilenced = !pWeapon->IsSilenced();
	pWeapon->SetSilenced( bSilenced );

	// Play the attach / detach animation (ACT_VM_ATTACH_SILENCER /
	// ACT_VM_DETACH_SILENCER). SendWeaponAnim falls back to idle if the
	// viewmodel lacks the sequence.
	pWeapon->SendWeaponAnim( bSilenced ? ACT_VM_ATTACH_SILENCER : ACT_VM_DETACH_SILENCER );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the laser sight. Toggles m_bLaserToggleState (networked);
// the client draws a beam + impact dot (see uh_laser.cpp) while it is set.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ToggleLaser( void )
{
	// The laser sight belongs to the SOCOM path; keeping a global player flag
	// but allowing every weapon to toggle it made the beam appear on unrelated
	// pistols, SMGs and rifles.
	CUHGunWeapon *pWeapon = dynamic_cast<CUHGunWeapon *>( GetActiveWeapon() );
	if ( !pWeapon || !FClassnameIs( pWeapon, "weapon_pistol_socom" ) )
	{
		m_bLaserToggleState = false;
		return;
	}

	// Keep the compatibility command on the same authoritative path as
	// IN_ATTACK2: weapon state, sprite lifecycle and sequences 12/13 must toggle
	// together rather than changing only the player's network HUD bit.
	pWeapon->SecondaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose: Give / take the pistol or rifle silencer (entity inputs
// "SetPistolSilencer" / "SetRifleSilencer"). The original reads a FIELD_BOOLEAN
// parameter (sub_101E36F0 / sub_101E3720), so "1" grants the silencer and "0"
// removes it — see the Entity List tutorial ("setpistolsilencer 1" / "... 0").
//-----------------------------------------------------------------------------
void CHL2_Player::InputSetPistolSilencer( inputdata_t &inputdata )
{
	m_bHavePistolSilencer = inputdata.value.Bool();
}

void CHL2_Player::InputSetRifleSilencer( inputdata_t &inputdata )
{
	m_bHaveRifleSilencer = inputdata.value.Bool();
}

//-----------------------------------------------------------------------------
// Purpose: Throw the active weapon forward (client command "DropWeapon").
// Decoded from sub_101F11D0: un-sight, refuse while in a vehicle / sprinting /
// holding a melee weapon, then Weapon_Drop with a forward velocity of 300.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_DropWeapon( void )
{
	if ( m_bDisableWeaponDrop )
		return;

	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( !pWeapon )
		return;

	// Come out of ironsight so the FOV / viewmodel don't stay zoomed.
	if ( m_bIronSighted )
		UH_ToggleIronsight();

	// Can't drop while in a vehicle.
	if ( GetVehicle() )
		return;

	// The original refuses to drop melee weapons (weapon-info MeleeWeapon flag).
	if ( pWeapon->GetWpnData().m_bMeleeWeapon )
		return;

	// Can't drop while sprinting.
	if ( IsSprinting() )
		return;

	// Toss it forward (original velocity = forward * 300).
	Vector forward;
	AngleVectors( EyeAngles(), &forward );
	Vector velocity = forward * 300.0f;

	Weapon_Drop( pWeapon, NULL, &velocity );

	// Underhell: re-evaluate the left-arm flashlight after the drop.
	UH_UpdateLeftArm();
}

//-----------------------------------------------------------------------------
// Purpose: Block / allow the "DropWeapon" command (map inputs, original
// "DisableDropWeapon" / "EnableDropWeapon" — datamap sub_101F2D30).
//-----------------------------------------------------------------------------
void CHL2_Player::InputDisableDropWeapon( inputdata_t &inputdata )
{
	m_bDisableWeaponDrop = true;
}

void CHL2_Player::InputEnableDropWeapon( inputdata_t &inputdata )
{
	m_bDisableWeaponDrop = false;
}
