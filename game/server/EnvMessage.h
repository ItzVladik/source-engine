//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENVMESSAGE_H
#define ENVMESSAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "entityoutput.h"
#include "util.h"


#define SF_MESSAGE_ONCE			0x0001		// Fade in, not out
#define SF_MESSAGE_ALL			0x0002		// Send to all clients

class CMessage : public CPointEntity
{
public:
	DECLARE_CLASS( CMessage, CPointEntity );

	CMessage();

	void	Spawn( void );
	void	Precache( void );

	inline void SetMessage( string_t iszMessage ) { m_iszMessage = iszMessage; }

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	void InputShowMessage( inputdata_t &inputdata );
	void InputMessage( inputdata_t &inputdata );
	void InputSetMessage( inputdata_t &inputdata );
	void InputSetMessagePriority1( inputdata_t &inputdata );
	void InputSetMessagePriority2( inputdata_t &inputdata );
	void InputSetMessagePriority3( inputdata_t &inputdata );
	void InputSetMessagePriority4( inputdata_t &inputdata );
	void InputSetMessagePriority5( inputdata_t &inputdata );
	void InputSetMessagePriority6( inputdata_t &inputdata );
	void InputSetMessagePriority7( inputdata_t &inputdata );
	void InputSetMessagePriority8( inputdata_t &inputdata );
	void InputSetMessagePriority9( inputdata_t &inputdata );
	void InputSetMessagePriority10( inputdata_t &inputdata );
	void InputSetMessagePriority11( inputdata_t &inputdata );
	void InputSetMessagePriority12( inputdata_t &inputdata );
	void InputSetMessagePriority13( inputdata_t &inputdata );
	void InputSetMessagePriority14( inputdata_t &inputdata );
	void InputSetMessagePriority15( inputdata_t &inputdata );
	void InputSetMessagePriority16( inputdata_t &inputdata );
	void InputRemoveMessagePriority( inputdata_t &inputdata );

	void SetMessageFromString( const char *pszMessage );
	const char *ParseTitlesReference( const char *pszInput, char *outBuf, int outBufSize );
	bool GetTitlesEntry( const char *pszEntryName, hudtextparms_t &outParms, char *outMessage, int outMessageSize );

	void ShowMessageText( const char *psz );
	const char *GetActiveMessage( void );
	void SetPriorityMessage( int prio, const char *psz );
	void UH_SyncGlobalMessagePriority( void );
	void UH_RestoreGlobalMessagePriority( void );

	string_t m_iszMessage;
	string_t m_iszSetMessage;
	float m_MessageVolume;
	int m_MessageAttenuation;
	float m_Radius;

	DECLARE_DATADESC();

	string_t m_sNoise;
	COutputEvent m_OnShowMessage;

	string_t m_iszMessagesPriority[16];
	int m_iCurrentPriority;
	int m_iGlobalEnvMessageIndex;
};

#endif // ENVMESSAGE_H
