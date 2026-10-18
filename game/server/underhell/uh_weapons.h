//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell weapon classes. Class names + buckets + models + ammo all
//          come from the mod's weapon scripts (scripts/weapon_*.txt), which the
//          SDK's weapon_parse reads automatically. Each class below only pins
//          the classname and the C++ derivation; the tuning (StaminaToDrain,
//          MeleeRange, PunchPitch, ExpOffset ironsight, ...) is read from the
//          script via GetWpnData().
//
// Original class list (serveror.dll RTTI + FGD/Weapon List.txt):
//   melee:  axe / baton / pipe / wrench / cleaver
//   pistol: glock / beretta / socom / python / dualberetta
//   smg:    mp5 / mp5_eod / mp7
//   shotgun: m3 / m5 / spas12 / xm1014
//   rifle:  g36k / sniper
//   bfg:    mgl / minigun
// Vanilla HL2 weapons (weapon_pistol / weapon_shotgun / weapon_smg1 /
// weapon_357 / weapon_rpg / weapon_crossbow / weapon_crowbar) stay vanilla.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UH_WEAPONS_H
#define UH_WEAPONS_H
#ifdef _WIN32
#pragma once
#endif

#include "basecombatweapon.h"
#include "basehlcombatweapon.h"
#include "basebludgeonweapon.h"

//-----------------------------------------------------------------------------
// Shared melee base: reads MeleeRange / MeleeRoF / StaminaToDrain from the
// weapon script. Damage is per-class (values from the original skill.cfg's
// sk_plr_dmg_* entries — see uh_weapons.cpp).
//-----------------------------------------------------------------------------
class CUHMeleeWeapon : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS( CUHMeleeWeapon, CBaseHLBludgeonWeapon );

public:
	virtual void	PrimaryAttack( void );	// starts wind-up; impact follows MeleeDelayedFire
	virtual void	SecondaryAttack( void );	// no vanilla secondary swing in Underhell melee
	virtual void	ItemPostFrame( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual float	GetDamageForActivity( Activity hitActivity ) { return GetDamage(); }
	virtual float	GetDamage( void )
	{
		CBaseCombatCharacter *pOwner = GetOwner();
		return ( pOwner && pOwner->IsPlayer() ? m_pPlayerDamage : m_pNPCDamage )->GetFloat();
	}
	virtual float	GetRange( void ) { return GetWpnData().m_flMeleeRange; }
	virtual float	GetFireRate( void ) { return GetWpnData().m_flMeleeRoF; }

	// NPC melee path (Swing() is player-only; the AE event routes here, like
	// the crowbar's Operator_HandleAnimEvent / HandleAnimEventMeleeHit).
	virtual void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void			HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	ConVar			*m_pPlayerDamage;		// sk_plr_dmg_<weapon> (skill.cfg)
	ConVar			*m_pNPCDamage;			// sk_npc_dmg_<weapon> (skill.cfg)
	bool			m_bDelayedMeleeAttack;
	float			m_flDelayedMeleeAttackTime;
};

//-----------------------------------------------------------------------------
// Shared gun base: fires bullets through the engine's bullet path with the
// per-weapon damage (skill.cfg sk_plr_dmg_*). Recoil (PunchPitch/PunchYaw) and
// spread (CrouchAccuracyMult/RunAccuracyMult/ExpOffset accuracy) are read from
// the weapon script via GetWpnData().
//-----------------------------------------------------------------------------
class CUHGunWeapon : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CUHGunWeapon, CBaseHLCombatWeapon );

public:
	virtual void	Precache( void );
	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual float	GetDamage( void ) { return m_pDamage->GetFloat(); }
	virtual float	GetFireRate( void ) { return m_flFireRate; }
	virtual const Vector &GetBulletSpread( void );
	virtual void	AddViewKick( void );

	// NPCs can fire this weapon as a ranged attack (the AI checks this bit in
	// GatherConditions before setting COND_CAN_RANGE_ATTACK1). Without it, a
	// combine soldier holding the gun never attempts to shoot.
	virtual int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	// NPC fire path. PrimaryAttack() is player-only; NPCs fire through anim
	// events. Two entry points, both routing to FireNPCPrimaryAttack():
	//   - Operator_ForceNPCFire()  — the new AE_NPC_WEAPON_FIRE event.
	//   - Operator_HandleAnimEvent() — the old EVENT_WEAPON_* events (EVENT_WEAPON_AR2,
	//     EVENT_WEAPON_SMG1, EVENT_WEAPON_SHOTGUN_FIRE, ...) the combine model's
	//     attack sequences actually fire. Without this the NPC aims (muzzle flash)
	//     but never fires a bullet.
	virtual void	Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary );
	virtual void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void			FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator );

	// Underhell silencer. "silencer_toggle" gates pistols (type 1) and rifles
	// (type 4) on the player carrying the matching silencer (m_bHavePistol/
	// RifleSilencer); everything else toggles freely (decode sub_101E2F50).
	// m_bSilenced + the silenced activities live on the shared base
	// CBaseHLCombatWeapon (networked, so the client viewmodel animates too).
	virtual int		GetWeaponType( void ) { return m_iWeaponType; }

	// Underhell select fire (decode sub_102B0D10 / sub_102B18E0). firemode_toggle
	// flips full-auto <-> semi. m_bFireOnEdge is the semi-auto trigger latch: it
	// is armed in WeaponIdle() (attack released) and consumed by PrimaryAttack()
	// so a held trigger only fires one shot in semi mode.
	void			UH_ToggleFireMode( void );
	virtual void	WeaponIdle( void );
	virtual void	ItemPostFrame( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	float			m_flFireRate;			// seconds between shots
	ConVar			*m_pDamage;				// sk_plr_dmg_<weapon> (skill.cfg)
	int				m_iWeaponType;			// 1 = pistol, 4 = rifle, 0 = other (silencer gating)
	int				m_iShotsPerFire;		// pellets per shot (shotguns = sk_plr_num_shotgun_pellets, 7)
	float			m_flAccuracyPenalty;	// grows per shot, decays over time
	int				m_iFireMode;			// 1 = full auto, 2 = semi (FIREMODE_*)
	bool			m_bFireOnEdge;			// semi-auto trigger latch
	bool			m_bFireModeInitialized;	// weapon-script FireMode applied once
	bool			m_bNeedPump;				// pending shotgun pump animation
	float			m_flPumpTime;				// time to play the pump after firing
	CHandle< CBaseEntity > m_hLaserDot;		// SOCOM env_laserdot
	bool			m_bSocomLaserOn;
};

//-----------------------------------------------------------------------------
// Declares one concrete weapon. The weapon script (scripts/weapon_<classname>
// .txt) supplies viewmodel, worldmodel, bucket, clip size and ammo. The short
// name (_shortName) is the C++ identifier used for the send/recv table, and it
// must match the client stub's class name in c_uh_weapons.cpp. _npcActivity is
// the NPC ranged-attack activity (ACT_RANGE_ATTACK_*) the acttable maps
// ACT_RANGE_ATTACK1 to, chosen from the weapon's anim_prefix.
//-----------------------------------------------------------------------------
#define UH_DECLARE_WEAPON( _className, _shortName, _npcActivity ) \
	class _className : public CUHGunWeapon \
	{ \
		DECLARE_CLASS( _className, CUHGunWeapon ); \
		DECLARE_ACTTABLE(); \
	public: \
		DECLARE_SERVERCLASS(); \
		_className(); \
	};

#define UH_DECLARE_MELEE( _className, _shortName ) \
	class _className : public CUHMeleeWeapon \
	{ \
		DECLARE_CLASS( _className, CUHMeleeWeapon ); \
		DECLARE_ACTTABLE(); \
	public: \
		DECLARE_SERVERCLASS(); \
		_className(); \
	};

//-----------------------------------------------------------------------------
// Melee
//-----------------------------------------------------------------------------
UH_DECLARE_MELEE( CWeaponAxe,		WeaponAxe )
UH_DECLARE_MELEE( CWeaponBaton,		WeaponBaton )
UH_DECLARE_MELEE( CWeaponPipe,		WeaponPipe )
UH_DECLARE_MELEE( CWeaponWrench,	WeaponWrench )
UH_DECLARE_MELEE( CWeaponCleaver,	WeaponCleaver )

//-----------------------------------------------------------------------------
// Pistols — ACT_RANGE_ATTACK_PISTOL (1:1 with the original acttables; the
// vanilla pistol / 357 map here too). Citizens / cops use this activity, so
// mapping to AR2 left them with no fire animation (T-pose + no shooting).
//-----------------------------------------------------------------------------
UH_DECLARE_WEAPON( CWeaponPistolGlock,		WeaponPistolGlock,		ACT_RANGE_ATTACK_PISTOL )
UH_DECLARE_WEAPON( CWeaponPistolBeretta,	WeaponPistolBeretta,	ACT_RANGE_ATTACK_PISTOL )
UH_DECLARE_WEAPON( CWeaponPistolSocom,		WeaponPistolSocom,		ACT_RANGE_ATTACK_PISTOL )
UH_DECLARE_WEAPON( CWeaponPython,			WeaponPython,			ACT_RANGE_ATTACK_PISTOL )
UH_DECLARE_WEAPON( CWeaponPistolDualies,	WeaponPistolDualies,	ACT_RANGE_ATTACK_PISTOL )

//-----------------------------------------------------------------------------
// SMGs — ACT_RANGE_ATTACK_SMG1 (combine_soldier.mdl has this pose, like the
// vanilla weapon_smg1). AR2 was a wrong fallback.
//-----------------------------------------------------------------------------
UH_DECLARE_WEAPON( CWeaponSMGMP5,		WeaponSMGMP5,		ACT_RANGE_ATTACK_SMG1 )
UH_DECLARE_WEAPON( CWeaponSMGMP5EOD,	WeaponSMGMP5EOD,	ACT_RANGE_ATTACK_SMG1 )
UH_DECLARE_WEAPON( CWeaponSMGMP7,		WeaponSMGMP7,		ACT_RANGE_ATTACK_SMG1 )

//-----------------------------------------------------------------------------
// Shotguns — combine soldiers have the shotgun attack animation.
//-----------------------------------------------------------------------------
UH_DECLARE_WEAPON( CWeaponShotgunM3,		WeaponShotgunM3,		ACT_RANGE_ATTACK_SHOTGUN )
UH_DECLARE_WEAPON( CWeaponShotgunM5,		WeaponShotgunM5,		ACT_RANGE_ATTACK_SHOTGUN )
UH_DECLARE_WEAPON( CWeaponShotgunSpas12,	WeaponShotgunSpas12,	ACT_RANGE_ATTACK_SHOTGUN )
UH_DECLARE_WEAPON( CWeaponShotgunXM1014,	WeaponShotgunXM1014,	ACT_RANGE_ATTACK_SHOTGUN )

//-----------------------------------------------------------------------------
// Rifles
//-----------------------------------------------------------------------------
UH_DECLARE_WEAPON( CWeaponG36K,		WeaponG36K,		ACT_RANGE_ATTACK_AR2 )
UH_DECLARE_WEAPON( CWeaponSniper,	WeaponSniper,	ACT_RANGE_ATTACK_AR2 )

//-----------------------------------------------------------------------------
// BFG — mgl = grenade launcher (shotgun pose); minigun = SMG pose (anim_prefix smg2).
//-----------------------------------------------------------------------------
UH_DECLARE_WEAPON( CWeaponBfgMgl,		WeaponBfgMgl,		ACT_RANGE_ATTACK_SHOTGUN )
UH_DECLARE_WEAPON( CWeaponBfgMinigun,	WeaponBfgMinigun,	ACT_RANGE_ATTACK_SMG1 )

// Helper used by the "give all weapons" cheat (impulse 101).
void UH_GiveAllWeapons( CBasePlayer *pPlayer );

#endif // UH_WEAPONS_H
