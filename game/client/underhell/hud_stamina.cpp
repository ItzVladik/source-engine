//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell stamina HUD element — implementation.
//
// Behavioural re-implementation of the original CHudStamina (sub_100CAC10):
//   * suit power < 35  -> "StaminaLow"    (bar turns red via HudAnimations)
//   * suit power >= 35 -> "StaminaNormal" (bar is the scheme FgColor, blue)
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_stamina.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudStamina );

#define STAMINA_INIT -1

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudStamina::CHudStamina( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudStamina" )
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
void CHudStamina::Init( void )
{
	m_flStamina = STAMINA_INIT;
	m_iIconTexture = -1;
}

void CHudStamina::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: The stamina bar is a persistent element: draw whenever the HUD
// itself is not hidden.
//-----------------------------------------------------------------------------
bool CHudStamina::ShouldDraw( void )
{
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Track the stamina value and fire the Low/Normal events.
// Original sub_100CAC10 fires the animation sequence every think.
//-----------------------------------------------------------------------------
void CHudStamina::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	float flSuitPower = pPlayer->m_HL2Local.m_flSuitPower;
	float flCurrent = ( flSuitPower <= 0.0f ) ? 0.0f : flSuitPower;

	const char *pszSequence = NULL;
	if ( flSuitPower >= 35.0f )
	{
		pszSequence = "StaminaNormal";
	}
	else if ( flSuitPower > 0.0f )
	{
		pszSequence = "StaminaLow";
	}
	// <= 0: no sequence (original skips the animation at 0).

	if ( pszSequence )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( pszSequence );
	}

	if ( m_flStamina != flCurrent )
	{
		m_flStamina = flCurrent;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the horizontal stamina bar, tinted by the (animated) FgColor.
//-----------------------------------------------------------------------------
void CHudStamina::Paint()
{
	// Icon sprite (gauge outline art) behind the bar, matching the original
	// which precaches "sprites/hud/hud_stamina" in its ctor.
	if ( m_iIconTexture < 0 )
	{
		m_iIconTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iIconTexture, "sprites/hud/hud_stamina", 1, false );
	}

	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTexture( m_iIconTexture );
	// Original sub_100CACD0 passes iconwide/icontall as the right/bottom
	// coordinates directly; they are not width/height values.
	vgui::surface()->DrawTexturedRect(
		(int)m_fIconX, (int)m_fIconY,
		(int)m_fIconWide, (int)m_fIconTall );

	float flChunkStep = m_flBarChunkWidth + m_flBarChunkGap;
	int chunkCount = ( flChunkStep > 0.0f ) ? (int)( m_flBarWidth / flChunkStep ) : 0;
	int enabledChunks = (int)( (float)chunkCount * ( m_flStamina / 100.0f ) + 0.5f );

	// Filled portion (left-to-right).
	vgui::surface()->DrawSetColor( GetFgColor() );
	for ( int i = 0; i < enabledChunks; i++ )
	{
		int xLeft = (int)( m_flBarInsetX + i * flChunkStep );
		vgui::surface()->DrawFilledRect(
			xLeft, (int)m_flBarInsetY,
			xLeft + (int)m_flBarChunkWidth, (int)( m_flBarInsetY + m_flBarHeight ) );
	}

	// Exhausted portion.
	Color col = GetFgColor();
	col[3] = m_iBarDisabledAlpha;
	vgui::surface()->DrawSetColor( col );
	for ( int i = enabledChunks; i < chunkCount; i++ )
	{
		int xLeft = (int)( m_flBarInsetX + i * flChunkStep );
		vgui::surface()->DrawFilledRect(
			xLeft, (int)m_flBarInsetY,
			xLeft + (int)m_flBarChunkWidth, (int)( m_flBarInsetY + m_flBarHeight ) );
	}
}
