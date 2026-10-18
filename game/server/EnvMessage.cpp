//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "EnvMessage.h"
#include "engine/IEngineSound.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "Color.h"
#include "gamestats.h"
#include "globalstate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_message, CMessage );

CMessage::CMessage()
{
	// Underhell: default before keyvalue parsing. "GlobalEnvMessageIndex" is
	// absent on most env_message entities, so the ints must not be garbage.
	m_iCurrentPriority = 0;
	m_iGlobalEnvMessageIndex = -1;
}

BEGIN_DATADESC( CMessage )

	DEFINE_KEYFIELD( m_iszMessage, FIELD_STRING, "message" ),
	DEFINE_KEYFIELD( m_sNoise, FIELD_SOUNDNAME, "messagesound" ),
	DEFINE_KEYFIELD( m_MessageAttenuation, FIELD_INTEGER, "messageattenuation" ),
	DEFINE_KEYFIELD( m_MessageVolume, FIELD_FLOAT, "messagevolume" ),

	DEFINE_FIELD( m_Radius, FIELD_FLOAT ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ShowMessage", InputShowMessage ),
	DEFINE_INPUTFUNC( FIELD_STRING, "InputMessage", InputMessage ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessage", InputSetMessage ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority1", InputSetMessagePriority1 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority2", InputSetMessagePriority2 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority3", InputSetMessagePriority3 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority4", InputSetMessagePriority4 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority5", InputSetMessagePriority5 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority6", InputSetMessagePriority6 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority7", InputSetMessagePriority7 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority8", InputSetMessagePriority8 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority9", InputSetMessagePriority9 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority10", InputSetMessagePriority10 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority11", InputSetMessagePriority11 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority12", InputSetMessagePriority12 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority13", InputSetMessagePriority13 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority14", InputSetMessagePriority14 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority15", InputSetMessagePriority15 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMessagePriority16", InputSetMessagePriority16 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "RemoveMessagePriority", InputRemoveMessagePriority ),

	DEFINE_ARRAY( m_iszMessagesPriority, FIELD_STRING, 16 ),

	DEFINE_FIELD( m_iszSetMessage, FIELD_STRING ),
	DEFINE_FIELD( m_iCurrentPriority, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iGlobalEnvMessageIndex, FIELD_INTEGER, "GlobalEnvMessageIndex" ),

	DEFINE_OUTPUT(m_OnShowMessage, "OnShowMessage"),

END_DATADESC()



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessage::Spawn( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	switch( m_MessageAttenuation )
	{
	case 1: // Medium radius
		m_Radius = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		m_Radius = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		m_Radius = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		m_Radius = SNDLVL_IDLE;
		break;
	}
	m_MessageAttenuation = 0;

	// Remap volume from [0,10] to [0,1].
	m_MessageVolume *= 0.1;

	// No volume, use normal
	if ( m_MessageVolume <= 0 )
	{
		m_MessageVolume = 1.0;
	}

	// Underhell: GlobalEnvMessageIndex shares the active message priority between
	// maps. The message strings themselves come from this map's keyvalues; the
	// global state only stores which priority slot is active (see
	// UH_SyncGlobalMessagePriority). A value < 0 disables the feature.
	if ( m_iGlobalEnvMessageIndex < 0 )
		m_iGlobalEnvMessageIndex = -1;

	UH_RestoreGlobalMessagePriority();
}


void CMessage::Precache( void )
{
	if ( m_sNoise != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_sNoise) );
	}
}

//-----------------------------------------------------------------------------
// Underhell: helper to set message from string param.
// The original had 16 priority slots, VMF uses SetMessagePriority1 with
// "@titles_*.txt_*" syntax like "@titles_Prologue.txt_Prologue_2_Objective_A"
// which means: load titles_Prologue.txt and show entry Prologue_2_Objective_A.
// That entry's Message is "#UnderHell_Prologue_2_Objective_A" which is
// localized via Underhell_english.txt. We parse the @ reference to get the
// actual titles entry name so UTIL_ShowMessage can find it.
//-----------------------------------------------------------------------------
const char *CMessage::ParseTitlesReference( const char *pszInput, char *outBuf, int outBufSize )
{
	if ( !pszInput || !*pszInput )
		return pszInput;

	// Strip leading @ or #? Keep # for localization, but handle @titles case.
	if ( pszInput[0] == '@' )
	{
		// Look for ".txt_" or ".txt " separator
		const char *pszAfterTxt = Q_stristr( pszInput, ".txt_" );
		if ( pszAfterTxt )
		{
			pszAfterTxt += 5; // skip ".txt_"
			Q_strncpy( outBuf, pszAfterTxt, outBufSize );
			return outBuf;
		}
		pszAfterTxt = Q_stristr( pszInput, ".txt " );
		if ( pszAfterTxt )
		{
			pszAfterTxt += 5;
			Q_strncpy( outBuf, pszAfterTxt, outBufSize );
			return outBuf;
		}
		// Fallback: after last '_' maybe? For "@titles_Prologue.txt_Prologue_2_Objective_A"
		// we already handled .txt_ case. If format is different, take after last '/':
		const char *pszLastSlash = strrchr( pszInput, '/' );
		const char *pszStart = pszLastSlash ? pszLastSlash + 1 : pszInput + 1; // skip @
		// If still contains path, try to extract entry name
		Q_strncpy( outBuf, pszStart, outBufSize );
		// If outBuf contains ".txt_", extract after
		char *p = (char*)Q_stristr( outBuf, ".txt_" );
		if ( p )
		{
			Q_strncpy( outBuf, p + 5, outBufSize );
			return outBuf;
		}
		return outBuf;
	}
	// If starts with #, keep as is for localization, but also allow.
	return pszInput;
}

// Tries to find a titles entry in scripts/titles_*.txt files.
// Returns true if found and fills outParms and outMessage (which is the
// localization token like "#UnderHell_House_Comment_Chest" or the raw message).
// This replicates original Underhell's handling of @titles_*.txt_* references
// and ensures correct color/position (middle bottom for chest comments vs
// middle for objectives). Uses filesystem and KeyValues parser.
// Note: scripts/titles.txt is GoldSrc-style titles file, not KeyValues, so
// we skip it to avoid "KeyValues Error: missing { in file scripts/titles.txt"
// spam in console. Only KeyValues-style titles_*.txt files are parsed.
bool CMessage::GetTitlesEntry( const char *pszEntryName, hudtextparms_t &outParms, char *outMessage, int outMessageSize )
{
	if ( !pszEntryName || !*pszEntryName )
		return false;

	// List of known titles files from Underhell (from scripts/ folder)
	// titles.txt is intentionally excluded — it's not KeyValues format and
	// causes console spam "missing { in file scripts/titles.txt"
	const char *pszFiles[] =
	{
		"titles_Prologue.txt",
		"titles_House.txt",
		"titles_Chapter1.txt",
		NULL
	};

	// Default parms (if not found in file, use objective style)
	Q_memset( &outParms, 0, sizeof(outParms) );
	outParms.channel = 1;
	outParms.x = -1;
	outParms.y = -1;
	outParms.effect = 0;
	outParms.r1 = 255; outParms.g1 = 255; outParms.b1 = 255; outParms.a1 = 255;
	outParms.r2 = 255; outParms.g2 = 255; outParms.b2 = 255; outParms.a2 = 255;
	outParms.fadeinTime = 0;
	outParms.fadeoutTime = 0;
	outParms.holdTime = 5.0f;
	outParms.fxTime = 0;

	for ( int i = 0; pszFiles[i]; ++i )
	{
		char szPath[256];
		Q_snprintf( szPath, sizeof(szPath), "scripts/%s", pszFiles[i] );

		// Try to load via filesystem (MOD then GAME)
		KeyValues *pKV = new KeyValues( pszFiles[i] );
		bool bLoaded = pKV->LoadFromFile( filesystem, szPath, "MOD" );
		if ( !bLoaded )
			bLoaded = pKV->LoadFromFile( filesystem, szPath, "GAME" );

		if ( !bLoaded )
		{
			pKV->deleteThis();
			continue;
		}

		// Titles files have entries as subkeys at root level
		// Each entry like "House_Comment_Chest" { "positionx" "0.1" ... "Message" "#UnderHell_..." }
		KeyValues *pEntry = pKV->FindKey( pszEntryName );
		if ( !pEntry )
		{
			// Some files have an extra top-level wrapper (file name as root)?
			// Try to find case-insensitive?
			for ( KeyValues *pSub = pKV->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey() )
			{
				if ( !Q_stricmp( pSub->GetName(), pszEntryName ) )
				{
					pEntry = pSub;
					break;
				}
			}
		}

		if ( pEntry )
		{
			// Fill parms from entry
			outParms.x = pEntry->GetFloat( "positionx", -1.0f );
			outParms.y = pEntry->GetFloat( "positiony", -1.0f );
			outParms.effect = pEntry->GetInt( "effect", 0 );
			outParms.fadeinTime = pEntry->GetFloat( "fadein", 0.0f );
			outParms.fadeoutTime = pEntry->GetFloat( "fadeout", 0.0f );
			outParms.holdTime = pEntry->GetFloat( "holdtime", 3.0f );
			outParms.fxTime = pEntry->GetFloat( "fxtime", 0.25f );
			outParms.r1 = pEntry->GetInt( "r1", 255 );
			outParms.g1 = pEntry->GetInt( "g1", 255 );
			outParms.b1 = pEntry->GetInt( "b1", 255 );
			outParms.a1 = pEntry->GetInt( "a1", 255 );
			outParms.r2 = pEntry->GetInt( "r2", 255 );
			outParms.g2 = pEntry->GetInt( "g2", 255 );
			outParms.b2 = pEntry->GetInt( "b2", 255 );
			outParms.a2 = pEntry->GetInt( "a2", 255 );
			outParms.channel = pEntry->GetInt( "channel", 1 );

			const char *pszMsg = pEntry->GetString( "Message", "" );
			if ( pszMsg && *pszMsg )
			{
				Q_strncpy( outMessage, pszMsg, outMessageSize );
			}
			else
			{
				// No Message field — use entry name as fallback
				Q_strncpy( outMessage, pszEntryName, outMessageSize );
			}

			pKV->deleteThis();
			return true;
		}

		pKV->deleteThis();
	}

	return false;
}

void CMessage::SetMessageFromString( const char *pszMessage )
{
	if ( !pszMessage || !*pszMessage )
		return;

	char szClean[512];
	const char *pszParsed = ParseTitlesReference( pszMessage, szClean, sizeof(szClean) );
	if ( !pszParsed )
		pszParsed = pszMessage;

	m_iszMessage = AllocPooledString( pszParsed );
}

//-----------------------------------------------------------------------------
// Purpose: Underhell — resolve a message string (handles "@titles_*.txt_*"
// references) and show it on the player's HUD. Shared by ShowMessage/InputMessage.
//-----------------------------------------------------------------------------
void CMessage::ShowMessageText( const char *psz )
{
	if ( !psz || !*psz )
		return;

	// Resolve "@titles_*.txt_<entry>" references to the bare entry name so the
	// titles lookup below can find it (InputMessage passes the raw parameter).
	char szClean[512];
	const char *pszParsed = ParseTitlesReference( psz, szClean, sizeof(szClean) );
	if ( pszParsed )
		psz = pszParsed;

	// Resolve a titles entry (scripts/titles_*.txt) so chest comments / objectives
	// render with the correct colour + position, not as a plain centred message.
	hudtextparms_t hparms;
	char szMessageFromTitles[512];
	if ( GetTitlesEntry( psz, hparms, szMessageFromTitles, sizeof(szMessageFromTitles) ) )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer )
			UTIL_HudMessage( pPlayer, hparms, szMessageFromTitles );
		else
			UTIL_HudMessageAll( hparms, szMessageFromTitles );
		return;
	}

	if ( m_spawnflags & SF_MESSAGE_ALL )
	{
		UTIL_ShowMessageAll( psz );
		return;
	}

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
		UTIL_ShowMessage( psz, pPlayer );
	else
		UTIL_ShowMessageAll( psz );
}

//-----------------------------------------------------------------------------
// Purpose: Underhell — active message = highest-priority stored slot, falling
// back to the "message" keyvalue. SetMessage beats every SetMessagePriorityN.
//-----------------------------------------------------------------------------
const char *CMessage::GetActiveMessage( void )
{
	if ( m_iszSetMessage != NULL_STRING && STRING(m_iszSetMessage)[0] )
		return STRING(m_iszSetMessage);

	for ( int i = 15; i >= 0; --i )
	{
		if ( m_iszMessagesPriority[i] != NULL_STRING && STRING(m_iszMessagesPriority[i])[0] )
			return STRING(m_iszMessagesPriority[i]);
	}

	if ( m_iszMessage != NULL_STRING )
		return STRING(m_iszMessage);

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Underhell — store a message into one of the 16 priority slots and,
// when it is now the highest set, promote it to the active message. Higher
// priority number wins; SetMessage (priority 17) wins over all of them.
//-----------------------------------------------------------------------------
void CMessage::SetPriorityMessage( int prio, const char *psz )
{
	if ( !psz || !*psz || prio < 1 || prio > 16 )
		return;

	m_iszMessagesPriority[prio - 1] = AllocPooledString( psz );

	if ( prio >= m_iCurrentPriority && m_iCurrentPriority != 17 )
		m_iCurrentPriority = prio;

	UH_SyncGlobalMessagePriority();
}

//-----------------------------------------------------------------------------
// Purpose: Underhell — GlobalEnvMessageIndex (0-7). Shares the ACTIVE priority
// between maps via the global state system's counter (the message strings come
// from each map's own keyvalues). TODO: original stores the message "as player
// state" — verify whether the string itself must survive the transition.
//-----------------------------------------------------------------------------
void CMessage::UH_SyncGlobalMessagePriority( void )
{
	if ( m_iGlobalEnvMessageIndex < 0 )
		return;

	char szGlobal[64];
	Q_snprintf( szGlobal, sizeof(szGlobal), "uh_envmessage_%d", m_iGlobalEnvMessageIndex );

	if ( !GlobalEntity_IsInTable( szGlobal ) )
		GlobalEntity_Add( szGlobal, STRING(gpGlobals->mapname), GLOBAL_ON );

	GlobalEntity_SetCounter( GlobalEntity_GetIndex( szGlobal ), m_iCurrentPriority );
}

void CMessage::UH_RestoreGlobalMessagePriority( void )
{
	if ( m_iGlobalEnvMessageIndex < 0 )
		return;

	char szGlobal[64];
	Q_snprintf( szGlobal, sizeof(szGlobal), "uh_envmessage_%d", m_iGlobalEnvMessageIndex );

	if ( GlobalEntity_IsInTable( szGlobal ) )
		m_iCurrentPriority = GlobalEntity_GetCounter( GlobalEntity_GetIndex( szGlobal ) );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CMessage::InputShowMessage( inputdata_t &inputdata )
{
	ShowMessageText( GetActiveMessage() );

	if ( m_sNoise != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_sNoise);
		ep.m_flVolume = m_MessageVolume;
		ep.m_SoundLevel = ATTN_TO_SNDLVL( m_Radius );

		EmitSound( filter, entindex(), ep );
	}

	if ( m_spawnflags & SF_MESSAGE_ONCE )
	{
		UTIL_Remove( this );
	}

	m_OnShowMessage.FireOutput( inputdata.pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: Underhell extension — display the message in the target parameter
// directly, independent of the entity's "message" keyvalue / stored priority.
//-----------------------------------------------------------------------------
void CMessage::InputMessage( inputdata_t &inputdata )
{
	ShowMessageText( inputdata.value.String() );

	if ( m_spawnflags & SF_MESSAGE_ONCE )
	{
		UTIL_Remove( this );
	}

	m_OnShowMessage.FireOutput( inputdata.pActivator, this );
}

void CMessage::InputSetMessage( inputdata_t &inputdata )
{
	const char *psz = inputdata.value.String();
	if ( psz && *psz )
	{
		char szClean[512];
		const char *pszParsed = ParseTitlesReference( psz, szClean, sizeof(szClean) );
		m_iszSetMessage = AllocPooledString( pszParsed ? pszParsed : psz );
		m_iCurrentPriority = 17;	// SetMessage is the highest priority.

		UH_SyncGlobalMessagePriority();
	}
}

void CMessage::InputSetMessagePriority1( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 1, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority2( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 2, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority3( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 3, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority4( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 4, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority5( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 5, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority6( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 6, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority7( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 7, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority8( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 8, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority9( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 9, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority10( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 10, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority11( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 11, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority12( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 12, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority13( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 13, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority14( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 14, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority15( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 15, p ? p : inputdata.value.String() ); }
void CMessage::InputSetMessagePriority16( inputdata_t &inputdata ) { char szClean[512]; const char *p = ParseTitlesReference( inputdata.value.String(), szClean, sizeof(szClean) ); SetPriorityMessage( 16, p ? p : inputdata.value.String() ); }

void CMessage::InputRemoveMessagePriority( inputdata_t &inputdata )
{
	// Target parameter is the priority number (1-16). Removing a slot makes the
	// previous (next-highest) stored message the active one again.
	const char *psz = inputdata.value.String();
	int iPrio = psz ? atoi( psz ) : 0;

	if ( iPrio >= 1 && iPrio <= 16 )
	{
		m_iszMessagesPriority[iPrio - 1] = NULL_STRING;
	}
	else
	{
		// No valid index: clear every priority slot.
		for ( int i = 0; i < 16; ++i )
			m_iszMessagesPriority[i] = NULL_STRING;
		m_iszSetMessage = NULL_STRING;
		m_iCurrentPriority = 0;
		UH_SyncGlobalMessagePriority();
		return;
	}

	// Recompute the active priority (highest remaining slot, SetMessage first).
	m_iCurrentPriority = 0;
	if ( m_iszSetMessage != NULL_STRING && STRING(m_iszSetMessage)[0] )
	{
		m_iCurrentPriority = 17;
	}
	else
	{
		for ( int i = 15; i >= 0; --i )
		{
			if ( m_iszMessagesPriority[i] != NULL_STRING && STRING(m_iszMessagesPriority[i])[0] )
			{
				m_iCurrentPriority = i + 1;
				break;
			}
		}
	}

	UH_SyncGlobalMessagePriority();
}


void CMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	inputdata_t inputdata;

	inputdata.pActivator	= NULL;
	inputdata.pCaller		= NULL;

	InputShowMessage( inputdata );
}

class CCredits : public CPointEntity
{
public:
	DECLARE_CLASS( CMessage, CPointEntity );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	InputRollCredits( inputdata_t &inputdata );
	void	InputRollOutroCredits( inputdata_t &inputdata );
	void	InputShowLogo( inputdata_t &inputdata );
	void	InputSetLogoLength( inputdata_t &inputdata );

	COutputEvent m_OnCreditsDone;

	virtual void OnRestore();
private:

	void		RollOutroCredits();

	bool		m_bRolledOutroCredits;
	float		m_flLogoLength;
};

LINK_ENTITY_TO_CLASS( env_credits, CCredits );

BEGIN_DATADESC( CCredits )
	DEFINE_INPUTFUNC( FIELD_VOID, "RollCredits", InputRollCredits ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RollOutroCredits", InputRollOutroCredits ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ShowLogo", InputShowLogo ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetLogoLength", InputSetLogoLength ),
	DEFINE_OUTPUT( m_OnCreditsDone, "OnCreditsDone"),

	DEFINE_FIELD( m_bRolledOutroCredits, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLogoLength, FIELD_FLOAT )
END_DATADESC()

void CCredits::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

static void CreditsDone_f( void )
{
	CCredits *pCredits = (CCredits*)gEntList.FindEntityByClassname( NULL, "env_credits" );

	if ( pCredits )
	{
		pCredits->m_OnCreditsDone.FireOutput( pCredits, pCredits );
	}
}

static ConCommand creditsdone("creditsdone", CreditsDone_f );

extern ConVar sv_unlockedchapters;

void CCredits::OnRestore()
{
	BaseClass::OnRestore();

	if ( m_bRolledOutroCredits )
	{
		// Roll them again so that the client .dll will send the "creditsdone" message and we'll
		//  actually get back to the main menu
		RollOutroCredits();
	}
}

void CCredits::RollOutroCredits()
{
	sv_unlockedchapters.SetValue( "15" );
	
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "CreditsMsg" );
		WRITE_BYTE( 3 );
	MessageEnd();
}

void CCredits::InputRollOutroCredits( inputdata_t &inputdata )
{
	RollOutroCredits();

	// In case we save restore
	m_bRolledOutroCredits = true;

	gamestats->Event_Credits();
}

void CCredits::InputShowLogo( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	if ( m_flLogoLength )
	{
		UserMessageBegin( user, "LogoTimeMsg" );
			WRITE_FLOAT( m_flLogoLength );
		MessageEnd();
	}
	else
	{
		UserMessageBegin( user, "CreditsMsg" );
			WRITE_BYTE( 1 );
		MessageEnd();
	}
}

void CCredits::InputSetLogoLength( inputdata_t &inputdata )
{
	m_flLogoLength = inputdata.value.Float();
}

void CCredits::InputRollCredits( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "CreditsMsg" );
		WRITE_BYTE( 2 );
	MessageEnd();
}
