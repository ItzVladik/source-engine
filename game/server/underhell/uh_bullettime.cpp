// Underhell bullet-time visual bullet implementation.
#include "cbase.h"
#include "underhell/uh_bullettime.h"
#include "hl2_player.h"
#include "igamesystem.h"

#include "tier0/memdbgon.h"

static void UH_BulletTimeChanged( IConVar *pVar, const char *pOldValue, float flOldValue );
ConVar bt_enabled( "bt_enabled", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "enable bullet time", UH_BulletTimeChanged );
ConVar bt_timescale( "bt_timescale", "0.3", FCVAR_NONE, "bullet time scale" );
ConVar bt_enemybulletspeed( "bt_enemybulletspeed", "500", FCVAR_NONE, "enemy bullet speed" );
ConVar bt_playerbulletspeed( "bt_playerbulletspeed", "2000", FCVAR_NONE, "player bullet speed" );
ConVar bt_plr_speed( "bt_plr_speed", "250", FCVAR_NONE, "player speed during bullet time" );

static bool g_bResolvingBtBullet = false;

class CBtBullet : public CBaseAnimating
{
	DECLARE_CLASS( CBtBullet, CBaseAnimating );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
public:
	CBtBullet() : m_flBulletSpeed( 0.0f ), m_iAmmoType( -1 ), m_iTracerFreq( 0 ),
		m_iDamage( 0 ), m_iPlayerDamage( 0 ), m_flDistance( MAX_TRACE_LENGTH ),
		m_nBulletFlags( 0 ), m_flDamageForceScale( 1.0f ), m_bPrimaryAttack( true ) {}

	void Precache()
	{
		PrecacheModel( "models/weapons/w_bullet.mdl" );
		PrecacheModel( "models/weapons/bt_9mm.mdl" );
		PrecacheModel( "models/weapons/bt_357.mdl" );
		PrecacheModel( "models/weapons/bt_762.mdl" );
		PrecacheModel( "models/weapons/w_pellet.mdl" );
	}

	void SpawnBullet( CBaseEntity *pShooter, const FireBulletsInfo_t &info,
		const Vector &direction, bool bEnemyBullet )
	{
		Precache();
		m_hShooter = pShooter;
		m_vecSource = info.m_vecSrc;
		m_vecForward = direction;
		VectorNormalize( m_vecForward );
		m_iAmmoType = info.m_iAmmoType;
		m_iTracerFreq = info.m_iTracerFreq;
		m_iDamage = info.m_iDamage;
		m_iPlayerDamage = info.m_iPlayerDamage;
		m_flDistance = info.m_flDistance;
		m_nBulletFlags = info.m_nFlags;
		m_flDamageForceScale = info.m_flDamageForceScale;
		m_hAdditionalIgnore = info.m_pAdditionalIgnoreEnt;
		m_bPrimaryAttack = info.m_bPrimaryAttack;
		m_flBulletSpeed = bEnemyBullet ? bt_enemybulletspeed.GetFloat() : bt_playerbulletspeed.GetFloat();

		const char *pszModel = "models/weapons/bt_762.mdl";
		if ( m_iAmmoType == 3 || m_iAmmoType == 4 ) pszModel = "models/weapons/bt_9mm.mdl";
		else if ( m_iAmmoType == 5 ) pszModel = "models/weapons/bt_357.mdl";
		else if ( m_iAmmoType == 7 ) pszModel = "models/weapons/w_pellet.mdl";

		SetModel( pszModel );
		SetAbsOrigin( info.m_vecSrc );
		QAngle angles;
		VectorAngles( m_vecForward, angles );
		SetAbsAngles( angles );
		SetSolid( SOLID_BBOX );
		UTIL_SetSize( this, vec3_origin, vec3_origin );
		SetMoveType( MOVETYPE_FLY );
		SetGravity( 0.0f );
		SetCollisionGroup( COLLISION_GROUP_NONE );
		SetOwnerEntity( pShooter );
		SetTouch( &CBtBullet::BulletTouch );
		SetThink( &CBtBullet::BulletThink );
		SetSimulatedEveryTick( true );
		SetAbsVelocity( m_vecForward * m_flBulletSpeed );
		SetNextThink( gpGlobals->curtime );
	}

	void BulletThink()
	{
		// sub_101078D0: host_timescale already slows simulation. Do not multiply
		// the configured projectile speed by bt_timescale a second time.
		const float flSpeed = UH_BulletTimeActive() ? m_flBulletSpeed : 2500.0f;
		SetAbsVelocity( m_vecForward * flSpeed );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}

	void BulletTouch( CBaseEntity *pOther )
	{
		if ( pOther == m_hShooter.Get() )
			return;

		CBaseEntity *pShooter = m_hShooter.Get();
		if ( pShooter )
		{
			FireBulletsInfo_t shot;
			shot.m_iShots = 1;
			shot.m_vecSrc = m_vecSource;
			shot.m_vecDirShooting = m_vecForward;
			shot.m_vecSpread = vec3_origin;
			shot.m_flDistance = m_flDistance;
			shot.m_iAmmoType = m_iAmmoType;
			shot.m_iTracerFreq = m_iTracerFreq;
			shot.m_iDamage = m_iDamage;
			shot.m_iPlayerDamage = m_iPlayerDamage;
			shot.m_nFlags = m_nBulletFlags;
			shot.m_flDamageForceScale = m_flDamageForceScale;
			shot.m_pAttacker = pShooter;
			shot.m_pAdditionalIgnoreEnt = m_hAdditionalIgnore.Get();
			shot.m_bPrimaryAttack = m_bPrimaryAttack;
			g_bResolvingBtBullet = true;
			pShooter->FireBullets( shot );
			g_bResolvingBtBullet = false;
		}
		UTIL_Remove( this );
	}

private:
	EHANDLE m_hShooter;
	Vector m_vecSource;
	Vector m_vecForward;
	float m_flBulletSpeed;
	int m_iAmmoType;
	int m_iTracerFreq;
	int m_iDamage;
	int m_iPlayerDamage;
	float m_flDistance;
	int m_nBulletFlags;
	float m_flDamageForceScale;
	EHANDLE m_hAdditionalIgnore;
	bool m_bPrimaryAttack;
};

LINK_ENTITY_TO_CLASS( btbullet, CBtBullet );
IMPLEMENT_SERVERCLASS_ST( CBtBullet, DT_BtBullet )
END_SEND_TABLE()
BEGIN_DATADESC( CBtBullet )
	DEFINE_FIELD( m_hShooter, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecSource, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecForward, FIELD_VECTOR ),
	DEFINE_FIELD( m_flBulletSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_iAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( m_iTracerFreq, FIELD_INTEGER ),
	DEFINE_FIELD( m_iDamage, FIELD_INTEGER ),
	DEFINE_FIELD( m_iPlayerDamage, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_nBulletFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDamageForceScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_hAdditionalIgnore, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bPrimaryAttack, FIELD_BOOLEAN ),
	DEFINE_ENTITYFUNC( BulletTouch ),
	DEFINE_THINKFUNC( BulletThink ),
END_DATADESC()

bool UH_BulletTimeActive() { return bt_enabled.GetBool(); }
void UH_SetBulletTime( bool bEnabled ) { bt_enabled.SetValue( bEnabled ? 1 : 0 ); }

void UH_ToggleBulletTime( CBasePlayer *pPlayer )
{
	const bool bEnable = !bt_enabled.GetBool();
	if ( pPlayer )
	{
		if ( bEnable )
		{
			pPlayer->EmitSound( "Player.bullettimestart" );
			pPlayer->EmitSound( "Player.bullettimeloop" );
		}
		else
		{
			pPlayer->StopSound( "Player.bullettimeloop" );
			pPlayer->EmitSound( "Player.bullettimeend" );
		}
	}
	bt_enabled.SetValue( bEnable ? 1 : 0 );
}

bool UH_BulletTimeDeferShot( CBaseEntity *pShooter, const FireBulletsInfo_t &info, const Vector &direction )
{
	if ( !UH_BulletTimeActive() || g_bResolvingBtBullet || !pShooter )
		return false;

	CBtBullet *pBullet = dynamic_cast<CBtBullet *>( CreateEntityByName( "btbullet" ) );
	if ( !pBullet )
		return false;
	// Like CWaterBullet::Spawn, the original btbullet factory is followed by
	// its parameterized spawn routine rather than generic DispatchSpawn.
	pBullet->SpawnBullet( pShooter, info, direction, !pShooter->IsPlayer() );
	return true;
}

void UH_BulletTimeSpawnTracer( CBaseCombatCharacter *pShooter, const Vector &start,
	const Vector &direction, int ammoType, bool bEnemyBullet )
{
	// Compatibility entry point for special weapon paths. Normal bullets are
	// deferred centrally from CBaseEntity::FireBullets so NPC and player shots
	// use the same CBtBullet lifecycle.
	if ( !pShooter ) return;
	FireBulletsInfo_t info;
	info.m_iShots = 1;
	info.m_vecSrc = start;
	info.m_vecDirShooting = direction;
	info.m_vecSpread = vec3_origin;
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = ammoType;
	UH_BulletTimeDeferShot( pShooter, info, direction );
}

static void UH_BulletTimeChanged( IConVar *pVar, const char *pOldValue, float flOldValue )
{
	ConVar *pScale = cvar->FindVar( "host_timescale" );
	ConVar *pNormalSpeed = cvar->FindVar( "hl2_normspeed" );
	if ( pScale ) pScale->SetValue( bt_enabled.GetBool() ? bt_timescale.GetFloat() : 1.0f );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2_Player *pPlayer = dynamic_cast< CHL2_Player * >( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer ) continue;
		pPlayer->SetMaxSpeed( bt_enabled.GetBool() ? bt_plr_speed.GetFloat() : ( pNormalSpeed ? pNormalSpeed->GetFloat() : 150.0f ) );
		engine->ClientCommand( pPlayer->edict(), bt_enabled.GetBool() ? "r_screenoverlay dev/bullettime" : "r_screenoverlay off" );
	}
}

void UH_BulletTimePlayerDied( CBasePlayer *pPlayer )
{
	if ( bt_enabled.GetBool() ) bt_enabled.SetValue( 0 );
}

void CHL2_Player::InputEnableBt( inputdata_t &inputdata )
{
	// Original EnableBT only clears the per-player gate. The map starts BT one
	// second later with alias "bt" -> impulse 110.
	m_bBulletTimeDisabled = false;
}

void CHL2_Player::InputDisableBt( inputdata_t &inputdata )
{
	m_bBulletTimeDisabled = true;
	if ( UH_BulletTimeActive() )
	{
		StopSound( "Player.bullettimeloop" );
		EmitSound( "Player.bullettimeend" );
		SetContextThink( &CHL2_Player::UH_EndBulletTimeThink, gpGlobals->curtime + 1.0f, "BulletTimeEndContext" );
	}
}

void CHL2_Player::UH_EndBulletTimeThink( void )
{
	if ( UH_BulletTimeActive() ) UH_SetBulletTime( false );
}
