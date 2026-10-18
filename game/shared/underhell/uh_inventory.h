//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell inventory — shared definitions.
//          Reconstructed 1:1 from the original client.dll / server.dll
//          (see klaxons1/underhell-hexrays for the decompiled reference).
//
// Compatibility notes:
//  * Item ids are the exact values the original binary stores in
//    CHL2_Player::m_iInventory[ 28 ] (byte offset 4928, 4-byte stride).
//  * The id -> classname / print-name table below mirrors the original
//    per-player table built at byte offset 5236 (34 entries).
//
// $NoKeywords: $
//=============================================================================//

#ifndef UH_INVENTORY_H
#define UH_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "Color.h"

// Number of slots in the player inventory. Hard-coded 28 in the original.
#define UH_INVENTORY_SLOTS 28

// The original holds one entry per id in the item name table (indices 0..32).
#define UH_INVENTORY_ITEM_TABLE_SIZE 33

//-----------------------------------------------------------------------------
// Item ids. Keep the numeric values — they are serialized inside
// m_iInventory slots and must match the original save format.
//-----------------------------------------------------------------------------
enum UHInventoryItem_t
{
	// Not yet mapped to the original id space (see TODO below).
	UH_ITEM_UNKNOWN = -1,

	UH_ITEM_NONE = 0,

	// Food — dropped as world models when used.
	UH_ITEM_APPLE_RED = 1,
	UH_ITEM_APPLE_GREEN = 2,
	UH_ITEM_BANANA = 3,
	UH_ITEM_BURRITO = 4,
	UH_ITEM_SANDWICH = 5,
	UH_ITEM_BANANA_BUNCH = 6,

	// Sodas — six flavours share item_uhsoda.
	UH_ITEM_SODA_FIRST = 7,
	UH_ITEM_SODA_LAST = 12,

	// Light sources.
	UH_ITEM_FLARE_PACK = 13,

	UH_ITEM_GLOWSTICK_FIRST = 14,
	UH_ITEM_GLOWSTICK_GREEN = 14,
	UH_ITEM_GLOWSTICK_RED = 15,
	UH_ITEM_GLOWSTICK_BLUE = 16,
	UH_ITEM_GLOWSTICK_YELLOW = 17,
	UH_ITEM_GLOWSTICK_PURPLE = 18,
	UH_ITEM_GLOWSTICK_LAST = 18,

	// Lit glowsticks. No world entity ("nothing" classname); the lit prop is
	// parented to the player and removed when the slot is consumed.
	UH_ITEM_LIT_GLOWSTICK_FIRST = 19,
	UH_ITEM_LIT_GLOWSTICK_LAST = 23,

	// Health items.
	UH_ITEM_PAINKILLERS = 24,
	UH_ITEM_SYRINGE = 25,
	UH_ITEM_BANDAGES = 26,
	UH_ITEM_HEALTHKIT = 27,
	UH_ITEM_HEALTH_VIAL = 28,

	UH_ITEM_CHOCOBAR = 29,
	UH_ITEM_ORANGE = 30,

	// Radios.
	UH_ITEM_FM_RADIO = 31,
	UH_ITEM_RADIO_CRACKER = 32,

	UH_ITEM_MAX,
};

//-----------------------------------------------------------------------------
// Per-id info. Matches the original name table order.
//-----------------------------------------------------------------------------
struct UHInventoryItemInfo_t
{
	const char *pszClassname;	// CreateEntityByName name; "nothing" = no world entity
	const char *pszPrintName;	// Display name (original strings)
};

const UHInventoryItemInfo_t *UH_GetInventoryItemInfo( int iItem );

inline bool UH_IsValidInventoryItem( int iItem )
{
	return ( iItem > UH_ITEM_NONE && iItem < UH_ITEM_MAX );
}

// Glowsticks occupy ids 14..18 (unlit) and 19..23 (lit, same color order).
inline bool UH_IsGlowstick( int iItem )
{
	return ( iItem >= UH_ITEM_GLOWSTICK_FIRST && iItem <= UH_ITEM_GLOWSTICK_LAST );
}

inline bool UH_IsLitGlowstick( int iItem )
{
	return ( iItem >= UH_ITEM_LIT_GLOWSTICK_FIRST && iItem <= UH_ITEM_LIT_GLOWSTICK_LAST );
}

// Using an unlit glowstick converts the slot to its lit counterpart (id + 5).
inline int UH_GetLitGlowstickItem( int iItem )
{
	return UH_IsGlowstick( iItem ) ? iItem + 5 : UH_ITEM_NONE;
}

// Original bodygroup mapping for the glowstick prop (per color).
inline int UH_GetGlowstickBodyGroup( int iItem )
{
	switch ( iItem )
	{
	case UH_ITEM_GLOWSTICK_GREEN:
	case UH_ITEM_GLOWSTICK_GREEN + 5:
		return 0;
	case UH_ITEM_GLOWSTICK_RED:
	case UH_ITEM_GLOWSTICK_RED + 5:
		return 2;
	case UH_ITEM_GLOWSTICK_BLUE:
	case UH_ITEM_GLOWSTICK_BLUE + 5:
		return 4;
	case UH_ITEM_GLOWSTICK_YELLOW:
	case UH_ITEM_GLOWSTICK_YELLOW + 5:
		return 6;
	case UH_ITEM_GLOWSTICK_PURPLE:
	case UH_ITEM_GLOWSTICK_PURPLE + 5:
		return 8;
	default:
		return 0;
	}
}

inline const char *UH_GetInventoryItemClass( int iItem )
{
	return UH_GetInventoryItemInfo( iItem )->pszClassname;
}

inline const char *UH_GetInventoryItemName( int iItem )
{
	return UH_GetInventoryItemInfo( iItem )->pszPrintName;
}

// Light colour per glowstick (green/red/blue/yellow/purple, original id order). Shared between the
// lit-glowstick pickup (uh_items.cpp) and the dropped glowstick prop.
inline Color UH_GetGlowstickColor( int iItem )
{
	switch ( UH_GetGlowstickBodyGroup( iItem ) )
	{
	case 0:		return Color( 0, 255, 0, 255 );		// green
	case 2:		return Color( 255, 0, 0, 255 );		// red
	case 4:		return Color( 80, 170, 255, 255 );	// blue
	case 6:		return Color( 240, 250, 50, 255 );	// yellow
	case 8:		return Color( 200, 25, 240, 255 );	// purple
	default:	return Color( 0, 255, 0, 255 );
	}
}

#endif // UH_INVENTORY_H
