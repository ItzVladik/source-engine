//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell endurance ("hunger") HUD element — the vertical gauge on
//          the left edge of the screen.
//
// The original CHudEndurance is a modded-vgui panel (sprite "sprites/hud/
// hud_endurance" + a chunked bar), registered as panel "HudEndurance" in
// scripts/HudLayout.res. This is a behavioural re-implementation on the
// OB-era vgui: a vertical chunked bar driven by C_BaseHLPlayer::m_iEndurance.
//
// Colour comes from the panel's FgColor, which scripts/HudAnimations.txt
// animates between the scheme's FgColor (blue), "230 230 50" (yellow) and
// "DamagedFg" (red) via the EnduranceLow/Medium/High events — exactly like
// the original (sub_100C8710 fires those events from OnThink).
//
// $NoKeywords: $
//=============================================================================//

#if !defined( HUD_ENDURANCE_H )
#define HUD_ENDURANCE_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudEndurance : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudEndurance, vgui::Panel );

public:
	CHudEndurance( const char *pElementName );
	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	OnThink( void );
	bool			ShouldDraw( void );

protected:
	virtual void	Paint();

private:
	// Original bar geometry (scripts/HudLayout.res "HudEndurance"):
	//   BarInsetX 7, BarInsetY 104, BarWidth 4, BarHeight 84,
	//   BarChunkHeight 1, BarChunkGap 0  -> a thin vertical gauge that fills
	//   bottom-up (BarInsetY is the bottom edge of the full bar).
	CPanelAnimationVarAliasType( float, m_flBarInsetX, "BarInsetX", "7", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarInsetY, "BarInsetY", "104", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarWidth, "BarWidth", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarHeight, "BarHeight", "84", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkHeight, "BarChunkHeight", "1", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkGap, "BarChunkGap", "0", "proportional_float" );

	// Exhausted portion is drawn with the foreground colour at this alpha.
	CPanelAnimationVar( int, m_iBarDisabledAlpha, "BarDisabledAlpha", "20" );

	// Icon sprite (scripts/HudLayout.res "HudEndurance": iconx 2, icony 116,
	// iconwide 16, icontall 134 — the vertical gauge outline behind the bar).
	CPanelAnimationVarAliasType( float, m_fIconX, "iconx", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fIconY, "icony", "116", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fIconWide, "iconwide", "16", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fIconTall, "icontall", "134", "proportional_float" );

	float	m_flEndurance;
	int		m_iIconTexture;
};

#endif // HUD_ENDURANCE_H
