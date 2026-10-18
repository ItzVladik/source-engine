#include "cbase.h"
#include "hl2_player.h"
#include "physics_prop_ragdoll.h"

#include "tier0/memdbgon.h"

extern ConVar uh_bodymousedamper;
extern ConVar hl2_normspeed;

static ConVar *UH_SensitivityVar()
{
	return cvar ? cvar->FindVar( "sensitivity" ) : NULL;
}

void CHL2_Player::UH_BeginCarryRagdoll( CBaseEntity *pRagdoll )
{
	if ( !pRagdoll || m_hCarryingRagdoll.Get() == pRagdoll ) return;

	m_hCarryingRagdoll = pRagdoll;
	ConVar *pSensitivity = UH_SensitivityVar();
	m_fSavedSensitivity = pSensitivity ? pSensitivity->GetFloat() : 0.0f;

	// The original direct field at CRagdollProp+1132 is ragdoll.listCount,
	// not root-object mass. Full bodies (>10 elements) are heavy; small severed
	// limb ragdolls remain unrestricted.
	CRagdollProp *pProp = dynamic_cast<CRagdollProp *>( pRagdoll );
	if ( pProp && pProp->GetRagdoll()->listCount > 10 )
	{
		// sub_102E1350 uses hl2_normspeed/3, not the player's current
		// (possibly sprinting) max speed.
		SetMaxSpeed( hl2_normspeed.GetFloat() / 3.0f );
		if ( pSensitivity )
		{
			float flDamper = max( uh_bodymousedamper.GetFloat(), 0.001f );
			pSensitivity->SetValue( m_fSavedSensitivity / flDamper );
		}
	}
}

void CHL2_Player::UH_UpdateCarryRagdollWeight( void )
{
	CBaseEntity *pRagdoll = m_hCarryingRagdoll.Get();
	if ( pRagdoll && IsHoldingEntity( pRagdoll ) )
	{
		CRagdollProp *pProp = dynamic_cast<CRagdollProp *>( pRagdoll );
		if ( pProp && pProp->GetRagdoll()->listCount > 10 )
			SetMaxSpeed( hl2_normspeed.GetFloat() / 3.0f );
		return;
	}
	if ( !pRagdoll && m_fSavedSensitivity == 0.0f ) return;

	// Pickup-controller shutdown in the original restores both values.
	SetMaxSpeed( hl2_normspeed.GetFloat() );
	ConVar *pSensitivity = UH_SensitivityVar();
	if ( pSensitivity && m_fSavedSensitivity > 0.0f )
		pSensitivity->SetValue( m_fSavedSensitivity );
	m_hCarryingRagdoll = NULL;
	m_fSavedSensitivity = 0.0f;
}
