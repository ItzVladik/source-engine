#ifndef UH_BULLETTIME_H
#define UH_BULLETTIME_H
#ifdef _WIN32
#pragma once
#endif

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class Vector;
struct FireBulletsInfo_t;

bool UH_BulletTimeActive();
void UH_SetBulletTime( bool bEnabled );
void UH_ToggleBulletTime( CBasePlayer *pPlayer );
bool UH_BulletTimeDeferShot( CBaseEntity *pShooter, const FireBulletsInfo_t &info, const Vector &direction );
void UH_BulletTimeSpawnTracer( CBaseCombatCharacter *pShooter, const Vector &start, const Vector &direction, int ammoType, bool bEnemyBullet );
void UH_BulletTimePlayerDied( CBasePlayer *pPlayer );

#endif
