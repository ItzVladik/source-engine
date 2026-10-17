//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell weapon classes — implementation.
//
// Each concrete weapon is a thin class: the weapon script provides the
// viewmodel / worldmodel / bucket / ammo, and the C++ class only registers the
// classname. Melee derives from CBaseHLBludgeonWeapon (swing + hit), guns from
// CBaseHLCombatWeapon (bullet fire via GetBulletSpread).
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "soundent.h"
#include "npcevent.h"
#include "rumble_shared.h"
#include "ai_basenpc.h"
#include "hl2_player.h"
#include "hl2/npc_metropolice.h"
#include "baseviewmodel_shared.h"
#include "sprite.h"
#include "uh_weapons.h"
#include "underhell/uh_bullettime.h"
#include "hl2/weapon_rpg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Melee swing: drain the weapon's StaminaToDrain from suit power, then swing.
//-----------------------------------------------------------------------------
void CUHMeleeWeapon::PrimaryAttack( void )
{
	CHL2_Player *pPlayer = ToBasePlayer( GetOwner() ) ? dynamic_cast<CHL2_Player *>( GetOwner() ) : NULL;
	if ( pPlayer )
	{
		// serveror.dll sub_102B0B50: melee cannot start below 15 stamina.
		// It is intentionally a fixed entry threshold, while the actual amount
		// drained is weapon-script StaminaToDrain.
		if ( pPlayer->SuitPower_GetCurrentPercentage() < 15.0f )
		{
			pPlayer->EmitSound( "Player.Voice.Melee.Exhausted" );
			return;
		}

		pPlayer->SuitPower_Drain( GetWpnData().m_flStaminaToDrain );
		pPlayer->EmitSound( pPlayer->SuitPower_GetCurrentPercentage() < 35.0f
			? "Player.Voice.Melee.Exhausted" : "Player.Voice.Melee" );
	}

	// Original sub_102B0A00 starts ACT_VM_HITCENTER now, but sub_102AFCC0
	// does not invoke the hit trace until MeleeDelayedFire has elapsed.
	m_bDelayedMeleeAttack = true;
	m_flDelayedMeleeAttackTime = gpGlobals->curtime + GetWpnData().m_flMeleeDelayedFire;
	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = m_flNextPrimaryAttack;

	// sub_102B0A00 sends activity 195 = ACT_VM_MISSCENTER for the wind-up.
	// ACT_VM_HITCENTER (189) is selected only later by the delayed trace.
	SendWeaponAnim( ACT_VM_MISSCENTER );
	if ( pPlayer )
	{
		pPlayer->SetAnimation( PLAYER_ATTACK1 );
		pPlayer->RumbleEffect( RUMBLE_CROWBAR_SWING, 0, RUMBLE_FLAG_RESTART );
	}
}

void CUHMeleeWeapon::ItemPostFrame( void )
{
	if ( m_bDelayedMeleeAttack && gpGlobals->curtime >= m_flDelayedMeleeAttackTime )
	{
		m_bDelayedMeleeAttack = false;
		// The animation and refire timers were established at wind-up start.
		// Resolve only the actual trace, damage, impact and swing sound now.
		Swing( false, true, true );
	}

	BaseClass::ItemPostFrame();
}

bool CUHMeleeWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bDelayedMeleeAttack = false;
	return BaseClass::Holster( pSwitchingTo );
}

void CUHMeleeWeapon::SecondaryAttack( void )
{
	// CBaseHLBludgeonWeapon exposes a vanilla IN_ATTACK2 swing, but the
	// Underhell melee vtable has only the primary sub_102B0B50 path.
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
}

//-----------------------------------------------------------------------------
// Purpose: NPC melee attack — Swing() is player-only, so the NPC's melee anim
// event routes here. Mirrors the crowbar's HandleAnimEventMeleeHit but uses
// the per-weapon sk_plr_dmg_<weapon> damage.
//-----------------------------------------------------------------------------
void CUHMeleeWeapon::HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	Vector vecDirection;
	AngleVectors( GetAbsAngles(), &vecDirection );

	CBaseEntity *pEnemy = pOperator->MyNPCPointer() ? pOperator->MyNPCPointer()->GetEnemy() : NULL;
	if ( pEnemy )
	{
		Vector vecDelta;
		VectorSubtract( pEnemy->WorldSpaceCenter(), pOperator->Weapon_ShootPosition(), vecDelta );
		VectorNormalize( vecDelta );

		Vector2D vecDelta2D = vecDelta.AsVector2D();
		Vector2DNormalize( vecDelta2D );
		if ( DotProduct2D( vecDelta2D, vecDirection.AsVector2D() ) > 0.8f )
		{
			vecDirection = vecDelta;
		}
	}

	// The baton has the original stunstick-derived NPC strike: 32 units and a
	// hull stretched downward. Other Underhell melee weapons use the crowbar
	// strike dimensions.
	const bool bBaton = FClassnameIs( this, "weapon_melee_baton" );
	Vector vecEnd;
	VectorMA( pOperator->Weapon_ShootPosition(), bBaton ? 32.0f : 50.0f, vecDirection, vecEnd );
	CBaseEntity *pHurt = pOperator->CheckTraceHullAttack( pOperator->Weapon_ShootPosition(), vecEnd,
		bBaton ? Vector(-16,-16,-40) : Vector(-16,-16,-16),
		bBaton ? Vector(16,16,16) : Vector(36,36,36),
		(int)GetDamage(), DMG_CLUB, bBaton ? 0.5f : 0.75f );

	if ( pHurt )
	{
		WeaponSound( MELEE_HIT );

		// CWeaponBaton::Operator_HandleAnimEvent in the original is the
		// stunstick handler, not the generic crowbar handler. In particular,
		// StunnedTarget fires npc_metropolice.OnStunnedPlayer. Chapter 03's
		// shower guards count that output to end the punishment sequence; if it
		// is omitted they can beat the player forever even though attacks animate.
		if ( bBaton )
		{
			CBasePlayer *pPlayer = ToBasePlayer( pHurt );
			CNPC_MetroPolice *pCop = dynamic_cast<CNPC_MetroPolice *>( pOperator );
			bool bKnockedOut = false;

			if ( pCop && pPlayer )
			{
				if ( pCop->ShouldKnockOutTarget( pHurt ) )
				{
					pPlayer->ViewPunch( QAngle( -16, random->RandomFloat( -48, -24 ), 2 ) );
					color32 white = { 255, 255, 255, 255 };
					UTIL_ScreenFade( pPlayer, white, 0.2f, 1.0f, FFADE_OUT|FFADE_PURGE|FFADE_STAYOUT );
					pCop->KnockOutTarget( pHurt );
					bKnockedOut = true;
				}
				else
				{
					pCop->StunnedTarget( pHurt );
				}
			}

			if ( pPlayer && !bKnockedOut && !( pPlayer->GetFlags() & FL_GODMODE ) )
			{
				pPlayer->ViewPunch( QAngle( -16, random->RandomFloat( -48, -24 ), 2 ) );

				Vector vecImpulse = pHurt->GetAbsOrigin() - GetAbsOrigin();
				if ( pPlayer->GetGroundEntity() == pOperator )
				{
					vecImpulse = vecDirection;
					vecImpulse.z = 0;
				}
				VectorNormalize( vecImpulse );
				vecImpulse *= 500.0f;
				if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
					vecImpulse.z = 0.0f;
				pHurt->ApplyAbsVelocityImpulse( vecImpulse );

				color32 red = { 128, 0, 0, 128 };
				UTIL_ScreenFade( pPlayer, red, 0.5f, 0.1f, FFADE_IN );
				pPlayer->ForceDropOfCarriedPhysObjects();
			}
		}

		trace_t traceHit;
		UTIL_TraceLine( pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit );
		ImpactEffect( traceHit );
	}
	else
	{
		WeaponSound( MELEE_MISS );
	}
}

void CUHMeleeWeapon::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch ( pEvent->event )
	{
	case EVENT_WEAPON_MELEE_HIT:
		HandleAnimEventMeleeHit( pEvent, pOperator );
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: SOCOM's original Precache path (sub_1027B980) explicitly loads
// sprites/laserpointer.vmt. Keeping it here is harmless for the thin sibling
// classes and guarantees SpriteCreate never performs a late model load.
//-----------------------------------------------------------------------------
void CUHGunWeapon::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( "sprites/laserpointer.vmt" );
}

//-----------------------------------------------------------------------------
// Gun fire: a single shot through the engine bullet path, with Underhell
// damage (skill.cfg sk_plr_dmg_*), recoil + accuracy. Mirrors the vanilla
// CBaseCombatWeapon::PrimaryAttack but injects the per-weapon damage the
// base leaves to the ammo definition.
//-----------------------------------------------------------------------------
void CUHGunWeapon::PrimaryAttack( void )
{
	// Constructors run before weapon-script data is available. Apply the
	// original UH_Weapon_Special FireMode on the first real attack instead of
	// leaving every thin weapon class in full-auto (the cause of automatic
	// Beretta/Glock/SOCOM fire while the trigger is held).
	if ( !m_bFireModeInitialized )
	{
		int iScriptFireMode = GetWpnData().m_iFireMode;
		if ( iScriptFireMode == FIREMODE_FULLAUTO || iScriptFireMode == FIREMODE_SEMI || iScriptFireMode == FIREMODE_3RNDBURST )
			m_iFireMode = iScriptFireMode;
		m_bFireModeInitialized = true;
	}

	// If the clip is empty (and we use clips), start a reload.
	if ( UsesClipsForAmmo1() && !m_iClip1 )
	{
		Reload();
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player *>( pPlayer );
	if ( pHL2Player && pHL2Player->UH_IsWeaponObstructed() )
		return;

	// Semi-auto gate (decode sub_102B18E0): in semi mode the shot is only fired
	// while the trigger latch is set; PrimaryAttack consumes it so a held
	// trigger fires a single shot. WeaponIdle() re-arms it on release.
	if ( m_iFireMode == FIREMODE_SEMI )
	{
		if ( !m_bFireOnEdge )
			return;
		m_bFireOnEdge = false;
	}

	// Prevent aim drift from rapid fire (same as the vanilla pistol).
	pPlayer->ViewPunchReset();

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pPlayer );

	WeaponSound( m_bSilenced ? SINGLE_SILENCED : SINGLE );
	pPlayer->DoMuzzleFlash();
	SendWeaponAnim( GetPrimaryAttackActivity() );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Build the shot with the per-weapon damage (the original reads the
	// sk_plr_dmg_<weapon> skill convar; the value is baked in per class).
	FireBulletsInfo_t info;
	info.m_vecSrc			= pPlayer->Weapon_ShootPosition();
	info.m_vecDirShooting	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
	info.m_vecSpread		= GetBulletSpread();
	info.m_flDistance		= MAX_TRACE_LENGTH;
	info.m_iAmmoType		= m_iPrimaryAmmoType;
	info.m_iTracerFreq		= 2;
	info.m_iShots			= m_iShotsPerFire;
	info.m_iDamage			= (int)GetDamage();
	info.m_iPlayerDamage	= (int)GetDamage();

	pPlayer->FireBullets( info );

	// Bullet time interception is centralized in CBaseEntity::FireBullets so
	// spread directions, NPC fire and player fire all create one CBtBullet per
	// actual shot without layering a second visual tracer.

	// Consume a round.
	if ( UsesClipsForAmmo1() )
	{
		m_iClip1--;
	}
	else
	{
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();

	// Underhell shotguns are pump-action. The old thin gun base used their
	// 0.8 s refire time but never sent the intervening pump sequence.
	if ( m_iShotsPerFire > 1 && !FClassnameIs( this, "weapon_shotgun_xm1014" ) )
	{
		m_bNeedPump = true;
		m_flPumpTime = gpGlobals->curtime + 0.4f;
	}

	// Out of ammo indicator.
	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	}

	AddViewKick();

	// Accumulate an accuracy penalty so spamming a gun spreads it out (the
	// vanilla pistol's model; the Underhell scripts carry the accuracy
	// multipliers, not the penalty curve).
	m_flAccuracyPenalty += ( m_iWeaponType == 1 ) ? 0.2f : 0.1f;
	m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, 1.5f );
}

//-----------------------------------------------------------------------------
// Purpose: SOCOM laser secondary attack. CWeaponPistolSocom::sub_1027B9E0
// toggles its laser state and sends activity 12/13; the player-level network
// bit carries that state to the compact client beam renderer.
//-----------------------------------------------------------------------------
void CUHGunWeapon::SecondaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && FClassnameIs( this, "weapon_pistol_socom" ) )
	{
		CHL2_Player *pPlayer = dynamic_cast< CHL2_Player * >( pOwner );
		m_bSocomLaserOn = !m_bSocomLaserOn;
		if ( pPlayer )
			pPlayer->UH_SetLaserOn( m_bSocomLaserOn );

		if ( !m_bSocomLaserOn )
		{
			UTIL_Remove( m_hLaserDot.Get() );
			m_hLaserDot = NULL;
		}
		else if ( !m_hLaserDot.Get() )
		{
			// The original SOCOM precaches laserpointer.vmt and owns a removable
			// sprite handle. env_laserdot uses redglow1.vmt and is the RPG path,
			// not the SOCOM visual.
			CSprite *pDot = CSprite::SpriteCreate( "sprites/laserpointer.vmt",
				pOwner->Weapon_ShootPosition(), false );
			if ( pDot )
			{
				pDot->SetTransparency( kRenderWorldGlow, 255, 255, 255, 255,
					kRenderFxNoDissipation );
				pDot->SetScale( 0.15f );
				pDot->SetGlowProxySize( 1.0f );
				pDot->SetOwnerEntity( this );
				m_hLaserDot = pDot;
			}
		}

		// Original sub_1027B9E0 selects SOCOM viewmodel sequence 12/13 with a
		// zero blend time. They are sequence indices, not global Activity ids.
		CBaseViewModel *pViewModel = pOwner->GetViewModel();
		if ( pViewModel )
		{
			pViewModel->SendViewModelMatchingSequence( m_bSocomLaserOn ? 12 : 13 );
			SetWeaponIdleTime( gpGlobals->curtime + pViewModel->SequenceDuration() );
		}
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.2f;
		return;
	}

	BaseClass::SecondaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose: firemode_toggle (decode sub_102B0D10, the weapon method the client
// command dispatches to at vtable+840). Flips full-auto <-> semi and plays the
// hard-coded pistol-empty click. The original is asymmetric: full->semi is
// silent, semi->full (and any other state) plays the click.
//-----------------------------------------------------------------------------
void CUHGunWeapon::UH_ToggleFireMode( void )
{
	// Only weapons authored with UH_Weapon_Special/FireMode expose select fire.
	// In the shipped scripts this is the G36K; pistol scripts omit the key and
	// retain their dedicated semi-auto trigger latch.
	if ( GetWpnData().m_iFireMode == 0 )
		return;

	if ( m_iFireMode == FIREMODE_FULLAUTO )
	{
		m_iFireMode = FIREMODE_SEMI;
	}
	else if ( m_iFireMode == FIREMODE_SEMI )
	{
		m_iFireMode = FIREMODE_FULLAUTO;
		EmitSound( "Weapon_Pistol.Empty" );
		return;
	}

	EmitSound( "Weapon_Pistol.Empty" );
}

//-----------------------------------------------------------------------------
// Purpose: Re-arm the semi-auto trigger latch when the attack button is
// released (decode sub_10279A80). WeaponIdle() is reached from ItemPostFrame
// only when no fire/reload buttons are held, so a released trigger always
// passes through here before the next shot can be fired in semi mode.
//-----------------------------------------------------------------------------
void CUHGunWeapon::WeaponIdle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	CHL2_Player *pHL2Player = pPlayer ? dynamic_cast<CHL2_Player *>( pPlayer ) : NULL;

	// Wall obstruction is the original raised/sideways state (activity 205),
	// not the scripted lowered state. Keep its looping idle after the short
	// 204 transition instead of letting BaseClass replace it with ACT_VM_IDLE.
	if ( pHL2Player && pHL2Player->UH_IsWeaponObstructed() )
	{
		if ( HasWeaponIdleTimeElapsed() )
			SendWeaponAnim( ACT_VM_IDLE_RAISED );
		if ( !( pPlayer->m_nButtons & IN_ATTACK ) )
			m_bFireOnEdge = true;
		return;
	}

	BaseClass::WeaponIdle();

	if ( pPlayer && !( pPlayer->m_nButtons & IN_ATTACK ) )
		m_bFireOnEdge = true;
}

void CUHGunWeapon::ItemPostFrame( void )
{
	CBasePlayer *pPenaltyOwner = ToBasePlayer( GetOwner() );
	if ( m_iWeaponType == 1 && pPenaltyOwner &&
		 !( pPenaltyOwner->m_nButtons & IN_ATTACK ) &&
		 gpGlobals->curtime > m_flNextPrimaryAttack )
	{
		// Original CWeaponPistol::UpdatePenaltyTime, sub_10279CB0.
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty - gpGlobals->frametime,
			0.0f, 1.5f );
	}

	// sub_1027F4E0's pump sits between the fire event and the 0.8 s refire
	// window. Run it from ItemPostFrame so it also happens while IN_ATTACK is
	// held; WeaponIdle alone is skipped during sustained fire.
	if ( m_bNeedPump && gpGlobals->curtime >= m_flPumpTime )
	{
		m_bNeedPump = false;
		WeaponSound( SPECIAL1 );
		SendWeaponAnim( ACT_SHOTGUN_PUMP );
	}

	if ( m_bSocomLaserOn && m_hLaserDot.Get() )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		if ( pOwner )
		{
			Vector start = pOwner->Weapon_ShootPosition();
			Vector dir = pOwner->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
			trace_t tr;
			UTIL_TraceLine( start, start + dir * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );
			m_hLaserDot->SetAbsOrigin( tr.endpos + tr.plane.normal * 0.5f );
		}
	}

	BaseClass::ItemPostFrame();
}

bool CUHGunWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_hLaserDot.Get() )
	{
		UTIL_Remove( m_hLaserDot.Get() );
		m_hLaserDot = NULL;
	}
	m_bSocomLaserOn = false;
	CHL2_Player *pPlayer = dynamic_cast< CHL2_Player * >( ToBasePlayer( GetOwner() ) );
	if ( pPlayer )
		pPlayer->UH_SetLaserOn( false );
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: NPC fire. PrimaryAttack() is player-only (it pulls the eye origin /
// view vectors off CBasePlayer). Uses the operator's shoot position + the AI's
// actual shoot trajectory (NOT the weapon's "muzzle" attachment, which the
// Underhell worldmodels don't reliably carry — a missing attachment made the
// bullets fire from a garbage position/direction). Damage is the per-weapon
// sk_plr_dmg_<weapon> value.
//-----------------------------------------------------------------------------
void CUHGunWeapon::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator )
{
	CAI_BaseNPC *pNPC = pOperator->MyNPCPointer();
	if ( !pNPC )
		return;

	Vector vecShootOrigin = pOperator->Weapon_ShootPosition();
	Vector vecShootDir = pNPC->GetActualShootTrajectory( vecShootOrigin );

	WeaponSound( SINGLE_NPC );

	CSoundEnt::InsertSound( SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(),
		SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

	// Explicit per-weapon damage (the vanilla ammo def would give the generic
	// sk_plr_dmg_<ammotype> value, not the Underhell per-weapon value). Shotguns
	// fire a spread of pellets (m_iShotsPerFire), like the vanilla shotgun.
	pOperator->FireBullets( m_iShotsPerFire, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED,
		MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2, -1, -1, (int)GetDamage(), NULL, false );

	if ( m_iClip1 > 0 )
		m_iClip1--;
}

//-----------------------------------------------------------------------------
// Purpose: NPC fire via the new AE_NPC_WEAPON_FIRE anim event (handled by the
// base Operator_HandleAnimEvent).
//-----------------------------------------------------------------------------
void CUHGunWeapon::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	FireNPCPrimaryAttack( pOperator );
}

//-----------------------------------------------------------------------------
// Purpose: NPC fire via the old EVENT_WEAPON_* anim events. The combine
// soldier model's attack sequences fire EVENT_WEAPON_AR2 / EVENT_WEAPON_SMG1 /
// EVENT_WEAPON_SHOTGUN_FIRE etc.; the base Operator_HandleAnimEvent only knows
// the new AE_NPC_WEAPON_FIRE event, so without this override the NPC aims (and
// shows the muzzle flash) but never fires a bullet. All ranged-fire events are
// funneled into FireNPCPrimaryAttack().
//-----------------------------------------------------------------------------
void CUHGunWeapon::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch ( pEvent->event )
	{
	case EVENT_WEAPON_AR1:
	case EVENT_WEAPON_AR2:
	case EVENT_WEAPON_SMG1:
	case EVENT_WEAPON_SMG2:
	case EVENT_WEAPON_HMG1:
	case EVENT_WEAPON_SHOTGUN_FIRE:
	case EVENT_WEAPON_PISTOL_FIRE:
	case EVENT_WEAPON_SNIPER_RIFLE_FIRE:
	case EVENT_WEAPON_SMG1_BURST1:
	case EVENT_WEAPON_SMG1_BURSTN:
		FireNPCPrimaryAttack( pOperator );
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//-----------------------------------------------------------------------------
// Spread: base cone scaled by crouch / run / ironsight accuracy multipliers
// from the weapon script, plus a ramped penalty while spamming.
//-----------------------------------------------------------------------------
const Vector &CUHGunWeapon::GetBulletSpread( void )
{
	static Vector cone;
	static Vector baseCone = VECTOR_CONE_4DEGREES;
	// Shotguns (multi-pellet) use the vanilla shotgun's wide cone.
	static Vector shotgunCone = VECTOR_CONE_10DEGREES;

	// NPCs ignore the player tuning.
	if ( GetOwner() && GetOwner()->IsNPC() )
	{
		cone = ( m_iShotsPerFire > 1 ) ? shotgunCone : baseCone;
		return cone;
	}

	const FileWeaponInfo_t &info = GetWpnData();

	if ( m_iShotsPerFire > 1 )
	{
		cone = shotgunCone;
	}
	else
	{
		// Spam penalty ramps the cone from ~1 to ~6 degrees.
		float ramp = RemapValClamped( m_flAccuracyPenalty, 0.0f, 1.5f, 0.0f, 1.0f );
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );
	}

	// Crouching is more accurate (lower value = tighter cone).
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			cone *= info.m_flCrouchAccuracyMult;
		}

		// Moving fast spreads the cone (RunAccuracyMult > 1).
		float flSpeed = pPlayer->GetAbsVelocity().Length2D();
		if ( flSpeed > 150.0f )
		{
			float flRunRamp = RemapValClamped( flSpeed, 150.0f, 400.0f, 1.0f, info.m_flRunAccuracyMult );
			cone *= flRunRamp;
		}
	}

	// Ironsighted: apply the ExpOffset accuracy multiplier (usually < 1).
	CHL2_Player *pHL2Player = ToBasePlayer( GetOwner() ) ? dynamic_cast<CHL2_Player *>( GetOwner() ) : NULL;
	if ( pHL2Player && pHL2Player->UH_IsIronSighted() && info.m_bHasExpOffset )
	{
		cone *= info.m_flAccuracy;
	}

	return cone;
}

//-----------------------------------------------------------------------------
// Recoil: PunchPitch/PunchYaw ranges from the weapon script, scaled by
// CrouchRecoilMult while ducked.
//-----------------------------------------------------------------------------
void CUHGunWeapon::AddViewKick( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	const FileWeaponInfo_t &info = GetWpnData();

	QAngle viewPunch;
	viewPunch.x = random->RandomFloat( info.m_flPunchPitchMin, info.m_flPunchPitchMax );
	viewPunch.y = random->RandomFloat( info.m_flPunchYawMin, info.m_flPunchYawMax );
	viewPunch.z = 0.0f;

	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		viewPunch *= info.m_flCrouchRecoilMult;
	}

	pPlayer->ViewPunch( viewPunch );
}

//-----------------------------------------------------------------------------
// Skill convars. skill.cfg sets these at map load (the original registers them
// default "0" and relies on skill.cfg — the defaults here mirror skill.cfg so
// the damage is correct even if skill.cfg is absent). GetDamage() reads them.
//-----------------------------------------------------------------------------
ConVar sk_plr_dmg_axe( "sk_plr_dmg_axe", "35" );
ConVar sk_npc_dmg_axe( "sk_npc_dmg_axe", "20" );
ConVar sk_plr_dmg_baton( "sk_plr_dmg_baton", "13" );
ConVar sk_npc_dmg_baton( "sk_npc_dmg_baton", "10" );
ConVar sk_plr_dmg_pipe( "sk_plr_dmg_pipe", "15" );
ConVar sk_npc_dmg_pipe( "sk_npc_dmg_pipe", "10" );
ConVar sk_plr_dmg_wrench( "sk_plr_dmg_wrench", "25" );
ConVar sk_npc_dmg_wrench( "sk_npc_dmg_wrench", "15" );
ConVar sk_plr_dmg_cleaver( "sk_plr_dmg_cleaver", "50" );
ConVar sk_npc_dmg_cleaver( "sk_npc_dmg_cleaver", "25" );
ConVar sk_plr_dmg_pistol_glock( "sk_plr_dmg_pistol_glock", "10" );
ConVar sk_plr_dmg_pistol_beretta( "sk_plr_dmg_pistol_beretta", "15" );
ConVar sk_plr_dmg_pistol_socom( "sk_plr_dmg_pistol_socom", "20" );
ConVar sk_plr_dmg_pistol_python( "sk_plr_dmg_pistol_python", "120" );
ConVar sk_plr_dmg_pistol_dualberetta( "sk_plr_dmg_pistol_dualberetta", "20" );
ConVar sk_plr_dmg_smg_mp5( "sk_plr_dmg_smg_mp5", "12" );
ConVar sk_plr_dmg_smg_mp5_eod( "sk_plr_dmg_smg_mp5_eod", "10" );
ConVar sk_plr_dmg_smg_mp7( "sk_plr_dmg_smg_mp7", "8" );
ConVar sk_plr_dmg_shotgun_m3( "sk_plr_dmg_shotgun_m3", "12" );
ConVar sk_plr_dmg_shotgun_m5( "sk_plr_dmg_shotgun_m5", "16" );
ConVar sk_plr_dmg_shotgun_spas12( "sk_plr_dmg_shotgun_spas12", "14" );
ConVar sk_plr_dmg_shotgun_xm1014( "sk_plr_dmg_shotgun_xm1014", "12" );
ConVar sk_plr_dmg_rifle_g36k( "sk_plr_dmg_rifle_g36k", "20" );
ConVar sk_plr_dmg_rifle_sniper( "sk_plr_dmg_rifle_sniper", "80" );
ConVar sk_plr_dmg_bfg_mgl( "sk_plr_dmg_bfg_mgl", "200" );
ConVar sk_plr_dmg_bfg_minigun( "sk_plr_dmg_bfg_minigun", "50" );

//-----------------------------------------------------------------------------
// Registers one weapon class under its entity name and its send table.
// Damage comes from the sk_plr_dmg_<weapon> convar. Fire rates extracted from
// serveror.dll:
//   - Glock/Beretta/Dualies/SOCOM: semi-auto latch, next attack +0.1 s
//     (sub_10279600/sub_1027A920/sub_1027AC00 and shared pistol path)
//   - Python: +0.75 s in sub_1027B490
//   - SMGs + BFG minigun: 0.075 s (GetFireRate vtable slot 277 -> sub_102801F0)
//   - G36K: 0.1 s (select-fire GetFireRate sub_103F5150)
// Shotgun / sniper / BFG MGL use custom pump/delay fire paths (not GetFireRate);
// their values below are close estimates — TODO: recover the exact delay.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Full NPC acttables (1:1 with the original / vanilla per weapon category).
// The single { ACT_RANGE_ATTACK1, X, true } entry we used before left NPCs with
// no weapon-specific idle/aim/run poses and, worse, mapped pistols/smgs to AR2
// (which citizen/cop models lack) -> T-pose + no shooting.
//-----------------------------------------------------------------------------
#define UH_ACTTABLE_PISTOL \
	{ ACT_IDLE,					ACT_IDLE_PISTOL,				true }, \
	{ ACT_IDLE_ANGRY,			ACT_IDLE_ANGRY_PISTOL,			true }, \
	{ ACT_RANGE_ATTACK1,		ACT_RANGE_ATTACK_PISTOL,		true }, \
	{ ACT_RELOAD,				ACT_RELOAD_PISTOL,				true }, \
	{ ACT_WALK_AIM,				ACT_WALK_AIM_PISTOL,			true }, \
	{ ACT_RUN_AIM,				ACT_RUN_AIM_PISTOL,				true }, \
	{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_PISTOL, true }, \
	{ ACT_RELOAD_LOW,			ACT_RELOAD_PISTOL_LOW,			false }, \
	{ ACT_RANGE_ATTACK1_LOW,	ACT_RANGE_ATTACK_PISTOL_LOW,	false }, \
	{ ACT_COVER_LOW,			ACT_COVER_PISTOL_LOW,			false }, \
	{ ACT_RANGE_AIM_LOW,		ACT_RANGE_AIM_PISTOL_LOW,		false }, \
	{ ACT_GESTURE_RELOAD,		ACT_GESTURE_RELOAD_PISTOL,		false }, \
	{ ACT_WALK,					ACT_WALK_PISTOL,				false }, \
	{ ACT_RUN,					ACT_RUN_PISTOL,					false },

#define UH_ACTTABLE_SMG1 \
	{ ACT_RANGE_ATTACK1,		ACT_RANGE_ATTACK_SMG1,			true }, \
	{ ACT_RELOAD,				ACT_RELOAD_SMG1,				true }, \
	{ ACT_IDLE,					ACT_IDLE_SMG1,					true }, \
	{ ACT_IDLE_ANGRY,			ACT_IDLE_ANGRY_SMG1,			true }, \
	{ ACT_WALK,					ACT_WALK_RIFLE,					true }, \
	{ ACT_WALK_AIM,				ACT_WALK_AIM_RIFLE,				true }, \
	{ ACT_WALK_CROUCH,			ACT_WALK_CROUCH_RIFLE,			true }, \
	{ ACT_WALK_CROUCH_AIM,		ACT_WALK_CROUCH_AIM_RIFLE,		true }, \
	{ ACT_RUN,					ACT_RUN_RIFLE,					true }, \
	{ ACT_RUN_AIM,				ACT_RUN_AIM_RIFLE,				true }, \
	{ ACT_RUN_CROUCH,			ACT_RUN_CROUCH_RIFLE,			true }, \
	{ ACT_RUN_CROUCH_AIM,		ACT_RUN_CROUCH_AIM_RIFLE,		true }, \
	{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_SMG1, true }, \
	{ ACT_RANGE_ATTACK1_LOW,	ACT_RANGE_ATTACK_SMG1_LOW,		false }, \
	{ ACT_COVER_LOW,			ACT_COVER_SMG1_LOW,				false }, \
	{ ACT_RANGE_AIM_LOW,		ACT_RANGE_AIM_SMG1_LOW,			false }, \
	{ ACT_RELOAD_LOW,			ACT_RELOAD_SMG1_LOW,			false }, \
	{ ACT_GESTURE_RELOAD,		ACT_GESTURE_RELOAD_SMG1,		false },

#define UH_ACTTABLE_SHOTGUN \
	{ ACT_IDLE,					ACT_IDLE_SMG1,					true }, \
	{ ACT_RANGE_ATTACK1,		ACT_RANGE_ATTACK_SHOTGUN,		true }, \
	{ ACT_RELOAD,				ACT_RELOAD_SHOTGUN,				false }, \
	{ ACT_WALK,					ACT_WALK_RIFLE,					true }, \
	{ ACT_IDLE_ANGRY,			ACT_IDLE_ANGRY_SHOTGUN,			true }, \
	{ ACT_WALK_AIM,				ACT_WALK_AIM_SHOTGUN,			true }, \
	{ ACT_WALK_CROUCH,			ACT_WALK_CROUCH_RIFLE,			true }, \
	{ ACT_WALK_CROUCH_AIM,		ACT_WALK_CROUCH_AIM_RIFLE,		true }, \
	{ ACT_RUN,					ACT_RUN_RIFLE,					true }, \
	{ ACT_RUN_AIM,				ACT_RUN_AIM_SHOTGUN,			true }, \
	{ ACT_RUN_CROUCH,			ACT_RUN_CROUCH_RIFLE,			true }, \
	{ ACT_RUN_CROUCH_AIM,		ACT_RUN_CROUCH_AIM_RIFLE,		true }, \
	{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_SHOTGUN, true }, \
	{ ACT_RANGE_ATTACK1_LOW,	ACT_RANGE_ATTACK_SHOTGUN_LOW,	true }, \
	{ ACT_GESTURE_RELOAD,		ACT_GESTURE_RELOAD_SHOTGUN,		false },

#define UH_ACTTABLE_AR2 \
	{ ACT_RANGE_ATTACK1,		ACT_RANGE_ATTACK_AR2,			true }, \
	{ ACT_RELOAD,				ACT_RELOAD_SMG1,				true }, \
	{ ACT_IDLE,					ACT_IDLE_SMG1,					true }, \
	{ ACT_IDLE_ANGRY,			ACT_IDLE_ANGRY_SMG1,			true }, \
	{ ACT_WALK,					ACT_WALK_RIFLE,					true }, \
	{ ACT_WALK_AIM,				ACT_WALK_AIM_RIFLE,				true }, \
	{ ACT_WALK_CROUCH,			ACT_WALK_CROUCH_RIFLE,			true }, \
	{ ACT_WALK_CROUCH_AIM,		ACT_WALK_CROUCH_AIM_RIFLE,		true }, \
	{ ACT_RUN,					ACT_RUN_RIFLE,					true }, \
	{ ACT_RUN_AIM,				ACT_RUN_AIM_RIFLE,				true }, \
	{ ACT_RUN_CROUCH,			ACT_RUN_CROUCH_RIFLE,			true }, \
	{ ACT_RUN_CROUCH_AIM,		ACT_RUN_CROUCH_AIM_RIFLE,		true }, \
	{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_AR2,	false }, \
	{ ACT_RANGE_ATTACK1_LOW,	ACT_RANGE_ATTACK_AR2_LOW,		false }, \
	{ ACT_COVER_LOW,			ACT_COVER_SMG1_LOW,				false }, \
	{ ACT_RANGE_AIM_LOW,		ACT_RANGE_AIM_AR2_LOW,			false }, \
	{ ACT_RELOAD_LOW,			ACT_RELOAD_SMG1_LOW,			false }, \
	{ ACT_GESTURE_RELOAD,		ACT_GESTURE_RELOAD_SMG1,		false },

#define UH_IMPLEMENT_WEAPON( _className, _entityName, _shortName, _fireRate, _damageConVar, _weaponType, _acttable, _shotsPerFire ) \
	acttable_t _className::m_acttable[] = \
	{ \
		_acttable \
	}; \
	IMPLEMENT_ACTTABLE( _className ); \
	IMPLEMENT_SERVERCLASS_ST( _className, DT_##_shortName ) \
	END_SEND_TABLE() \
	LINK_ENTITY_TO_CLASS( _entityName, _className ); \
	PRECACHE_WEAPON_REGISTER( _entityName ); \
	_className::_className() { m_flFireRate = _fireRate; m_pDamage = &_damageConVar; m_iWeaponType = _weaponType; m_iShotsPerFire = _shotsPerFire; m_flAccuracyPenalty = 0.0f; m_iFireMode = ( _weaponType == 1 ) ? FIREMODE_SEMI : FIREMODE_FULLAUTO; m_bFireOnEdge = true; m_bFireModeInitialized = false; m_bNeedPump = false; m_flPumpTime = 0.0f; m_hLaserDot = NULL; m_bSocomLaserOn = false; }

#define UH_IMPLEMENT_MELEE( _className, _entityName, _shortName, _playerDamageConVar, _npcDamageConVar ) \
	acttable_t _className::m_acttable[] = \
	{ \
		{ ACT_MELEE_ATTACK1, ACT_MELEE_ATTACK_SWING, true }, \
	}; \
	IMPLEMENT_ACTTABLE( _className ); \
	IMPLEMENT_SERVERCLASS_ST( _className, DT_##_shortName ) \
	END_SEND_TABLE() \
	LINK_ENTITY_TO_CLASS( _entityName, _className ); \
	PRECACHE_WEAPON_REGISTER( _entityName ); \
	_className::_className() { m_pPlayerDamage = &_playerDamageConVar; m_pNPCDamage = &_npcDamageConVar; m_bDelayedMeleeAttack = false; m_flDelayedMeleeAttackTime = 0.0f; }

//-----------------------------------------------------------------------------
// Melee
//-----------------------------------------------------------------------------
UH_IMPLEMENT_MELEE( CWeaponAxe,		weapon_melee_axe,		WeaponAxe,		sk_plr_dmg_axe,		sk_npc_dmg_axe )
UH_IMPLEMENT_MELEE( CWeaponBaton,		weapon_melee_baton,		WeaponBaton,	sk_plr_dmg_baton,		sk_npc_dmg_baton )
UH_IMPLEMENT_MELEE( CWeaponPipe,		weapon_melee_pipe,		WeaponPipe,		sk_plr_dmg_pipe,		sk_npc_dmg_pipe )
UH_IMPLEMENT_MELEE( CWeaponWrench,		weapon_melee_wrench,	WeaponWrench,	sk_plr_dmg_wrench,	sk_npc_dmg_wrench )
UH_IMPLEMENT_MELEE( CWeaponCleaver,		weapon_cleaver,			WeaponCleaver,	sk_plr_dmg_cleaver,	sk_npc_dmg_cleaver )

//-----------------------------------------------------------------------------
// Pistols — semi-auto. Service pistols use a 0.1 s mechanical cooldown but
// require a fresh trigger edge; Python uses its authored 0.75 s delay.
// Weapon type 1 = pistol (silencer-gated on m_bHavePistolSilencer).
//-----------------------------------------------------------------------------
UH_IMPLEMENT_WEAPON( CWeaponPistolGlock,	weapon_pistol_glock,		WeaponPistolGlock,		0.1f, sk_plr_dmg_pistol_glock, 1, UH_ACTTABLE_PISTOL, 1 )
UH_IMPLEMENT_WEAPON( CWeaponPistolBeretta,	weapon_pistol_beretta,		WeaponPistolBeretta,	0.1f, sk_plr_dmg_pistol_beretta, 1, UH_ACTTABLE_PISTOL, 1 )
UH_IMPLEMENT_WEAPON( CWeaponPistolSocom,	weapon_pistol_socom,		WeaponPistolSocom,		0.1f, sk_plr_dmg_pistol_socom, 1, UH_ACTTABLE_PISTOL, 1 )
UH_IMPLEMENT_WEAPON( CWeaponPython,			weapon_pistol_python,		WeaponPython,			0.75f, sk_plr_dmg_pistol_python, 1, UH_ACTTABLE_PISTOL, 1 )
UH_IMPLEMENT_WEAPON( CWeaponPistolDualies,	weapon_pistol_dualberetta,	WeaponPistolDualies,	0.1f, sk_plr_dmg_pistol_dualberetta, 1, UH_ACTTABLE_PISTOL, 1 )

//-----------------------------------------------------------------------------
// SMGs — full auto, 0.075 s (exact, GetFireRate).
//-----------------------------------------------------------------------------
UH_IMPLEMENT_WEAPON( CWeaponSMGMP5,			weapon_smg_mp5,		WeaponSMGMP5,		0.075f, sk_plr_dmg_smg_mp5, 0, UH_ACTTABLE_SMG1, 1 )
UH_IMPLEMENT_WEAPON( CWeaponSMGMP5EOD,		weapon_smg_mp5_eod,	WeaponSMGMP5EOD,	0.075f, sk_plr_dmg_smg_mp5_eod, 0, UH_ACTTABLE_SMG1, 1 )
UH_IMPLEMENT_WEAPON( CWeaponSMGMP7,			weapon_smg_mp7,		WeaponSMGMP7,		0.075f, sk_plr_dmg_smg_mp7, 0, UH_ACTTABLE_SMG1, 1 )

//-----------------------------------------------------------------------------
// Shotguns — 12 pellets from the shipped sk_plr_num_shotgun_pellets setting.
// M3/M5/SPAS use the pump path; XM1014 is self-loading and skips it.
//-----------------------------------------------------------------------------
UH_IMPLEMENT_WEAPON( CWeaponShotgunM3,		weapon_shotgun_m3,		WeaponShotgunM3,		0.8f, sk_plr_dmg_shotgun_m3, 0, UH_ACTTABLE_SHOTGUN, 12 )
UH_IMPLEMENT_WEAPON( CWeaponShotgunM5,		weapon_shotgun_m5,		WeaponShotgunM5,		0.8f, sk_plr_dmg_shotgun_m5, 0, UH_ACTTABLE_SHOTGUN, 12 )
UH_IMPLEMENT_WEAPON( CWeaponShotgunSpas12,	weapon_shotgun_spas12,	WeaponShotgunSpas12,	0.8f, sk_plr_dmg_shotgun_spas12, 0, UH_ACTTABLE_SHOTGUN, 12 )
UH_IMPLEMENT_WEAPON( CWeaponShotgunXM1014,	weapon_shotgun_xm1014,	WeaponShotgunXM1014,	0.8f, sk_plr_dmg_shotgun_xm1014, 0, UH_ACTTABLE_SHOTGUN, 12 )

//-----------------------------------------------------------------------------
// Rifles — G36K is select-fire (0.1 s full-auto). Weapon type 4 = rifle.
// The sniper is bolt-action: refire is gated on the bolt sequence duration
// (like the vanilla sniper), so 1.0 s.
//-----------------------------------------------------------------------------
UH_IMPLEMENT_WEAPON( CWeaponG36K,			weapon_rifle_g36k,		WeaponG36K,		0.1f, sk_plr_dmg_rifle_g36k, 4, UH_ACTTABLE_AR2, 1 )
UH_IMPLEMENT_WEAPON( CWeaponSniper,			weapon_rifle_sniper,	WeaponSniper,	1.0f, sk_plr_dmg_rifle_sniper, 4, UH_ACTTABLE_AR2, 1 )

//-----------------------------------------------------------------------------
// BFG — minigun is 0.075 s (exact, GetFireRate); MGL is a single-shot grenade
// launcher (custom fire path, ~1.0 s — TODO exact).
//-----------------------------------------------------------------------------
UH_IMPLEMENT_WEAPON( CWeaponBfgMgl,			weapon_bfg_mgl,			WeaponBfgMgl,		1.0f, sk_plr_dmg_bfg_mgl, 0, UH_ACTTABLE_SHOTGUN, 1 )
UH_IMPLEMENT_WEAPON( CWeaponBfgMinigun,	weapon_bfg_minigun,		WeaponBfgMinigun,	0.075f, sk_plr_dmg_bfg_minigun, 0, UH_ACTTABLE_SMG1, 1 )

//-----------------------------------------------------------------------------
// Purpose: The original impulse-101 loadout (decode sub_101EC700 case 101):
// full ammo for every type, then the Underhell weapon set + crossbow + frag.
// Cleaver and the BFG weapons are NOT part of the cheat. Matches the original
// exactly; the vanilla base class's impulse 101 is not also called.
//-----------------------------------------------------------------------------
void UH_GiveAllWeapons( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	pPlayer->GiveAmmo( 255, "Pistol" );
	pPlayer->GiveAmmo( 255, "AR2" );
	pPlayer->GiveAmmo( 5, "AR2AltFire" );
	pPlayer->GiveAmmo( 255, "SMG1" );
	pPlayer->GiveAmmo( 255, "Buckshot" );
	pPlayer->GiveAmmo( 3, "smg1_grenade" );
	pPlayer->GiveAmmo( 3, "rpg_round" );
	pPlayer->GiveAmmo( 5, "grenade" );
	pPlayer->GiveAmmo( 32, "357" );
	pPlayer->GiveAmmo( 16, "XBowBolt" );
	pPlayer->GiveAmmo( 5, "Hopwire" );

	pPlayer->GiveNamedItem( "weapon_frag" );
	pPlayer->GiveNamedItem( "weapon_melee_pipe" );
	pPlayer->GiveNamedItem( "weapon_melee_axe" );
	pPlayer->GiveNamedItem( "weapon_melee_wrench" );
	pPlayer->GiveNamedItem( "weapon_melee_baton" );
	pPlayer->GiveNamedItem( "weapon_crossbow" );
	pPlayer->GiveNamedItem( "weapon_pistol_glock" );
	pPlayer->GiveNamedItem( "weapon_pistol_socom" );
	pPlayer->GiveNamedItem( "weapon_pistol_beretta" );
	pPlayer->GiveNamedItem( "weapon_pistol_python" );
	pPlayer->GiveNamedItem( "weapon_pistol_dualberetta" );
	pPlayer->GiveNamedItem( "weapon_smg_mp5" );
	pPlayer->GiveNamedItem( "weapon_smg_mp7" );
	pPlayer->GiveNamedItem( "weapon_smg_mp5_eod" );
	pPlayer->GiveNamedItem( "weapon_shotgun_spas12" );
	pPlayer->GiveNamedItem( "weapon_shotgun_m3" );
	pPlayer->GiveNamedItem( "weapon_shotgun_m5" );
	pPlayer->GiveNamedItem( "weapon_shotgun_xm1014" );
	pPlayer->GiveNamedItem( "weapon_rifle_g36k" );
	pPlayer->GiveNamedItem( "weapon_rifle_sniper" );

	if ( pPlayer->GetHealth() < 100 )
	{
		pPlayer->TakeHealth( 25, DMG_GENERIC );
	}
}
