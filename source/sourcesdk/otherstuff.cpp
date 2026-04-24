#include "tier0/dbg.h"
#include <tier1/strtools.h>
#include "Platform.hpp"
#define INCLUDED_STEAM2_USERID_STRUCTS
#include <steamcommon.h>
#include "sourcesdk/baseclient.h"
#include "sourcesdk/dt_encode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PropTypeFns g_PropTypeFns[DPT_NUMSendPropTypes];

//-----------------------------------------------------------------------------
// Purpose: returns true if a wide character is a "mean" space; that is,
//			if it is technically a space or punctuation, but causes disruptive
//			behavior when used in names, web pages, chat windows, etc.
//
//			characters in this set are removed from the beginning and/or end of strings
//			by Q_AggressiveStripPrecedingAndTrailingWhitespaceW() 
//-----------------------------------------------------------------------------
static bool Q_IsMeanSpaceW( wchar_t wch )
{
	bool bIsMean = false;

	switch ( wch )
	{
	case L'\x0082':	  // BREAK PERMITTED HERE
	case L'\x0083':	  // NO BREAK PERMITTED HERE
	case L'\x00A0':	  // NO-BREAK SPACE
	case L'\x034F':   // COMBINING GRAPHEME JOINER
	case L'\x2000':   // EN QUAD
	case L'\x2001':   // EM QUAD
	case L'\x2002':   // EN SPACE
	case L'\x2003':   // EM SPACE
	case L'\x2004':   // THICK SPACE
	case L'\x2005':   // MID SPACE
	case L'\x2006':   // SIX SPACE
	case L'\x2007':   // figure space
	case L'\x2008':   // PUNCTUATION SPACE
	case L'\x2009':   // THIN SPACE
	case L'\x200A':   // HAIR SPACE
	case L'\x200B':   // ZERO-WIDTH SPACE
	case L'\x200C':   // ZERO-WIDTH NON-JOINER
	case L'\x200D':   // ZERO WIDTH JOINER
	case L'\x200E':	  // LEFT-TO-RIGHT MARK
	case L'\x2028':   // LINE SEPARATOR
	case L'\x2029':   // PARAGRAPH SEPARATOR
	case L'\x202F':   // NARROW NO-BREAK SPACE
	case L'\x2060':   // word joiner
	case L'\xFEFF':   // ZERO-WIDTH NO BREAK SPACE
	case L'\xFFFC':   // OBJECT REPLACEMENT CHARACTER
		bIsMean = true;
		break;
	}

	return bIsMean;
}

//-----------------------------------------------------------------------------
// Purpose: Strips all evil characters (ie. zero-width no-break space)
//			from a string.
//-----------------------------------------------------------------------------
bool Q_RemoveAllEvilCharacters2( char *pch )
{
	// convert to unicode
	int cch = Q_strlen( pch );
	int cubDest = (cch + 1 ) * sizeof( wchar_t );
	wchar_t *pwch = (wchar_t *)stackalloc( cubDest );
	int cwch = Q_UTF8ToUnicode( pch, pwch, cubDest ) / sizeof( wchar_t );

	bool bStrippedWhitespace = false;

	// Walk through and skip over evil characters
	int nWalk = 0;
	for( int i=0; i<cwch; ++i )
	{
		if( !Q_IsMeanSpaceW( pwch[i] ) )
		{
			pwch[nWalk] = pwch[i];
			++nWalk;
		}
		else
		{
			bStrippedWhitespace = true;
		}
	}

	// Null terminate
	pwch[nWalk-1] = L'\0';
	

	// copy back, if necessary
	if ( bStrippedWhitespace )
	{
		Q_UnicodeToUTF8( pwch, pch, cch );
	}

	return bStrippedWhitespace;
}

void CBaseClient::SetSteamID( const CSteamID &steamID )
{
	m_SteamID = steamID;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes a steam ID from a Steam2 ID string
// Input:	pchSteam2ID -	Steam2 ID (as a string #:#:#) to convert
//			eUniverse -		universe this ID belongs to
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
bool SteamIDFromSteam2String( const char *pchSteam2ID, EUniverse eUniverse, CSteamID *pSteamIDOut )
{
	Assert( pchSteam2ID );

	// Convert the Steam2 ID string to a Steam2 ID structure
	TSteamGlobalUserID steam2ID;
	steam2ID.m_SteamInstanceID = 0;
	steam2ID.m_SteamLocalUserID.Split.High32bits = 0;
	steam2ID.m_SteamLocalUserID.Split.Low32bits	= 0;

	const char *pchTSteam2ID = pchSteam2ID;

	// Customer support is fond of entering steam IDs in the following form:  STEAM_n:x:y
	const char *pchOptionalLeadString = "STEAM_";
	if ( V_strnicmp( pchSteam2ID, pchOptionalLeadString, V_strlen( pchOptionalLeadString ) ) == 0 )
		pchTSteam2ID = pchSteam2ID + V_strlen( pchOptionalLeadString );

	char cExtraCharCheck = 0;

	int cFieldConverted = sscanf( pchTSteam2ID, "%hu:%u:%u%c", &steam2ID.m_SteamInstanceID, 
		&steam2ID.m_SteamLocalUserID.Split.High32bits, &steam2ID.m_SteamLocalUserID.Split.Low32bits, &cExtraCharCheck );

	// Validate the conversion ... a special case is steam2 instance ID 1 which is reserved for special DoD handling
	if ( cExtraCharCheck != 0 || cFieldConverted == EOF || cFieldConverted < 2 || ( cFieldConverted < 3 && steam2ID.m_SteamInstanceID != 1 ) )
		return false;

	// Now convert to steam ID from the Steam2 ID structure
	*pSteamIDOut = SteamIDFromSteam2UserID( &steam2ID, eUniverse );
	return true;
}