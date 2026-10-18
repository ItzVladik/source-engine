//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell stamina HUD element — the horizontal bar along the bottom
//          left, replacing the vanilla HL2 suit-power bar (CHudSuitPower).
//
// The original CHudStamina (panel "HudStamina" in scripts/HudLayout.res)
// shows the sprint stamina (suit power). Colour is the panel FgColor, animated
// by scripts/HudAnimations.txt (StaminaLow -> red, StaminaNormal -> scheme
// FgColor) — same scheme as CHudEndurance.
//
// $NoKeywords: $
//=============================================================================//

#if !defined( HUD_STAMINA_H )
#define HUD_STAMINA_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudStamina : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudStamina, vgui::Panel );

public:
	CHudStamina( const char *pElementName );
	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	OnThink( void );
	bool			ShouldDraw( void );

protected:
	virtual void	Paint();

private:
	// Original bar geometry (scripts/HudLayout.res "HudStamina"):
	//   BarInsetX 26, BarInsetY 7, BarWidth 210, BarHeight 4,
	//   BarChunkWidth 1, BarChunkGap 0  -> a thin horizontal bar.
	CPanelAnimationVarAliasType( float, m_flBarInsetX, "BarInsetX", "26", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarInsetY, "BarInsetY", "7", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarWidth, "BarWidth", "210", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarHeight, "BarHeight", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkWidth, "BarChunkWidth", "1", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkGap, "BarChunkGap", "0", "proportional_float" );

	CPanelAnimationVar( int, m_iBarDisabledAlpha, "BarDisabledAlpha", "20" );

	// Icon sprite (scripts/HudLayout.res "HudStamina": iconx 1, icony -6,
	// iconwide 24, icontall 24 — the gauge outline art drawn behind the bar).
	CPanelAnimationVarAliasType( float, m_fIconX, "iconx", "1", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fIconY, "icony", "-6", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fIconWide, "iconwide", "24", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fIconTall, "icontall", "24", "proportional_float" );

	float	m_flStamina;
	int		m_iIconTexture;
};

#endif // HUD_STAMINA_H
