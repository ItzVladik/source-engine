#include "cbase.h"
#include "npc_basezombie.h"
#include "ai_schedule.h"
#include "ai_motor.h"
#include "soundent.h"
#include "explode.h"
#include "basepropdoor.h"
#include "particle_parse.h"

#include "tier0/memdbgon.h"

ConVar uh_butcher_health( "uh_butcher_health", "99999999", FCVAR_ARCHIVE );
ConVar sk_butcher_dmg_charge( "sk_butcher_dmg_charge", "20", FCVAR_ARCHIVE );
ConVar uh_butcher_speed( "uh_butcher_speed", "1.25", FCVAR_ARCHIVE );
ConVar uh_butcher_charge_cooldown( "uh_butcher_charge_cooldown", "5.0", FCVAR_ARCHIVE );

// Original private AI ids from sub_101A45E0.
int ACT_BUTCHER_UH_INFECTED_ATTACK_FAST;
int ACT_BUTCHER_ZOMBIE_WALLPOUND;
int ACT_BUTCHER_ZOMBIE_BREAKTHROUGH;

enum
{
	COND_BUTCHER_UH_PHYSICS_TARGET = 76,
	COND_BUTCHER_UH_PHYSICS_TARGET_INVALID = 77,
	COND_BUTCHER_UH_HAS_CHARGE_TARGET = 78,
	COND_BUTCHER_UH_CAN_CHARGE = 79,
	COND_BUTCHER_UH_DOOR_OPENED = 80,
	COND_BUTCHER_UH_BLOCKED_BY_DOOR = 81,
};
enum
{
	TASK_BUTCHER_CHARGE = 250,
	TASK_BUTCHER_GET_PATH_TO_PHYSOBJECT = 251,
	TASK_BUTCHER_SHOVE_PHYSOBJECT = 252,
	TASK_BUTCHER_SUMMON = 253,
	TASK_BUTCHER_SET_FLINCH_ACTIVITY = 254,
	TASK_BUTCHER_GET_PATH_TO_CHARGE_POSITION = 255,
	TASK_BUTCHER_GET_PATH_TO_NEAREST_NODE = 256,
	TASK_BUTCHER_GET_CHASE_PATH_ENEMY_TOLERANCE = 257,
	TASK_BUTCHER_OPPORTUNITY_THROW = 258,
	TASK_BUTCHER_FIND_PHYSOBJECT = 259,
};
enum
{
	SCHED_BUTCHER_UH_CHARGE = 100,
	SCHED_BUTCHER_UH_CHARGE_TARGET = 105,
};

class CNPC_UH_Butcher : public CNPC_BaseZombie
{
	DECLARE_CLASS( CNPC_UH_Butcher, CNPC_BaseZombie );
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	CNPC_UH_Butcher() : m_bChargeEnabled( true ), m_bCower( false ), m_bCharging( false ), m_flNextCharge( 0.0f ), m_flChargeEndTime( 0.0f ) {}
	void Spawn(); void Precache(); void PrescheduleThink();
	int SelectSchedule();
	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );
	const char *GetHeadcrabClassname() { return "npc_headcrab"; }
	const char *GetHeadcrabModel() { return "models/headcrabclassic.mdl"; }
	const char *GetLegsModel() { return "models/zombie/zombie_soldier_legs.mdl"; }
	const char *GetTorsoModel() { return "models/zombie/zombie_soldier_torso.mdl"; }
	const char *GetMoanSound( int ) { return "NPC_Butcher.Idle"; }
	void PainSound( const CTakeDamageInfo & ) { EmitSound( "NPC_Butcher.Alert" ); }
	void AlertSound() { EmitSound( "NPC_Butcher.Alert" ); }
	void IdleSound() { EmitSound( "NPC_Butcher.Idle" ); }
	void DeathSound( const CTakeDamageInfo & ) { EmitSound( "NPC_Butcher.Die" ); }
	void AttackSound() { EmitSound( "NPC_Butcher.Melee" ); }
	void AttackHitSound() { EmitSound( "NPC_Butcher.Melee" ); }
	void AttackMissSound() { EmitSound( "Zombie.AttackMiss" ); }
	void FootstepSound( bool bRightFoot ) { EmitSound( bRightFoot ? "NPC_Butcher.FootstepRight" : "NPC_Butcher.FootstepLeft" ); }
	void FootscuffSound( bool bRightFoot ) { FootstepSound( bRightFoot ); }
	void InputEnableCharge( inputdata_t & ) { m_bChargeEnabled = true; }
	void InputDisableCharge( inputdata_t & ) { m_bChargeEnabled = false; m_bCharging = false; SetAbsVelocity( vec3_origin ); }
	void InputSetCowerOn( inputdata_t & ) { m_bCower = true; }
	void InputSetCowerOff( inputdata_t & ) { m_bCower = false; }
	void InputChargeEntity( inputdata_t &inputdata );
private:
	void DoCharge( CBaseEntity *pTarget );
	void ChargeImpact( CBaseEntity *pHit, const Vector &dir );
	bool m_bChargeEnabled, m_bCower, m_bCharging;
	float m_flNextCharge, m_flChargeEndTime;
	EHANDLE m_hChargeTarget, m_hChargeStart;
};
LINK_ENTITY_TO_CLASS( npc_butcher, CNPC_UH_Butcher );
BEGIN_DATADESC( CNPC_UH_Butcher )
	DEFINE_FIELD( m_bChargeEnabled, FIELD_BOOLEAN ), DEFINE_FIELD( m_bCower, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bCharging, FIELD_BOOLEAN ), DEFINE_FIELD( m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( m_flChargeEndTime, FIELD_TIME ), DEFINE_FIELD( m_hChargeTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hChargeStart, FIELD_EHANDLE ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableCharge", InputEnableCharge ), DEFINE_INPUTFUNC( FIELD_VOID, "DisableCharge", InputDisableCharge ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetCowerOn", InputSetCowerOn ), DEFINE_INPUTFUNC( FIELD_VOID, "SetCowerOff", InputSetCowerOff ),
	DEFINE_INPUTFUNC( FIELD_STRING, "ChargeEntity", InputChargeEntity ),
END_DATADESC()
void CNPC_UH_Butcher::Precache()
{
	PrecacheModel( "models/butcher.mdl" );
	PrecacheModel( "models/headcrabclassic.mdl" );
	PrecacheModel( "models/zombie/zombie_soldier_legs.mdl" );
	PrecacheModel( "models/zombie/zombie_soldier_torso.mdl" );
	PrecacheScriptSound( "NPC_Butcher.FootstepRight" );
	PrecacheScriptSound( "NPC_Butcher.FootstepLeft" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "Zombie.Die" );
	PrecacheScriptSound( "NPC_Butcher.Alert" );
	PrecacheScriptSound( "NPC_Butcher.Idle" );
	PrecacheScriptSound( "Metal.Door_Breach" );
	PrecacheScriptSound( "ATV_engine_null" );
	PrecacheScriptSound( "NPC_Butcher.Charge" );
	PrecacheScriptSound( "NPC_Butcher.ChargeHit" );
	PrecacheScriptSound( "NPC_Butcher.Melee" );
	PrecacheParticleSystem( "door_explosion_shockwave" );
	BaseClass::Precache();
}
void CNPC_UH_Butcher::Spawn() { Precache(); SetModel( "models/butcher.mdl" ); BaseClass::Spawn(); SetHealth( uh_butcher_health.GetInt() ); SetMaxHealth( GetHealth() ); SetPlaybackRate( uh_butcher_speed.GetFloat() ); }
void CNPC_UH_Butcher::DoCharge( CBaseEntity *pTarget )
{
	if ( !pTarget || !m_bChargeEnabled || m_bCower || m_bCharging || gpGlobals->curtime < m_flNextCharge )
		return;

	Vector dir = pTarget->WorldSpaceCenter() - WorldSpaceCenter();
	dir.z = 0.0f;
	if ( VectorNormalize( dir ) == 0.0f )
		return;

	m_bCharging = true;
	m_flChargeEndTime = gpGlobals->curtime + 2.0f;
	m_flNextCharge = gpGlobals->curtime + uh_butcher_charge_cooldown.GetFloat();
	GetMotor()->SetIdealYawAndUpdate( UTIL_VecToYaw( dir ), AI_KEEP_YAW_SPEED );
	SetAbsVelocity( dir * 512.0f );
	EmitSound( "NPC_Butcher.Charge" );
}

void CNPC_UH_Butcher::ChargeImpact( CBaseEntity *pHit, const Vector &dir )
{
	if ( !pHit || pHit == this )
		return;

	EmitSound( "NPC_Butcher.ChargeHit" );
	CTakeDamageInfo info( this, this, sk_butcher_dmg_charge.GetFloat(), DMG_CLUB );
	CalculateMeleeDamageForce( &info, dir, pHit->WorldSpaceCenter(), 5.0f );
	pHit->TakeDamage( info );

	CBasePropDoor *pDoor = dynamic_cast<CBasePropDoor *>( pHit );
	if ( pDoor && !pDoor->IsDoorLocked() )
		pDoor->UHBreachDoor( this, this, true, pHit->WorldSpaceCenter() );
}

void CNPC_UH_Butcher::PrescheduleThink()
{
	BaseClass::PrescheduleThink();
	if ( m_bCower )
	{
		m_bCharging = false;
		SetAbsVelocity( vec3_origin );
		return;
	}

	CBaseEntity *pTarget = m_hChargeTarget.Get() ? m_hChargeTarget.Get() : GetEnemy();
	if ( m_bCharging )
	{
		Vector dir = pTarget ? pTarget->WorldSpaceCenter() - WorldSpaceCenter() : GetAbsVelocity();
		dir.z = 0.0f;
		if ( VectorNormalize( dir ) == 0.0f )
		{
			m_bCharging = false;
			SetAbsVelocity( vec3_origin );
			return;
		}
		GetMotor()->SetIdealYawAndUpdate( UTIL_VecToYaw( dir ), AI_KEEP_YAW_SPEED );
		SetAbsVelocity( dir * 512.0f );
		trace_t tr;
		UTIL_TraceHull( WorldSpaceCenter(), WorldSpaceCenter() + dir * 72.0f,
			Vector( -24, -24, -36 ), Vector( 24, 24, 48 ), MASK_SHOT_HULL,
			this, COLLISION_GROUP_NONE, &tr );
		if ( tr.m_pEnt && tr.m_pEnt != this )
		{
			ChargeImpact( tr.m_pEnt, dir );
			m_bCharging = false;
			SetAbsVelocity( vec3_origin );
		}
		else if ( gpGlobals->curtime >= m_flChargeEndTime )
		{
			m_bCharging = false;
			SetAbsVelocity( vec3_origin );
		}
		return;
	}

	if ( m_hChargeTarget.Get() )
		SetCondition( COND_BUTCHER_UH_HAS_CHARGE_TARGET );
	else
		ClearCondition( COND_BUTCHER_UH_HAS_CHARGE_TARGET );

	if ( m_bChargeEnabled && pTarget && gpGlobals->curtime >= m_flNextCharge &&
		 ( pTarget->WorldSpaceCenter() - WorldSpaceCenter() ).LengthSqr() <= 4000000.0f )
		SetCondition( COND_BUTCHER_UH_CAN_CHARGE );
	else
		ClearCondition( COND_BUTCHER_UH_CAN_CHARGE );
}

int CNPC_UH_Butcher::SelectSchedule()
{
	if ( !m_bCower && !m_bCharging && m_bChargeEnabled && gpGlobals->curtime >= m_flNextCharge )
	{
		if ( HasCondition( COND_BUTCHER_UH_HAS_CHARGE_TARGET ) )
			return SCHED_BUTCHER_UH_CHARGE_TARGET;
		if ( HasCondition( COND_BUTCHER_UH_CAN_CHARGE ) )
			return SCHED_BUTCHER_UH_CHARGE;
	}
	return BaseClass::SelectSchedule();
}

void CNPC_UH_Butcher::StartTask( const Task_t *pTask )
{
	if ( pTask->iTask == TASK_BUTCHER_CHARGE )
	{
		CBaseEntity *pTarget = m_hChargeTarget.Get() ? m_hChargeTarget.Get() : GetEnemy();
		DoCharge( pTarget );
		if ( !m_bCharging )
			TaskFail( FAIL_NO_TARGET );
		return;
	}
	BaseClass::StartTask( pTask );
}

void CNPC_UH_Butcher::RunTask( const Task_t *pTask )
{
	if ( pTask->iTask == TASK_BUTCHER_CHARGE )
	{
		if ( !m_bCharging )
			TaskComplete();
		return;
	}
	BaseClass::RunTask( pTask );
}

void CNPC_UH_Butcher::InputChargeEntity( inputdata_t &inputdata )
{
	char names[256];
	Q_strncpy( names, inputdata.value.String(), sizeof( names ) );
	char *pTargetName = strtok( names, " " );
	char *pStartName = strtok( NULL, " " );
	m_hChargeTarget = pTargetName ? gEntList.FindEntityByName( NULL, pTargetName, this ) : NULL;
	m_hChargeStart = pStartName ? gEntList.FindEntityByName( NULL, pStartName, this ) : NULL;
	if ( m_hChargeStart.Get() )
		SetAbsOrigin( m_hChargeStart->GetAbsOrigin() );
	if ( m_hChargeTarget.Get() )
	{
		SetEnemy( m_hChargeTarget.Get() );
		SetCondition( COND_BUTCHER_UH_HAS_CHARGE_TARGET );
		if ( gpGlobals->curtime >= m_flNextCharge )
			SetSchedule( SCHED_BUTCHER_UH_CHARGE_TARGET );
	}
}

AI_BEGIN_CUSTOM_NPC( npc_butcher, CNPC_UH_Butcher )
	DECLARE_CONDITION( COND_BUTCHER_UH_PHYSICS_TARGET )
	DECLARE_CONDITION( COND_BUTCHER_UH_PHYSICS_TARGET_INVALID )
	DECLARE_CONDITION( COND_BUTCHER_UH_HAS_CHARGE_TARGET )
	DECLARE_CONDITION( COND_BUTCHER_UH_CAN_CHARGE )
	DECLARE_CONDITION( COND_BUTCHER_UH_DOOR_OPENED )
	DECLARE_CONDITION( COND_BUTCHER_UH_BLOCKED_BY_DOOR )

	DECLARE_ACTIVITY( ACT_BUTCHER_UH_INFECTED_ATTACK_FAST )
	DECLARE_ACTIVITY( ACT_BUTCHER_ZOMBIE_WALLPOUND )
	DECLARE_ACTIVITY( ACT_BUTCHER_ZOMBIE_BREAKTHROUGH )

	DECLARE_TASK( TASK_BUTCHER_CHARGE )
	DECLARE_TASK( TASK_BUTCHER_GET_PATH_TO_PHYSOBJECT )
	DECLARE_TASK( TASK_BUTCHER_SHOVE_PHYSOBJECT )
	DECLARE_TASK( TASK_BUTCHER_SUMMON )
	DECLARE_TASK( TASK_BUTCHER_SET_FLINCH_ACTIVITY )
	DECLARE_TASK( TASK_BUTCHER_GET_PATH_TO_CHARGE_POSITION )
	DECLARE_TASK( TASK_BUTCHER_GET_PATH_TO_NEAREST_NODE )
	DECLARE_TASK( TASK_BUTCHER_GET_CHASE_PATH_ENEMY_TOLERANCE )
	DECLARE_TASK( TASK_BUTCHER_OPPORTUNITY_THROW )
	DECLARE_TASK( TASK_BUTCHER_FIND_PHYSOBJECT )

	DEFINE_SCHEDULE
	(
		SCHED_BUTCHER_UH_CHARGE,
		"	Tasks"
		"		TASK_STOP_MOVING 0"
		"		TASK_SET_FAIL_SCHEDULE SCHEDULE:SCHED_RUN_RANDOM"
		"		TASK_FACE_ENEMY 0"
		"		TASK_BUTCHER_CHARGE 0"
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_BUTCHER_UH_CHARGE_TARGET,
		"	Tasks"
		"		TASK_STOP_MOVING 0"
		"		TASK_SET_FAIL_SCHEDULE SCHEDULE:SCHED_RUN_RANDOM"
		"		TASK_FACE_ENEMY 0"
		"		TASK_BUTCHER_CHARGE 0"
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
	)
AI_END_CUSTOM_NPC()
