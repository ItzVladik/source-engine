//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell dot reticle HUD element — a tiny centered two-ring marker
//          that lights up while the player uses +use and then fades out.
//
// Original CHudDotReticle (panel "HudDotReticle" in scripts/HudLayout.res,
// class "CHudDotReticle" in the client RTTI dump). Decoded:
//   * constructor sub_100BCC90 — registers the panel + 4 animation vars
//     (dotx/doty/dottall/dotwide), SetAlpha(128), SetHiddenBits(4096).
//   * paint sub_100BC870 — draws the dot with
//     alpha = (3.0 - (curtime - flTriggerTime)) * 85, i.e. full (255) right at
//     the trigger and a linear fade to 0 over 3.0 s; skipped while iron-sighted
//     (m_bIronSighted @4140).
//   * the trigger timestamp is a client-local float at player offset 3456,
//     stamped every CreateMove command containing IN_USE.
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_DOTRETICLE_H
#define HUD_DOTRETICLE_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudDotReticle : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDotReticle, vgui::Panel );

public:
	CHudDotReticle( const char *pElementName );
	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	OnThink( void );
	bool			ShouldDraw( void );

protected:
	virtual void	Paint();

private:
	// Original dot placement (scripts/HudLayout.res "HudDotReticle": xpos c-8,
	// ypos c-8, wide 16, tall 16 — a centered 16x16 panel; dotx/doty place the
	// dot at the center).
	CPanelAnimationVarAliasType( float, m_fdotx, "dotx", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fdoty, "doty", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fdotwide, "dotwide", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fdottall, "dottall", "4", "proportional_float" );

};

#endif // HUD_DOTRETICLE_H
