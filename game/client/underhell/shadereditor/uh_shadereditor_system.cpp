// Minimal runtime integration for Biohazard90's Source Shader Editor.
// The editor UI/model preview code is intentionally not embedded in client.dll;
// the external 2007 DLL owns shader compilation and post-processing graphs.

#include "cbase.h"
#include "underhell/shadereditor/uh_shadereditor_system.h"
#include "client_factorylist.h"
#include "iviewrender.h"
#include "view.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_uh_shadereditor_debug( "cl_uh_shadereditor_debug", "0", 0,
	"Log Underhell Source Shader Editor startup and render callbacks" );

CUHShaderEditorSystem g_UHShaderEditorSystem;

CUHShaderEditorSystem::CUHShaderEditorSystem()
	: CAutoGameSystemPerFrame( "UHShaderEditor" ),
	  m_pModule( NULL ),
	  m_pEditor( NULL )
{
}

bool CUHShaderEditorSystem::Init()
{
	factorylist_t factories;
	FactoryList_Retrieve( factories );

	char modulePath[MAX_PATH * 4];
#if defined( SOURCE_2013 )
	const char *moduleName = "shadereditor_2013.dll";
#else
	const char *moduleName = "shadereditor_2007.dll";
#endif
	Q_snprintf( modulePath, sizeof( modulePath ), "%s/bin/%s",
		engine->GetGameDirectory(), moduleName );

	m_pModule = Sys_LoadModule( modulePath );
	if ( !m_pModule )
	{
		Warning( "[UH shader] Cannot load %s\n", modulePath );
		return true; // Shader Editor is optional; never abort client.dll startup.
	}

	CreateInterfaceFn factory = Sys_GetFactory( m_pModule );
	m_pEditor = factory ? static_cast<IVShaderEditor *>(
		factory( SHADEREDIT_INTERFACE_VERSION, NULL ) ) : NULL;
	if ( !m_pEditor )
	{
		Warning( "[UH shader] Unable to obtain %s from %s\n",
			SHADEREDIT_INTERFACE_VERSION, modulePath );
		return true;
	}

	ConVarRef developer( "developer", true );
	const bool showPrimaryDebug = developer.IsValid() && developer.GetBool();

	// Runtime-only integration. The model-preview/editor UI is deliberately not
	// linked into the game client; launch-time editing can be restored separately.
	if ( !m_pEditor->Init( factories.appSystemFactory, gpGlobals, NULL,
		false, showPrimaryDebug, SKYMASK_OFF ) )
	{
		Warning( "[UH shader] ShaderEditor005 initialization failed\n" );
		m_pEditor = NULL;
		return true;
	}

	m_pEditor->LockClientCallbacks();
	m_pEditor->LockViewRenderCallbacks();
	m_pEditor->PrecacheData();
	Msg( "[UH shader] ShaderEditor005 initialized; 2007 procedural shader library requested\n" );
	return true;
}

void CUHShaderEditorSystem::Shutdown()
{
	if ( m_pEditor )
	{
		m_pEditor->Shutdown();
		m_pEditor = NULL;
	}
	if ( m_pModule )
	{
		Sys_UnloadModule( m_pModule );
		m_pModule = NULL;
	}
}

void CUHShaderEditorSystem::Update( float frametime )
{
	if ( m_pEditor )
		m_pEditor->OnFrame( frametime );
}

void CUHShaderEditorSystem::PreRender()
{
	if ( !m_pEditor || !view )
		return;

	const CViewSetup *setup = view->GetPlayerViewSetup();
	if ( !setup )
		return;

	CViewSetup_SEdit_Shared stableSetup( *setup );
	m_pEditor->OnPreRender( &stableSetup );
	m_pEditor->OnSceneRender();

	if ( cl_uh_shadereditor_debug.GetBool() )
		DevMsg( "[UH shader] pre/scene render\n" );
}

void CUHShaderEditorSystem::PostRender()
{
	if ( m_pEditor )
		m_pEditor->OnPostRender( true );
}
