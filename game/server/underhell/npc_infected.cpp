//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell "Infected" NPC — audited reconstruction from serveror.dll
//          decompiled hexrays (sub_101A6F00, sub_101A7410, sub_101A7B70, sub_101A6620,
//          sub_101A6FF0, sub_101A5A10, etc) + FGD underhell.fgd.
//
// Original: CNPC_UH_Infected is a CAI_BlendingHost<CNPC_BaseZombie> with the
//           CDefaultPlayerPickupVPhysics secondary interface. Its custom layer
//           adds:
//             * 8 variants (inmate/guard/worker/rural/doctor/uniform/office/urban)
//             * disable keyvalues (inmate..urban) + SpeedModifier + additionalequipment
//             * authored arm-loss bodygroups and capability restrictions
//             * custom conditions/tasks/schedules (sprint slots, climb touch,
//               unstick jump, radio investigate, door bash)
//             * Zombine-style timed sprint and fast-melee translation
//             * OnSpotInfectedBody output (via uh_ai.cpp spot logic)
//             * gib models per variant
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "ai_memory.h"
#include "ai_blended_movement.h"
#include "ai_route.h"
#include "npc_basezombie.h"
#include "npcevent.h"
#include "activitylist.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "soundent.h"
#include "props.h"
#include "physics_npc_solver.h"
#include "basepropdoor.h"
#include "doors.h"
#include "underhell/uh_items.h"
#include "physics_prop_ragdoll.h"
#include "basecombatcharacter.h"
#include "basecombatweapon.h"
#include "hl2_player.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ---------------------------------------------------------------------------
// ConVars from original binaries (strings extracted from serveror.dll)
// ---------------------------------------------------------------------------
// Exact constructor defaults/flags from the server Diaphora initializers.
static ConVar uh_infected_health( "uh_infected_health", "50", 0, "Infected health" );
static ConVar uh_infected_door_dist( "uh_infected_door_dist", "37", 0, "Door bash distance" );
static ConVar uh_infected_efficiency( "uh_infected_efficiency", "1", 0, "Efficiency" );
static ConVar uh_infected_randspeedenabled( "uh_infected_randspeedenabled", "1", 0, "Random speed enabled" );
static ConVar uh_infectedcower( "uh_infectedcower", "0", 0, "Cower?" );

// ---------------------------------------------------------------------------
// Variant table (models + gib folder). Order matches FGD.
// ---------------------------------------------------------------------------
struct UHInfectedVariant_t
{
	const char *pszKey;
	const char *pszModel;
	const char *pszGibLeftArm;
	const char *pszGibRightArm;
	const char *pszGibLeftLeg;
	const char *pszGibRightLeg;
	const char *pszHelmet; // optional helmet model that can drop
};

// IDs are serveror.dll IDs, not FGD display order (sub_101A6620).
static const UHInfectedVariant_t s_InfectedVariants[] =
{
	{ "inmate",  "models/infected/infected_inmate.mdl",  "models/gibs/bodyparts/infected/inmate_leftarm.mdl",  "models/gibs/bodyparts/infected/inmate_rightarm.mdl",  "models/gibs/bodyparts/infected/inmate_leftleg.mdl",  "models/gibs/bodyparts/infected/inmate_rightleg.mdl",  NULL },
	{ "worker",  "models/infected/infected_worker.mdl",  "models/gibs/bodyparts/infected/worker_leftarm.mdl",  "models/gibs/bodyparts/infected/worker_rightarm.mdl",  "models/gibs/bodyparts/infected/worker_leftleg.mdl",  "models/gibs/bodyparts/infected/worker_rightleg.mdl",  "models/items/worker_helmet.mdl" },
	{ "doctor",  "models/infected/infected_doctor.mdl",  "models/gibs/bodyparts/infected/doctor_leftarm.mdl",  "models/gibs/bodyparts/infected/doctor_rightarm.mdl",  "models/gibs/bodyparts/infected/doctor_leftleg.mdl",  "models/gibs/bodyparts/infected/doctor_rightleg.mdl",  NULL },
	{ "uniform", "models/infected/infected_uniform.mdl", "models/gibs/bodyparts/infected/uniform_leftarm.mdl", "models/gibs/bodyparts/infected/uniform_rightarm.mdl", "models/gibs/bodyparts/infected/uniform_leftleg.mdl", "models/gibs/bodyparts/infected/uniform_rightleg.mdl", NULL },
	{ "urban",   "models/infected/infected_urban.mdl",   "models/gibs/bodyparts/infected/urban_leftarm.mdl",   "models/gibs/bodyparts/infected/urban_rightarm.mdl",   "models/gibs/bodyparts/infected/urban_leftleg.mdl",   "models/gibs/bodyparts/infected/urban_rightleg.mdl",   NULL },
	{ "rural",   "models/infected/infected_rural.mdl",   "models/gibs/bodyparts/infected/rural_leftarm.mdl",   "models/gibs/bodyparts/infected/rural_rightarm.mdl",   "models/gibs/bodyparts/infected/rural_leftleg.mdl",   "models/gibs/bodyparts/infected/rural_rightleg.mdl",   NULL },
	{ "guard",   "models/infected/infected_guard.mdl",   "models/gibs/bodyparts/infected/guard_leftarm.mdl",   "models/gibs/bodyparts/infected/guard_rightarm.mdl",   "models/gibs/bodyparts/infected/guard_leftleg.mdl",   "models/gibs/bodyparts/infected/guard_rightleg.mdl",   "models/items/guard_helmet.mdl" },
	{ "office",  "models/infected/infected_office.mdl",  "models/gibs/bodyparts/infected/office_leftarm.mdl",  "models/gibs/bodyparts/infected/office_rightarm.mdl",  "models/gibs/bodyparts/infected/office_leftleg.mdl",  "models/gibs/bodyparts/infected/office_rightleg.mdl",  NULL },
};

#define UH_INFECTED_VARIANT_COUNT ARRAYSIZE(s_InfectedVariants)

// ---------------------------------------------------------------------------
// Custom AI Ids from decompiled sub_101A7410
// ---------------------------------------------------------------------------
enum
{
	TASK_UH_INFECTED_UNSTICK_JUMP = 250,
	TASK_ZOMBIE_YAW_TO_DOOR = 251,
	TASK_ZOMBIE_ATTACK_DOOR = 252,
	// original typo: TASK_ZOMBIE_BREAKTHROUG = 253 (without H)
	TASK_ZOMBIE_BREAKTHROUG = 253,
	TASK_UH_DESTROY_RADIO = 254,
	TASK_UH_RADIO_PICKUP = 255,
	TASK_UH_THROW_ITEM = 256,
};

enum
{
	SCHED_UH_INFECTED_UNSTICK_JUMP = 101,
	SCHED_UH_INFECTED_CLIMBING_UNSTICK_JUMP = 102,
	SCHED_ZOMBIE_BASH_DOOR = 103,
	SCHED_UH_INFECTED_INVESTIGATE_RADIO = 104,
};

enum
{
	COND_UH_INFECTED_GRENADE = 76,
	COND_UH_INFECTED_RANDOMRUN = 77,
	COND_UH_INFECTED_CLIMB_TOUCH = 78,
	COND_DOOR_OPENED = 79,
	COND_BLOCKED_BY_DOOR = 80,
};

enum
{
	SQUAD_SLOT_UH_INFECTED_SPRINT1 = 8,
	SQUAD_SLOT_UH_INFECTED_SPRINT2 = 9,
};

// Activities (from decompiled)
static int ACT_UH_INFECTED_ATTACK_FAST = -1;
static int ACT_RUN_PANICKED = -1;
static int ACT_ZOMBIE_WALLPOUND = -1;
static int ACT_ZOMBIE_BREAKTHROUGH = -1;

// ---------------------------------------------------------------------------
// CNPC_UH_Infected
// ---------------------------------------------------------------------------
class CNPC_UH_Infected : public CAI_BlendingHost<CNPC_BaseZombie>, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS( CNPC_UH_Infected, CAI_BlendingHost<CNPC_BaseZombie> );
	DECLARE_DATADESC();

public:
	CNPC_UH_Infected();

	void Spawn( void );
	void Precache( void );
	void SetZombieModel( void );

	Class_T Classify( void ) { return CLASS_ZOMBIE; }

	// BaseZombie overrides
	const char *GetHeadcrabClassname( void ) { return "npc_headcrab"; }
	const char *GetHeadcrabModel( void ) { return "models/headcrabclassic.mdl"; }
	const char *GetLegsModel( void ) { return "models/zombie/zombie_soldier_legs.mdl"; }
	const char *GetTorsoModel( void ) { return "models/zombie/zombie_soldier_torso.mdl"; }
	bool CanBecomeLiveTorso() { return false; }
	bool ShouldBecomeTorso( const CTakeDamageInfo &, float ) { return false; }
	HeadcrabRelease_t ShouldReleaseHeadcrab( const CTakeDamageInfo &, float ) { return RELEASE_NO; }
	const char *GetMoanSound( int nSound );
	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void AttackHitSound( void );
	void AttackMissSound( void );
	void FootstepSound( bool fRightFoot );
	void FootscuffSound( bool fRightFoot );

	int TranslateSchedule( int scheduleType );
	int SelectSchedule( void );
	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	void ClimbTouch( CBaseEntity *pOther );
	void LeapAttackTouch( CBaseEntity *pOther ) { ClimbTouch( pOther ); }
	bool OnCalcBaseMove( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult );

	void PrescheduleThink( void );
	void OnScheduleChange( void );
	void UpdateEfficiency( bool bInPVS );
	void BuildScheduleTestBits( void );

	int GetSoundInterests( void );


	void Event_Killed( const CTakeDamageInfo &info );
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );

	Activity NPC_TranslateActivity( Activity baseAct );

	bool IsUnreachable( CBaseEntity *pEntity ) { return false; } // infected ignores unreachable

	// Inputs
	void InputSetSpeedModifier( inputdata_t &inputdata );
	void InputSetCowerOn( inputdata_t & ) { m_bCanCower = true; }
	void InputSetCowerOff( inputdata_t & ) { m_bCanCower = false; }

private:
	void PickBodyVariant( void );
	void ApplySpeedModifier( void );
	void ApplyBodygroups( void );
	bool AllowedToSprint( void );
	void Sprint( bool bMadSprint = false );
	void StopSprint( void );
	bool IsInfectedSprinting( void ) const { return m_flSprintTime > gpGlobals->curtime; }

	// Keyvalues
	float       m_flSpeedModifier; // zero = random/default; nonzero is verbatim
	bool        m_bDisableInmate;
	bool        m_bDisableGuard;
	bool        m_bDisableWorker;
	bool        m_bDisableRural;
	bool        m_bDisableDoctor;
	bool        m_bDisableUniform;
	bool        m_bDisableOffice;
	bool        m_bDisableUrban;

	// Runtime
	int         m_iInfectedVariant; // 0..7
	bool        m_bCanCower;
	float       m_flSprintTime;
	float       m_flSprintRestTime;
	float       m_flSuperFastAttackTime;
	float       m_flSpeedScale;   // SpeedModifier or constructor random 0.8..1.3

	// For door bash (borrowed from CZombie)
	CHandle<CBasePropDoor> m_hBlockingDoor;
	float       m_flDoorBashYaw;
	CRandSimTimer m_DurationDoorBash;
	CSimTimer   m_NextTimeToStartDoorBash;

public:
	DEFINE_CUSTOM_AI;
};

LINK_ENTITY_TO_CLASS( npc_infected, CNPC_UH_Infected );

BEGIN_DATADESC( CNPC_UH_Infected )
	DEFINE_KEYFIELD( m_flSpeedModifier, FIELD_FLOAT, "SpeedModifier" ),
	DEFINE_KEYFIELD( m_bDisableInmate, FIELD_BOOLEAN, "inmate" ),
	DEFINE_KEYFIELD( m_bDisableGuard, FIELD_BOOLEAN, "guard" ),
	DEFINE_KEYFIELD( m_bDisableWorker, FIELD_BOOLEAN, "worker" ),
	DEFINE_KEYFIELD( m_bDisableRural, FIELD_BOOLEAN, "rural" ),
	DEFINE_KEYFIELD( m_bDisableDoctor, FIELD_BOOLEAN, "doctor" ),
	DEFINE_KEYFIELD( m_bDisableUniform, FIELD_BOOLEAN, "uniform" ),
	DEFINE_KEYFIELD( m_bDisableOffice, FIELD_BOOLEAN, "office" ),
	DEFINE_KEYFIELD( m_bDisableUrban, FIELD_BOOLEAN, "urban" ),

	DEFINE_FIELD( m_iInfectedVariant, FIELD_INTEGER ),
	DEFINE_FIELD( m_bCanCower, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flSprintTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSprintRestTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSuperFastAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSpeedScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_hBlockingDoor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDoorBashYaw, FIELD_FLOAT ),
	DEFINE_EMBEDDED( m_DurationDoorBash ),
	DEFINE_EMBEDDED( m_NextTimeToStartDoorBash ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetSpeedModifier", InputSetSpeedModifier ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetCowerOn", InputSetCowerOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetCowerOff", InputSetCowerOff ),

	DEFINE_ENTITYFUNC( ClimbTouch ),

END_DATADESC()

// ---------------------------------------------------------------------------
// Custom AI registration – mirrors sub_101A7410 exactly
// ---------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_infected, CNPC_UH_Infected )

	DECLARE_SQUADSLOT( SQUAD_SLOT_UH_INFECTED_SPRINT1 )
	DECLARE_SQUADSLOT( SQUAD_SLOT_UH_INFECTED_SPRINT2 )

	DECLARE_CONDITION( COND_UH_INFECTED_GRENADE )
	DECLARE_CONDITION( COND_UH_INFECTED_RANDOMRUN )
	DECLARE_CONDITION( COND_UH_INFECTED_CLIMB_TOUCH )
	DECLARE_CONDITION( COND_BLOCKED_BY_DOOR )
	DECLARE_CONDITION( COND_DOOR_OPENED )

	DECLARE_TASK( TASK_UH_INFECTED_UNSTICK_JUMP )
	DECLARE_TASK( TASK_ZOMBIE_YAW_TO_DOOR )
	DECLARE_TASK( TASK_ZOMBIE_ATTACK_DOOR )
	DECLARE_TASK( TASK_ZOMBIE_BREAKTHROUG )
	DECLARE_TASK( TASK_UH_DESTROY_RADIO )
	DECLARE_TASK( TASK_UH_RADIO_PICKUP )
	DECLARE_TASK( TASK_UH_THROW_ITEM )

	// Activities – resolved from engine strings, same names as original DLL
	DECLARE_ACTIVITY( ACT_UH_INFECTED_ATTACK_FAST )
	DECLARE_ACTIVITY( ACT_RUN_PANICKED )
	DECLARE_ACTIVITY( ACT_ZOMBIE_WALLPOUND )
	DECLARE_ACTIVITY( ACT_ZOMBIE_BREAKTHROUGH )

	DEFINE_SCHEDULE
	(
		SCHED_UH_INFECTED_UNSTICK_JUMP,
		"	Tasks"
		"		TASK_UH_INFECTED_UNSTICK_JUMP	0"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_UH_INFECTED_CLIMBING_UNSTICK_JUMP,
		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_UH_INFECTED_UNSTICK_JUMP	0"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ZOMBIE_BASH_DOOR,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_ZOMBIE_WALLPOUND"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		"		TASK_ZOMBIE_YAW_TO_DOOR		0"
		"		TASK_FACE_IDEAL			0"
		"		TASK_ZOMBIE_ATTACK_DOOR		0"
		"		TASK_ZOMBIE_BREAKTHROUG		0"
		"	Interrupts"
		"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_DOOR_OPENED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_UH_INFECTED_INVESTIGATE_RADIO,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_STORE_LASTPOSITION		0"
		"		TASK_SET_TOLERANCE_DISTANCE	128"
		"		TASK_GET_PATH_TO_TARGET		0"
		"		TASK_FACE_IDEAL			0"
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_STOP_MOVING		0"
		"		TASK_WAIT			5"
		"		TASK_FACE_TARGET		0"
		"		TASK_ITEM_PICKUP		0"
		"		TASK_UH_DESTROY_RADIO		0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_RUN_RANDOM"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)

AI_END_CUSTOM_NPC()

// ---------------------------------------------------------------------------
// Construction – mirrors sub_101A7B70
// ---------------------------------------------------------------------------
CNPC_UH_Infected::CNPC_UH_Infected() : m_DurationDoorBash( 2.0f, 6.0f ), m_NextTimeToStartDoorBash( 3.0f )
{
	// sub_101A7B70: exact custom-member defaults.
	m_hBlockingDoor = NULL;
	m_flDoorBashYaw = 0.0f;
	m_flSpeedModifier = 0.0f;
	m_iInfectedVariant = -1;
	m_bCanCower = false;
	m_flSprintTime = 0.0f;
	m_flSprintRestTime = 0.0f;
	m_flSuperFastAttackTime = 0.0f;
	m_flSpeedScale = uh_infected_randspeedenabled.GetBool() ? random->RandomFloat( 0.8f, 1.3f ) : 1.0f;

	// Default disable flags = enabled (1 = No disable) per FGD
	m_bDisableInmate = true;
	m_bDisableGuard = true;
	m_bDisableWorker = true;
	m_bDisableRural = true;
	m_bDisableDoctor = true;
	m_bDisableUniform = true;
	m_bDisableOffice = true;
	m_bDisableUrban = true;
}

// ---------------------------------------------------------------------------
// Precache – mirrors sub_101A5410
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::Precache( void )
{
	BaseClass::Precache();

	for ( int i = 0; i < UH_INFECTED_VARIANT_COUNT; i++ )
	{
		PrecacheModel( s_InfectedVariants[i].pszModel );
		if ( s_InfectedVariants[i].pszGibLeftArm )
			PrecacheModel( s_InfectedVariants[i].pszGibLeftArm );
		if ( s_InfectedVariants[i].pszGibRightArm )
			PrecacheModel( s_InfectedVariants[i].pszGibRightArm );
		if ( s_InfectedVariants[i].pszGibLeftLeg )
			PrecacheModel( s_InfectedVariants[i].pszGibLeftLeg );
		if ( s_InfectedVariants[i].pszGibRightLeg )
			PrecacheModel( s_InfectedVariants[i].pszGibRightLeg );
		if ( s_InfectedVariants[i].pszHelmet )
			PrecacheModel( s_InfectedVariants[i].pszHelmet );
	}

	// Additional generic gibs from original precache list
	PrecacheModel( "models/items/worker_helmet.mdl" );
	PrecacheModel( "models/items/respirator.mdl" );
	PrecacheModel( "models/items/guard_helmet.mdl" );

	// Weapons that can be given as additionalequipment
	PrecacheModel( "models/weapons/w_pipe_pg.mdl" );
	PrecacheModel( "models/weapons/w_wrench_pg.mdl" );
	PrecacheModel( "models/weapons/w_axe_pg.mdl" );
	PrecacheModel( "models/weapons/w_baton_pg.mdl" );

	PrecacheScriptSound( "Zombie.FootstepLeft" );
	PrecacheScriptSound( "Zombie.FootstepRight" );
	PrecacheScriptSound( "Zombine.ScuffLeft" );
	PrecacheScriptSound( "Zombine.ScuffRight" );
	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "Zombine.Die" );
	PrecacheScriptSound( "Zombine.Pain" );
	PrecacheScriptSound( "Zombine.Alert" );
	PrecacheScriptSound( "Zombine.Idle" );
	PrecacheScriptSound( "Zombine.ReadyGrenade" );
	PrecacheScriptSound( "Zombine.Charge" );
	PrecacheScriptSound( "Metal.Door_Breach" );
	PrecacheScriptSound( "ATV_engine_null" );
	PrecacheScriptSound( "Zombie.Attack" );
}

// ---------------------------------------------------------------------------
// Pick variant based on disable flags – mirrors sub_101A6FF0
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::PickBodyVariant( void )
{
	CUtlVector<int> enabled;

	// FGD: 1 = enabled (No disable), 0 = disabled (Yes)
	if ( m_bDisableInmate )  enabled.AddToTail( 0 );
	if ( m_bDisableGuard )   enabled.AddToTail( 6 );
	if ( m_bDisableWorker )  enabled.AddToTail( 1 );
	if ( m_bDisableRural )   enabled.AddToTail( 5 );
	if ( m_bDisableDoctor )  enabled.AddToTail( 2 );
	if ( m_bDisableUniform ) enabled.AddToTail( 3 );
	if ( m_bDisableOffice )  enabled.AddToTail( 7 );
	if ( m_bDisableUrban )   enabled.AddToTail( 4 );

	if ( enabled.Count() == 0 )
	{
		// Everything disabled – fallback to inmate (original did same)
		m_iInfectedVariant = 0;
		return;
	}

	m_iInfectedVariant = enabled[random->RandomInt( 0, enabled.Count() - 1 )];
}

// ---------------------------------------------------------------------------
// Speed modifier – original sub_101A7B70 random 0.8..1.3 + SpeedModifier key
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::ApplySpeedModifier( void )
{
	// Constructor sub_101A7B70 chooses 0.8..1.3 when random speed is enabled.
	// Spawn then replaces it verbatim when the SpeedModifier key is nonzero.
	if ( m_flSpeedModifier != 0.0f )
		m_flSpeedScale = m_flSpeedModifier;
}

// ---------------------------------------------------------------------------
// Bodygroups and capability transitions – sub_101A6620
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::ApplyBodygroups( void )
{
	const int type = m_iInfectedVariant;
	m_nSkin = random->RandomInt( 0, ( type == 0 || type == 6 || type == 7 ) ? 2 :
		( type == 1 || type == 2 ) ? 5 : 8 );

	// sub_101A6620 rolls the head before the arm-loss percentile.
	const int headValue = ( type == 6 ) ? min( random->RandomInt( 0, 19 ), 9 ) : random->RandomInt( 0, 8 );
	int armState;
	const int roll = random->RandomInt( 0, 99 );
	if ( type == 1 || type == 2 || type == 6 )
	{
		if ( roll > 29 ) armState = 0;
		else if ( roll >= 5 ) armState = random->RandomInt( 1, 2 );
		else armState = 3;
	}
	else
	{
		if ( roll > 29 ) armState = roll % 2;
		else if ( roll >= 5 ) armState = roll % 2 + ( random->RandomInt( 0, 1 ) ? 2 : 4 );
		else armState = 6 + random->RandomInt( 0, 1 );
	}

	int arms = FindBodygroupByName( "arms" );
	if ( arms >= 0 ) SetBodygroup( arms, min( armState, GetBodygroupCount( arms ) - 1 ) );

	int head = FindBodygroupByName( "head" );
	int body = FindBodygroupByName( "body" );
	if ( type == 6 )
	{
		int helmetValue = random->RandomInt( 0, 3 );
		int helmet = FindBodygroupByName( "helmet" );
		int respirator = FindBodygroupByName( "respirator" );
		if ( helmet >= 0 ) SetBodygroup( helmet, helmetValue );
		if ( head >= 0 ) SetBodygroup( head, headValue );
		if ( respirator >= 0 ) SetBodygroup( respirator, helmetValue == 3 ? 0 : random->RandomInt( 0, 1 ) );
		if ( helmetValue == 3 ) SetSequenceByName( (char *)"VisorDown" );
	}
	else
	{
		if ( head >= 0 ) SetBodygroup( head, headValue );
		if ( body >= 0 && type != 1 && type != 2 ) SetBodygroup( body, armState % 2 );
	}

	if ( type == 1 )
	{
		int helmet = FindBodygroupByName( "helmet" );
		if ( helmet >= 0 ) SetBodygroup( helmet, random->RandomInt( 0, 9 ) == 0 );
		int gl = FindBodygroupByName( "Glove_L" );
		int gr = FindBodygroupByName( "Glove_R" );
		if ( gl >= 0 && ( armState == 0 || armState == 2 ) ) SetBodygroup( gl, random->RandomInt( 0, 1 ) );
		if ( gr >= 0 && ( armState == 0 || armState == 1 ) ) SetBodygroup( gr, random->RandomInt( 0, 1 ) );
	}

	// With no mapper-authored equipment, sub_101A6620 gives an intact worker
	// a wrench/axe and an intact guard a baton with a one-in-ten chance.
	bool mayGetRandomWeapon = ( type == 1 && armState <= 1 ) || ( type == 6 && armState == 0 );
	if ( m_spawnEquipment == NULL_STRING && mayGetRandomWeapon && random->RandomInt( 0, 9 ) == 0 )
	{
		if ( type == 1 )
			m_spawnEquipment = AllocPooledString( random->RandomInt( 0, 4 ) ? "weapon_melee_wrench" : "weapon_melee_axe" );
		else if ( type == 6 )
			m_spawnEquipment = AllocPooledString( "weapon_melee_baton" );
	}

	// sub_101A6620: arm bodygroups directly control climbing and weapon use.
	if ( type == 1 || type == 2 || type == 6 )
	{
		if ( armState == 0 )
			CapabilitiesAdd( bits_CAP_MOVE_CLIMB | bits_CAP_USE_WEAPONS );
		else if ( armState == 1 )
		{
			CapabilitiesRemove( bits_CAP_MOVE_CLIMB );
			CapabilitiesAdd( bits_CAP_USE_WEAPONS );
		}
		else
			CapabilitiesRemove( bits_CAP_MOVE_CLIMB | bits_CAP_USE_WEAPONS );
	}
	else
	{
		if ( armState <= 1 )
			CapabilitiesAdd( bits_CAP_MOVE_CLIMB | bits_CAP_USE_WEAPONS );
		else if ( armState <= 3 )
		{
			CapabilitiesRemove( bits_CAP_MOVE_CLIMB );
			CapabilitiesAdd( bits_CAP_USE_WEAPONS );
		}
		else
			CapabilitiesRemove( bits_CAP_MOVE_CLIMB | bits_CAP_USE_WEAPONS );
	}
}

// ---------------------------------------------------------------------------
// Set model based on variant – called from BaseZombie Spawn chain
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::SetZombieModel( void )
{
	if ( m_iInfectedVariant < 0 || m_iInfectedVariant >= UH_INFECTED_VARIANT_COUNT )
		PickBodyVariant();

	const UHInfectedVariant_t &var = s_InfectedVariants[m_iInfectedVariant];
	SetModel( var.pszModel );
	ApplyBodygroups();

	SetHullType( HULL_HUMAN );
	SetHullSizeNormal( true );
	SetDefaultEyeOffset();
	SetActivity( ACT_IDLE );
}

// ---------------------------------------------------------------------------
// Spawn – sub_101A6FF0
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::Spawn( void )
{
	// sub_101A6FF0. BaseZombie::Spawn calls our SetZombieModel override after
	// clearing/initialising the common zombie state.
	Precache();
	PickBodyVariant();
	ApplySpeedModifier();

	m_fIsTorso = false;
	m_fIsHeadless = false;
	SetBloodColor( BLOOD_COLOR_RED );
	m_iHealth = max( 1, uh_infected_health.GetInt() );
	SetMaxHealth( m_iHealth );
	m_flFieldOfView = 0.2f;

	CapabilitiesClear();
	// 0x20000A in sub_101A6FF0. BaseZombie::Spawn adds its common ground
	// movement and innate-claw capabilities afterwards.
	CapabilitiesAdd( bits_CAP_MOVE_JUMP | bits_CAP_MOVE_CLIMB | bits_CAP_USE_WEAPONS );

	BaseClass::Spawn();

	// Infected weapon animations and arm-state checks are authored for melee
	// equipment only. Reject a mapper-supplied firearm as well as world pickups.
	CBaseCombatWeapon *pSpawnWeapon = GetActiveWeapon();
	if ( pSpawnWeapon && !pSpawnWeapon->GetWpnData().m_bMeleeWeapon )
	{
		Weapon_Drop( pSpawnWeapon, NULL, NULL );
		UTIL_Remove( pSpawnWeapon );
	}

	// The original overrides SequenceDuration as baseDuration / speed scale.
	// SDK 2007's SequenceDuration is non-virtual, so playback rate is the
	// source-level equivalent available to this reconstruction.
	SetPlaybackRate( m_flSpeedScale );

	m_flSprintTime = 0.0f;
	m_flSprintRestTime = 0.0f;
	m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0f, 4.0f );

	// The original sets custom condition 77 unconditionally at spawn. It is
	// consumed by SelectSchedule as SCHED_RUN_RANDOM.
	SetCondition( COND_UH_INFECTED_RANDOMRUN );
}

// ---------------------------------------------------------------------------
// Sounds – mirror fast zombie but with infected variants
// ---------------------------------------------------------------------------
const char *CNPC_UH_Infected::GetMoanSound( int nSound )
{
	// sub_101A7DA0 -> the single-entry moan table off_10633280.
	return "ATV_engine_null";
}

void CNPC_UH_Infected::PainSound( const CTakeDamageInfo &info )
{
	// sub_101A7EC0 suppresses late pain vocalisations below half health and
	// while dissolving.
	if ( !( GetFlags() & FL_DISSOLVING ) && GetHealth() >= GetMaxHealth() / 2 )
		EmitSound( "Zombine.Pain" );
}

void CNPC_UH_Infected::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "Zombine.Die" );
}

void CNPC_UH_Infected::AlertSound( void )
{
	EmitSound( "Zombine.Alert" );
	m_flNextMoanSound += random->RandomFloat( 2.0f, 4.0f );
}

void CNPC_UH_Infected::IdleSound( void )
{
	if ( GetState() == NPC_STATE_IDLE && random->RandomFloat( 0, 1 ) == 0 )
		return;
	if ( IsSlumped() )
		return;
	EmitSound( "Zombine.Idle" );
	MakeAISpookySound( 360.0f );
}

void CNPC_UH_Infected::AttackSound( void )
{
	EmitSound( "Zombie.Attack" );
}

void CNPC_UH_Infected::AttackHitSound( void )
{
	EmitSound( "Zombie.AttackHit" );
}

void CNPC_UH_Infected::AttackMissSound( void )
{
	EmitSound( "Zombie.AttackMiss" );
}

void CNPC_UH_Infected::FootstepSound( bool fRightFoot )
{
	if ( fRightFoot )
		EmitSound( "Zombie.FootstepRight" );
	else
		EmitSound( "Zombie.FootstepLeft" );
}

void CNPC_UH_Infected::FootscuffSound( bool fRightFoot )
{
	if ( fRightFoot )
		EmitSound( "Zombine.ScuffRight" );
	else
		EmitSound( "Zombine.ScuffLeft" );
}

// ---------------------------------------------------------------------------
// ClimbTouch – reconstructed from sub_101A5A10
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::ClimbTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	if ( pOther->IsPlayer() )
	{
		// Push player aside
		Vector vecDir = pOther->WorldSpaceCenter() - WorldSpaceCenter();
		vecDir.z = 0.0f;
		VectorNormalize( vecDir );
		vecDir *= 200.0f;

		pOther->VelocityPunch( vecDir );

		// If dismounting or blocked, set climb touch condition
		if ( GetActivity() != ACT_CLIMB_DISMOUNT ||
		     ( pOther->GetGroundEntity() == NULL && GetNavigator()->IsGoalActive() &&
		       pOther->GetAbsOrigin().z - GetNavigator()->GetCurWaypointPos().z < -1.0f ) )
		{
			SetCondition( COND_UH_INFECTED_CLIMB_TOUCH );
		}

		SetTouch( NULL );
	}
	else if ( FClassnameIs( pOther, "prop_physics" ) || dynamic_cast<CPhysicsProp *>( pOther ) )
	{
		// Create solver to push physics prop (original sub_101CABB0)
		NPCPhysics_CreateSolver( this, pOther, true, 5.0f );
	}
}

// ---------------------------------------------------------------------------
// Melee conditions – 64 units, dot 0.7, similar to fast zombie
// ---------------------------------------------------------------------------
bool CNPC_UH_Infected::OnCalcBaseMove( AILocalMoveGoal_t *pMoveGoal,
	float distClear, AIMoveResult_t *pResult )
{
	// sub_101A5DA0: unlike stock CAI_BaseNPC this does not open an upcoming
	// prop door normally. A closed, kickable prop door becomes the bash target.
	if ( pMoveGoal && pMoveGoal->directTrace.pObstruction )
	{
		CBasePropDoor *pDoor = dynamic_cast<CBasePropDoor *>( pMoveGoal->directTrace.pObstruction );
		if ( pDoor && pDoor->IsDoorClosed() && pDoor->IsUHKickableDoor() )
		{
			m_hBlockingDoor = pDoor;
			m_flDoorBashYaw = UTIL_VecToYaw( pMoveGoal->directTrace.vHitNormal * -1.0f );
			SetCondition( COND_BLOCKED_BY_DOOR );
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// GetSoundInterests – also listen for the Underhell FM radio / radiocracker
// attract sound (SOUND_FMRADIO) so OnListened() can raise COND_HEAR_FMRADIO.
// ---------------------------------------------------------------------------
int CNPC_UH_Infected::GetSoundInterests( void )
{
	return BaseClass::GetSoundInterests() | SOUND_FMRADIO;
}

// ---------------------------------------------------------------------------
// GatherConditions – infected ignores unreachable, handles climb touch
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// PrescheduleThink – sprint logic + moan
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::PrescheduleThink( void )
{
	// sub_101A5020: infected use the Zombine idle cadence, but do not gather
	// grenade conditions.
	if ( gpGlobals->curtime > m_flNextMoanSound )
	{
		if ( CanPlayMoanSound() )
		{
			IdleSound();
			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 10.0f, 15.0f );
		}
		else
		{
			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 2.5f, 5.0f );
		}
	}
	BaseClass::PrescheduleThink();
}

void CNPC_UH_Infected::OnScheduleChange( void )
{
	// sub_101A50B0: a sprinting infected gets its fast melee translation for
	// one second after reaching melee range.
	if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) && IsInfectedSprinting() )
		m_flSuperFastAttackTime = gpGlobals->curtime + 1.0f;
	BaseClass::OnScheduleChange();
}

void CNPC_UH_Infected::UpdateEfficiency( bool bInPVS )
{
	// sub_101A5B80.
	if ( uh_infected_efficiency.GetBool() )
	{
		SetEfficiency( GetSleepState() != AISS_AWAKE ? AIE_DORMANT : AIE_EFFICIENT );
		SetMoveEfficiency( AIME_NORMAL );
	}
	else
	{
		BaseClass::UpdateEfficiency( bInPVS );
	}
}

bool CNPC_UH_Infected::AllowedToSprint( void )
{
	// sub_101A5E50, structurally identical to the stock Zombine routine except
	// that infected never carry a grenade.
	if ( IsOnFire() || IsInfectedSprinting() || !GetEnemy() )
		return false;

	int chance = 10;
	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player *>( AI_GetSinglePlayer() );
	if ( pPlayer && !pPlayer->FInViewCone( this ) )
		chance = 20;

	if ( GetHealth() > GetMaxHealth() * 0.5f )
	{
		if ( IsStrategySlotRangeOccupied( SQUAD_SLOT_UH_INFECTED_SPRINT1,
			SQUAD_SLOT_UH_INFECTED_SPRINT2 ) )
			return false;
		if ( random->RandomInt( 0, 100 ) > chance )
			return false;
		if ( m_flSprintRestTime > gpGlobals->curtime )
			return false;
	}

	return ( GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter() ).Length() <= 1024.0f;
}

void CNPC_UH_Infected::Sprint( bool bMadSprint )
{
	// sub_101A5210.
	if ( IsInfectedSprinting() )
		return;

	OccupyStrategySlotRange( SQUAD_SLOT_UH_INFECTED_SPRINT1,
		SQUAD_SLOT_UH_INFECTED_SPRINT2 );
	GetNavigator()->SetMovementActivity( (Activity)ACT_RUN_PANICKED );

	float duration = random->RandomFloat( 3.5f, 5.5f );
	if ( bMadSprint )
		duration = 9999.0f;
	m_flSprintTime = gpGlobals->curtime + duration;
	m_flSprintRestTime = m_flSprintTime + random->RandomFloat( 2.5f, 5.0f );
	EmitSound( "Zombine.Charge" );
}

void CNPC_UH_Infected::StopSprint( void )
{
	GetNavigator()->SetMovementActivity( ACT_WALK );
	m_flSprintTime = gpGlobals->curtime;
	m_flSprintRestTime = m_flSprintTime + random->RandomFloat( 2.5f, 5.0f );
}

// ---------------------------------------------------------------------------
// BuildScheduleTestBits – interrupt on climb touch, door block
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::BuildScheduleTestBits( void )
{
	// sub_101A5810.
	BaseClass::BuildScheduleTestBits();
	SetCustomInterruptCondition( COND_BLOCKED_BY_DOOR );

	Activity activity = GetActivity();
	if ( activity == ACT_CLIMB_UP || activity == ACT_CLIMB_DOWN || activity == ACT_CLIMB_DISMOUNT )
		SetCustomInterruptCondition( COND_UH_INFECTED_CLIMB_TOUCH );
	else
		ClearCustomInterruptCondition( COND_UH_INFECTED_CLIMB_TOUCH );

	if ( IsCurSchedule( SCHED_FORCED_GO ) || IsCurSchedule( SCHED_FORCED_GO_RUN ) ||
		 IsCurSchedule( SCHED_RUN_FROM_ENEMY_FALLBACK ) ||
		 IsCurSchedule( SCHED_UH_INFECTED_INVESTIGATE_RADIO ) )
	{
		SetCustomInterruptCondition( COND_NEW_ENEMY );
		SetCustomInterruptCondition( COND_SEE_ENEMY );
	}

	if ( IsCurSchedule( SCHED_NEW_WEAPON ) )
	{
		CBaseEntity *pEnemy = GetEnemy();
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(
			Weapon_FindUsable( Vector( 540, 540, 100 ) ) );
		if ( pEnemy && pWeapon && pWeapon->GetWpnData().m_bMeleeWeapon &&
			WorldSpaceCenter().DistTo( pEnemy->WorldSpaceCenter() ) <=
			WorldSpaceCenter().DistTo( pWeapon->WorldSpaceCenter() ) )
		{
			SetCustomInterruptCondition( COND_NEW_ENEMY );
			SetCustomInterruptCondition( COND_SEE_ENEMY );
		}
	}
}

// ---------------------------------------------------------------------------
// SelectSchedule – main infected intelligence
// ---------------------------------------------------------------------------
int CNPC_UH_Infected::SelectSchedule( void )
{
	// sub_101A5BD0.
	if ( GetHealth() <= 0 )
		return BaseClass::SelectSchedule();

	if ( HasCondition( COND_UH_INFECTED_CLIMB_TOUCH ) )
		return SCHED_UH_INFECTED_UNSTICK_JUMP;

	if ( HasCondition( COND_BLOCKED_BY_DOOR ) && m_hBlockingDoor )
	{
		ClearCondition( COND_BLOCKED_BY_DOOR );
		if ( m_NextTimeToStartDoorBash.Expired() )
		{
			SetTarget( m_hBlockingDoor.Get() );
			return SCHED_ZOMBIE_BASH_DOOR;
		}
		m_hBlockingDoor = NULL;
	}

	if ( HasCondition( COND_BETTER_WEAPON_AVAILABLE ) )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(
			Weapon_FindUsable( Vector( 540, 540, 100 ) ) );
		if ( pWeapon && pWeapon->GetWpnData().m_bMeleeWeapon )
		{
			pWeapon->Lock( 10.0f, this );
			SetTarget( pWeapon );
			return SCHED_NEW_WEAPON;
		}

		// Do not let BaseZombie's fallback schedule pick up firearms.
		ClearCondition( COND_BETTER_WEAPON_AVAILABLE );
	}

	if ( HasCondition( COND_HEAR_FMRADIO ) && !HasCondition( COND_SEE_ENEMY ) )
	{
		CSound *pSound = GetBestSound( SOUND_FMRADIO );
		if ( pSound && pSound->IsSoundType( SOUND_FMRADIO ) && pSound->m_hOwner )
		{
			SetTarget( pSound->m_hOwner.Get() );
			return SCHED_UH_INFECTED_INVESTIGATE_RADIO;
		}
	}

	if ( HasCondition( COND_UH_INFECTED_RANDOMRUN ) )
		return SCHED_RUN_RANDOM;

	if ( !HasCondition( COND_SEE_ENEMY ) && ( uh_infectedcower.GetBool() || m_bCanCower ) )
		return SCHED_RUN_FROM_ENEMY_FALLBACK;

	return BaseClass::SelectSchedule();
}

int CNPC_UH_Infected::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	// sub_101A62B0 only specializes a blocked-door failure.
	if ( HasCondition( COND_BLOCKED_BY_DOOR ) && m_hBlockingDoor )
	{
		ClearCondition( COND_BLOCKED_BY_DOOR );
		if ( m_NextTimeToStartDoorBash.Expired() && failedSchedule != SCHED_ZOMBIE_BASH_DOOR )
			return SCHED_ZOMBIE_BASH_DOOR;
		m_hBlockingDoor = NULL;
	}
	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

int CNPC_UH_Infected::TranslateSchedule( int scheduleType )
{
	// sub_101A5130.
	if ( scheduleType == SCHED_UH_INFECTED_UNSTICK_JUMP )
	{
		Activity activity = GetActivity();
		if ( activity == ACT_CLIMB_UP || activity == ACT_CLIMB_DOWN || activity == ACT_CLIMB_DISMOUNT )
			return SCHED_UH_INFECTED_CLIMBING_UNSTICK_JUMP;
		return SCHED_UH_INFECTED_UNSTICK_JUMP;
	}
	return BaseClass::TranslateSchedule( scheduleType );
}

// ---------------------------------------------------------------------------
// StartTask / RunTask – unstick jump, door bash, radio
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::StartTask( const Task_t *pTask )
{
	// sub_101A6350.
	switch ( pTask->iTask )
	{
	case TASK_ZOMBIE_YAW_TO_DOOR:
		if ( m_hBlockingDoor )
			GetMotor()->SetIdealYaw( m_flDoorBashYaw );
		TaskComplete();
		return;

	case TASK_ZOMBIE_ATTACK_DOOR:
		if ( IsActivityFinished() )
			TaskComplete();
		if ( m_hBlockingDoor )
		{
			Vector forward;
			GetVectors( &forward, NULL, NULL );
			Vector delta = m_hBlockingDoor->GetAbsOrigin() - GetAbsOrigin();
			float move = DotProduct( forward, delta ) - uh_infected_door_dist.GetFloat();
			SetAbsOrigin( GetAbsOrigin() + forward * move );
		}
		else
		{
			TaskComplete();
		}
		m_DurationDoorBash.Reset();
		SetIdealActivity( (Activity)ACT_ZOMBIE_WALLPOUND );
		return;

	case TASK_ZOMBIE_BREAKTHROUG:
		if ( GetActivity() == (Activity)ACT_ZOMBIE_BREAKTHROUGH && IsActivityFinished() )
			TaskComplete();
		if ( m_hBlockingDoor )
		{
			SetIdealActivity( (Activity)ACT_ZOMBIE_BREAKTHROUGH );
			m_hBlockingDoor->UHBreachDoor( this, this, true, Vector( 480, 0, 0 ) );
			m_hBlockingDoor = NULL;
		}
		return;

	case TASK_UH_DESTROY_RADIO:
	case TASK_UH_RADIO_PICKUP:
		if ( GetTarget() )
			UTIL_Remove( GetTarget() );
		if ( pTask->iTask == TASK_UH_RADIO_PICKUP )
			SetIdealActivity( ACT_PICKUP_RACK );
		TaskComplete();
		return;

	case TASK_UH_THROW_ITEM:
		// Original obtains the target's throw vector, then follows the same
		// pickup/remove completion path. No explosion is created here.
		if ( GetTarget() )
			UTIL_Remove( GetTarget() );
		SetIdealActivity( ACT_PICKUP_RACK );
		TaskComplete();
		return;

	default:
		BaseClass::StartTask( pTask );
		return;
	}
}

void CNPC_UH_Infected::RunTask( const Task_t *pTask )
{
	// sub_101A5FB0, matching the stock Zombine sprint lifecycle.
	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT_STEP:
	case TASK_WAIT_FOR_MOVEMENT:
		BaseClass::RunTask( pTask );
		if ( IsOnFire() && IsInfectedSprinting() )
			StopSprint();
		if ( GetEnemy() )
		{
			if ( AllowedToSprint() )
			{
				Sprint( GetHealth() <= GetMaxHealth() * 0.5f );
				return;
			}
			if ( GetNavigator()->GetMovementActivity() != ACT_WALK && !IsInfectedSprinting() )
				GetNavigator()->SetMovementActivity( ACT_WALK );
		}
		else
		{
			GetNavigator()->SetMovementActivity( ACT_WALK );
		}
		return;

	case TASK_UH_INFECTED_UNSTICK_JUMP:
		if ( GetFlags() & FL_ONGROUND )
			TaskComplete();
		return;

	case TASK_ZOMBIE_ATTACK_DOOR:
		if ( IsActivityFinished() )
		{
			if ( m_DurationDoorBash.Expired() )
			{
				TaskComplete();
				m_NextTimeToStartDoorBash.Reset();
			}
			else
			{
				ResetIdealActivity( (Activity)ACT_ZOMBIE_WALLPOUND );
			}
		}
		return;

	case TASK_ZOMBIE_BREAKTHROUG:
	case TASK_UH_RADIO_PICKUP:
		if ( IsActivityFinished() )
			TaskComplete();
		return;

	default:
		BaseClass::RunTask( pTask );
		return;
	}
}

// ---------------------------------------------------------------------------
// Activity translation – fast attack window
// ---------------------------------------------------------------------------
Activity CNPC_UH_Infected::NPC_TranslateActivity( Activity baseAct )
{
	// sub_101A50F0.
	if ( baseAct == ACT_MELEE_ATTACK1 && gpGlobals->curtime < m_flSuperFastAttackTime )
		return (Activity)ACT_UH_INFECTED_ATTACK_FAST;
	return BaseClass::NPC_TranslateActivity( baseAct );
}

// ---------------------------------------------------------------------------
// Damage / death
// ---------------------------------------------------------------------------
int CNPC_UH_Infected::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// Base NPC damage processing invokes the virtual PainSound once. Calling it
	// here as well produced doubled sounds and reactions.
	return BaseClass::OnTakeDamage_Alive( info );
}

void CNPC_UH_Infected::Event_Killed( const CTakeDamageInfo &info )
{
	// sub_101A5170: only the worker has this class-specific death drop.
	if ( m_iInfectedVariant == 1 )
	{
		int helmet = FindBodygroupByName( "helmet" );
		if ( helmet >= 0 && GetBodygroup( helmet ) != 0 )
		{
			Vector origin = WorldSpaceCenter();
			QAngle angles = GetAbsAngles();
			GetAttachment( "Eyes", origin, angles );
			DropItem( "item_helmet_worker", origin, angles );
			SetBodygroup( helmet, 0 );
		}
	}
	BaseClass::Event_Killed( info );
}

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------
void CNPC_UH_Infected::InputSetSpeedModifier( inputdata_t &inputdata )
{
	m_flSpeedModifier = inputdata.value.Float();
	m_flSpeedScale = m_flSpeedModifier;
	SetPlaybackRate( m_flSpeedScale );
}
