//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell laser sight — client-side beam + impact dot.
//
// CWeaponPistolSocom owns its own env_laserdot lifecycle in the original
// server DLL (create/update on deploy, destroy on holster). The client draws
// the matching beam from the active SOCOM; it is not a player-wide console
// toggle. Sprites match the original laser materials.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_basehlplayer.h"
#include "igamesystem.h"
#include "iviewrender_beams.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Laser beam + dot, redrawn every rendered frame while the local player has
// the laser sight toggled on.
//-----------------------------------------------------------------------------
class CUHLaserSight : public CAutoGameSystemPerFrame
{
	typedef CAutoGameSystemPerFrame BaseClass;

public:
	CUHLaserSight() : BaseClass( "CUHLaserSight" ), m_iBeamModel( -1 ), m_iDotModel( -1 ), m_flNextDraw( 0.0f ) {}

	virtual void LevelInitPostEntity( void )
	{
		// Precache the two sprites (client model indices).
		m_iBeamModel = C_BaseEntity::PrecacheModel( "sprites/laserbeam.vmt" );
		m_iDotModel  = C_BaseEntity::PrecacheModel( "sprites/laserdot.vmt" );
	}

	// Client-side per-frame hook (NOT FrameUpdatePostEntityThink — that is
	// server-only). Called once per rendered frame.
	virtual void Update( float frametime )
	{
		// SOCOM now owns a real server env_laserdot. Do not layer a second
		// client-only beam on top of it; the old fake beam was the source of the
		// incorrect/flickering laser visual.
		return;
	}

private:
	int m_iBeamModel;
	int m_iDotModel;
	float m_flNextDraw;
};

static CUHLaserSight g_UHLaserSight;
