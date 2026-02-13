//=============================================================================//
//
// Purpose: Common PNG image loader.
//
//=============================================================================//

#ifndef IMG_PNG_LOADER_H
#define IMG_PNG_LOADER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlbuffer.h"
#include "filesystem.h"

bool PNGtoRGBA( IFileSystem *filesystem, const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );
bool PNGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height );

bool PNGSupported();

#endif // IMG_PNG_LOADER_H
