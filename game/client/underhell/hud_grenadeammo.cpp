// Underhell standalone grenade-ammo HUD (original CHudGrenadeAmmo).
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "c_basehlplayer.h"
#include "ammodef.h"
#include "iclientmode.h"
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>

#include "tier0/memdbgon.h"

class CHudGrenadeAmmo : public CHudNumericDisplay, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudGrenadeAmmo, CHudNumericDisplay );
public:
	CHudGrenadeAmmo( const char *name ) : BaseClass( NULL, "HudGrenadeAmmo" ), CHudElement( name )
	{
		m_iAmmo = -1;
		m_iTexture = -1;
		SetHiddenBits( HIDEHUD_WEAPONSELECTION | HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
	}
	void Init() { m_iAmmo = -1; SetAlpha( 255 ); }
	void Reset() { m_iAmmo = -1; SetDisplayValue( 0 ); SetAlpha( 255 ); }
	void OnThink()
	{
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		if ( !player ) return;
		int ammoType = GetAmmoDef()->Index( "grenade" );
		int ammo = ammoType >= 0 ? player->GetAmmoCount( ammoType ) : 0;
		if ( ammo != m_iAmmo )
		{
			const char *sequence = ammo <= 0 ? "GrenadeEmpty" :
				m_iAmmo >= 0 && ammo < m_iAmmo ? "GrenadeDecreased" : "GrenadeIncreased";
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( sequence );
			m_iAmmo = ammo;
			SetDisplayValue( ammo );
		}
	}
	bool ShouldDraw()
	{
		// Do not gate the first OnThink behind m_iAmmo: hidden VGUI panels are
		// not guaranteed to think, leaving the constructor value -1 forever.
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		int ammoType = GetAmmoDef()->Index( "grenade" );
		int ammo = ( player && ammoType >= 0 ) ? player->GetAmmoCount( ammoType ) : 0;
		if ( ammo != m_iAmmo )
		{
			m_iAmmo = ammo;
			SetDisplayValue( ammo );
		}
		return ammo > 0 && CHudElement::ShouldDraw();
	}
	void Paint()
	{
		BaseClass::Paint();
		if ( m_iTexture < 0 )
		{
			m_iTexture = vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile( m_iTexture, "sprites/hud/weapons/frag", 1, false );
		}
		vgui::surface()->DrawSetTexture( m_iTexture );
		// Exact packed colour in original Paint sub_101ADB40: 0xfffc7f02.
		vgui::surface()->DrawSetColor( 2, 127, 252, 255 );
		// contourwide/contourtall are right/bottom coordinates, not dimensions.
		vgui::surface()->DrawTexturedRect( (int)m_fContourX, (int)m_fContourY,
			(int)m_fContourWide, (int)m_fContourTall );
	}
private:
	CPanelAnimationVarAliasType( float, m_fContourX, "contourx", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fContourY, "contoury", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fContourTall, "contourtall", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_fContourWide, "contourwide", "32", "proportional_float" );
	int m_iAmmo;
	int m_iTexture;
};

DECLARE_HUDELEMENT( CHudGrenadeAmmo );
