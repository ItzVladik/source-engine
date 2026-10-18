//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell inventory UI panel.
//
// Original: CInventoryPanel : vgui::Frame, 28 vgui::DragnDropSlot children
// (ImageButton : ImagePanel). Layout from hexrays sub_1012E360.
// LMB = NewSelection (sub_10130D00, code 107). RMB = ContextMenu Use/Drop
// (sub_10130D00, code 108 / sub_10130DC0).
// Background is Frame Texture1 (PaintBackgroundType 1): Inventory.vtf is
// stretched to the live panel rect. Slots are UV-mapped into that rect.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UH_C_INVENTORY_PANEL_H
#define UH_C_INVENTORY_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Menu.h"

#include "underhell/uh_inventory.h"

//-----------------------------------------------------------------------------
// Interface exposed by the original panel (client-side inventory UI).
//-----------------------------------------------------------------------------
class IInventoryPanel
{
public:
	virtual ~IInventoryPanel() {}
};

class CInventoryPanel;

//-----------------------------------------------------------------------------
// One inventory slot. Original: vgui::DragnDropSlot : ImageButton : ImagePanel.
// OB SDK has no those classes — ImagePanel + a vgui::Menu is the stand-in.
//-----------------------------------------------------------------------------
class CInventorySlotPanel : public vgui::ImagePanel
{
	DECLARE_CLASS_SIMPLE( CInventorySlotPanel, vgui::ImagePanel );

public:
	CInventorySlotPanel( vgui::Panel *pParent, const char *pszName, int iSlot );

	void SetSlotContents( int iItem, const char *pszSprite, const char *pszTextToken );
	void Clear( void );
	void SetSelected( bool bSelected );

	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void OnCommand( const char *command );
	virtual void Paint( void );

private:
	void OpenContextMenu( void );
	void IssueItemCommand( const char *pszCommand );

	int				m_iSlot;
	int				m_iItem;		// 0 = empty (original this[98])
	bool			m_bSelected;
	vgui::Menu		*m_pContextMenu;	// original this[100], "ContextMenu"
};

//-----------------------------------------------------------------------------
// The inventory frame. Created lazily by GetInventoryPanel().
//-----------------------------------------------------------------------------
class CInventoryPanel : public vgui::Frame, public IInventoryPanel
{
	DECLARE_CLASS_SIMPLE( CInventoryPanel, vgui::Frame );

public:
	CInventoryPanel( vgui::VPANEL parent );
	virtual ~CInventoryPanel( void );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout( void );
	virtual void PaintBackground( void );
	virtual void OnThink( void );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnClose( void );
	virtual void OnCommand( const char *command );
	virtual bool HasUserConfigSettings( void );

	void Toggle( void );
	void RequestRefresh( void )	{ m_bNeedsRefresh = true; }
	void SelectSlot( int iSlot );	// NewSelection (this[139])

private:
	void LayoutSlots( void );
	void ClearSlots( void );
	void RefreshSlots( void );
	void LoadBgTexture( void );
	void ClampToScreen( void );

	MESSAGE_FUNC( OnNewSelection, "NewSelection" );
	MESSAGE_FUNC( OnNewMouseReleased, "NewMouseReleased" );

	CInventorySlotPanel		*m_pSlots[UH_INVENTORY_SLOTS];
	int						m_iBgTexture;
	bool					m_bNeedsRefresh;
	int						m_iSelectedSlot;	// original this[139]
	int						m_iLastToggleFrame;
};

CInventoryPanel *GetInventoryPanel( void );

#endif // UH_C_INVENTORY_PANEL_H
