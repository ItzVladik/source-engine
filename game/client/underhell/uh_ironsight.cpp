//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell ironsight — client side.
//
// There is intentionally NO client "ironsight_toggle" command here. The
// original does not register one either (the string only appears in a
// server-resync path, sub_100D8E90): the keybinding forwards "ironsight_toggle"
// to the server through the engine's client-command route (exactly like
// dropitem/useitem/switch). The server toggles the networked flags —
// CHL2_Player::m_bIronSighted and the viewmodel's m_bExpSighted — and the
// client viewmodel slides by reading its own m_bExpSighted every frame in
// CBaseViewModel::CalcViewModelView (baseviewmodel_shared.cpp).
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
