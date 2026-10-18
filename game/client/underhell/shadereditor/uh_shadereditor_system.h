// Underhell runtime bridge for Source Shader Editor.
// Kept isolated so the same bridge can be adapted to Source SDK 2013.
#ifndef UH_SHADEREDITOR_SYSTEM_H
#define UH_SHADEREDITOR_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "underhell/shadereditor/ivshadereditor.h"

class CUHShaderEditorSystem : public CAutoGameSystemPerFrame
{
public:
	CUHShaderEditorSystem();
	virtual bool Init();
	virtual void Shutdown();
	virtual void Update( float frametime );
	virtual void PreRender();
	virtual void PostRender();

	bool IsReady() const { return m_pEditor != NULL; }
	IVShaderEditor *GetEditor() const { return m_pEditor; }

private:
	CSysModule *m_pModule;
	IVShaderEditor *m_pEditor;
};

extern CUHShaderEditorSystem g_UHShaderEditorSystem;

#endif
