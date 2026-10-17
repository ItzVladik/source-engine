//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell flashlight battery HUD element — the battery gauge on the
//          left edge, showing the remaining charge and the battery count.
//
// Original CHudUHBattery (panel "HudUHBattery" in scripts/HudLayout.res):
//   * draws the "sprites/hud/hud_battery_contour" outline,
//   * draws a vertical chunked charge bar (HullColor "2 127 252 192"),
//   * prints the discrete battery count as "x<N>" (sub_100BDC80).
//
// $NoKeywords: $
//=============================================================================//

#if !defined( HUD_UHBATTERY_H )
#define HUD_UHBATTERY_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudUHBattery : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudUHBattery, vgui::Panel );

public:
	CHudUHBattery( const char *pElementName );
	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	OnThink( void );
	bool			ShouldDraw( void );

protected:
	virtual void	Paint();

private:
	// Original bar geometry (scripts/HudLayout.res "HudUHBattery").
	CPanelAnimationVar( Color, m_HullColor, "HullColor", "2 127 252 192" );
	CPanelAnimationVar( int, m_iHullDisabledAlpha, "HullDisabledAlpha", "0" );

	CPanelAnimationVarAliasType( float, m_flBarInsetX, "BarInsetX", "6", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarInsetY, "BarInsetY", "31", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarWidth, "BarWidth", "14", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarHeight, "BarHeight", "23", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkHeight, "BarChunkHeight", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkGap, "BarChunkGap", "1", "proportional_float" );

	// Contour sprite (battery outline).
	CPanelAnimationVarAliasType( float, m_flContourX, "contourx", "1", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flContourY, "contoury", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flContourWide, "contourwide", "24", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flContourTall, "contourtall", "42", "proportional_float" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudNumbers" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HUDBarText" );

	int		m_iBatteryCount;	// cached m_iUHBatteryCount
	int		m_iContourTexture;
	float	m_flLastCharge;	// cached m_HL2Local.m_flFlashBattery
	bool	m_bActive;		// flashlight/night vision/count-change visibility trigger
};

#endif // HUD_UHBATTERY_H
