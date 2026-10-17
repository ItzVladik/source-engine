//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell weapon client classes. Thin stubs (no extra networked
//          state) so the client can spawn the weapon entities the server
//          creates. Melee derives from C_BaseHLBludgeonWeapon, guns from
//          C_BaseHLCombatWeapon.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_weapon__stubs.h"
#include "basehlcombatweapon_shared.h"
#include "hl2/c_basehlcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Melee
STUB_WEAPON_CLASS( weapon_melee_axe, WeaponAxe, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_melee_baton, WeaponBaton, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_melee_pipe, WeaponPipe, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_melee_wrench, WeaponWrench, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_cleaver, WeaponCleaver, C_BaseHLBludgeonWeapon );

// Pistols
STUB_WEAPON_CLASS( weapon_pistol_glock, WeaponPistolGlock, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_pistol_beretta, WeaponPistolBeretta, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_pistol_socom, WeaponPistolSocom, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_pistol_python, WeaponPython, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_pistol_dualberetta, WeaponPistolDualies, C_BaseHLCombatWeapon );

// SMGs
STUB_WEAPON_CLASS( weapon_smg_mp5, WeaponSMGMP5, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_smg_mp5_eod, WeaponSMGMP5EOD, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_smg_mp7, WeaponSMGMP7, C_BaseHLCombatWeapon );

// Shotguns
STUB_WEAPON_CLASS( weapon_shotgun_m3, WeaponShotgunM3, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_shotgun_m5, WeaponShotgunM5, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_shotgun_spas12, WeaponShotgunSpas12, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_shotgun_xm1014, WeaponShotgunXM1014, C_BaseHLCombatWeapon );

// Rifles
STUB_WEAPON_CLASS( weapon_rifle_g36k, WeaponG36K, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_rifle_sniper, WeaponSniper, C_BaseHLCombatWeapon );

// BFG
STUB_WEAPON_CLASS( weapon_bfg_mgl, WeaponBfgMgl, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_bfg_minigun, WeaponBfgMinigun, C_BaseHLCombatWeapon );
