//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell hermit-cards HUD element — the card-deck icon + collected
//          count on the right edge of the screen.
//
// Original CHudUHHermitCards (panel "HudUHHermitCards" in scripts/
// HudLayout.res): draws the "sprites/hud/hud_hermitcards" contour, the number
// of collected cards ("<N>/52"), and — while a quest is active — the quest
// progress ("<cur>/<total>"). It only appears once the first card is collected
// (m_bDisplayHermitCard flips true), then fades out ~3s after the last change.
//
// $NoKeywords: $
//=============================================================================//

#if !defined( HUD_HERMITCARDS_H )
#define HUD_HERMITCARDS_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudUHHermitCards : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudUHHermitCards, vgui::Panel );

public:
	CHudUHHermitCards( const char *pElementName );
	virtual void	Init( void );
	virtual void	Reset( void );
	virtual void	OnThink( void );
	bool			ShouldDraw( void );

protected:
	virtual void	Paint();

private:
	// Original contour geometry (scripts/HudLayout.res "HudUHHermitCards").
	CPanelAnimationVarAliasType( float, m_flContourX, "contourx", "34", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flContourY, "contoury", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flContourWide, "contourwide", "78", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flContourTall, "contourtall", "54", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flText1X, "text1x", "78", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flText1Y, "text1y", "30", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flText2X, "text2x", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flText2Y, "text2y", "30", "proportional_float" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudNumbers" );

	int		m_iCardsCount;		// cached m_iUHHermitCardsCount
	int		m_iQuestCurrent;	// cached m_iUHHermitCurrentQuestCount
	int		m_iQuestTotal;		// cached m_iUHHermitTotalQuestCount
	bool	m_bDisplayCard;		// cached m_bDisplayHermitCard
	int		m_iContourTexture;
	int		m_iAlpha;			// fade alpha (0 = hidden)
	float	m_flLastChangeTime;
};

#endif // HUD_HERMITCARDS_H
