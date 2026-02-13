#ifndef GAMEPADUI_STRING_H
#define GAMEPADUI_STRING_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui/ILocalize.h"
#include "tier1/utlvector.h"

extern vgui::ILocalize *g_pVGuiLocalize;

class GamepadUIString
{
public:
    GamepadUIString()
    {
    }

    GamepadUIString( const char *pszText )
    {
        SetText( pszText );
    }

    GamepadUIString( const wchar_t *pszText )
    {
        SetText( pszText );
    }

    GamepadUIString( const wchar_t *pszText, int nLength )
    {
        SetText( pszText, nLength );
    }

    const wchar_t *String() const
    {
        if ( m_ManagedText.Count() )
            return m_ManagedText.Base();

        return L"";
    }

    int Length() const
    {
        if ( m_ManagedText.Count() )
            return m_ManagedText.Count() - 1;

        return 0;
    }

    bool IsEmpty() const
    {
        return Length() == 0;
    }

    void SetText( const char *pszText )
    {
        m_ManagedText.Purge();

        if ( !pszText || !*pszText )
            return;

        const wchar_t *pszFoundText = g_pVGuiLocalize->Find( pszText );
        if ( !pszFoundText )
        {
            SetRawUTF8( pszText );
        }
        else
        {
            int nChars = V_wcslen( pszFoundText );
            SetText( pszFoundText, nChars );
        }
    }

    void SetText( const wchar_t *pszText, int nLength )
    {
        m_ManagedText.Purge();

        if ( !pszText || !nLength )
            return;

        m_ManagedText.EnsureCapacity( nLength + 1 );
        for ( int i = 0; i < nLength; i++ )
            m_ManagedText.AddToTail( pszText[ i ] );
        m_ManagedText.AddToTail( L'\0' );
    }

    void SetText( const wchar_t *pszText )
    {
        if ( !pszText )
            SetText( NULL, 0 );
        else
            SetText( pszText, V_wcslen( pszText ) );
    }

    void SetRawUTF8( const char* pszText )
    {
	if ( !pszText || !*pszText )
            return;

	m_ManagedText.Purge();

	int len = V_UTF8ToUnicode(pszText, nullptr, 0);
	if (len <= 0)
		return;

	wchar_t *unicode = (wchar_t*)malloc(len * sizeof(wchar_t));
	if ( !unicode )
		return;

	int vlen = V_UTF8ToUnicode(pszText, unicode, len * sizeof(wchar_t));
	if ( vlen > 0 )
    	{
		SetText(unicode, vlen / sizeof(wchar_t) - 1);
	}

	free(unicode);
    }
private:
    CCopyableUtlVector< wchar_t > m_ManagedText;
};

#endif // GAMEPADUI_STRING_H

