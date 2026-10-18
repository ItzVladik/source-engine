//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell bleeding HUD element — a blood-drop icon shown while the
//          player is bleeding, its opacity scaling with the bleed severity.
//
// Original CHudBleeding (panel "HudBleeding" in scripts/HudLayout.res) draws
// the "sprites/hud/hud_blooddrop" sprite tinted red, alpha =
// m_iBleedCounter * 2.55 (sub_100BE800).
//
// $NoKeywords: $
//=============================================================================//

#if !defined( HUD_BLEEDING_H )
#define HUD_BLEEDING_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudBleeding : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBleeding, vgui::Panel );

public:
	CHudBleeding( const char *pElementName );
	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	OnThink( void );
	bool			ShouldDraw( void );

protected:
	virtual void	Paint();

private:
	// Original blood-drop sprite placement (scripts/HudLayout.res "HudBleeding").
	CPanelAnimationVarAliasType( float, m_flBloodX, "bloodx", "1", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBloodY, "bloody", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBloodWide, "bloodwide", "24", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBloodTall, "bloodtall", "42", "proportional_float" );

	int		m_iBleedCounter;	// cached m_iBleedCounter
	int		m_iBloodTexture;
};

#endif // HUD_BLEEDING_H
