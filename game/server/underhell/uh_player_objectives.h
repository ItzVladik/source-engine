//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell objectives — declarations (server-side).
//
// This header exists for code quality / portability reasons — it groups
// objective-related player methods separately from inventory. The actual
// method declarations live in hl2_player.h (CHL2_Player), this file just
// documents the module boundary.
//
// Original: sub_101F11D0 in serveror.dll (CBasePlayer::ClientCommand) which
// handled DispObj, GiveSign, SkipScene via gEntList.FindEntityByName +
// CLogicRelay::InputTrigger.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UH_PLAYER_OBJECTIVES_H
#define UH_PLAYER_OBJECTIVES_H
#ifdef _WIN32
#pragma once
#endif

// CHL2_Player methods are declared in hl2_player.h:
//   bool UH_HandleObjectiveCommand(const CCommand &args);
//   void UH_DisplayObjective();
//   void UH_TriggerMapEntity(const char *pszTargetName);

#endif // UH_PLAYER_OBJECTIVES_H
