//========= Copyright (c) 2008, Mxthe (Underhell). All rights reserved. ============//
//
// Purpose: Underhell night vision + gas mask (server-side CHL2_Player).
//
// Decoded from the original binary:
//   - "NightVision_Toggle" client command -> CHL2_Player vtable 404
//     (sub_102E19B0): mutual exclusion with the gas mask, ownership gate
//     (m_bHaveNightVision @2138 && m_bNightVisionEnabled @2140), battery gate
//     (m_iUHBatteryCount @5044 / m_flUHBatteryCharge @5128), toggles the
//     networked m_bNightVisionOn @3369, flips the "NightVision" playermodel
//     bodygroup and plays Player.nvon / Player.nvoff.
//   - "GasMask_Toggle" client command -> sub_101ED380: mutual exclusion with
//     night vision, ownership gate (m_bHaveGasMask @2139 && m_bGasMaskEnabled
//     @2141), toggles the networked m_bGasMaskOn @3370, starts/stops the
//     looping "GasMask.Breath.Normal" breath sound, flips the "GasMask"
//     playermodel bodygroup and plays Player.GasMaskOn / Player.GasMaskOff.
//   - "SetNightVision" / "SetGasMask" inputs (sub_101E36C0 / sub_101E3750)
//     grant/take the gear by setting m_bHaveNightVision / m_bHaveGasMask.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_player.h"
#include "soundenvelope.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Toggle night vision (client command "NightVision_Toggle").
//
// Mirrors sub_102E19B0. The overlay itself is drawn client-side from the
// networked m_bNightVisionOn; the server just owns the state, the battery
// gate and the third-person goggles bodygroup.
//
// TODO (decode leftovers, not ported):
//   - the original also clears the "r_flashlightscissor" convar on toggle;
//   - it adds/removes an effects flag (0x400) on the player entity — a custom
//     flag outside the SDK's EF_MAX_BITS, so it is omitted here;
//   - night vision drains the flashlight battery while on (the auto-off path
//     in sub_102E3DE0) — the turn-on gate is here, the continuous drain is not.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ToggleNightVision( void )
{
	// Can't use night vision while the gas mask is on.
	if ( m_bGasMaskOn )
	{
		EmitSound( "HL2Player.UseDeny" );
		return;
	}

	if ( !m_bHaveNightVision || !m_bNightVisionEnabled )
		return;

	if ( !m_bNightVisionOn )
	{
		// Turning on: refuse without a battery (or a nearly-full current charge).
		if ( m_iUHBatteryCount <= 0 && m_HL2Local.m_flFlashBattery <= 10.0f )
		{
			EmitSound( "HL2Player.UseDeny" );
			return;
		}

		m_bNightVisionOn = true;
		EmitSound( "Player.nvon" );
	}
	else
	{
		m_bNightVisionOn = false;
		EmitSound( "Player.nvoff" );
	}

	// Third-person goggles: flip the "NightVision" bodygroup (down = 1).
	int iGroup = FindBodygroupByName( "NightVision" );
	if ( iGroup >= 0 )
		SetBodygroup( iGroup, m_bNightVisionOn ? 1 : 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the gas mask (client command "GasMask_Toggle"). Mirrors
// sub_101ED380, including the looping breath sound (CSoundPatch, so it can be
// stopped when the mask comes off).
//-----------------------------------------------------------------------------
void CHL2_Player::UH_ToggleGasMask( void )
{
	// Can't put the mask on while night vision is on.
	if ( m_bNightVisionOn )
	{
		EmitSound( "HL2Player.UseDeny" );
		return;
	}

	if ( !m_bHaveGasMask || !m_bGasMaskEnabled )
		return;

	m_bGasMaskOn = !m_bGasMaskOn;

	if ( m_bGasMaskOn )
	{
		// Start the looping breath sound.
		if ( m_pGasMaskBreathLoop == NULL )
		{
			CPASAttenuationFilter filter( this );
			m_pGasMaskBreathLoop = CSoundEnvelopeController::GetController().SoundCreate(
				filter, entindex(), "GasMask.Breath.Normal" );
			CSoundEnvelopeController::GetController().Play( m_pGasMaskBreathLoop, 1.0f, 100 );
		}
		EmitSound( "Player.GasMaskOn" );
	}
	else
	{
		if ( m_pGasMaskBreathLoop != NULL )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pGasMaskBreathLoop );
			m_pGasMaskBreathLoop = NULL;
		}
		EmitSound( "Player.GasMaskOff" );
	}

	// Third-person mask: flip the "GasMask" bodygroup (on = 1).
	int iGroup = FindBodygroupByName( "GasMask" );
	if ( iGroup >= 0 )
		SetBodygroup( iGroup, m_bGasMaskOn ? 1 : 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Stop the breath loop if the player dies / resets while masked.
// Called from Event_Killed / Spawn reset paths via UH_StopGasMaskBreath.
//-----------------------------------------------------------------------------
void CHL2_Player::UH_StopGasMaskBreath( void )
{
	if ( m_pGasMaskBreathLoop != NULL )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pGasMaskBreathLoop );
		m_pGasMaskBreathLoop = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: "SetNightVision" input — grant/take the night vision goggles
// (sub_101E36C0 sets m_bHaveNightVision). Taking them while on leaves the
// flag stale in the original; we force the overlay off so the screen doesn't
// stay green.
//-----------------------------------------------------------------------------
void CHL2_Player::InputSetNightVision( inputdata_t &inputdata )
{
	m_bHaveNightVision = inputdata.value.Bool();

	if ( !m_bHaveNightVision && m_bNightVisionOn )
	{
		m_bNightVisionOn = false;
		EmitSound( "Player.nvoff" );

		int iGroup = FindBodygroupByName( "NightVision" );
		if ( iGroup >= 0 )
			SetBodygroup( iGroup, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: "SetGasMask" input — grant/take the gas mask (sub_101E3750 sets
// m_bHaveGasMask).
//-----------------------------------------------------------------------------
void CHL2_Player::InputSetGasMask( inputdata_t &inputdata )
{
	m_bHaveGasMask = inputdata.value.Bool();

	if ( !m_bHaveGasMask && m_bGasMaskOn )
	{
		m_bGasMaskOn = false;
		UH_StopGasMaskBreath();
		EmitSound( "Player.GasMaskOff" );

		int iGroup = FindBodygroupByName( "GasMask" );
		if ( iGroup >= 0 )
			SetBodygroup( iGroup, 0 );
	}
}
