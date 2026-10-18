//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell night vision + gas mask full-screen overlays (client).
//
// Decoded from the original client (sub_10141600): while the local player has
// m_bNightVisionOn / m_bGasMaskOn set (networked from the server), the client
// draws the "shader/nightvision" and "shader/gasmask" materials full-screen.
// The materials + their shaders live in the game install (Underhell/materials
// + shaders), so this only needs to draw them — the shader does the tint /
// depth effect / mask vignette.
//
// Implemented as an IScreenSpaceEffect registered with the screen-space effect
// manager (rendered from CViewRender::PerformScreenSpaceEffects each frame).
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "screenspaceeffects.h"
#include "materialsystem/materialsystemutil.h"
#include "materialsystem/imaterial.h"
#include "view_scene.h"
#include "c_basehlplayer.h"
#include "underhell/shadereditor/uh_shadereditor_system.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CUHGearOverlayEffect — draws the night vision / gas mask overlays.
//-----------------------------------------------------------------------------
class CUHGearOverlayEffect : public IScreenSpaceEffect
{
public:
	CUHGearOverlayEffect( void ) : m_bRuntimeMaterialsCreated( false ) {}

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *params ) {}
	// Always enabled — the Render() path gates itself on the player's networked
	// night-vision / gas-mask flags (same pattern as CStunEffect).
	virtual void Enable( bool bEnable ) {}
	virtual bool IsEnabled( ) { return true; }

	virtual void Render( int x, int y, int w, int h );

private:
	void EnsureRuntimeMaterials();
	CMaterialReference m_NightVisionMaterial;
	CMaterialReference m_GasMaskMaterial;
	bool m_bRuntimeMaterialsCreated;
};

ADD_SCREENSPACE_EFFECT( CUHGearOverlayEffect, underhell_gear );

//-----------------------------------------------------------------------------
void CUHGearOverlayEffect::Init( void )
{
	m_bRuntimeMaterialsCreated = false;
}

//-----------------------------------------------------------------------------
void CUHGearOverlayEffect::Shutdown( void )
{
	m_NightVisionMaterial.Shutdown();
	m_GasMaskMaterial.Shutdown();
}

//-----------------------------------------------------------------------------
// Resolve the original wrapper VMTs only after ShaderEditor005 has registered
// editor_shader. This avoids caching an error material during early startup.
//-----------------------------------------------------------------------------
void CUHGearOverlayEffect::EnsureRuntimeMaterials()
{
	if ( m_bRuntimeMaterialsCreated || !g_UHShaderEditorSystem.IsReady() )
		return;

	// The original materials are thin wrappers around Shader Editor's single
	// registered CPU-side shader. Their $shadername selects the compiled graph:
	//
	// "editor_shader" { "$shadername" "postproc_nightvision" }
	//
	// The graph name itself is not a Source shader and cannot be used as the
	// KeyValues root.
	m_NightVisionMaterial.Init( "shader/nightvision", TEXTURE_GROUP_OTHER );
	IMaterial *pNightVision = m_NightVisionMaterial;
	if ( !pNightVision || IsErrorMaterial( pNightVision ) )
	{
		m_NightVisionMaterial.Shutdown();
		Warning( "[UH shader] shader/nightvision.vmt failed to resolve after ShaderEditor startup\n" );
	}

	m_GasMaskMaterial.Init( "shader/gasmask", TEXTURE_GROUP_OTHER );
	IMaterial *pGasMask = m_GasMaskMaterial;
	if ( !pGasMask || IsErrorMaterial( pGasMask ) )
	{
		m_GasMaskMaterial.Shutdown();
		Warning( "[UH shader] shader/gasmask.vmt failed to resolve after ShaderEditor startup\n" );
	}

	m_bRuntimeMaterialsCreated = true;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the overlays when the local player has the gear active.
//-----------------------------------------------------------------------------
void CUHGearOverlayEffect::Render( int x, int y, int w, int h )
{
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>( C_BasePlayer::GetLocalPlayer() );
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	EnsureRuntimeMaterials();

	if ( pPlayer->m_bNightVisionOn && m_NightVisionMaterial.IsValid() )
	{
		DrawScreenEffectMaterial( m_NightVisionMaterial, x, y, w, h );
	}

	if ( pPlayer->m_bGasMaskOn && m_GasMaskMaterial.IsValid() )
	{
		DrawScreenEffectMaterial( m_GasMaskMaterial, x, y, w, h );
	}
}
