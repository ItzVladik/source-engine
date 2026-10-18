//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell dot reticle HUD element — implementation.
//
// Behavioural re-implementation of the original CHudDotReticle:
//   * a small centered white ring with a black outer outline,
//   * lights at full alpha when the player presses +use,
//   * fades linearly to zero over 3.0 s (alpha = (3.0 - elapsed) * 85, from
//     the original paint sub_100BC870),
//   * hidden while iron-sighted.
//
// The trigger timestamp is stored on C_BaseHLPlayer and stamped from CreateMove
// whenever the generated command contains IN_USE, matching client+3456.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_dotreticle.h"
#include "underhell/uh_freeaim.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>

void ScreenToWorld( int mousex, int mousey, float fov, const Vector& vecRenderOrigin,
	const QAngle& vecRenderAngles, Vector& vecPickingRay );

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!! 
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudDotReticle );

// Fade window + rate, hard-coded in the original (sub_100BC870): full alpha at
// the trigger, linear to 0 over this many seconds.
#define UH_DOTRETICLE_FADE_TIME 3.0f
#define UH_DOTRETICLE_FADE_RATE 85.0f

// The original SetHiddenBits(4096) = (1<<12) — an Underhell custom hide bit
// beyond the vanilla HIDEHUD_BITCOUNT (12). The mod toggles it via its
// SetStatusVisibility / SetHudVisibility player inputs.
#define HIDEHUD_UH_RETICLE ( 1 << 12 )

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudDotReticle::CHudDotReticle( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudDotReticle" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_UH_RETICLE );
	SetAlpha( 128 );
}

//-----------------------------------------------------------------------------
// Purpose: Init / reset
//-----------------------------------------------------------------------------
void CHudDotReticle::Init( void )
{
}

void CHudDotReticle::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Fully custom-painted: disable the default vgui background/border so
// the .res PaintBackgroundType 2 (grey rounded box) is never drawn. Must run in
// ApplySchemeSettings — LoadControlSettings("scripts/HudLayout.res") re-applies
// the .res AFTER the constructor and would otherwise re-enable the background.
//-----------------------------------------------------------------------------
void CHudDotReticle::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Persistent element; the dot is drawn (and faded) in Paint, which
// early-outs once the fade window has elapsed.
//-----------------------------------------------------------------------------
bool CHudDotReticle::ShouldDraw( void )
{
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Timestamping is performed by C_BaseHLPlayer::CreateMove.
//-----------------------------------------------------------------------------
void CHudDotReticle::OnThink( void )
{
	// Timestamping happens in C_BaseHLPlayer::CreateMove, as in the original.
}

//-----------------------------------------------------------------------------
// Purpose: Draw the fading dot at the center of the panel.
//-----------------------------------------------------------------------------
void CHudDotReticle::Paint()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Original sub_100BC870 also projects the free-aim cursor to a world ray
	// and forwards it to the server each paint pass.
	Vector2D cursor;
	if ( UH_FreeAimGetCursor( cursor ) )
	{
		int wide, tall;
		engine->GetScreenSize( wide, tall );
		if ( wide > 0 && tall > 0 )
		{
			Vector ray;
			ScreenToWorld( (int)( ( cursor.x * 0.25f + 0.5f ) * wide ),
				(int)( ( cursor.y * 0.25f + 0.5f ) * tall ), pPlayer->GetFOV(),
				pPlayer->EyePosition(), pPlayer->EyeAngles(), ray );
			UH_FreeAimClampDirection( pPlayer->EyeAngles(), ray );

			// Original sub_100BC870 sends the normalized ray itself.  Sending a
			// world-space endpoint made server attacks interpret huge coordinates
			// as a direction, which is why the kick could strike the ceiling.
			engine->ClientCmd( VarArgs( "update_freeaim %f %f %f", ray.x, ray.y, ray.z ) );
		}
	}

	// The original skips the dot while iron-sighted (sub_100BC870 checks
	// m_bIronSighted @4140).
	if ( pPlayer->m_bIronSighted )
		return;

	float flElapsed = gpGlobals->curtime - pPlayer->m_flUseReticleTime;
	if ( flElapsed < 0.0f || flElapsed >= UH_DOTRETICLE_FADE_TIME )
		return;

	// Linear fade: 255 right at the trigger, 0 at the end of the window.
	int iAlpha = clamp( (int)( ( UH_DOTRETICLE_FADE_TIME - flElapsed ) * UH_DOTRETICLE_FADE_RATE ), 0, 255 );
	if ( iAlpha <= 0 )
		return;

	// vgui::ISurface vtable+384 is DrawOutlinedCircle (the IAppSystem base adds
	// two slots). The original draws an 8-segment white radius-2 ring followed
	// by a black radius-3 outline at dotx/doty.
	int cx = (int)m_fdotx;
	int cy = (int)m_fdoty;

	vgui::surface()->DrawSetColor( 255, 255, 255, iAlpha );
	vgui::surface()->DrawOutlinedCircle( cx, cy, 2, 8 );

	vgui::surface()->DrawSetColor( 0, 0, 0, iAlpha );
	vgui::surface()->DrawOutlinedCircle( cx, cy, 3, 8 );
}
