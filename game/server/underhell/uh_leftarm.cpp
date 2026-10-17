//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell second hand / left arm (viewmodel index 1).
//
// The left arm holds the flashlight (while a one-handed weapon is active), a
// flare (from a flare pack), or a grenade mid-throw. Decoded from the original
// binary:
//   - Throw_Nade (sub_101ED130): staged throw — the grenade viewmodel goes into
//     the left hand, the arm raises, and the actual throw happens 0.4 s later
//     from the "FlashLightContext" think (sub_101EE050).
//   - Flashlight deploy (sub_101F0C60): toggles the hand-held flashlight
//     viewmodel in the left arm (vs the shoulder-mounted flashlight).
//   - Flare throw (sub_101E9580): spawns a lit flare prop_physics with a 90 s
//     fuse and a 200 u/s throw velocity.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "props.h"
#include "baseviewmodel_shared.h"
#include "ammodef.h"
#include "grenade_frag.h"
#include "hl2/weapon_flaregun.h"
#include "sprite.h"
#include "hl2/info_darknessmode_lightsource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *UH_LEFTARM_GRENADE    = "models/weapons/v_grenade.mdl";
static const char *UH_LEFTARM_FLASHLIGHT = "models/weapons/v_flashlight_pg.mdl";
static const char *UH_LEFTARM_FLARE      = "models/weapons/v_flare_pg.mdl";
static const char *UH_FLARE_PROP         = "models/PG_props/pg_obj/pg_flare.mdl";

#define UH_THROW_STAGE_DELAY  0.4f   // command -> grenade actually leaves the hand
#define UH_FLARE_FUSE         90.0f  // total burn starts when equipped
static ConVar uh_flare_throw_scale( "uh_flare_throw_scale", "1200", FCVAR_ARCHIVE );
CBaseEntity *CreateFlare( Vector vOrigin, QAngle angles, CBaseEntity *pOwner, float flDuration );
#define UH_GRENADE_TIMER      3.0f   // frag detonation fuse (vanilla GRENADE_TIMER)

//-----------------------------------------------------------------------------
// Purpose: set (or clear) the left-arm viewmodel's model, skin and raised state.
// Pass model == NULL to holster the arm entirely (hide the viewmodel).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_SetLeftArmModel( const char *pszModel, int nSkin, bool bDeployed )
{
	CBaseViewModel *vm = GetViewModel( 1 );
	if ( !vm )
		return;

	if ( pszModel && *pszModel )
	{
		vm->SetWeaponModel( pszModel, NULL );
		vm->m_nSkin = nSkin;
		vm->RemoveEffects( EF_NODRAW );
		// Sequence 0 = raised/idle, 1 = holstered (matches the kick viewmodel).
		vm->SendViewModelMatchingSequence( bDeployed ? 0 : 1 );
	}
	else
	{
		// Nothing in the left hand: clear the model and hide the viewmodel.
		vm->SetWeaponModel( NULL, NULL );
		vm->AddEffects( EF_NODRAW );
	}

	m_bLeftArmDeployed = bDeployed;
}

//-----------------------------------------------------------------------------
// Purpose: Decide what the left arm should be holding right now. Priority:
// grenade throw in progress > flare > hand-held flashlight > nothing.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_UpdateLeftArm( void )
{
	// A grenade throw is a transient 0.4 s state; leave it alone.
	if ( m_bFlareMarker )
		return;

	if ( m_bHoldingFlare )
	{
		UH_SetLeftArmModel( UH_LEFTARM_FLARE, 1, true );
		return;
	}

	// Clean up a held-only visual if a save/input cleared the flare state.
	if ( m_hHeldFlareSprite ) { UTIL_Remove( m_hHeldFlareSprite ); m_hHeldFlareSprite = NULL; }
	if ( m_hHeldFlareEffect ) { UTIL_Remove( m_hHeldFlareEffect ); m_hHeldFlareEffect = NULL; }

	// Hand-held (non-shoulder) flashlight, raised while on and the left arm is
	// free: no weapon, a one-handed weapon (pistol) or melee. Must match
	// FlashlightTurnOn's gate exactly, otherwise the flashlight can be ON but
	// its left-hand viewmodel never appears.
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	bool bLeftArmFree = !pWeapon || pWeapon->GetWpnData().m_bOneHanded || pWeapon->GetWpnData().m_bMeleeWeapon;

	bool bWantFlashlight = FlashlightIsOn() && UH_IsFlashlightInLeftArm() && bLeftArmFree;
	CBaseViewModel *pViewModel = GetViewModel( 1 );
	bool bFlashlightModel = pViewModel &&
		!Q_stricmp( STRING( pViewModel->GetModelName() ), UH_LEFTARM_FLASHLIGHT );

	if ( bWantFlashlight )
	{
		// sub_101F0C60: model is installed, then sequence 1 plays the raise
		// animation. Keeping the model alive is what makes the transition
		// visible instead of snapping the left hand into its idle pose.
		if ( !bFlashlightModel )
		{
			pViewModel->SetWeaponModel( UH_LEFTARM_FLASHLIGHT, NULL );
			pViewModel->m_nSkin = 1;
			pViewModel->RemoveEffects( EF_NODRAW );
			pViewModel->SendViewModelMatchingSequence( 1 );
		}
		m_bFlashlightHolstered = false;
		m_bLeftArmDeployed = true;
		return;
	}

	if ( bFlashlightModel && !m_bFlashlightHolstered )
	{
		// Original sequence 2 is the lower/holster animation. Defer hiding the
		// viewmodel until it has had time to play.
		pViewModel->SendViewModelMatchingSequence( 2 );
		m_bFlashlightHolstered = true;
		m_bLeftArmDeployed = false;
		SetContextThink( &CHL2_Player::UH_FlashlightViewModelThink,
			gpGlobals->curtime + 0.25f, "FlashlightViewModelThink" );
		return;
	}

	UH_SetLeftArmModel( NULL, 0, false );
}

//-----------------------------------------------------------------------------
// Purpose: Complete sequence 2 after its lower/holster window. This is kept
// separate from FlashLightContext, which the original uses for grenade throws.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_FlashlightViewModelThink( void )
{
	if ( FlashlightIsOn() && UH_IsFlashlightInLeftArm() )
	{
		UH_UpdateLeftArm();
		return;
	}

	CBaseViewModel *pViewModel = GetViewModel( 1 );
	if ( pViewModel && !Q_stricmp( STRING( pViewModel->GetModelName() ), UH_LEFTARM_FLASHLIGHT ) )
	{
		pViewModel->SetWeaponModel( NULL, NULL );
		pViewModel->AddEffects( EF_NODRAW );
	}
	m_bLeftArmDeployed = false;
}

//-----------------------------------------------------------------------------
// Purpose: Holster the left arm (used on weapon switch / drop / death).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_HolsterLeftArm( void )
{
	m_bLeftArmDeployed = false;
	m_bFlashlightHolstered = true;
	UH_SetLeftArmModel( NULL, 0, false );
}

//-----------------------------------------------------------------------------
// Purpose: Find a weapon the left hand can hold the flashlight with — a
// "OneHanded" weapon (pistol), preferring a non-melee one and falling back to
// melee. Decoded from sub_101E60C0 (called by the flashlight deploy
// sub_101F0C60): with a two-handed weapon active the original scans every
// weapon slot and switches to a one-handed weapon so the left arm is free to
// raise v_flashlight_pg.mdl. Returns NULL if the player has no one-handed
// weapon at all.
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CHL2_Player::UH_FindOneHandedWeapon( void )
{
	CBaseCombatWeapon *pMeleeFallback = NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( !pWeapon )
			continue;

		if ( !pWeapon->GetWpnData().m_bOneHanded )
			continue;

		// Prefer a non-melee one-handed weapon (pistol) like the original,
		// which switches to the first non-melee OneHanded it finds.
		if ( !pWeapon->GetWpnData().m_bMeleeWeapon )
			return pWeapon;

		pMeleeFallback = pWeapon;
	}

	return pMeleeFallback;
}

//-----------------------------------------------------------------------------
// Purpose: Put a flare in the left hand (flare-pack "useitem").
//-----------------------------------------------------------------------------
void CHL2_Player::UH_EquipFlare( void )
{
	m_bHoldingFlare = true;
	m_flFlareStartTime = gpGlobals->curtime;
	UH_UpdateLeftArm();
	CBaseViewModel *pDeployVM = GetViewModel( 1 );
	if ( pDeployVM ) { pDeployVM->SetCycle( 0.0f ); pDeployVM->SetPlaybackRate( 1.0f ); pDeployVM->SendViewModelMatchingSequence( 1 ); }

	if ( m_hHeldFlareEffect ) UTIL_Remove( m_hHeldFlareEffect );
	m_hHeldFlareEffect = NULL;
	if ( m_hHeldFlareSprite ) UTIL_Remove( m_hHeldFlareSprite );
	m_hHeldFlareSprite = NULL;
	CBaseViewModel *pVM = GetViewModel( 1 );
	if ( pVM )
	{
		Vector org; QAngle ang;
		int attachment = pVM->LookupAttachment( "fuse" );
		if ( attachment > 0 && pVM->GetAttachment( attachment, org, ang ) )
		{
			CFlare *pFlare = CFlare::Create( org, ang, this, UH_FLARE_FUSE );
			if ( pFlare )
			{
				// sub_10172E80 performs this setup after creating the flare:
				// stop its fly-gravity movement, make it non-interactive, freeze
				// gravity, preserve the fuse's world transform, then attach it to
				// viewmodel 1.  Parenting a live MOVETYPE_FLYGRAVITY flare (our old
				// code) leaves the networked effect in an invalid/moving state, so
				// the client never keeps its glow at the flare in the player's hand.
				pFlare->m_bPropFlare = true;
				pFlare->SetMoveType( MOVETYPE_NONE );
				pFlare->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE );
				pFlare->SetGravity( 0.0f );
				pFlare->SetAbsOrigin( org );
				pFlare->SetParent( pVM, attachment );

				// Also matches the final AddEntityToDarknessCheck call in the
				// original.  This is the server-side illumination registration;
				// C_Flare supplies the visible client dlight/elight and sprites.
				AddEntityToDarknessCheck( pFlare, 307.20001f );
				m_hHeldFlareEffect = pFlare;

				// C_Flare's particle emitter is world-space and can be culled when
				// its entity is parented to a predicted viewmodel. The original has
				// a bright fuse sprite in addition to the visible flare model; keep a
				// networked world-glow sprite directly on the same VM attachment.
				CSprite *pGlow = CSprite::SpriteCreate( "effects/yellowflare_noz.vmt", org, false );
				if ( pGlow )
				{
					pGlow->SetTransparency( kRenderWorldGlow, 255, 150, 80, 255, kRenderFxNoDissipation );
					pGlow->SetScale( 0.18f );
					pGlow->SetGlowProxySize( 2.0f );
					pGlow->SetAttachment( pVM, attachment );
					m_hHeldFlareSprite = pGlow;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Throw a grenade (client command "Throw_Nade"). Decoded from
// sub_101ED130: if holding a flare, throw the flare; otherwise stage the
// grenade throw — the grenade viewmodel goes into the left hand, the arm
// raises, and the grenade actually leaves 0.4 s later.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ThrowNade( void )
{
	if ( GetHealth() <= 0 )
		return;

	if ( IsSprinting() )
		return;

	// A held flare is thrown instead of a grenade.
	if ( m_bHoldingFlare )
	{
		CBaseViewModel *pFlareVM = GetViewModel( 1 );
		if ( pFlareVM ) { pFlareVM->SetCycle( 0.0f ); pFlareVM->SetPlaybackRate( 1.0f ); pFlareVM->SendViewModelMatchingSequence( 4 ); }
		m_bFlareMarker = true;
		float delay = pFlareVM ? max( 0.1f, pFlareVM->SequenceDuration() * 0.5f ) : 0.35f;
		SetContextThink( &CHL2_Player::UH_LeftArmContextThink, gpGlobals->curtime + delay, "FlashLightContext" );
		return;
	}

	// Underhell grenades are AMMO, not a weapon: picking up a weapon_frag
	// converts it into "grenade" ammo (CHL2_Player::BumpWeapon) and removes the
	// entity, so Weapon_OwnsThisType("weapon_frag") is always NULL here. The
	// original gates the throw on the grenade ammo count instead
	// (sub_101ED130 -> sub_100CF610 reads GetAmmoCount("grenade") > 0).
	int iGrenadeAmmo = GetAmmoDef()->Index( "grenade" );
	if ( iGrenadeAmmo < 0 || GetAmmoCount( iGrenadeAmmo ) <= 0 )
		return;

	// Un-sight (the original does this before throwing).
	if ( m_bIronSighted )
		UH_ToggleIronsight();

	// Stage the throw: grenade in the left hand + raise the arm.
	m_bFlareMarker = true;
	UH_SetLeftArmModel( UH_LEFTARM_GRENADE, 1, true );

	// Original sends authored sequence 1 on the left-arm grenade model.
	CBaseViewModel *pGrenadeVM = GetViewModel( 1 );
	if ( pGrenadeVM ) { pGrenadeVM->SetCycle( 0.0f ); pGrenadeVM->SetPlaybackRate( 1.0f ); pGrenadeVM->SendViewModelMatchingSequence( 1 ); }
	CBaseCombatWeapon *pCurrentWeapon = GetActiveWeapon();
	if ( pCurrentWeapon ) pCurrentWeapon->SendWeaponAnim( ACT_VM_IDLE );

	SetContextThink( &CHL2_Player::UH_LeftArmContextThink, gpGlobals->curtime + UH_THROW_STAGE_DELAY, "FlashLightContext" );
}

//-----------------------------------------------------------------------------
// Purpose: The delayed grenade throw (original "FlashLightContext" think,
// sub_101EE050). Fires the actual grenade projectile and restores the left arm.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_LeftArmContextThink( void )
{
	m_bFlareMarker = false;
	if ( m_bHoldingFlare )
	{
		UH_ThrowFlare();
		return;
	}

	// Actually throw the grenade: spawn the frag projectile and consume one
	// grenade ammo. This replicates CWeaponFrag::ThrowGrenade + DecrementAmmo
	// directly against the player's "grenade" ammo (the original stages the
	// throw here rather than through a weapon_frag the player never owns).
	int iGrenadeAmmo = GetAmmoDef()->Index( "grenade" );
	if ( iGrenadeAmmo >= 0 && GetAmmoCount( iGrenadeAmmo ) > 0 )
	{
		Vector vecEye = EyePosition();
		Vector vForward, vRight;
		EyeVectors( &vForward, &vRight, NULL );

		Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
		vForward[2] += 0.1f;

		Vector vecThrow;
		GetVelocity( &vecThrow, NULL );
		vecThrow += vForward * 1200.0f;

		Fraggrenade_Create( vecSrc, vec3_angle, vecThrow,
							AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ),
							this, UH_GRENADE_TIMER, false );

		RemoveAmmo( 1, iGrenadeAmmo );
	}

	// Restore the left arm (flashlight / flare / nothing).
	UH_UpdateLeftArm();
}

//-----------------------------------------------------------------------------
// Purpose: Throw the held flare (original sub_101E9580). Spawns a lit flare
// prop_physics in front of the player with a 200 u/s throw and a 90 s fuse.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ThrowFlare( void )
{
	Vector vecForward;
	EyeVectors( &vecForward );
	VectorNormalize( vecForward );
	Vector vecSrc = Weapon_ShootPosition();

	if ( m_hHeldFlareEffect )
	{
		UTIL_Remove( m_hHeldFlareEffect );
		m_hHeldFlareEffect = NULL;
	}
	if ( m_hHeldFlareSprite )
	{
		UTIL_Remove( m_hHeldFlareSprite );
		m_hHeldFlareSprite = NULL;
	}

	CBaseEntity *pFlare = CreateEntityByName( "prop_physics" );
	if ( !pFlare ) return;
	pFlare->SetModel( UH_FLARE_PROP );
	pFlare->SetAbsOrigin( vecSrc );
	pFlare->SetAbsAngles( GetAbsAngles() );
	DispatchSpawn( pFlare );
	pFlare->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );

	const float flRemaining = max( 0.1f, m_flFlareStartTime + UH_FLARE_FUSE - gpGlobals->curtime );
	CBreakableProp *pProp = dynamic_cast<CBreakableProp *>( pFlare );
	if ( pProp ) pProp->CreateFlare( flRemaining );

	IPhysicsObject *pPhys = pFlare->VPhysicsGetObject();
	if ( pPhys )
	{
		Vector vecVel = vecForward * uh_flare_throw_scale.GetFloat();
		AngularImpulse angImp( 200.0f, 200.0f, 200.0f );
		pPhys->SetVelocity( &vecVel, &angImp );
	}

	m_bHoldingFlare = false;
	m_flFlareStartTime = 0.0f;
	UH_UpdateLeftArm();
}

//-----------------------------------------------------------------------------
// Purpose: Melee strike with the lit flare. The original starts sequence 4,
// then executes its bludgeon swing from FlareHitContext after half of the
// animation (sub_101E97E0 / sub_101F2A20).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_StartFlareStrike( void )
{
	if ( !m_bHoldingFlare || m_bFlareStrikePending || gpGlobals->curtime < m_flNextFlareStrike )
		return;

	CBaseViewModel *pVM = GetViewModel( 1 );
	if ( !pVM )
		return;

	pVM->SetCycle( 0.0f );
	pVM->SetPlaybackRate( 1.0f );
	// sub_101E96F0 randomly selects one of the three authored hit animations.
	pVM->SendViewModelMatchingSequence( random->RandomInt( 5, 7 ) );
	ViewPunch( QAngle( -1.0f, 0.0f, 0.0f ) );
	SuitPower_Drain( 5.0f );
	m_bFlareStrikePending = true;

	SetContextThink( &CHL2_Player::UH_FlareHitContextThink,
		gpGlobals->curtime + 0.35f, "FlareHitContext" );
}

void CHL2_Player::UH_FlareHitContextThink( void )
{
	if ( !m_bFlareStrikePending )
		return;

	m_bFlareStrikePending = false;
	m_flNextFlareStrike = gpGlobals->curtime + 0.15f;

	if ( !m_bHoldingFlare || !IsAlive() )
		return;

	CBaseEntity *pHit = CheckTraceHullAttack( 64.0f, Vector( -16, -16, -16 ),
		Vector( 16, 16, 16 ), 10, DMG_CLUB | DMG_BURN, 1.0f );
	if ( pHit )
	{
		EmitSound( "Weapon_Crowbar.Melee_Hit" );
	}
	else
	{
		EmitSound( "Weapon_Crowbar.Single" );
	}

	CBaseViewModel *pVM = GetViewModel( 1 );
	if ( pVM )
		pVM->SendViewModelMatchingSequence( 1 );
}
