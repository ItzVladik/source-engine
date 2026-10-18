//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
// Client network shell for the visible bullet-time projectile.
#include "cbase.h"

#include "tier0/memdbgon.h"

class C_BtBullet : public C_BaseAnimating
{
	DECLARE_CLASS( C_BtBullet, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
public:
	bool ShouldDraw( void ) { return true; }
};

IMPLEMENT_CLIENTCLASS_DT( C_BtBullet, DT_BtBullet, CBtBullet )
END_RECV_TABLE()
