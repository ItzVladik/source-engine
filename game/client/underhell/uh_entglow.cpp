//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Underhell's L4D-style entity outline. The original client binary contains
// the CEntGlowEffect from the Source 2007 L4D Glow Effect implementation and
// receives m_bGlow/m_GlowColor on DT_BaseEntity.
//
//=============================================================================

#include "cbase.h"
#include "screenspaceeffects.h"
#include "cliententitylist.h"
#include "model_types.h"
#include "studio.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/IMaterialVar.h"
#include "KeyValues.h"

#include "tier0/memdbgon.h"

static ConVar cl_ge_glowenabled( "cl_ge_glowenabled", "1", FCVAR_ARCHIVE, "Enable/Disable Glow" );
static ConVar cl_ge_glowscale( "cl_ge_glowscale", "0.2", FCVAR_CHEAT );
static ConVar cl_ge_glowstencil( "cl_ge_glowstencil", "1", FCVAR_CHEAT );

class CEntGlowEffect : public IScreenSpaceEffect
{
public:
	CEntGlowEffect() : m_bEnabled( true ) {}

	virtual void Init();
	virtual void Shutdown();
	virtual void SetParameters( KeyValues *params ) {}
	virtual void Enable( bool enable ) { m_bEnabled = enable; }
	virtual bool IsEnabled() { return m_bEnabled; }
	virtual void Render( int x, int y, int w, int h );

private:
	struct GlowEntry_t
	{
		C_BaseEntity *entity;
		Color color;
	};

	void BuildGlowList( CUtlVector<GlowEntry_t> &entries );
	void RenderToStencil( const GlowEntry_t &entry, IMatRenderContext *context );
	void RenderToGlowTexture( const GlowEntry_t &entry, IMatRenderContext *context );

	bool m_bEnabled;
	CTextureReference m_GlowBuffer1;
	CTextureReference m_GlowBuffer2;
	CMaterialReference m_WhiteMaterial;
	CMaterialReference m_EffectMaterial;
	CMaterialReference m_BlurX;
	CMaterialReference m_BlurY;
};

ADD_SCREENSPACE_EFFECT( CEntGlowEffect, ge_entglow );

void CEntGlowEffect::Init()
{
	KeyValues *white = new KeyValues( "VertexLitGeneric" );
	white->SetString( "$basetexture", "vgui/white" );
	white->SetInt( "$selfillum", 1 );
	white->SetString( "$selfillummask", "vgui/white" );
	white->SetInt( "$vertexalpha", 1 );
	white->SetInt( "$model", 1 );
	m_WhiteMaterial.Init( "__geglowwhite", TEXTURE_GROUP_CLIENT_EFFECTS, white );
	m_WhiteMaterial->Refresh();

	KeyValues *composite = new KeyValues( "UnlitGeneric" );
	composite->SetString( "$basetexture", "_rt_FullFrameFB" );
	composite->SetInt( "$additive", 1 );
	m_EffectMaterial.Init( "__geglowcomposite", TEXTURE_GROUP_CLIENT_EFFECTS, composite );
	m_EffectMaterial->Refresh();

	m_GlowBuffer1.InitRenderTarget( ScreenWidth() / 2, ScreenHeight() / 2,
		RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SEPARATE,
		false, (char *)"_rt_geglowbuff1" );
	m_GlowBuffer2.InitRenderTarget( ScreenWidth() / 2, ScreenHeight() / 2,
		RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SEPARATE,
		false, (char *)"_rt_geglowbuff2" );

	// The guide shipped these as pp/ge_blurx and pp/ge_blury VMTs. Build the
	// same materials at runtime so the outline does not depend on a missing zip.
	KeyValues *blurX = new KeyValues( "BlurFilterX" );
	blurX->SetString( "$basetexture", "_rt_geglowbuff1" );
	blurX->SetFloat( "$bloomscale", 2.0f );
	m_BlurX.Init( "__geglowblurx", TEXTURE_GROUP_CLIENT_EFFECTS, blurX );
	m_BlurX->Refresh();

	KeyValues *blurY = new KeyValues( "BlurFilterY" );
	blurY->SetString( "$basetexture", "_rt_geglowbuff2" );
	blurY->SetFloat( "$bloomamount", 2.0f );
	m_BlurY.Init( "__geglowblury", TEXTURE_GROUP_CLIENT_EFFECTS, blurY );
	m_BlurY->Refresh();
}

void CEntGlowEffect::Shutdown()
{
	m_WhiteMaterial.Shutdown();
	m_EffectMaterial.Shutdown();
	m_BlurX.Shutdown();
	m_BlurY.Shutdown();
	m_GlowBuffer1.Shutdown();
	m_GlowBuffer2.Shutdown();
}

void CEntGlowEffect::BuildGlowList( CUtlVector<GlowEntry_t> &entries )
{
	const int highest = ClientEntityList().GetHighestEntityIndex();
	for ( int i = 0; i <= highest; ++i )
	{
		C_BaseEntity *entity = ClientEntityList().GetBaseEntity( i );
		if ( !entity || !entity->IsGlowEnabled() || entity->IsDormant() )
			continue;

		const color32 &c = entity->GetGlowColor();
		GlowEntry_t entry;
		entry.entity = entity;
		entry.color = Color( c.r, c.g, c.b, c.a );
		entries.AddToTail( entry );
	}

}

void CEntGlowEffect::RenderToStencil( const GlowEntry_t &entry, IMatRenderContext *context )
{
	context->SetStencilEnable( true );
	context->SetStencilFailOperation( STENCILOPERATION_KEEP );
	context->SetStencilZFailOperation( STENCILOPERATION_KEEP );
	context->SetStencilPassOperation( STENCILOPERATION_REPLACE );
	context->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_ALWAYS );
	context->SetStencilWriteMask( 1 );
	context->SetStencilReferenceValue( 1 );
	context->DepthRange( 0.0f, 0.01f );

	render->SetBlend( 0.0f );
	modelrender->ForcedMaterialOverride( m_WhiteMaterial );
	entry.entity->DrawModel( STUDIO_RENDER );
	modelrender->ForcedMaterialOverride( NULL );
	render->SetBlend( 1.0f );

	context->DepthRange( 0.0f, 1.0f );
	context->SetStencilEnable( false );
}

void CEntGlowEffect::RenderToGlowTexture( const GlowEntry_t &entry, IMatRenderContext *context )
{
	context->PushRenderTargetAndViewport( m_GlowBuffer1 );
	modelrender->SuppressEngineLighting( true );

	float color[4] =
	{
		entry.color.r() / 255.0f,
		entry.color.g() / 255.0f,
		entry.color.b() / 255.0f,
		entry.color.a() / 255.0f
	};
	bool found = false;
	IMaterialVar *var = m_WhiteMaterial->FindVar( "$selfillumtint", &found, false );
	if ( found )
		var->SetVecValue( color, 4 );
	var = m_WhiteMaterial->FindVar( "$alpha", &found, false );
	if ( found )
		var->SetFloatValue( color[3] );

	modelrender->ForcedMaterialOverride( m_WhiteMaterial );
	entry.entity->DrawModel( STUDIO_RENDER );
	modelrender->ForcedMaterialOverride( NULL );
	modelrender->SuppressEngineLighting( false );
	context->PopRenderTargetAndViewport();
}

void CEntGlowEffect::Render( int x, int y, int w, int h )
{
	if ( !m_bEnabled || !cl_ge_glowenabled.GetBool() )
		return;

	CUtlVector<GlowEntry_t> entries;
	BuildGlowList( entries );
	if ( entries.Count() == 0 )
		return;

	CMatRenderContextPtr context( materials );
	bool found = false;
	IMaterialVar *var = m_BlurX->FindVar( "$basetexture", &found, false );
	if ( found ) var->SetTextureValue( m_GlowBuffer1 );
	var = m_BlurY->FindVar( "$basetexture", &found, false );
	if ( found ) var->SetTextureValue( m_GlowBuffer2 );
	var = m_EffectMaterial->FindVar( "$basetexture", &found, false );
	if ( found ) var->SetTextureValue( m_GlowBuffer1 );
	var = m_BlurX->FindVar( "$bloomscale", &found, false );
	if ( found ) var->SetFloatValue( 10.0f * cl_ge_glowscale.GetFloat() );
	var = m_BlurY->FindVar( "$bloomamount", &found, false );
	if ( found ) var->SetFloatValue( 10.0f * cl_ge_glowscale.GetFloat() );

	context->ClearColor4ub( 0, 0, 0, 255 );
	context->PushRenderTargetAndViewport( m_GlowBuffer1 );
	context->ClearBuffers( true, true );
	context->PopRenderTargetAndViewport();
	context->PushRenderTargetAndViewport( m_GlowBuffer2 );
	context->ClearBuffers( true, true );
	context->PopRenderTargetAndViewport();
	context->ClearStencilBufferRectangle( 0, 0, ScreenWidth(), ScreenHeight(), 0 );

	for ( int i = 0; i < entries.Count(); ++i )
	{
		if ( cl_ge_glowstencil.GetBool() )
			RenderToStencil( entries[i], context );
		RenderToGlowTexture( entries[i], context );
	}

	context->PushRenderTargetAndViewport( m_GlowBuffer2 );
	context->DrawScreenSpaceQuad( m_BlurX );
	context->PopRenderTargetAndViewport();
	context->PushRenderTargetAndViewport( m_GlowBuffer1 );
	context->DrawScreenSpaceQuad( m_BlurY );
	context->PopRenderTargetAndViewport();

	if ( cl_ge_glowstencil.GetBool() )
	{
		context->SetStencilEnable( true );
		context->SetStencilReferenceValue( 0 );
		context->SetStencilTestMask( 1 );
		context->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
		context->SetStencilPassOperation( STENCILOPERATION_ZERO );
	}
	context->DrawScreenSpaceQuad( m_EffectMaterial );
	context->SetStencilEnable( false );
}
