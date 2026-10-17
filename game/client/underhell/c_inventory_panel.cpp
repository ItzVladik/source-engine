//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell inventory UI panel — implementation.
//
// Reconstructed 1:1 from the original client.dll:
//   * CInventoryPanel        — sub_1012EDC0 (ctor), sub_1012E360 (layout),
//                              sub_1012E6C0 (OnThink), sub_1012E2C0 (NewSelection),
//                              sub_1012E590 (OnCommand turnoff/useitem/dropitem)
//   * DragnDropSlot          — sub_101310D0 (ctor + ContextMenu Use/Drop),
//                              sub_10130D00 (LMB=107 NewSelection, RMB=108 menu),
//                              sub_10130DC0 (menu -> useitem/dropitem %i),
//                              sub_10130ED0 (LMB useitem — treated as double-click)
//   * layout                 — Inventory.vtf is 1024x512. PaintBackgroundType 1
//                              stretches Texture1 over the Frame (engine path).
//                              The .res places the Frame at 368,84 — not
//                              centered. Pocket cells are UV-mapped from the
//                              1024x512 art into the live panel size so a
//                              scaled Frame still keeps icons in the pockets.
//                              28 slots fill row-major: 3x8 + 4.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_basehlplayer.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/IInput.h"
#include "vgui_controls/controls.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/Menu.h"
#include "inputsystem/buttoncode.h"
#include "c_inventory_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Per-id slot visuals. Sprite paths and localization tokens are the original
// strings (hexrays sub_1012E6C0). vgui images are rooted at materials/vgui/,
// so the original prefixes "../" to reach materials/Sprites/.
//-----------------------------------------------------------------------------
struct UHInventorySlotInfo_t
{
	const char *pszSprite;
	const char *pszTextToken;
};

#define UH_INV_ITEM( _file )	"../Sprites/Hud/Items/" _file
#define UH_INV_BLANK			"../Sprites/Hud/Inventory/Blank"
#define UH_INV_BG				"../Sprites/Hud/Inventory/Inventory"

static const UHInventorySlotInfo_t s_InventorySlotInfo[UH_INVENTORY_ITEM_TABLE_SIZE] =
{
	{ NULL,								NULL },										// 0
	{ UH_INV_ITEM( "AppleRed" ),		"#UnderHell_Inventory_Apple" },				// 1
	{ UH_INV_ITEM( "AppleGreen" ),		"#UnderHell_Inventory_Apple" },				// 2
	{ UH_INV_ITEM( "Banana" ),			"#UnderHell_Inventory_Banana" },			// 3
	{ UH_INV_ITEM( "Burritos" ),		"#UnderHell_Inventory_Burrito" },			// 4
	{ UH_INV_ITEM( "Sandwich" ),		"#UnderHell_Inventory_Sandwich" },			// 5
	{ UH_INV_ITEM( "Banana" ),			"#UnderHell_Inventory_BananaBunch" },		// 6
	{ UH_INV_ITEM( "Soda1" ),			"#UnderHell_Inventory_Soda" },				// 7
	{ UH_INV_ITEM( "Soda2" ),			"#UnderHell_Inventory_Soda" },				// 8
	{ UH_INV_ITEM( "Soda3" ),			"#UnderHell_Inventory_Soda" },				// 9
	{ UH_INV_ITEM( "Soda4" ),			"#UnderHell_Inventory_Soda" },				// 10
	{ UH_INV_ITEM( "Soda5" ),			"#UnderHell_Inventory_Soda" },				// 11
	{ UH_INV_ITEM( "SodaPowerPunch" ),	"#UnderHell_Inventory_MegaSoda" },			// 12
	{ UH_INV_ITEM( "Flares" ),			"#UnderHell_Inventory_Flare" },				// 13
	{ UH_INV_ITEM( "GlowstickGreen" ),	"#UnderHell_Inventory_Glowstick_Green" },	// 14
	{ UH_INV_ITEM( "GlowstickRed" ),		"#UnderHell_Inventory_Glowstick_Red" },		// 15
	{ UH_INV_ITEM( "GlowstickBlue" ),	"#UnderHell_Inventory_Glowstick_Blue" },	// 16
	{ UH_INV_ITEM( "GlowstickYellow" ),	"#UnderHell_Inventory_Glowstick_Yellow" },	// 17
	{ UH_INV_ITEM( "GlowstickPurple" ),	"#UnderHell_Inventory_Glowstick_Purple" },	// 18
	{ UH_INV_ITEM( "GlowstickGreenLit" ),"#UnderHell_Inventory_Glowstick_Green" },	// 19
	{ UH_INV_ITEM( "GlowstickRedLit" ),	"#UnderHell_Inventory_Glowstick_Red" },		// 20
	{ UH_INV_ITEM( "GlowstickBlueLit" ),	"#UnderHell_Inventory_Glowstick_Blue" },	// 21
	{ UH_INV_ITEM( "GlowstickYellowLit" ),"#UnderHell_Inventory_Glowstick_Yellow" },	// 22
	{ UH_INV_ITEM( "GlowstickPurpleLit" ),"#UnderHell_Inventory_Glowstick_Purple" },	// 23
	{ NULL,								"#UnderHell_Inventory_Painkillers" },		// 24
	{ NULL,								"#UnderHell_Inventory_Syringe" },			// 25
	{ UH_INV_ITEM( "Bandages" ),		"#UnderHell_Inventory_Bandages" },			// 26
	{ UH_INV_ITEM( "HealthKit" ),		"#UnderHell_Inventory_HealthKit" },			// 27
	{ UH_INV_ITEM( "HealthSpray" ),		"#UnderHell_Inventory_HealthVial" },		// 28
	{ UH_INV_ITEM( "Chocobar" ),			"#UnderHell_Inventory_Chocobar" },			// 29
	{ UH_INV_ITEM( "Orange" ),			"#UnderHell_Inventory_Orange" },			// 30
	{ UH_INV_ITEM( "FMRadios" ),			"#UnderHell_Inventory_FMRadio" },			// 31
	{ UH_INV_ITEM( "RadioCrackers" ),	"#UnderHell_Inventory_RadioCracker" },		// 32
};

// Inventory.vtf is 1024x512. The opaque metal plate is centered in that
// canvas at ~(174,87)-(849,424). Pockets are an 8x4 grid, pitch 84.
// Panel::PaintBackground type 1 stretches this whole VTF over GetSize(),
// so slot positions must be mapped through the live panel size — raw VTF
// pixels only match when the Frame is actually 1024x512.
#define UH_TEX_W			1024
#define UH_TEX_H			512
#define UH_GRID_X0			174
#define UH_GRID_Y0			87
#define UH_GRID_PITCH		84
#define UH_SLOT_INSET		3
#define UH_SLOT_COLS		8
#define UH_SLOT_ROWS		4

static int UH_MapTexX( int nTexX, int nPanelW )
{
	return ( nTexX * nPanelW ) / UH_TEX_W;
}

static int UH_MapTexY( int nTexY, int nPanelH )
{
	return ( nTexY * nPanelH ) / UH_TEX_H;
}

//-----------------------------------------------------------------------------
// Singleton.
//-----------------------------------------------------------------------------
static CInventoryPanel *s_pInventoryPanel = NULL;

CInventoryPanel *GetInventoryPanel( void )
{
	if ( !s_pInventoryPanel )
	{
		s_pInventoryPanel = new CInventoryPanel( vgui::surface()->GetEmbeddedPanel() );
	}

	return s_pInventoryPanel;
}

//-----------------------------------------------------------------------------
// Console commands.
//-----------------------------------------------------------------------------
CON_COMMAND( cl_inventoryToggle, "Toggles the inventory." )
{
	engine->ClientCmd( "UpdateInventory" );

	if ( GetInventoryPanel() )
	{
		GetInventoryPanel()->Toggle();
	}
}

CON_COMMAND( UpdateInventory, "Updates the inventory" )
{
	if ( GetInventoryPanel() )
	{
		GetInventoryPanel()->RequestRefresh();
	}
}

//-----------------------------------------------------------------------------
// CInventorySlotPanel
//-----------------------------------------------------------------------------
CInventorySlotPanel::CInventorySlotPanel( vgui::Panel *pParent, const char *pszName, int iSlot )
	: BaseClass( pParent, pszName )
{
	m_iSlot = iSlot;
	m_iItem = 0;
	m_bSelected = false;
	m_pContextMenu = NULL;

	SetShouldScaleImage( true );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( true );

	// Original sub_101310D0: Menu "ContextMenu" with items "Use"/"Drop"
	// commanding "MyUse"/"MyDrop" back to this slot.
	m_pContextMenu = new vgui::Menu( this, "ContextMenu" );
	m_pContextMenu->AddMenuItem( "Use", "Use", "MyUse", this );
	m_pContextMenu->AddMenuItem( "Drop", "Drop", "MyDrop", this );
	m_pContextMenu->SetVisible( false );

	Clear();
}

void CInventorySlotPanel::SetSlotContents( int iItem, const char *pszSprite, const char *pszTextToken )
{
	m_iItem = iItem;

	if ( pszSprite && pszSprite[0] )
	{
		SetImage( pszSprite );
	}
	else
	{
		SetImage( UH_INV_BLANK );
	}

	if ( pszTextToken && pszTextToken[0] )
	{
		GetTooltip()->SetText( pszTextToken );
		GetTooltip()->SetTooltipFormatToMultiLine();
	}
	else
	{
		GetTooltip()->SetText( "" );
		GetTooltip()->HideTooltip();
	}
}

void CInventorySlotPanel::Clear( void )
{
	m_iItem = 0;
	m_bSelected = false;
	SetImage( UH_INV_BLANK );
	GetTooltip()->SetText( "" );
	GetTooltip()->HideTooltip();
}

void CInventorySlotPanel::SetSelected( bool bSelected )
{
	m_bSelected = bSelected;
}

void CInventorySlotPanel::OpenContextMenu( void )
{
	if ( !m_iItem || !m_pContextMenu )
		return;

	vgui::Menu::PlaceContextMenu( this, m_pContextMenu );
}

void CInventorySlotPanel::IssueItemCommand( const char *pszCommand )
{
	char szCmd[32];
	Q_snprintf( szCmd, sizeof( szCmd ), "%s %d", pszCommand, m_iSlot );
	// Original: engine vtable +24 (ClientCmd) with from-client = 1.
	engine->ClientCmd( szCmd );

	CInventoryPanel *pParent = dynamic_cast<CInventoryPanel *>( GetParent() );
	if ( pParent )
	{
		pParent->RequestRefresh();
	}

	GetTooltip()->SetText( "" );
	GetTooltip()->HideTooltip();
}

void CInventorySlotPanel::OnMousePressed( vgui::MouseCode code )
{
	// sub_10130D00: 107 = MOUSE_LEFT -> NewSelection; 108 = MOUSE_RIGHT -> menu.
	if ( !m_iItem )
		return;

	CInventoryPanel *pParent = dynamic_cast<CInventoryPanel *>( GetParent() );

	if ( code == MOUSE_LEFT )
	{
		if ( pParent )
		{
			pParent->SelectSlot( m_iSlot );
		}
		// Original LMB only changes selection. Use/Drop remains on RMB;
		// double-click is the direct-use shortcut.
	}
	else if ( code == MOUSE_RIGHT )
	{
		if ( pParent )
		{
			pParent->SelectSlot( m_iSlot );
		}
		OpenContextMenu();
	}
}

void CInventorySlotPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	// sub_10130ED0: code 107 (MOUSE_LEFT) with a filled slot -> useitem %i.
	if ( code == MOUSE_LEFT && m_iItem )
	{
		IssueItemCommand( "useitem" );
	}
}

void CInventorySlotPanel::OnCommand( const char *command )
{
	// sub_10130DC0: menu item name "Use" / "Drop" -> useitem/dropitem %i.
	if ( !Q_stricmp( command, "MyUse" ) || !Q_stricmp( command, "Use" ) )
	{
		IssueItemCommand( "useitem" );
		return;
	}
	if ( !Q_stricmp( command, "MyDrop" ) || !Q_stricmp( command, "Drop" ) )
	{
		IssueItemCommand( "dropitem" );
		return;
	}

	BaseClass::OnCommand( command );
}

void CInventorySlotPanel::Paint( void )
{
	BaseClass::Paint();

	if ( m_bSelected )
	{
		int w, h;
		GetSize( w, h );
		vgui::surface()->DrawSetColor( 255, 200, 0, 220 );
		vgui::surface()->DrawOutlinedRect( 0, 0, w, h );
	}
}

//-----------------------------------------------------------------------------
// CInventoryPanel
//-----------------------------------------------------------------------------
CInventoryPanel::CInventoryPanel( vgui::VPANEL parent )
	: BaseClass( NULL, "InventoryPanel", true )
{
	SetParent( parent );

	m_iSelectedSlot = -1;
	m_bNeedsRefresh = true;
	m_iLastToggleFrame = -1;
	m_iBgTexture = -1;

	SetVisible( false );
	SetEnabled( true );
	SetSizeable( false );
	SetMoveable( false );
	SetMenuButtonVisible( false );
	SetCloseButtonVisible( false );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	// Frame itself is not proportional (.res 1024x512 / 368,84 are raw
	// pixels). Slots are UV-mapped so they still track Texture1 if the
	// engine later stretches the Frame.
	SetProportional( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 1 );

	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		char szName[32];
		Q_snprintf( szName, sizeof( szName ), "UHImage%d", i + 1 );
		m_pSlots[i] = new CInventorySlotPanel( this, szName, i );
	}

	// Original ctor: LoadSchemeFromFile("resource/SourceScheme.res") + SetScheme.
	vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" );
	if ( hScheme )
	{
		SetScheme( hScheme );
	}

	LoadControlSettings( "resource/UI/InventoryPanel.res" );

	SetTitleBarVisible( false );
	SetMenuButtonVisible( false );
	SetCloseButtonVisible( false );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetSizeable( false );
	SetMoveable( false );
	SetPaintBorderEnabled( false );
	SetBorder( NULL );
	SetPaintBackgroundType( 1 );
	SetZPos( 100 );

	LoadBgTexture();
	// Do not invent a centered position. inventorypanel.res is xpos 368
	// ypos 84 wide 1024 tall 512. Only clamp so a 1280x720 desktop does
	// not clip the right/bottom edge.
	ClampToScreen();
	SetBgColor( Color( 255, 255, 255, 255 ) );

	LayoutSlots();

	SetVisible( false );

	DevMsg( "InventoryPanel has been constructed\n" );
}

CInventoryPanel::~CInventoryPanel( void )
{
	s_pInventoryPanel = NULL;
}

void CInventoryPanel::LoadBgTexture( void )
{
	if ( m_iBgTexture >= 0 )
		return;

	// Same path InventoryPanel.res uses for Texture1. DrawSetTextureFile
	// looks under materials/, so this hits Sprites/Hud/Inventory/Inventory.
	m_iBgTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iBgTexture, "Sprites/Hud/Inventory/Inventory", 1, false );
}

void CInventoryPanel::ClampToScreen( void )
{
	int iScreenW, iScreenH;
	vgui::surface()->GetScreenSize( iScreenW, iScreenH );

	int x, y, w, h;
	GetBounds( x, y, w, h );
	if ( w < 1 )
		w = UH_TEX_W;
	if ( h < 1 )
		h = UH_TEX_H;

	if ( x + w > iScreenW )
		x = iScreenW - w;
	if ( y + h > iScreenH )
		y = iScreenH - h;
	if ( x < 0 )
		x = 0;
	if ( y < 0 )
		y = 0;

	SetPos( x, y );
}

void CInventoryPanel::LayoutSlots( void )
{
	int nWide = GetWide();
	int nTall = GetTall();
	if ( nWide < 1 )
		nWide = UH_TEX_W;
	if ( nTall < 1 )
		nTall = UH_TEX_H;

	// Map each pocket from VTF texels into the rectangle the engine
	// actually draws Texture1 into (0,0)-(nWide,nTall).
	const int nInsetX = UH_MapTexX( UH_SLOT_INSET, nWide );
	const int nInsetY = UH_MapTexY( UH_SLOT_INSET, nTall );

	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		const int iCol = i % UH_SLOT_COLS;
		const int iRow = i / UH_SLOT_COLS;
		const int nTexX = UH_GRID_X0 + iCol * UH_GRID_PITCH;
		const int nTexY = UH_GRID_Y0 + iRow * UH_GRID_PITCH;
		const int iX = UH_MapTexX( nTexX, nWide ) + nInsetX;
		const int iY = UH_MapTexY( nTexY, nTall ) + nInsetY;
		const int iW = max( 1, UH_MapTexX( UH_GRID_PITCH, nWide ) - nInsetX * 2 );
		const int iH = max( 1, UH_MapTexY( UH_GRID_PITCH, nTall ) - nInsetY * 2 );
		m_pSlots[i]->SetShouldScaleImage( true );
		m_pSlots[i]->SetSize( iW, iH );
		m_pSlots[i]->SetPos( iX, iY );
		m_pSlots[i]->SetVisible( true );
	}
}

void CInventoryPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Frame::ApplySchemeSettings installs FrameBorder + scheme BgColor and
	// can flip proportional from the parent. That would tint/inset Texture1
	// and desync slot pixels from the stretched art.
	SetProportional( false );
	SetTitleBarVisible( false );
	SetMenuButtonVisible( false );
	SetCloseButtonVisible( false );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetPaintBorderEnabled( false );
	SetBorder( NULL );
	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 1 );
	SetBgColor( Color( 255, 255, 255, 255 ) );
	LoadBgTexture();
}

bool CInventoryPanel::HasUserConfigSettings( void )
{
	// Frame defaults to true and will rewrite xpos/ypos from user config
	// or MoveToCenterOfScreen. The original panel stays on the .res coords.
	return false;
}

void CInventoryPanel::PerformLayout( void )
{
	BaseClass::PerformLayout();

	// Do not pin 1024x512 here — if the engine scaled the Frame, slots
	// must follow the live size via UV mapping.
	ClampToScreen();
	LayoutSlots();
}

void CInventoryPanel::PaintBackground( void )
{
	// Engine path (Panel::PaintBackground type 1 / DrawTexturedBox):
	// stretch Inventory.vtf over the live panel. Do not go through
	// Frame::PaintBackground — that also paints the title-bar strip.
	int wide, tall;
	GetSize( wide, tall );

	LoadBgTexture();
	if ( m_iBgTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_iBgTexture );
		vgui::surface()->DrawTexturedRect( 0, 0, wide, tall );
	}
}

void CInventoryPanel::OnKeyCodePressed( vgui::KeyCode code )
{
	// Do not Toggle here — OnKeyCodeTyped also fires for the same key,
	// and cl_inventoryToggle may fire too. One I would flip twice.
	BaseClass::OnKeyCodePressed( code );
}

void CInventoryPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	// While this Frame has input focus, the engine's keybinding for
	// cl_inventoryToggle is never reached (vgui eats the key first), so the
	// toggle key has to be handled here too — otherwise a second "I" cannot
	// close the panel. ESC and the toggle key both close it.
	if ( code == KEY_ESCAPE || code == KEY_I )
	{
		if ( IsVisible() )
			Toggle();
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}

void CInventoryPanel::OnClose( void )
{
	SetVisible( false );
}

void CInventoryPanel::OnCommand( const char *command )
{
	// sub_1012E590: "turnoff" hides; "useitem"/"dropitem" fire for this[139].
	if ( !Q_stricmp( command, "turnoff" ) )
	{
		SetVisible( false );
		return;
	}

	if ( !Q_stricmp( command, "useitem" ) || !Q_stricmp( command, "dropitem" ) )
	{
		if ( m_iSelectedSlot >= 0 && m_iSelectedSlot < UH_INVENTORY_SLOTS )
		{
			char szCmd[32];
			Q_snprintf( szCmd, sizeof( szCmd ), "%s %d", command, m_iSelectedSlot );
			engine->ClientCmd( szCmd );
			m_bNeedsRefresh = true;
		}
		return;
	}

	BaseClass::OnCommand( command );
}

void CInventoryPanel::SelectSlot( int iSlot )
{
	// sub_1012E2C0: this[139] = NewSelection value.
	m_iSelectedSlot = iSlot;

	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		m_pSlots[i]->SetSelected( i == iSlot );
	}
}

void CInventoryPanel::OnThink( void )
{
	BaseClass::OnThink();

	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>( C_BasePlayer::GetLocalPlayer() );
	if ( !pPlayer )
	{
		SetVisible( false );
		return;
	}

	// The inventory is NOT gated on the suit — the original OnThink only
	// checks the player is alive and the inventory is enabled (a suit-gated
	// panel would never open before the suit is picked up).
	if ( pPlayer->GetHealth() <= 0 )
	{
		SetVisible( false );
		return;
	}

	if ( !pPlayer->m_bInventoryEnabled )
	{
		SetVisible( false );
		return;
	}

	if ( m_bNeedsRefresh )
	{
		ClearSlots();
		RefreshSlots();
		m_bNeedsRefresh = false;
	}
}

void CInventoryPanel::Toggle( void )
{
	// A single key press can be delivered twice in the same frame — once to
	// the focused Frame (OnKeyCodeTyped) and once to the engine key binding
	// (cl_inventoryToggle). Debounce per frame so one press = one toggle, but
	// do not block legitimate rapid re-toggles (a long time-based window made
	// quick open/close/open presses unreliable).
	if ( m_iLastToggleFrame == gpGlobals->framecount )
	{
		return;
	}
	m_iLastToggleFrame = gpGlobals->framecount;

	if ( IsVisible() )
	{
		SetVisible( false );
	}
	else
	{
		ClearSlots();
		RefreshSlots();
		m_bNeedsRefresh = false;

		ClampToScreen();
		LayoutSlots();

		SetVisible( true );
		MoveToFront();
	}
}

void CInventoryPanel::ClearSlots( void )
{
	m_iSelectedSlot = -1;
	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		m_pSlots[i]->Clear();
	}
}

void CInventoryPanel::RefreshSlots( void )
{
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>( C_BasePlayer::GetLocalPlayer() );
	if ( !pPlayer )
		return;

	for ( int i = 0; i < UH_INVENTORY_SLOTS; ++i )
	{
		int iItem = pPlayer->m_iInventory[i];
		if ( !UH_IsValidInventoryItem( iItem ) )
			continue;

		const UHInventorySlotInfo_t &info = s_InventorySlotInfo[iItem];
		m_pSlots[i]->SetSlotContents( iItem, info.pszSprite, info.pszTextToken );
	}
}

void CInventoryPanel::OnNewSelection( void )
{
}

void CInventoryPanel::OnNewMouseReleased( void )
{
}
