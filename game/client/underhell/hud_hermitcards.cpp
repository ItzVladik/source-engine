//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell hermit-cards HUD element — implementation.
//
// Re-implementation of CHudUHHermitCards (think sub_100BCFA0, paint
// sub_100BD080):
//   * shows the hud_hermitcards contour + "<N>/52" (collected cards),
//   * shows "   <cur>/<total>" quest progress while a quest is active,
//   * appears when the count / display flag changes, fades out ~3s later.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_hermitcards.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudUHHermitCards );

// How long the panel stays lit after the last change (original: 3.0 s).
#define UH_HERMIT_FADE_TIME 3.0f

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudUHHermitCards::CHudUHHermitCards( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudUHHermitCards" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	m_iCardsCount = -1;
	m_iQuestCurrent = 0;
	m_iQuestTotal = 0;
	m_bDisplayCard = false;
	m_iContourTexture = -1;
	m_iAlpha = 0;
	m_flLastChangeTime = -100.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Init / reset
//-----------------------------------------------------------------------------
void CHudUHHermitCards::Init( void )
{
	m_iCardsCount = -1;
	m_iAlpha = 0;
	m_flLastChangeTime = -100.0f;
}

void CHudUHHermitCards::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Only draw once a card has been collected or the fade is in flight.
//-----------------------------------------------------------------------------
bool CHudUHHermitCards::ShouldDraw( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	return ( pPlayer->m_bDisplayHermitCard || m_iAlpha > 0 ) && CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Track the card / quest counts. Fade in on change, fade out after
// UH_HERMIT_FADE_TIME seconds of stability.
//-----------------------------------------------------------------------------
void CHudUHHermitCards::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int iCards = pPlayer->m_iUHHermitCardsCount;
	bool bDisplay = pPlayer->m_bDisplayHermitCard;

	if ( m_iCardsCount == -1 )
	{
		// First think: cache, do not light up yet.
		m_iCardsCount = iCards;
		m_bDisplayCard = bDisplay;
		m_iAlpha = 0;
		m_flLastChangeTime = -100.0f;
		return;
	}

	if ( iCards != m_iCardsCount || bDisplay != m_bDisplayCard )
	{
		m_flLastChangeTime = gpGlobals->curtime;
	}

	// sub_100BCFA0 is binary, not a gradual VGUI fade: the panel remains fully
	// opaque for three seconds after a change, then switches straight to alpha 0.
	m_iAlpha = ( gpGlobals->curtime - m_flLastChangeTime < UH_HERMIT_FADE_TIME ) ? 255 : 0;

	m_iCardsCount = iCards;
	m_iQuestCurrent = pPlayer->m_iUHHermitCurrentQuestCount;
	m_iQuestTotal = pPlayer->m_iUHHermitTotalQuestCount;
	m_bDisplayCard = bDisplay;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the card deck contour, the card count and quest progress.
//-----------------------------------------------------------------------------
void CHudUHHermitCards::Paint()
{
	if ( m_iAlpha <= 0 )
		return;

	if ( m_iContourTexture < 0 )
	{
		m_iContourTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iContourTexture, "sprites/hud/hud_hermitcards", 1, false );
	}

	vgui::surface()->DrawSetColor( 255, 255, 255, m_iAlpha );

	// Card deck outline.
	vgui::surface()->DrawSetTexture( m_iContourTexture );
	vgui::surface()->DrawTexturedRect(
		(int)m_flContourX, (int)m_flContourY,
		(int)( m_flContourX + m_flContourWide ), (int)( m_flContourY + m_flContourTall ) );

	// Collected cards: "<N>/52".
	wchar_t szText[32];
	swprintf( szText, L"%i/52", m_iCardsCount );

	vgui::surface()->DrawSetTextFont( m_hNumberFont );
	vgui::surface()->DrawSetTextColor( 255, 255, 255, m_iAlpha );
	vgui::surface()->DrawSetTextPos( (int)m_flText1X, (int)m_flText1Y );
	vgui::surface()->DrawUnicodeString( szText );

	// Quest progress (only while a quest is active).
	if ( m_iQuestTotal > 0 )
	{
		swprintf( szText, L"   %i/%i", m_iQuestCurrent, m_iQuestTotal );
		vgui::surface()->DrawSetTextPos( (int)m_flText2X, (int)m_flText2Y );
		vgui::surface()->DrawUnicodeString( szText );
	}
}
