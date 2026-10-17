//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell endurance ("hunger") HUD element — implementation.
//
// Behavioural re-implementation of the original CHudEndurance (sub_100C8710):
//   * value < 20  -> "EnduranceLow"    (bar turns red via HudAnimations)
//   * value < 50  -> "EnduranceMedium" (bar turns yellow)
//   * value >= 50 -> "EnduranceHigh"   (bar is the scheme FgColor, blue)
// The original fires the animation every think (no early-out on unchanged
// value); the cached value is only used to avoid re-running when the gauge
// does not change.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_endurance.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudEndurance );

#define ENDURANCE_INIT -1

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudEndurance::CHudEndurance( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudEndurance" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	// Hidden with the health cluster, when the player is dead, or when no suit
	// is equipped (the original bars only exist once the suit is on).
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	m_iIconTexture = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Init / reset
//-----------------------------------------------------------------------------
void CHudEndurance::Init( void )
{
	m_flEndurance = ENDURANCE_INIT;
	m_iIconTexture = -1;
}

void CHudEndurance::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: The endurance gauge is a persistent element: draw whenever the HUD
// itself is not hidden.
//-----------------------------------------------------------------------------
bool CHudEndurance::ShouldDraw( void )
{
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Track the endurance value and fire the Low/Medium/High events.
// Original sub_100C8710 fires the animation sequence every think.
//-----------------------------------------------------------------------------
void CHudEndurance::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int iEndurance = pPlayer->m_iEndurance;
	float flCurrent = ( iEndurance <= 0 ) ? 0.0f : (float)iEndurance;

	const char *pszSequence = NULL;
	if ( iEndurance >= 50 )
	{
		pszSequence = "EnduranceHigh";
	}
	else if ( iEndurance >= 20 )
	{
		pszSequence = "EnduranceMedium";
	}
	else if ( iEndurance > 0 )
	{
		pszSequence = "EnduranceLow";
	}
	// iEndurance <= 0: no sequence (original skips the animation at 0).

	if ( pszSequence )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( pszSequence );
	}

	if ( m_flEndurance != flCurrent )
	{
		m_flEndurance = flCurrent;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the vertical endurance gauge. Chunks fill bottom-up and are
// tinted by the (animated) FgColor.
//-----------------------------------------------------------------------------
void CHudEndurance::Paint()
{
	// Icon sprite (vertical gauge outline) behind the bar, matching the
	// original which precaches "sprites/hud/hud_endurance" in its ctor.
	if ( m_iIconTexture < 0 )
	{
		m_iIconTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iIconTexture, "sprites/hud/hud_endurance", 1, false );
	}

	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTexture( m_iIconTexture );
	// Original sub_100C87B0 treats iconwide/icontall as right/bottom panel
	// coordinates. Adding iconx/icony shifted and clipped both HUD icons.
	vgui::surface()->DrawTexturedRect(
		(int)m_fIconX, (int)m_fIconY,
		(int)m_fIconWide, (int)m_fIconTall );

	float flChunkStep = m_flBarChunkHeight + m_flBarChunkGap;
	int chunkCount = ( flChunkStep > 0.0f ) ? (int)( m_flBarHeight / flChunkStep ) : 0;
	int enabledChunks = (int)( (float)chunkCount * ( m_flEndurance / 100.0f ) + 0.5f );

	// Filled portion (bottom-up).
	vgui::surface()->DrawSetColor( GetFgColor() );
	for ( int i = 0; i < enabledChunks; i++ )
	{
		int yBottom = (int)( m_flBarInsetY - i * flChunkStep );
		int yTop = yBottom - (int)m_flBarChunkHeight;
		vgui::surface()->DrawFilledRect(
			(int)m_flBarInsetX, yTop,
			(int)( m_flBarInsetX + m_flBarWidth ), yBottom );
	}

	// Exhausted portion.
	Color col = GetFgColor();
	col[3] = m_iBarDisabledAlpha;
	vgui::surface()->DrawSetColor( col );
	for ( int i = enabledChunks; i < chunkCount; i++ )
	{
		int yBottom = (int)( m_flBarInsetY - i * flChunkStep );
		int yTop = yBottom - (int)m_flBarChunkHeight;
		vgui::surface()->DrawFilledRect(
			(int)m_flBarInsetX, yTop,
			(int)( m_flBarInsetX + m_flBarWidth ), yBottom );
	}
}
