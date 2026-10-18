//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell NPC AI extensions + dismemberment, installed on
//          CAI_BaseNPC (the FGD exposes them on BaseNPC).
//
// Behaviour 1:1 with the original, decoded from the FGD (underhell_base.fgd
// BaseNPC) + the ModDB tutorials:
//   - "Enemy AI Improvements & Sneaky Gameplay"  -> uh_fos / uh_viewdistance /
//     temp squads (squadtemp) / uh_spotbodies + OnSpot* outputs.
//   - "Bodygroups, Gibs, Ragdolls and Decals"    -> uh_bodygroup string parser,
//     Gib* inputs + gib-on-damage, the uh_gibhealth / uh_headhealth /
//     uh_helmethealth / uh_maxsergibs / uh_maxseragdolls / uh_ragdollcollisiontype /
//     uh_bodymousedamper / uh_maxitems convars.
//
// Note: the original's bodygroup parser "WILL crash" on a bad bodygroup name
// (per the tutorial); this port fails soft (FindBodygroupByName == -1 -> skip)
// so a typo can't take down the game.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_senses.h"
#include "igamesystem.h"
#include "ragdoll_shared.h"
#include "physics_prop_ragdoll.h"
#include "physics_shared.h"
#include "decals.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Dismemberment tuning convars (original names + defaults).
//-----------------------------------------------------------------------------
static ConVar uh_gibhealth( "uh_gibhealth", "80", FCVAR_ARCHIVE,
	"Damage to gib a leg. Arms take 50% of this value." );
static ConVar uh_headhealth( "uh_headhealth", "21", FCVAR_ARCHIVE,
	"Damage to destroy the head." );
static ConVar uh_helmethealth( "uh_helmethealth", "30", FCVAR_ARCHIVE,
	"Damage a helmet takes before being shot off." );
static ConVar uh_maxsergibs( "uh_maxsergibs", "8", FCVAR_ARCHIVE,
	"Max server-side gibs before the oldest becomes a client ragdoll." );
static ConVar uh_maxseragdolls( "uh_maxseragdolls", "16", FCVAR_ARCHIVE,
	"Max server-side ragdolls before the oldest becomes a client ragdoll." );
static ConVar uh_ragdollcollisiontype( "uh_ragdollcollisiontype", "1", FCVAR_ARCHIVE,
	"Collision group for server-side ragdolls (-1 = client-side, 0-19 = group)." );
ConVar uh_bodymousedamper( "uh_bodymousedamper", "4", FCVAR_ARCHIVE,
	"Mouse sensitivity divider while dragging a ragdoll (weight illusion)." );
static ConVar uh_maxitems( "uh_maxitems", "32", FCVAR_ARCHIVE,
	"Limit on enemy-dropped items/objects." );

//-----------------------------------------------------------------------------
// Gib model lookup: a severed limb model exists per body. The original uses a
// model-specific folder ("models\\Gibs\\BodyParts\\Soldier\\leftarm.mdl" etc.).
// Unknown models fall back to no gib (bodygroup change + blood still apply).
//-----------------------------------------------------------------------------
static int UH_FindBodygroup( CBaseAnimating *pBody, const char *pszName );

enum
{
	UH_GIBTYPE_NONE = -1,
	UH_GIBTYPE_INFECTED_INMATE = 0,
	UH_GIBTYPE_INFECTED_WORKER = 1,
	UH_GIBTYPE_INFECTED_DOCTOR = 2,
	UH_GIBTYPE_INFECTED_UNIFORM = 3,
	UH_GIBTYPE_INFECTED_URBAN = 4,
	UH_GIBTYPE_INFECTED_RURAL = 5,
	UH_GIBTYPE_INFECTED_GUARD = 6,
	UH_GIBTYPE_INFECTED_OFFICE = 7,
	UH_GIBTYPE_COMBINE = 8,
	UH_GIBTYPE_PMC = 10,
};

enum
{
	UH_PART_HEAD = 0,
	UH_PART_LEFTARM,
	UH_PART_RIGHTARM,
	UH_PART_LEFTLEG,
	UH_PART_RIGHTLEG,
};

static int UH_LimbModelIndex( const char *pszLimb )
{
	if ( !V_stricmp( pszLimb, "leftarm" ) ) return 0;
	if ( !V_stricmp( pszLimb, "rightarm" ) ) return 1;
	if ( !V_stricmp( pszLimb, "leftleg" ) ) return 2;
	if ( !V_stricmp( pszLimb, "rightleg" ) ) return 3;
	return -1;
}

static const char *UH_GibModelFor( CBaseAnimating *pBody, const char *pszLimb )
{
	const int i = UH_LimbModelIndex( pszLimb );
	if ( i < 0 )
		return NULL;

	if ( CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC *>( pBody ) )
		return pNPC->m_iszUHGibModel[i] != NULL_STRING ? STRING( pNPC->m_iszUHGibModel[i] ) : NULL;
	if ( CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp *>( pBody ) )
		return pRagdoll->m_iszUHGibModel[i] != NULL_STRING ? STRING( pRagdoll->m_iszUHGibModel[i] ) : NULL;
	return NULL;
}

static void UH_SetGibPath( CAI_BaseNPC *pNPC, int i, const char *pszPath )
{
	pNPC->m_iszUHGibModel[i] = pszPath ? AllocPooledString( pszPath ) : NULL_STRING;
}

static bool UH_UsesSixStateArms( int iType )
{
	return iType != UH_GIBTYPE_INFECTED_WORKER &&
		iType != UH_GIBTYPE_INFECTED_DOCTOR &&
		iType != UH_GIBTYPE_INFECTED_GUARD &&
		iType != UH_GIBTYPE_COMBINE && iType != UH_GIBTYPE_PMC;
}

static int UH_InfectedTypeForModel( const char *pszModel )
{
	static const char *s_Variants[] = { "inmate", "worker", "doctor", "uniform", "urban", "rural", "guard", "office" };
	for ( int i = 0; i < ARRAYSIZE( s_Variants ); ++i )
		if ( V_stristr( pszModel, s_Variants[i] ) ) return i;
	return UH_GIBTYPE_INFECTED_INMATE;
}

// Establish the same persistent state that the original constructors place at
// CAI_BaseNPC+1708. Only authored Combine/Infected families are gibable.
static void UH_ConfigureNPCDismemberment( CAI_BaseNPC *pNPC )
{
	// A restored or re-activated NPC already owns authoritative counters.
	if ( pNPC->m_bUHGibable ) return;
	const char *pszClass = pNPC->GetClassname();
	const char *pszModel = STRING( pNPC->GetModelName() );
	const bool bInfected = !V_stricmp( pszClass, "npc_infected" ) || V_stristr( pszModel, "infected_" );
	const bool bPMC = V_stristr( pszModel, "pmc" ) != NULL;
	const bool bPrison = V_stristr( pszModel, "prisonguard" ) != NULL;
	const bool bCombine = !V_stricmp( pszClass, "npc_combine_s" ) || V_stristr( pszModel, "combine_soldier" );

	pNPC->m_bUHGibable = bInfected || bPMC || bPrison || bCombine;
	if ( !pNPC->m_bUHGibable )
		return;

	pNPC->m_iUHGibType = bInfected ? UH_InfectedTypeForModel( pszModel ) : bPMC ? UH_GIBTYPE_PMC : UH_GIBTYPE_COMBINE;
	const float flScale = bInfected ? 0.5f : 1.0f;
	pNPC->m_iUHPartHealth[UH_PART_HEAD] = (int)( uh_headhealth.GetFloat() * flScale );
	pNPC->m_iUHPartHealth[UH_PART_LEFTARM] = (int)( uh_gibhealth.GetFloat() * 0.5f * flScale );
	pNPC->m_iUHPartHealth[UH_PART_RIGHTARM] = pNPC->m_iUHPartHealth[UH_PART_LEFTARM];
	pNPC->m_iUHPartHealth[UH_PART_LEFTLEG] = (int)( uh_gibhealth.GetFloat() * flScale );
	pNPC->m_iUHPartHealth[UH_PART_RIGHTLEG] = pNPC->m_iUHPartHealth[UH_PART_LEFTLEG];
	pNPC->m_iUHHelmetHealth = uh_helmethealth.GetInt();
	pNPC->m_nUHSeveredParts = 0;
	int iArms = UH_FindBodygroup( pNPC, "arms" );
	if ( iArms >= 0 )
	{
		const int n = pNPC->GetBodygroup( iArms );
		if ( UH_UsesSixStateArms( pNPC->m_iUHGibType ) )
		{
			if ( n & 2 ) pNPC->m_nUHSeveredParts |= 1u << UH_PART_LEFTARM;
			if ( n & 4 ) pNPC->m_nUHSeveredParts |= 1u << UH_PART_RIGHTARM;
		}
		else
		{
			if ( n & 1 ) pNPC->m_nUHSeveredParts |= 1u << UH_PART_LEFTARM;
			if ( n & 2 ) pNPC->m_nUHSeveredParts |= 1u << UH_PART_RIGHTARM;
		}
	}
	int iInitialHead = UH_FindBodygroup( pNPC, "head" );
	if ( iInitialHead >= 0 )
	{
		const int n = pNPC->GetBodygroup( iInitialHead );
		if ( ( pNPC->m_iUHGibType == UH_GIBTYPE_INFECTED_GUARD && n >= 10 ) ||
			( ( pNPC->m_iUHGibType == UH_GIBTYPE_COMBINE || pNPC->m_iUHGibType == UH_GIBTYPE_PMC ) && n == 1 ) ||
			( pNPC->m_iUHGibType != UH_GIBTYPE_INFECTED_GUARD && pNPC->m_iUHGibType != UH_GIBTYPE_COMBINE && pNPC->m_iUHGibType != UH_GIBTYPE_PMC && n == 9 ) )
			pNPC->m_nUHSeveredParts |= 1u << UH_PART_HEAD;
	}
	int iInitialLegs = UH_FindBodygroup( pNPC, "legs" );
	if ( iInitialLegs >= 0 )
	{
		const int n = pNPC->GetBodygroup( iInitialLegs );
		if ( n & 1 ) pNPC->m_nUHSeveredParts |= 1u << UH_PART_LEFTLEG;
		if ( n & 2 ) pNPC->m_nUHSeveredParts |= 1u << UH_PART_RIGHTLEG;
	}

	const char *pszFolder = NULL;
	const char *pszPrefix = "";
	if ( bInfected )
	{
		pszFolder = "models/gibs/bodyparts/infected";
		static const char *s_Variants[] = { "inmate", "worker", "doctor", "uniform", "urban", "rural", "guard", "office" };
		for ( int i = 0; i < ARRAYSIZE( s_Variants ); ++i )
			if ( V_stristr( pszModel, s_Variants[i] ) ) { pszPrefix = s_Variants[i]; break; }
	}
	else if ( bPMC ) { pszFolder = "models/gibs/bodyparts/pmc"; pszPrefix = "pmc"; }
	else if ( bPrison ) pszFolder = "models/gibs/bodyparts/soldier_prisonguard";
	else pszFolder = "models/gibs/bodyparts/soldier";

	static const char *s_Limbs[] = { "leftarm", "rightarm", "leftleg", "rightleg" };
	for ( int i = 0; i < 4; ++i )
	{
		const char *pszSep = pszPrefix[0] ? "_" : "";
		UH_SetGibPath( pNPC, i, UTIL_VarArgs( "%s/%s%s%s.mdl", pszFolder, pszPrefix, pszSep, s_Limbs[i] ) );
	}

	// Heavy Combine legs use separately authored meshes. Store the selected
	// paths now so the exact choice survives NPC -> ragdoll conversion.
	int iLegs = UH_FindBodygroup( pNPC, "legs" );
	if ( !bInfected && !bPMC && iLegs >= 0 && pNPC->GetBodygroup( iLegs ) >= 4 )
	{
		UH_SetGibPath( pNPC, 2, "models/gibs/bodyparts/soldier/leftleg2.mdl" );
		UH_SetGibPath( pNPC, 3, "models/gibs/bodyparts/soldier/rightleg2.mdl" );
		// sub_103596C0 doubles both leg counters for the heavy authored set.
		pNPC->m_iUHPartHealth[UH_PART_LEFTLEG] *= 2;
		pNPC->m_iUHPartHealth[UH_PART_RIGHTLEG] *= 2;
	}

	pNPC->m_bForceServerRagdoll = ( uh_ragdollcollisiontype.GetInt() >= 0 );
}

//-----------------------------------------------------------------------------
// Bodygroup removal. Bodygroup names + values from the tutorial's
// combine_soldier table; the transition removes the named side. If the model
// lacks the bodygroup, fail soft (the original would crash here).
//-----------------------------------------------------------------------------
// sub_10031BF0 selects the arm transition by its saved family discriminator:
// types 1/2/6/8/10 use 0/1/2/3; generic families use 0/2/4/6.
static int UH_FindBodygroup( CBaseAnimating *pBody, const char *pszName )
{
	int i = pBody->FindBodygroupByName( pszName );
	if ( i >= 0 )
		return i;

	char alternate[64];
	V_strncpy( alternate, pszName, sizeof( alternate ) );
	if ( alternate[0] >= 'a' && alternate[0] <= 'z' )
		alternate[0] -= 'a' - 'A';
	else if ( alternate[0] >= 'A' && alternate[0] <= 'Z' )
		alternate[0] += 'a' - 'A';
	return pBody->FindBodygroupByName( alternate );
}

static bool UH_RemoveBodygroupSide( CBaseAnimating *pBody, const char *pszGroup, int iSide )
{
	int iGroup = UH_FindBodygroup( pBody, pszGroup );
	if ( iGroup < 0 )
		return false;

	const int iCurrent = pBody->GetBodygroup( iGroup );
	const int iCount = pBody->GetBodygroupCount( iGroup );
	int iNew = iCurrent;
	int iType = UH_GIBTYPE_NONE;
	if ( CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC *>( pBody ) ) iType = pNPC->m_iUHGibType;
	else if ( CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp *>( pBody ) ) iType = pRagdoll->m_iUHGibType;

	if ( !V_stricmp( pszGroup, "arms" ) )
	{
		// sub_10031BF0 cases 4/5: types 1/2/6/8/10 use +1/+2;
		// the generic families use the authored +2/+4 state set.
		const int iMask = UH_UsesSixStateArms( iType ) ? ( iSide == 0 ? 2 : 4 ) : ( iSide == 0 ? 1 : 2 );
		iNew = iCurrent | iMask;
	}
	else if ( !V_stricmp( pszGroup, "legs" ) )
	{
		// The leg bodygroup has ordinary and heavy sets.  Preserve the heavy
		// base (bit 2) and mark the severed side in its low bits.
		const int iMask = iSide == 0 ? 1 : 2;
		iNew = iCurrent | iMask;
	}
	else
	{
		iNew = iCurrent | ( iSide == 0 ? 1 : 2 );
	}

	if ( iNew == iCurrent || iNew >= iCount )
		return false;
	pBody->SetBodygroup( iGroup, iNew );
	pBody->ResetSequenceInfo();
	return true;
}

// sub_10031BF0 uses the saved family discriminator: type 8/10 uses head 1,
// guard type 6 uses 10/11, and the generic infected families use head 9.
static int UH_DestroyedHeadBodygroup( CBaseAnimating *pBody )
{
	int iType = UH_GIBTYPE_NONE;
	if ( CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC *>( pBody ) ) iType = pNPC->m_iUHGibType;
	else if ( CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp *>( pBody ) ) iType = pRagdoll->m_iUHGibType;

	int iHead = UH_FindBodygroup( pBody, "head" );
	if ( iType == UH_GIBTYPE_COMBINE || iType == UH_GIBTYPE_PMC ) return 1;
	if ( iType == UH_GIBTYPE_INFECTED_GUARD )
		return ( iHead >= 0 && pBody->GetBodygroup( iHead ) >= 9 ) ? 11 : 10;
	// Original default branch uses destroyed-head value 9. Reduced
	// replacement models get the only authored destroyed state instead.
	if ( iHead >= 0 && pBody->GetBodygroupCount( iHead ) > 9 ) return 9;
	return 1;
}

//-----------------------------------------------------------------------------
// Bodygroup string parser: "Helmet2Arms1Legs4" -> FindBodygroupByName + value.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UH_ApplySpawnSettings( void )
{
	// Bodygroups ("uh_bodygroup" keyvalue).
	if ( m_uh_bodygroup != NULL_STRING )
	{
		const char *psz = STRING( m_uh_bodygroup );
		const char *p = psz;
		while ( *p )
		{
			// Read the leading name (letters), then the value (digits).
			char name[64];
			int n = 0;
			while ( *p && !((*p) >= '0' && (*p) <= '9') && n < (int)sizeof(name) - 1 )
				name[n++] = *p++;
			name[n] = '\0';

			if ( *p == '\0' )
				break;	// trailing name with no number — ignore.

			int value = atoi( p );
			while ( *p && ((*p) >= '0' && (*p) <= '9') )
				p++;

			if ( name[0] && value >= 0 )
			{
				int iGroup = FindBodygroupByName( name );
				if ( iGroup >= 0 )
					SetBodygroup( iGroup, value );
			}
		}
	}
	else
	{
		// No explicit bodygroups: randomize armor / helmet / legs, exactly like
		// the original (sub_10338D30): Armor 0-2, Helmet 0-5, Legs 0 or 4
		// (heavy). This is what gives combine soldiers their helmets + armor
		// variety. Clamped to each bodygroup's actual variant count.
		int iArmor = FindBodygroupByName( "Armor" );
		if ( iArmor >= 0 )
			SetBodygroup( iArmor, random->RandomInt( 0, min( 2, GetBodygroupCount( iArmor ) - 1 ) ) );

		int iHelmet = FindBodygroupByName( "Helmet" );
		if ( iHelmet >= 0 )
			SetBodygroup( iHelmet, random->RandomInt( 0, min( 5, GetBodygroupCount( iHelmet ) - 1 ) ) );

		int iLegs = FindBodygroupByName( "Legs" );
		if ( iLegs >= 0 )
		{
			int iHeavy = ( GetBodygroupCount( iLegs ) > 4 ) ? 4 : 0;
			SetBodygroup( iLegs, random->RandomInt( 0, 1 ) ? iHeavy : 0 );
		}
	}

	// Configure persistent part-health/model state after bodygroups and the final
	// model have been selected. Non-authored NPC classes remain non-gibable.
	UH_ConfigureNPCDismemberment( this );

	// Field of view ("uh_fos" in degrees -> dot product).
	if ( m_flUhFOV > 0.0f )
	{
		// The SDK stores m_flFieldOfView as a cosine; a larger dot = narrower
		// cone. Convert degrees to the dot product.
		m_flFieldOfView = cos( DEG2RAD( m_flUhFOV * 0.5f ) );
	}

	// View distance ("uh_viewdistance"). The original (sub_10022930) sets BOTH
	// the NPC's "too far to attack" distance AND the senses' look distance
	// (CAI_Senses::m_LookDist), so the soldier can't SEE the player past that
	// range. Setting only m_flDistTooFar left the senses at the 2048 default.
	if ( m_flUhViewDistance > 0.0f )
	{
		m_flDistTooFar = m_flUhViewDistance;
		GetSenses()->SetDistLook( m_flUhViewDistance );
	}
}

//-----------------------------------------------------------------------------
// Precache the severed-limb and helmet models for this NPC's body, so they are
// available when dismemberment / helmet-loss spawns them dynamically.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UH_PrecacheGibModels( void )
{
	const char *pszModel = STRING( GetModelName() );

	// Severed limbs (arms + legs) — lowercase paths, matching serveror.dll.
	if ( V_stristr( pszModel, "combine_soldier_prisonguard" ) )
	{
		PrecacheModel( "models/gibs/bodyparts/soldier_prisonguard/leftarm.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier_prisonguard/rightarm.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier_prisonguard/leftleg.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier_prisonguard/rightleg.mdl" );
	}
	else if ( V_stristr( pszModel, "combine_soldier" ) )
	{
		PrecacheModel( "models/gibs/bodyparts/soldier/leftarm.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier/rightarm.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier/leftleg.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier/rightleg.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier/leftleg2.mdl" );
		PrecacheModel( "models/gibs/bodyparts/soldier/rightleg2.mdl" );
	}

	// Helmet / respirator / gasmask drops (the original spawns item_* entities,
	// which precache their own models) + the knock-off sound.
	if ( V_stristr( pszModel, "prisonguard" ) )
	{
		PrecacheModel( "models/items/helmet.mdl" );
		PrecacheModel( "models/items/gasmask.mdl" );
	}
	else if ( V_stristr( pszModel, "combine_soldier" ) )
	{
		PrecacheModel( "models/items/helmet_visor.mdl" );
		PrecacheModel( "models/items/respirator.mdl" );
	}
	if ( V_stristr( pszModel, "pmc" ) )
	{
		PrecacheModel( "models/items/pmc_helmet.mdl" );
		PrecacheModel( "models/items/pmc_headset.mdl" );
		PrecacheModel( "models/items/pmc_cap.mdl" );
	}

	// Dismemberment blood sprays + sounds (1:1 with sub_10021D80 precache).
	PrecacheParticleSystem( "blood_zombie_split_spray" );
	PrecacheParticleSystem( "blood_advisor_puncture_withdraw" );
	PrecacheScriptSound( "Player.Helmet" );
	PrecacheScriptSound( "Player.Splat" );
	PrecacheScriptSound( "Player.HeadShot" );
}

//-----------------------------------------------------------------------------
// Spawn a severed limb as a ragdoll prop (matches the original sub_101CDCC0,
// which creates a "prop_ragdoll" with the per-bodypart gib model). A ragdoll
// limb flops naturally; a plain prop_physics with these models renders as an
// invisible/static body.
//-----------------------------------------------------------------------------
static CUtlVector<EHANDLE> s_UHServerGibs;

static CBaseEntity *UH_SpawnGibProp( const char *pszModel, const Vector &vecPosition, const QAngle &angPosition, const Vector &vecDir, CBaseEntity *pOwner )
{
	if ( uh_maxsergibs.GetInt() <= 0 ) return NULL;
	for ( int i = s_UHServerGibs.Count() - 1; i >= 0; --i )
		if ( !s_UHServerGibs[i].Get() ) s_UHServerGibs.FastRemove( i );
	while ( s_UHServerGibs.Count() >= max( 0, uh_maxsergibs.GetInt() ) && s_UHServerGibs.Count() )
	{
		UTIL_Remove( s_UHServerGibs[0].Get() );
		s_UHServerGibs.Remove( 0 );
	}
	CBaseAnimating *pAnimatingOwner = pOwner ? pOwner->GetBaseAnimating() : NULL;
	if ( !pAnimatingOwner || !pszModel || !pszModel[0] )
		return NULL;

	// sub_101CDCC0 creates and spawns an independent prop_ragdoll using the
	// supplied body-part model. It does not bone-merge with the source and does
	// not inject source velocity.
	CRagdollProp *pGib = static_cast<CRagdollProp *>(
		CBaseEntity::CreateNoSpawn( "prop_ragdoll", vecPosition, angPosition, pOwner ) );
	if ( !pGib )
		return NULL;
	pGib->SetModelName( AllocPooledString( pszModel ) );
	pGib->AddSpawnFlags( 0x0004 ); // SF_RAGDOLLPROP_DEBRIS
	DispatchSpawn( pGib );
	pGib->m_nSkin = pAnimatingOwner->m_nSkin;
	pGib->SetOwnerEntity( pOwner );
	s_UHServerGibs.AddToTail( pGib );
	(void)vecDir;
	return pGib;
}

// The original creates a sub-ragdoll at the authored sever attachment, not
// at the bullet impact point.  The impact may be anywhere along a forearm or
// calf and produced visibly detached/floating gibs in the old port.
static void UH_GetLimbSpawnTransform( CBaseAnimating *pBody, int iHitGroup, const Vector &vecFallback, Vector &vecOrigin, QAngle &angOrigin )
{
	const char *pszDistal = NULL;
	const char *pszProximal = NULL;
	switch ( iHitGroup )
	{
	case HITGROUP_LEFTARM: pszDistal = "ForeArm_L"; pszProximal = "UpperArm_L"; break;
	case HITGROUP_RIGHTARM: pszDistal = "ForeArm_R"; pszProximal = "UpperArm_R"; break;
	case HITGROUP_LEFTLEG: pszDistal = "Calf_L"; pszProximal = "Thigh_L"; break;
	case HITGROUP_RIGHTLEG: pszDistal = "Calf_R"; pszProximal = "Thigh_R"; break;
	}
	vecOrigin = vecFallback;
	angOrigin = vec3_angle;
	// sub_10031BF0/sub_101CE6F0 use the proximal sever point as origin but
	// retain the distal attachment's orientation for the detached model.
	if ( pszDistal && pszProximal )
	{
		Vector vecUnused; QAngle angUnused;
		pBody->GetAttachment( pszDistal, vecUnused, angOrigin );
		pBody->GetAttachment( pszProximal, vecOrigin, angUnused );
	}
}

// sub_10031BF0 copies the relevant glove variant to an arm sub-ragdoll.
// Without this, a gloved soldier leaves behind a bare detached hand because a
// new prop starts with model bodygroups at zero.
static void UH_CopyLimbBodygroups( CBaseAnimating *pSource, CBaseAnimating *pGib, int iHitGroup )
{
	int iType = UH_GIBTYPE_NONE;
	if ( CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC *>( pSource ) ) iType = pNPC->m_iUHGibType;
	else if ( CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp *>( pSource ) ) iType = pRagdoll->m_iUHGibType;
	if ( iType == UH_GIBTYPE_INFECTED_WORKER &&
		( iHitGroup == HITGROUP_LEFTARM || iHitGroup == HITGROUP_RIGHTARM ) )
		pGib->m_nSkin = pSource->m_nSkin > 2 ? 1 : 0;

	const char *pszGlove = NULL;
	if ( iHitGroup == HITGROUP_LEFTARM )
		pszGlove = "Glove_L";
	else if ( iHitGroup == HITGROUP_RIGHTARM )
		pszGlove = "Glove_R";
	if ( !pszGlove )
		return;

	int iSource = UH_FindBodygroup( pSource, pszGlove );
	int iGib = UH_FindBodygroup( pGib, pszGlove );
	if ( iSource >= 0 && iGib >= 0 )
		pGib->SetBodygroup( iGib, min( pSource->GetBodygroup( iSource ), pGib->GetBodygroupCount( iGib ) - 1 ) );
	if ( iType == UH_GIBTYPE_INFECTED_WORKER && iSource >= 0 )
		pSource->SetBodygroup( iSource, 0 );
}

//-----------------------------------------------------------------------------
// Blood sprays for a severed limb — 1:1 with sub_10031BF0:
//   arms: "blood_zombie_split_spray" on the body ("UpperArm_L/R") + on the
//         severed gib ("ForeArm_L/R")
//   legs: "blood_advisor_puncture_withdraw" on the severed gib ("Calf_L/R")
//   head: "Blood_Trace" decal (handled separately via the trace)
//-----------------------------------------------------------------------------
static void UH_DispatchAttachedBlood( const char *pszParticle, CBaseAnimating *pEntity, const char *pszAttachment, const Vector &vecFallback )
{
	if ( !pEntity || !pszAttachment ) return;
	const int iAttachment = pEntity->LookupAttachment( pszAttachment );
	if ( iAttachment > 0 )
	{
		DispatchParticleEffect( pszParticle, PATTACH_POINT_FOLLOW, pEntity, iAttachment );
		return;
	}

	// Replacement models occasionally omit Underhell's authored attachments.
	// Keep the effect visible at the sever point rather than silently dropping it.
	DispatchParticleEffect( pszParticle, vecFallback, pEntity->GetAbsAngles(), pEntity );
}

static void UH_DispatchLimbBlood( CBaseAnimating *pBody, int iHitGroup, CBaseAnimating *pGib, const Vector &vecPosition, const Vector &vecDir )
{
	const char *pszParticle = NULL;
	const char *pszBodyAttach = NULL;
	const char *pszGibAttach = NULL;

	switch ( iHitGroup )
	{
	case HITGROUP_LEFTARM:
		pszParticle = "blood_zombie_split_spray";
		pszBodyAttach = "UpperArm_L";
		pszGibAttach = "ForeArm_L";
		break;
	case HITGROUP_RIGHTARM:
		pszParticle = "blood_zombie_split_spray";
		pszBodyAttach = "UpperArm_R";
		pszGibAttach = "ForeArm_R";
		break;
	case HITGROUP_LEFTLEG:
		pszParticle = "blood_advisor_puncture_withdraw";
		pszGibAttach = "Calf_L";
		break;
	case HITGROUP_RIGHTLEG:
		pszParticle = "blood_advisor_puncture_withdraw";
		pszGibAttach = "Calf_R";
		break;
	case HITGROUP_HEAD:
		// Decal, not a particle: trace backwards from the impact and lay the
		// "Blood_Trace" (headshot "brainy" blood) decal.
		{
			trace_t tr;
			UTIL_TraceLine( vecPosition, vecPosition - vecDir * 16.0f, MASK_SHOT, pBody, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction < 1.0f )
				UTIL_DecalTrace( &tr, "Blood_Trace" );
		}
		return;
	}

	if ( !pszParticle )
		return;

	if ( pBody && pszBodyAttach )
		UH_DispatchAttachedBlood( pszParticle, pBody, pszBodyAttach, vecPosition );
	if ( pGib && pszGibAttach )
		UH_DispatchAttachedBlood( pszParticle, pGib, pszGibAttach, vecPosition );
	else if ( pszGibAttach )
		DispatchParticleEffect( pszParticle, vecPosition, vec3_angle );
}

// Select the exact item family used by the original CNPC_CombineS branches.
// PMC is tested before generic combine because its model may use a soldier
// skeleton/bodygroups but drops its own helmet item.
static const char *UH_HelmetItemFor( CBaseAnimating *pBody )
{
	const char *pszModel = STRING( pBody->GetModelName() );
	if ( V_stristr( pszModel, "pmc" ) )
		return "item_helmet_pmc";
	if ( V_stristr( pszModel, "prisonguard" ) )
		return "item_helmet_prison";
	if ( V_stristr( pszModel, "worker" ) )
		return "item_helmet_worker";
	return "item_helmet_guard";
}

static void UH_SetDroppedItemVariant( CBaseEntity *pItem, int iVariant )
{
	CBaseAnimating *pAnimating = pItem ? pItem->GetBaseAnimating() : NULL;
	if ( !pAnimating ) return;
	// sub_101CB6F0 ends in SetBodygroup( 1, variant ); this is not a skin.
	pAnimating->SetBodygroup( 1, iVariant );
}

//-----------------------------------------------------------------------------
// Shoot the helmet off: HELMET bodygroup -> 0 and drop the helmet model.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UH_ShootOffHelmet( const Vector &vecPosition, const Vector &vecDir )
{
	int iGroup = UH_FindBodygroup( this, "helmet" );
	if ( iGroup < 0 || GetBodygroup( iGroup ) < 1 )
		return;	// no helmet worn

	const int iHelmetVariant = GetBodygroup( iGroup );
	SetBodygroup( iGroup, 0 );

	// PMC values 4/5 are headset variants; value 5 additionally carries a cap.
	const char *pszItem = UH_HelmetItemFor( this );
	if ( m_iUHGibType == UH_GIBTYPE_PMC && iHelmetVariant > 3 )
		pszItem = "item_headset_pmc";

	CBaseEntity *pHelmet = CreateEntityByName( pszItem );
	if ( pHelmet )
	{
		// sub_10031BF0 drops head equipment from Eyes, rather than from the
		// arbitrary bullet impact on the hitbox.
		Vector vecDropOrigin = vecPosition;
		QAngle angDrop = vec3_angle;
		GetAttachment( "Eyes", vecDropOrigin, angDrop );
		pHelmet->SetAbsOrigin( vecDropOrigin );
		pHelmet->SetAbsAngles( angDrop );
		DispatchSpawn( pHelmet );
		UH_SetDroppedItemVariant( pHelmet, iHelmetVariant );

		IPhysicsObject *pPhys = pHelmet->VPhysicsGetObject();
		if ( pPhys )
		{
			Vector vecDropVelocity = vecDir * 0.25f;
			pPhys->SetVelocity( &vecDropVelocity, NULL );
		}
	}

	if ( m_iUHGibType == UH_GIBTYPE_PMC && iHelmetVariant == 5 )
	{
		CBaseEntity *pCap = CreateEntityByName( "item_cap_pmc" );
		if ( pCap )
		{
			Vector vecDropOrigin = vecPosition;
			QAngle angDrop = vec3_angle;
			GetAttachment( "Eyes", vecDropOrigin, angDrop );
			pCap->SetAbsOrigin( vecDropOrigin );
			pCap->SetAbsAngles( angDrop );
			DispatchSpawn( pCap );
			UH_SetDroppedItemVariant( pCap, 5 );
		}
	}

	EmitSound( "Player.Helmet" );
}

//-----------------------------------------------------------------------------
// Remove a worn gear bodygroup and drop the matching item (1:1 with
// sub_10031BF0, which spawns item_respirator_guard / item_gasmask_guard when
// a head is destroyed while a respirator / gasmask is worn).
//-----------------------------------------------------------------------------
static void UH_DropGearItem( CBaseAnimating *pNPC, const char *pszBodygroup, const char *pszItem, const Vector &vecPosition, const Vector &vecDir )
{
	int iGroup = UH_FindBodygroup( pNPC, pszBodygroup );
	if ( iGroup < 0 || pNPC->GetBodygroup( iGroup ) < 1 )
		return;	// not worn

	pNPC->SetBodygroup( iGroup, 0 );

	CBaseEntity *pItem = CreateEntityByName( pszItem );
	if ( !pItem )
		return;

	Vector vecDropOrigin = vecPosition;
	QAngle angDrop = vec3_angle;
	pNPC->GetAttachment( "Eyes", vecDropOrigin, angDrop );
	pItem->SetAbsOrigin( vecDropOrigin );
	pItem->SetAbsAngles( angDrop );
	DispatchSpawn( pItem );

	IPhysicsObject *pPhys = pItem->VPhysicsGetObject();
	if ( pPhys )
	{
		Vector vecDropVelocity = vecDir * 0.25f;
		pPhys->SetVelocity( &vecDropVelocity, NULL );
	}
}

//-----------------------------------------------------------------------------
// Gib a body part: change the bodygroup to remove the limb and spawn a severed
// gib model at the hit position.
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::UH_GibBodyPart( int iHitGroup, const Vector &vecPosition, const Vector &vecDir )
{
	int idx = -1;
	const char *pszLimb = NULL;
	switch ( iHitGroup )
	{
	case HITGROUP_HEAD: idx = UH_PART_HEAD; break;
	case HITGROUP_LEFTARM: idx = UH_PART_LEFTARM; pszLimb = "leftarm"; break;
	case HITGROUP_RIGHTARM: idx = UH_PART_RIGHTARM; pszLimb = "rightarm"; break;
	case HITGROUP_LEFTLEG: idx = UH_PART_LEFTLEG; pszLimb = "leftleg"; break;
	case HITGROUP_RIGHTLEG: idx = UH_PART_RIGHTLEG; pszLimb = "rightleg"; break;
	}
	if ( idx < 0 || ( m_nUHSeveredParts & ( 1u << idx ) ) )
		return false;

	bool bRemoved = false;
	switch ( iHitGroup )
	{
	case HITGROUP_LEFTARM: bRemoved = UH_RemoveBodygroupSide( this, "arms", 0 ); break;
	case HITGROUP_RIGHTARM: bRemoved = UH_RemoveBodygroupSide( this, "arms", 1 ); break;
	case HITGROUP_LEFTLEG: bRemoved = UH_RemoveBodygroupSide( this, "legs", 0 ); break;
	case HITGROUP_RIGHTLEG: bRemoved = UH_RemoveBodygroupSide( this, "legs", 1 ); break;
	case HITGROUP_HEAD:
		{
			int iGroup = UH_FindBodygroup( this, "head" );
			int iDestroyed = UH_DestroyedHeadBodygroup( this );
			if ( iGroup >= 0 && GetBodygroup( iGroup ) != iDestroyed && iDestroyed < GetBodygroupCount( iGroup ) )
			{
				SetBodygroup( iGroup, iDestroyed );
				bRemoved = true;
			}
			if ( m_iUHGibType == UH_GIBTYPE_INFECTED_GUARD )
				UH_DropGearItem( this, "respirator", "item_respirator_guard", vecPosition, vecDir );
			if ( m_iUHGibType == UH_GIBTYPE_COMBINE && V_stristr( STRING( GetModelName() ), "prisonguard" ) )
				UH_DropGearItem( this, "gasmask", "item_gasmask_prison", vecPosition, vecDir );
		}
		break;
	}
	if ( !bRemoved )
		return false;

	m_nUHSeveredParts |= 1u << idx;
	CBaseEntity *pGib = NULL;
	if ( pszLimb )
	{
		const char *pszModel = UH_GibModelFor( this, pszLimb );
		if ( pszModel )
		{
			Vector vecGibOrigin; QAngle angGibOrigin;
			UH_GetLimbSpawnTransform( this, iHitGroup, vecPosition, vecGibOrigin, angGibOrigin );
			pGib = UH_SpawnGibProp( pszModel, vecGibOrigin, angGibOrigin, vecDir, this );
			if ( pGib ) UH_CopyLimbBodygroups( this, pGib->GetBaseAnimating(), iHitGroup );
		}
	}

	RemoveAllDecals();
	if ( iHitGroup != HITGROUP_HEAD ) EmitSound( "Player.Splat" );
	UH_DispatchLimbBlood( this, iHitGroup, pGib ? pGib->GetBaseAnimating() : NULL, vecPosition, vecDir );
	if ( iHitGroup == HITGROUP_HEAD )
	{
		EmitSound( "Player.HeadShot" );
		UTIL_BloodSpray( vecPosition, vecDir, BLOOD_COLOR_RED, 8, FX_BLOODSPRAY_ALL );
	}

	// Original arm-loss branches invalidate weapon use. The right arm is the
	// firing arm and drops the active weapon; either arm disables move-and-shoot.
	if ( iHitGroup == HITGROUP_LEFTLEG || iHitGroup == HITGROUP_RIGHTLEG )
	{
		// Both leg cases in sub_10031BF0 set the living NPC's health to zero.
		SetHealth( 0 );
	}
	else if ( iHitGroup == HITGROUP_LEFTARM || iHitGroup == HITGROUP_RIGHTARM )
	{
		CapabilitiesRemove( bits_CAP_MOVE_SHOOT );
		if ( iHitGroup == HITGROUP_RIGHTARM )
		{
			CapabilitiesRemove( bits_CAP_RANGE_ATTACK_GROUP | bits_CAP_AIM_GUN );
			if ( GetActiveWeapon() ) Weapon_Drop( GetActiveWeapon() );
			ClearSchedule( "Lost right arm" );
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Accumulate damage per hitgroup and gib a limb once its threshold is crossed.
// A worn helmet absorbs head damage and is shot off (uh_helmethealth) before
// the head itself can be destroyed (uh_headhealth).
//-----------------------------------------------------------------------------
bool CAI_BaseNPC::UH_ConsiderGib( int iHitGroup, float flDamage, const Vector &vecPosition, const Vector &vecDir )
{
	if ( !m_bUHGibable || flDamage <= 0.0f || !IsAlive() )
		return false;

	int idx = -1;
	switch ( iHitGroup )
	{
	case HITGROUP_HEAD: idx = UH_PART_HEAD; break;
	case HITGROUP_LEFTARM: idx = UH_PART_LEFTARM; break;
	case HITGROUP_RIGHTARM: idx = UH_PART_RIGHTARM; break;
	case HITGROUP_LEFTLEG: idx = UH_PART_LEFTLEG; break;
	case HITGROUP_RIGHTLEG: idx = UH_PART_RIGHTLEG; break;
	default: return false;
	}
	if ( m_nUHSeveredParts & ( 1u << idx ) )
		return false;

	if ( iHitGroup == HITGROUP_HEAD )
	{
		int iHelmet = UH_FindBodygroup( this, "helmet" );
		if ( iHelmet >= 0 && GetBodygroup( iHelmet ) > 0 &&
			!( m_iUHGibType == UH_GIBTYPE_PMC && GetBodygroup( iHelmet ) > 3 ) )
		{
			m_iUHHelmetHealth = (int)( (float)m_iUHHelmetHealth - flDamage );
			if ( m_iUHHelmetHealth > 0 )
				return true;
			UH_ShootOffHelmet( vecPosition, vecDir );
			// sub_10031BF0 recursively processes case 8 with the same damage info:
			// the shot which breaks the helmet also damages the head.
		}
	}

	m_iUHPartHealth[idx] = (int)( (float)m_iUHPartHealth[idx] - flDamage );
	if ( m_iUHPartHealth[idx] > 0 )
		return false;
	return UH_GibBodyPart( iHitGroup, vecPosition, vecDir );
}

//-----------------------------------------------------------------------------
// Spot dead bodies: when enabled, scan for ragdoll/corpse entities in front of
// the NPC and fire the matching output once each.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UH_SpotBodiesThink( void )
{
	if ( !m_bUhSpotBodies )
		return;

	if ( gpGlobals->curtime < m_flNextSpotBodiesTime )
		return;
	m_flNextSpotBodiesTime = gpGlobals->curtime + 0.5f;

	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );

	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "prop_ragdoll" ) ) != NULL )
	{
		// Roughly in front + within view distance.
		Vector toBody = pEnt->GetAbsOrigin() - EyePosition();
		float dist = VectorNormalize( toBody );
		if ( dist > m_flUhViewDistance && m_flUhViewDistance > 0 )
			continue;
		if ( DotProduct( toBody, forward ) < 0.3f )
			continue;

		// TODO: track which bodies we've already reported (a set of EHANDLEs).
		// For now, fire on each visible body (throttled by the 0.5s scan).

		// Classify: soldier / infected / default. The body's classname isn't
		// preserved on the ragdoll, so use the model name as a proxy.
		// FireOutput( pActivator, pCaller ): !activator = this NPC (who spotted),
		// !caller = the body — matching the tutorial ("!caller is the body").
		const char *pszModel = STRING( pEnt->GetModelName() );
		if ( V_stristr( pszModel, "combine_soldier" ) )
			m_OnSpotSoldierBody.FireOutput( this, pEnt );
		else if ( V_stristr( pszModel, "infected" ) )
			m_OnSpotInfectedBody.FireOutput( this, pEnt );
		else
			m_OnSpotDefaultBody.FireOutput( this, pEnt );

		break;	// one body per scan
	}
}

//-----------------------------------------------------------------------------
// Temporary squads: allied NPCs that see each other (and both have squadtemp
// enabled) join a temporary squad so they share target info. When LOS is lost
// the temp squad is broken. Simplified: if we have an enemy and another allied
// NPC (not in our squad) is nearby + looking at us, share our enemy with it.
//-----------------------------------------------------------------------------
void CAI_BaseNPC::UH_TempSquadUpdate( void )
{
	if ( !m_bUhSquadTemp )
		return;

	if ( !GetEnemy() )
		return;

	if ( gpGlobals->curtime < m_flNextTempSquadTime )
		return;
	m_flNextTempSquadTime = gpGlobals->curtime + 0.5f;

	// Find a nearby allied NPC (any class, per the tutorial) that can see us.
	CBaseEntity *pEnt = gEntList.FirstEnt();
	while ( pEnt )
	{
		if ( pEnt == this )
		{
			pEnt = gEntList.NextEnt( pEnt );
			continue;
		}

		CAI_BaseNPC *pOther = pEnt->MyNPCPointer();
		if ( pOther && pOther != GetEnemy() && pOther->IRelationType( this ) == D_LI
			&& pOther->m_bUhSquadTemp && pOther->IsAlive() )
		{
			if ( ( pOther->GetAbsOrigin() - GetAbsOrigin() ).Length2D() <= 256.0f
				&& FInViewCone( pOther ) )
			{
				// Share our enemy (the original shares target info while in LOS).
				pOther->UpdateEnemyMemory( GetEnemy(), GetEnemy()->GetAbsOrigin(), this );
				pOther->SetEnemy( GetEnemy() );
				return;
			}
		}

		pEnt = gEntList.NextEnt( pEnt );
	}
}

//-----------------------------------------------------------------------------
// Inputs (FGD BaseNPC).
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InputSetSquadTemp( inputdata_t &inputdata )
{
	m_bUhSquadTemp = ( atoi( inputdata.value.String() ) != 0 );
}

void CAI_BaseNPC::InputSetFos( inputdata_t &inputdata )
{
	// Original sub_100228B0: FOS in degrees -> m_flFieldOfView = cos(fos/2).
	m_flUhFOV = atof( inputdata.value.String() );
	if ( m_flUhFOV > 0.0f )
	{
		m_flFieldOfView = cos( DEG2RAD( m_flUhFOV * 0.5f ) );
	}
}

void CAI_BaseNPC::InputSetViewDistance( inputdata_t &inputdata )
{
	m_flUhViewDistance = atof( inputdata.value.String() );
	m_flDistTooFar = m_flUhViewDistance;
	GetSenses()->SetDistLook( m_flUhViewDistance );
}

void CAI_BaseNPC::InputSetSpotBodiesOn( inputdata_t &inputdata )
{
	m_bUhSpotBodies = true;
}

void CAI_BaseNPC::InputSetSpotBodiesOff( inputdata_t &inputdata )
{
	m_bUhSpotBodies = false;
}

static void UH_ForceGibInput( CAI_BaseNPC *pNPC, int iHitGroup, const Vector &vecPosition )
{
	if ( !pNPC->m_bUHGibable )
	{
		Warning( "Gib input called on non-gibable NPC, ignoring the input\n" );
		return;
	}
	int idx = iHitGroup == HITGROUP_HEAD ? UH_PART_HEAD :
		iHitGroup == HITGROUP_LEFTARM ? UH_PART_LEFTARM :
		iHitGroup == HITGROUP_RIGHTARM ? UH_PART_RIGHTARM :
		iHitGroup == HITGROUP_LEFTLEG ? UH_PART_LEFTLEG : UH_PART_RIGHTLEG;
	float flDamage = (float)( pNPC->m_iUHPartHealth[idx] + 1 );
	if ( iHitGroup == HITGROUP_HEAD ) flDamage += pNPC->m_iUHHelmetHealth + 1;
	pNPC->UH_ConsiderGib( iHitGroup, flDamage, vecPosition, vec3_origin );
}

void CAI_BaseNPC::InputGibHead( inputdata_t &inputdata )
{
	UH_ForceGibInput( this, HITGROUP_HEAD, EyePosition() );
}

void CAI_BaseNPC::InputGibLeftArm( inputdata_t &inputdata )
{
	UH_ForceGibInput( this, HITGROUP_LEFTARM, GetAbsOrigin() );
}

void CAI_BaseNPC::InputGibRightArm( inputdata_t &inputdata )
{
	UH_ForceGibInput( this, HITGROUP_RIGHTARM, GetAbsOrigin() );
}

void CAI_BaseNPC::InputGibLeftLeg( inputdata_t &inputdata )
{
	UH_ForceGibInput( this, HITGROUP_LEFTLEG, GetAbsOrigin() );
}

void CAI_BaseNPC::InputGibRightLeg( inputdata_t &inputdata )
{
	UH_ForceGibInput( this, HITGROUP_RIGHTLEG, GetAbsOrigin() );
}

// Copy the exact constructor-time counters and authored model paths into the
// server corpse, matching sub_10401A20/sub_10402000/sub_100C4360.
void UH_TransferDismembermentState( CBaseAnimating *pAnimating, CRagdollProp *pRagdoll )
{
	CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC *>( pAnimating );
	if ( !pNPC || !pRagdoll ) return;
	pRagdoll->m_bUHGibable = pNPC->m_bUHGibable;
	pRagdoll->m_iUHGibType = pNPC->m_iUHGibType;
	pRagdoll->m_iUHHelmetHealth = pNPC->m_iUHHelmetHealth;
	pRagdoll->m_nUHSeveredParts = pNPC->m_nUHSeveredParts;
	for ( int i = 0; i < 5; ++i ) pRagdoll->m_iUHPartHealth[i] = pNPC->m_iUHPartHealth[i];
	for ( int i = 0; i < 4; ++i ) pRagdoll->m_iszUHGibModel[i] = pNPC->m_iszUHGibModel[i];
	const int iCollision = uh_ragdollcollisiontype.GetInt();
	if ( iCollision >= 0 && iCollision <= 19 ) pRagdoll->SetCollisionGroup( iCollision );
}

//-----------------------------------------------------------------------------
// Ragdoll / gib management (decoded from the original's CRagdollProp additions
// + the ragdoll manager list at 106960D0/D8/DC/E4/E8/F0). Three behaviours:
//
//   1. uh_ragdollcollisiontype — the collision group applied to NPC ragdolls
//      (-1 = client-side, else a 0..19 collision group index).
//   2. uh_maxseragdolls / uh_maxsergibs — cap server-side ragdolls/gibs; the
//      oldest is retired (the SDK LRU fades it out; the original converts it to
//      a client ragdoll, which is equivalent visually).
//   3. Dragging a body sprays "blood_drop" decals while the player holds it
//      (FVPHYSICS_PLAYER_HELD), like the original's DraggedThink.
//-----------------------------------------------------------------------------
class CUHRagdollManager : public CAutoGameSystemPerFrame
{
	typedef CAutoGameSystemPerFrame BaseClass;

public:
	CUHRagdollManager() : BaseClass( "CUHRagdollManager" ), m_flNextScan( 0.0f ) {}

	virtual void LevelInitPostEntity( void )
	{
		// Feed the SDK's existing ragdoll LRU our max count.
		s_RagdollLRU.SetMaxRagdollCount( uh_maxseragdolls.GetInt() );
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		if ( gpGlobals->curtime < m_flNextScan )
			return;
		m_flNextScan = gpGlobals->curtime + 0.25f;

		// Keep the LRU cap in sync (convar may change at runtime).
		s_RagdollLRU.SetMaxRagdollCount( uh_maxseragdolls.GetInt() );

		CBaseEntity *pEnt = gEntList.FirstEnt();
		while ( pEnt )
		{
			if ( FClassnameIs( pEnt, "prop_ragdoll" ) )
			{
				CRagdollProp *pRagdoll = assert_cast<CRagdollProp *>( pEnt );

				// Only NPC-spawned ragdolls (owner = the dead NPC). Hammer-placed
				// prop_ragdolls have no owner and are excluded, per the tutorial.
				if ( pRagdoll->GetOwnerEntity() )
				{
					// sub_101CCD80 (DraggedThink): a held corpse leaves a blood
					// decal only after it has moved more than four units in XY, then
					// updates its anchor every 0.25 s. The former implementation
					// sprayed a decal continuously even while the body was stationary.
					IPhysicsObject *pPhys = pRagdoll->VPhysicsGetObject();
					bool bHeld = pPhys && ( pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD );
					Vector vecPos = pRagdoll->GetAbsOrigin();
					if ( bHeld )
					{
						if ( pRagdoll->m_bUHDragged &&
							( fabs( vecPos.x - pRagdoll->m_vecUHDraggedLastPos.x ) > 4.0f ||
							  fabs( vecPos.y - pRagdoll->m_vecUHDraggedLastPos.y ) > 4.0f ) )
						{
							Vector vecDown = vecPos - Vector( 0, 0, 32.0f );
							trace_t tr;
							UTIL_TraceLine( vecPos, vecDown, MASK_SOLID_BRUSHONLY, pRagdoll, COLLISION_GROUP_NONE, &tr );
							if ( tr.fraction < 1.0f )
								UTIL_DecalTrace( &tr, "blood_drop" );
						}
						pRagdoll->m_bUHDragged = true;
						pRagdoll->m_vecUHDraggedLastPos = vecPos;
					}
					else
					{
						pRagdoll->m_bUHDragged = false;
					}
				}
			}

			pEnt = gEntList.NextEnt( pEnt );
		}
	}

private:
	float m_flNextScan;
};

static CUHRagdollManager g_UHRagdollManager;

//-----------------------------------------------------------------------------
// Ragdoll dismemberment: shoot limbs off a dead body. Called from
// CRagdollProp::TraceAttack. Accumulates per-hitgroup damage and, once a limb's
// remaining-health counter reaches zero, hides the authored bodygroup and
// creates an independent body-part ragdoll without breaking source constraints.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Map a ragdoll physics bone -> hitgroup. Ragdolls are SOLID_VPHYSICS, so a
// bullet trace against them returns physicsbone but leaves hitgroup = generic
// (CRagdollProp::TestCollision only sets tr.physicsbone). Recover the hitgroup
// from the physics bone's studio bone via the model's hitbox set.
//-----------------------------------------------------------------------------
// Resolve a studio bone to its hitgroup. A ragdoll physics element often uses
// a parent of the hitbox bone, so callers also walk its physics parents.
static int UH_StudioBoneToHitgroup( CStudioHdr *pHdr, int iStudioBone )
{
	if ( !pHdr || iStudioBone < 0 )
		return HITGROUP_GENERIC;

	for ( int iSet = 0; iSet < pHdr->numhitboxsets(); iSet++ )
	{
		mstudiohitboxset_t *pSet = pHdr->pHitboxSet( iSet );
		for ( int i = 0; i < pSet->numhitboxes; i++ )
		{
			mstudiobbox_t *pBox = pSet->pHitbox( i );
			if ( pBox->bone == iStudioBone )
				return pBox->group;
		}
	}
	return HITGROUP_GENERIC;
}

static int UH_RagdollBoneToHitgroup( CRagdollProp *pRagdoll, int iPhysicsBone )
{
	ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll();
	CStudioHdr *pHdr = pRagdoll->GetModelPtr();
	if ( iPhysicsBone < 0 || iPhysicsBone >= pRagdollPhys->listCount || !pHdr )
		return HITGROUP_GENERIC;

	// Check the struck element then its parents. This covers a physics bone
	// such as a forearm whose hitbox is authored on the upper-arm bone.
	for ( int iElement = iPhysicsBone; iElement >= 0; iElement = pRagdollPhys->list[iElement].parentIndex )
	{
		int iHitgroup = UH_StudioBoneToHitgroup( pHdr, pRagdollPhys->boneIndex[iElement] );
		if ( iHitgroup != HITGROUP_GENERIC )
			return iHitgroup;
	}
	return HITGROUP_GENERIC;
}

void UH_RagdollDismember( CRagdollProp *pRagdoll, int iHitGroup, float flDamage, int iPhysicsBone, const Vector &pos, const Vector &dir )
{
	if ( !pRagdoll || !pRagdoll->m_bUHGibable || flDamage <= 0.0f )
		return;

	// sub_101CE6F0 uses authored physics-element numbers. Prison/PMC ragdolls
	// first remap their expanded physics layout, then use the common table.
	int iPartBone = iPhysicsBone;
	if ( pRagdoll->m_iUHGibType == UH_GIBTYPE_COMBINE || pRagdoll->m_iUHGibType == UH_GIBTYPE_PMC )
	{
		switch ( iPhysicsBone )
		{
		case 2: case 6: iPartBone = 9; break;
		case 3: case 4: iPartBone = 7; break;
		case 8: case 9: iPartBone = 3; break;
		case 10: iPartBone = 12; break;
		case 11: case 12: iPartBone = 1; break;
		}
	}
	switch ( iPartBone )
	{
	case 1: case 2: iHitGroup = HITGROUP_LEFTLEG; break;
	case 3: case 4: iHitGroup = HITGROUP_RIGHTLEG; break;
	case 7: case 8: iHitGroup = HITGROUP_LEFTARM; break;
	case 9: case 10: iHitGroup = HITGROUP_RIGHTARM; break;
	case 12: iHitGroup = HITGROUP_HEAD; break;
	default:
		if ( iHitGroup == HITGROUP_GENERIC ) iHitGroup = UH_RagdollBoneToHitgroup( pRagdoll, iPhysicsBone );
		break;
	}

	int idx = -1; const char *pszLimb = NULL;
	switch ( iHitGroup )
	{
	case HITGROUP_HEAD: idx = UH_PART_HEAD; break;
	case HITGROUP_LEFTARM: idx = UH_PART_LEFTARM; pszLimb = "leftarm"; break;
	case HITGROUP_RIGHTARM: idx = UH_PART_RIGHTARM; pszLimb = "rightarm"; break;
	case HITGROUP_LEFTLEG: idx = UH_PART_LEFTLEG; pszLimb = "leftleg"; break;
	case HITGROUP_RIGHTLEG: idx = UH_PART_RIGHTLEG; pszLimb = "rightleg"; break;
	default: return;
	}
	if ( pRagdoll->m_nUHSeveredParts & ( 1u << idx ) )
		return;

	// Corpse case 12 removes headgear immediately, then applies the same shot to
	// the head counter. Helmet health is only a living-NPC state in the original.
	if ( iHitGroup == HITGROUP_HEAD )
	{
		int iHelmet = UH_FindBodygroup( pRagdoll, "helmet" );
		if ( iHelmet >= 0 && pRagdoll->GetBodygroup( iHelmet ) > 0 &&
			( pRagdoll->m_iUHGibType == UH_GIBTYPE_INFECTED_GUARD ||
			  pRagdoll->m_iUHGibType == UH_GIBTYPE_COMBINE ||
			  pRagdoll->m_iUHGibType == UH_GIBTYPE_PMC ) )
		{
			const int iVariant = pRagdoll->GetBodygroup( iHelmet );
			pRagdoll->SetBodygroup( iHelmet, 0 );
			pRagdoll->RemoveAllDecals();
			const char *pszItem = UH_HelmetItemFor( pRagdoll );
			if ( pRagdoll->m_iUHGibType == UH_GIBTYPE_PMC && iVariant > 3 ) pszItem = "item_headset_pmc";
			CBaseEntity *pItem = CreateEntityByName( pszItem );
			if ( pItem )
			{
				Vector org = pos; QAngle ang = vec3_angle;
				pRagdoll->GetAttachment( "Eyes", org, ang );
				pItem->SetAbsOrigin( org ); pItem->SetAbsAngles( ang ); DispatchSpawn( pItem );
				UH_SetDroppedItemVariant( pItem, iVariant );
			}
			if ( pRagdoll->m_iUHGibType == UH_GIBTYPE_PMC && iVariant == 5 )
			{
				CBaseEntity *pCap = CreateEntityByName( "item_cap_pmc" );
				if ( pCap )
				{
					Vector org = pos; QAngle ang = vec3_angle;
					pRagdoll->GetAttachment( "Eyes", org, ang );
					pCap->SetAbsOrigin( org ); pCap->SetAbsAngles( ang ); DispatchSpawn( pCap );
					UH_SetDroppedItemVariant( pCap, 5 );
				}
			}
		}
	}

	pRagdoll->m_iUHPartHealth[idx] = (int)( (float)pRagdoll->m_iUHPartHealth[idx] - flDamage );
	if ( pRagdoll->m_iUHPartHealth[idx] > 0 )
		return;

	bool bRemoved = false;
	switch ( iHitGroup )
	{
	case HITGROUP_LEFTARM: bRemoved = UH_RemoveBodygroupSide( pRagdoll, "arms", 0 ); break;
	case HITGROUP_RIGHTARM: bRemoved = UH_RemoveBodygroupSide( pRagdoll, "arms", 1 ); break;
	case HITGROUP_LEFTLEG: bRemoved = UH_RemoveBodygroupSide( pRagdoll, "legs", 0 ); break;
	case HITGROUP_RIGHTLEG: bRemoved = UH_RemoveBodygroupSide( pRagdoll, "legs", 1 ); break;
	case HITGROUP_HEAD:
		{
			int iGroup = UH_FindBodygroup( pRagdoll, "head" );
			int iDestroyed = UH_DestroyedHeadBodygroup( pRagdoll );
			if ( iGroup >= 0 && iDestroyed < pRagdoll->GetBodygroupCount( iGroup ) && pRagdoll->GetBodygroup( iGroup ) != iDestroyed )
			{ pRagdoll->SetBodygroup( iGroup, iDestroyed ); bRemoved = true; }
			if ( pRagdoll->m_iUHGibType == UH_GIBTYPE_INFECTED_GUARD )
				UH_DropGearItem( pRagdoll, "respirator", "item_respirator_guard", pos, dir );
			if ( pRagdoll->m_iUHGibType == UH_GIBTYPE_COMBINE && V_stristr( STRING( pRagdoll->GetModelName() ), "prisonguard" ) )
				UH_DropGearItem( pRagdoll, "gasmask", "item_gasmask_prison", pos, dir );
		}
		break;
	}
	if ( !bRemoved ) return;
	pRagdoll->m_nUHSeveredParts |= 1u << idx;

	CBaseEntity *pGib = NULL;
	if ( pszLimb )
	{
		const char *pszModel = UH_GibModelFor( pRagdoll, pszLimb );
		if ( pszModel )
		{
			Vector org; QAngle ang;
			UH_GetLimbSpawnTransform( pRagdoll, iHitGroup, pos, org, ang );
			pGib = UH_SpawnGibProp( pszModel, org, ang, dir, pRagdoll );
			if ( pGib ) UH_CopyLimbBodygroups( pRagdoll, pGib->GetBaseAnimating(), iHitGroup );
		}
	}

	pRagdoll->RemoveAllDecals();
	if ( iHitGroup != HITGROUP_HEAD ) pRagdoll->EmitSound( "Player.Splat" );
	UH_DispatchLimbBlood( pRagdoll, iHitGroup, pGib ? pGib->GetBaseAnimating() : NULL, pos, dir );
	if ( iHitGroup == HITGROUP_LEFTLEG )
		UH_DispatchAttachedBlood( "blood_advisor_puncture_withdraw", pRagdoll, "Thigh_L", pos );
	else if ( iHitGroup == HITGROUP_RIGHTLEG )
		UH_DispatchAttachedBlood( "blood_advisor_puncture_withdraw", pRagdoll, "Thigh_R", pos );
	else if ( iHitGroup == HITGROUP_HEAD )
	{
		pRagdoll->EmitSound( "Player.HeadShot" );
		UH_DispatchAttachedBlood( "blood_zombie_split_spray", pRagdoll, "Neck", pos );
	}
}

//-----------------------------------------------------------------------------
// Underhell BaseNPC map movement inputs (FGD: RushEntity / WalkToEntity).
//-----------------------------------------------------------------------------
static CBaseEntity *UH_FindNPCMoveTarget( CAI_BaseNPC *pNPC, inputdata_t &inputdata )
{
	const char *name = inputdata.value.String();
	if ( !name || !*name )
		return NULL;
	return gEntList.FindEntityByName( NULL, name, pNPC, inputdata.pActivator, inputdata.pCaller );
}

void CAI_BaseNPC::InputRushEntity( inputdata_t &inputdata )
{
	CBaseEntity *target = UH_FindNPCMoveTarget( this, inputdata );
	if ( !target )
		return;
	SetTarget( target );
	SetGoalEnt( target );
	SetSchedule( SCHED_FORCED_GO_RUN );
}

void CAI_BaseNPC::InputWalkToEntity( inputdata_t &inputdata )
{
	CBaseEntity *target = UH_FindNPCMoveTarget( this, inputdata );
	if ( !target )
		return;
	SetTarget( target );
	SetGoalEnt( target );
	SetSchedule( SCHED_FORCED_GO );
}
