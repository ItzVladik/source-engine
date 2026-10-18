//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell flashlight battery HUD element — implementation.
//
// Behavioural re-implementation of CHudUHBattery (sub_100BDF90 think,
// sub_100BDC80 paint): a contour outline + a vertical charge bar + the
// discrete battery count ("x<N>"). The original also fades the gauge out when
// it is stable; the count and the flashlight/nightvision toggles bring it
// back to full alpha.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_uhbattery.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudUHBattery );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudUHBattery::CHudUHBattery( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudUHBattery" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	// Original hidden mask is 16432 = player-dead | need-suit | custom bit 14.
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT | ( 1 << 14 ) );

	m_iBatteryCount = -1;
	m_iContourTexture = -1;
	m_flLastCharge = 0.0f;
	m_bActive = false;
	SetAlpha( 128 );
}

//-----------------------------------------------------------------------------
// Purpose: Init / reset
//-----------------------------------------------------------------------------
void CHudUHBattery::Init( void )
{
	m_iBatteryCount = -1;
	m_flLastCharge = 0.0f;
	m_bActive = false;
	SetAlpha( 128 );
}

void CHudUHBattery::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Persistent element; drawn whenever the HUD itself is not hidden.
//-----------------------------------------------------------------------------
bool CHudUHBattery::ShouldDraw( void )
{
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Track the battery count. When it (or the flashlight / nightvision)
// changes, bring the gauge to full alpha; otherwise fade it out.
//-----------------------------------------------------------------------------
void CHudUHBattery::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	m_bActive = pPlayer->m_bFlashlightOn || pPlayer->m_bNightVisionOn ||
		pPlayer->m_iUHBatteryCount != m_iBatteryCount;
	if ( m_bActive )
	{
		SetAlpha( 255 );
	}
	else
	{
		// SetAlpha takes an integer: original GetAlpha()-0.1 therefore drops by
		// one alpha unit per think after truncation.
		SetAlpha( clamp( (int)( GetAlpha() - 0.1f ), 0, 255 ) );
	}

	m_iBatteryCount = pPlayer->m_iUHBatteryCount;
	const float flCharge = max( pPlayer->m_HL2Local.m_flFlashBattery, 0.0f );
	if ( m_flLastCharge != flCharge )
	{
		m_flLastCharge = flCharge;
		InvalidateLayout();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the battery outline, the charge bar and the count.
//-----------------------------------------------------------------------------
void CHudUHBattery::Paint()
{
	// The original drives panel alpha. Do not leave disabled chunks visible
	// after the contour/text have faded away.
	const int iPanelAlpha = GetAlpha();
	if ( !m_bActive && iPanelAlpha <= 0 )
		return;

	// Charge bar: discrete chunks filling bottom-up from m_HL2Local.m_flFlashBattery
	// (0..100), exactly like the original sub_100BDC80 — chunkCount =
	// BarHeight / (BarChunkHeight + BarChunkGap), enabledChunks =
	// round(charge/100 * chunkCount), filled chunks at the bottom, exhausted
	// above them.
	float flCharge = clamp( m_flLastCharge, 0.0f, 100.0f );

	float flChunkStep = m_flBarChunkHeight + m_flBarChunkGap;
	int chunkCount = ( flChunkStep > 0.0f ) ? (int)( m_flBarHeight / flChunkStep ) : 0;
	int enabledChunks = (int)( (float)chunkCount * ( flCharge / 100.0f ) + 0.5f );

	// Filled chunks (bottom-up).
	vgui::surface()->DrawSetColor( m_HullColor[0], m_HullColor[1], m_HullColor[2], m_HullColor[3] );
	int y = (int)m_flBarInsetY;
	for ( int i = 0; i < enabledChunks; i++ )
	{
		vgui::surface()->DrawFilledRect(
			(int)m_flBarInsetX, y,
			(int)( m_flBarInsetX + m_flBarWidth ), y + (int)m_flBarChunkHeight );
		y -= (int)flChunkStep;
	}

	// Exhausted chunks (drawn with the disabled alpha).
	vgui::surface()->DrawSetColor( m_HullColor[0], m_HullColor[1], m_HullColor[2],
		m_iHullDisabledAlpha );
	for ( int i = enabledChunks; i < chunkCount; i++ )
	{
		vgui::surface()->DrawFilledRect(
			(int)m_flBarInsetX, y,
			(int)( m_flBarInsetX + m_flBarWidth ), y + (int)m_flBarChunkHeight );
		y -= (int)flChunkStep;
	}

	// Contour outline (drawn ON TOP of the bar, matching sub_100BDC80 which
	// draws the chunked bar first and the hud_battery_contour texture last).
	if ( m_iContourTexture < 0 )
	{
		m_iContourTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iContourTexture, "sprites/hud/hud_battery_contour", 1, false );
	}

	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTexture( m_iContourTexture );
	vgui::surface()->DrawTexturedRect(
		(int)m_flContourX, (int)m_flContourY,
		(int)( m_flContourX + m_flContourWide ), (int)( m_flContourY + m_flContourTall ) );

	// Battery count: original prints "   x<N>" (3 leading spaces, which push the
	// digits right) at text position (0,0) — the panel's top-left. Match that.
	wchar_t szText[16];
	swprintf( szText, L"   x%i", m_iBatteryCount );

	vgui::surface()->DrawSetTextFont( m_hNumberFont );
	vgui::surface()->DrawSetTextColor( m_HullColor[0], m_HullColor[1], m_HullColor[2], m_HullColor[3] );
	vgui::surface()->DrawSetTextPos( 0, 0 );
	vgui::surface()->DrawUnicodeString( szText );
}
