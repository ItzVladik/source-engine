//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell inventory — item registry shared by client and server.
//          Values reconstructed from the original binaries.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "underhell/uh_inventory.h"

//-----------------------------------------------------------------------------
// Item table. Index = item id (see UHInventoryItem_t).
//
// Original classnames are preserved verbatim — "nothing" marks ids that have
// no world entity (lit glowsticks: the lit prop is a player child instead).
//
// Names and order verified against server sub_102E27A0 and the client inventory
// switch around 0x10130F50.
//-----------------------------------------------------------------------------
static const UHInventoryItemInfo_t s_InventoryItemTable[] =
{
	{ "",				"none"					},	// UH_ITEM_NONE
	{ "item_apple",		"Apple (Red)"			},	// UH_ITEM_APPLE_RED
	{ "item_apple",		"Apple (Green)"			},	// UH_ITEM_APPLE_GREEN
	{ "item_banana",	"Banana"				},	// UH_ITEM_BANANA
	{ "item_burrito",	"Burrito"				},	// UH_ITEM_BURRITO
	{ "item_sandwich",	"Sandwich"				},	// UH_ITEM_SANDWICH
	{ "item_bananabunch","Banana Bunch"			},	// UH_ITEM_BANANA_BUNCH
	{ "item_uhsoda",	"Soda1"					},	// UH_ITEM_SODA_FIRST .. UH_ITEM_SODA_LAST
	{ "item_uhsoda",	"Soda2"					},
	{ "item_uhsoda",	"Soda3"					},
	{ "item_uhsoda",	"Soda4"					},
	{ "item_uhsoda",	"Soda5"					},
	{ "item_uhsoda",	"Mega Soda"				},
	{ "item_flarepack",	"Flare"					},	// UH_ITEM_FLARE_PACK
	{ "item_glowstick",	"GlowStick (Green)"		},	// id 14
	{ "item_glowstick",	"GlowStick (Red)"		},	// id 15
	{ "item_glowstick",	"GlowStick (Blue)"		},	// id 16
	{ "item_glowstick",	"GlowStick (Yellow)"	},	// id 17
	{ "item_glowstick",	"GlowStick (Purple)"	},	// id 18
	{ "nothing",		"Lit GlowStick (Green)"},	// id 19
	{ "nothing",		"Lit GlowStick (Red)"	},	// id 20
	{ "nothing",		"Lit GlowStick (Blue)"	},	// id 21
	{ "nothing",		"Lit GlowStick (Yellow)"},	// id 22
	{ "nothing",		"Lit GlowStick (Purple)"},	// id 23
	{ "item_painkillers","PainKillers"			},	// UH_ITEM_PAINKILLERS
	{ "item_syringe",	"Syringe"				},	// UH_ITEM_SYRINGE
	{ "item_bandages",	"Bandages"				},	// UH_ITEM_BANDAGES
	{ "item_healthkit",	"Healthkit"				},	// UH_ITEM_HEALTHKIT
	{ "item_healthvial","Health Vial"			},	// UH_ITEM_HEALTH_VIAL
	{ "item_chocobar",	"Chocolate Bar"			},	// UH_ITEM_CHOCOBAR
	{ "item_orange",	"Orange"				},	// UH_ITEM_ORANGE
	{ "item_fmradio",	"FM Radio"				},	// UH_ITEM_FM_RADIO
	{ "item_radiocracker","Radio Cracker"		},	// UH_ITEM_RADIO_CRACKER
};

const UHInventoryItemInfo_t *UH_GetInventoryItemInfo( int iItem )
{
	// SDK's COMPILE_TIME_ASSERT expands to switch(0) — function scope only.
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_InventoryItemTable ) == UH_INVENTORY_ITEM_TABLE_SIZE );

	if ( iItem < 0 || iItem >= UH_INVENTORY_ITEM_TABLE_SIZE )
		return &s_InventoryItemTable[UH_ITEM_NONE];

	return &s_InventoryItemTable[iItem];
}
