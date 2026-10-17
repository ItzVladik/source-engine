//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell bleeding HUD element — implementation.
//
// Re-implementation of CHudBleeding (sub_100BE800): a red blood-drop icon
// whose alpha tracks m_iBleedCounter * 2.55 (more severe bleeding = more
// opaque), hidden while the player is not bleeding.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_bleeding.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudBleeding );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudBleeding::CHudBleeding( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudBleeding" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	m_iBleedCounter = 0;
	m_iBloodTexture = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Init / reset
//-----------------------------------------------------------------------------
void CHudBleeding::Init( void )
{
	m_iBleedCounter = 0;
}

void CHudBleeding::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Only draw while bleeding.
//-----------------------------------------------------------------------------
bool CHudBleeding::ShouldDraw( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	return ( pPlayer->m_iBleedCounter > 0 ) && CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Cache the bleed counter for the paint.
//-----------------------------------------------------------------------------
void CHudBleeding::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	m_iBleedCounter = max( 0, pPlayer->m_iBleedCounter );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the blood drop, tinted red with alpha = bleedCounter * 2.55.
//-----------------------------------------------------------------------------
void CHudBleeding::Paint()
{
	if ( m_iBloodTexture < 0 )
	{
		m_iBloodTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_iBloodTexture, "sprites/hud/hud_blooddrop", 1, false );
	}

	int iAlpha = clamp( (int)( m_iBleedCounter * 2.55f ), 0, 255 );

	vgui::surface()->DrawSetColor( 255, 0, 0, iAlpha );
	vgui::surface()->DrawSetTexture( m_iBloodTexture );
	vgui::surface()->DrawTexturedRect(
		(int)m_flBloodX, (int)m_flBloodY,
		(int)( m_flBloodX + m_flBloodWide ), (int)( m_flBloodY + m_flBloodTall ) );
}
