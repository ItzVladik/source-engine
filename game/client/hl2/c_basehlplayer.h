//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#if !defined( C_BASEHLPLAYER_H )
#define C_BASEHLPLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "c_baseplayer.h"
#include "c_hl2_playerlocaldata.h"
#include "underhell/uh_inventory.h"

class C_BaseHLPlayer : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_BaseHLPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

						C_BaseHLPlayer();

	virtual void		OnDataChanged( DataUpdateType_t updateType );

	void				Weapon_DropPrimary( void );
		
	float				GetFOV();
	void				Zoom( float FOVOffset, float time );
	float				GetZoom( void );
	bool				IsZoomed( void )	{ return m_HL2Local.m_bZooming; }

	bool				IsSprinting( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_SPRINT; }
	bool				IsFlashlightActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_FLASHLIGHT; }
	bool				IsBreatherActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_BREATHER; }

	virtual int			DrawModel( int flags );
	virtual	void		BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	LadderMove_t		*GetLadderMove() { return &m_HL2Local.m_LadderMove; }
	virtual void		ExitLadder();
	bool				IsSprinting() const { return m_fIsSprinting; }
	
	// Input handling
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	void			PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );
	void			PerformClientSideNPCSpeedModifiers( float flFrameTime, CUserCmd *pCmd );

	bool				IsWeaponLowered( void ) { return m_HL2Local.m_bWeaponLowered; }

public:

	int							m_iInventory[UH_INVENTORY_SLOTS];

	C_HL2PlayerLocalData		m_HL2Local;
	EHANDLE				m_hClosestNPC;
	float				m_flSpeedModTime;
	bool				m_fIsSprinting;

	bool						m_bShoulderFlashlight;
	bool						m_bFlashlightOn;
	bool						m_bDisplayHermitCard;
	bool						m_bInventoryEnabled;
	int							m_iUHBatteryCount;
	int							m_iUHHermitCardsCount;
	int							m_iUHHermitCurrentQuestCount;
	int							m_iUHHermitTotalQuestCount;
	int							m_iEndurance;
	int							m_iBleedCounter;
	float						m_flUHBatteryCharge;
	bool						m_bIronSighted;
	float						m_fIronsightedTime;
	float						m_flUseReticleTime;
	bool						m_bHavePistolSilencer;
	bool						m_bHaveRifleSilencer;
	bool						m_bLaserToggleState;
	bool						m_bKickMarker;
	bool						m_bNightVisionOn;
	bool						m_bGasMaskOn;
	bool						m_bLeftArmDeployed;
	bool						m_bHoldingFlare;

private:
	C_BaseHLPlayer( const C_BaseHLPlayer & ); // not defined, not accessible
	
	bool				TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	float				m_flZoomStart;
	float				m_flZoomEnd;
	float				m_flZoomRate;
	float				m_flZoomStartTime;

	bool				m_bPlayUseDenySound;		// Signaled by PlayerUse, but can be unset by HL2 ladder code...
	float				m_flSpeedMod;
	float				m_flExitSpeedMod;


friend class CHL2GameMovement;
};


#endif
