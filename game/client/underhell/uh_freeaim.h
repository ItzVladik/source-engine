//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell free-aim cursor shared between CInput and the viewmodel.
//          The state is client-only: it describes a normalized on-screen aim
//          displacement, not the player's networked view angles.
//
//=============================================================================//

#ifndef UH_FREEAIM_H
#define UH_FREEAIM_H
#ifdef _WIN32
#pragma once
#endif

class Vector2D;
class CUserCmd;

#ifdef CLIENT_DLL
void UH_FreeAimUpdateCursor( float mouseX, float mouseY, CUserCmd *pCmd );
bool UH_FreeAimGetCursor( Vector2D &cursor );
void UH_FreeAimClampDirection( const QAngle &viewAngles, Vector &direction );
#endif

#endif // UH_FREEAIM_H
