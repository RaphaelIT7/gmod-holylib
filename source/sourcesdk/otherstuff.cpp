#include "tier0/dbg.h"
#include <tier1/strtools.h>
#include "Platform.hpp"
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
// Purpose: Initializes a steam ID from a string
// Input  : pchSteamID -		text representation of a Steam ID
//-----------------------------------------------------------------------------
void CSteamID::SetFromString( const char *pchSteamID, EUniverse eDefaultUniverse )
{
	uint nAccountID = 0;
	uint nInstance = 1;
	EUniverse eUniverse = eDefaultUniverse;
	EAccountType eAccountType = k_EAccountTypeIndividual;
#ifdef DBGFLAG_ASSERT
	// TF Merge -- Assert is debug-only and we have unused variable warnings on :-/
    const char *pchSteamIDString = pchSteamID;
#endif
    CSteamID StrictID;

    StrictID.SetFromStringStrict( pchSteamID, eDefaultUniverse );

	if ( *pchSteamID == '[' )
		pchSteamID++;

	// BUGBUG Rich use the Q_ functions
	if (*pchSteamID == 'A')
	{
		// This is test only
		pchSteamID++; // skip the A
		eAccountType = k_EAccountTypeAnonGameServer;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :

		if ( auto *brace = strchr( pchSteamID, '(' ) )
			sscanf( brace, "(%u)", &nInstance );
		const char *pchColon = strchr( pchSteamID, ':' );
		if ( pchColon && *pchColon != 0 && strchr( pchColon+1, ':' ))
		{
			sscanf( pchSteamID, "%u:%u:%u", (uint*)&eUniverse, &nAccountID, &nInstance );
		}
		else if ( pchColon )
		{
			sscanf( pchSteamID, "%u:%u", (uint*)&eUniverse, &nAccountID );
		}
		else
		{
			sscanf( pchSteamID, "%u", &nAccountID );
		}

		if ( nAccountID == 0 )
		{
			// i dont care what number you entered
			CreateBlankAnonLogon(eUniverse);
		}
		else
		{
			InstancedSet( nAccountID, nInstance, eUniverse, eAccountType );
		}
        // Catch cases where we're allowing sloppy input that we
        // might not want to allow.
        AssertMsg1( this->operator==( StrictID ), "Steam ID does not pass strict parsing: '%s'", pchSteamIDString );
		return;
	}
	else if (*pchSteamID == 'G')
	{
		pchSteamID++; // skip the G
		eAccountType = k_EAccountTypeGameServer;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'C')
	{
		pchSteamID++; // skip the C
		eAccountType = k_EAccountTypeContentServer;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'g')
	{
		pchSteamID++; // skip the g
		eAccountType = k_EAccountTypeClan;
		nInstance = 0;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'c')
	{
		pchSteamID++; // skip the c
		eAccountType = k_EAccountTypeChat;
		nInstance = k_EChatInstanceFlagClan;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'L')
	{
		pchSteamID++; // skip the c
		eAccountType = k_EAccountTypeChat;
		nInstance = k_EChatInstanceFlagLobby;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'T')
	{
		pchSteamID++; // skip the T
		eAccountType = k_EAccountTypeChat;
		nInstance = 0;	// Anon chat
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'U')
	{
		pchSteamID++; // skip the U
		eAccountType = k_EAccountTypeIndividual;
		nInstance = 1;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}
	else if (*pchSteamID == 'i')
	{
		pchSteamID++; // skip the i
		eAccountType = k_EAccountTypeInvalid;
		nInstance = 1;
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
	}

	if ( strchr( pchSteamID, ':' ) )
	{
		if (*pchSteamID == '[')
			pchSteamID++; // skip the optional [
		sscanf( pchSteamID, "%u:%u", (uint*)&eUniverse, &nAccountID );
		if ( eUniverse == k_EUniverseInvalid )
			eUniverse = eDefaultUniverse; 
	}
	else
	{
        uint64 unVal64 = 0;
        
		sscanf( pchSteamID, "%llu", &unVal64 );
        if ( unVal64 > UINT64_MAX )
        {
            // Assume a full 64-bit Steam ID.
            SetFromUint64( unVal64 );
            // Catch cases where we're allowing sloppy input that we
            // might not want to allow.
            AssertMsg1( this->operator==( StrictID ), "Steam ID does not pass strict parsing: '%s'", pchSteamIDString );
            return;
        }
        else
        {
            nAccountID = (uint)unVal64;
        }
	}	
	
	Assert( (eUniverse > k_EUniverseInvalid) && (eUniverse < k_EUniverseMax) );

	InstancedSet( nAccountID, nInstance, eUniverse, eAccountType );

    // Catch cases where we're allowing sloppy input that we
    // might not want to allow.
    AssertMsg1( this->operator==( StrictID ), "Steam ID does not pass strict parsing: '%s'", pchSteamIDString );
}

static const char *DecimalToUint64( const char *pchStr, uint64 unLimit,
                                    uint64 *punVal )
{
    const char *pchStart = pchStr;
    uint64 unVal = 0;

    while ( *pchStr >= '0' && *pchStr <= '9' )
    {
        uint64 unNext = unVal * 10;
        
        if ( unNext < unVal )
        {
            // 64-bit overflow.
            return nullptr;
        }

        unVal = unNext + (uint64)( *pchStr - '0' );
        if ( unVal > unLimit )
        {
            // Limit overflow.
            return nullptr;
        }

        pchStr++;
    }
    if ( pchStr == pchStart )
    {
        // No number at all.
        return nullptr;
    }

    *punVal = unVal;
    return pchStr;
}

// SetFromString allows many partially-correct strings, constraining how
// we might be able to change things in the future.
// SetFromStringStrict requires the exact string forms that we support
// and is preferred when the caller knows it's safe to be strict.
// Returns whether the string parsed correctly.  The ID may
// still be invalid even if the string parsed correctly.
// If the string didn't parse correctly the ID will always be invalid.
bool CSteamID::SetFromStringStrict( const char *pchSteamID, EUniverse eDefaultUniverse )
{
	uint nAccountID = 0;
	uint nInstance = 1;
    uint unMaxVal = 2;
	EUniverse eUniverse = eDefaultUniverse;
	EAccountType eAccountType = k_EAccountTypeIndividual;
    char chPrefix;
    bool bBracket = false;
    bool bValid = true;
    uint64 unVal[3];
    const char *pchEnd;

    // Start invalid.
    Clear();
    
    if ( !pchSteamID )
    {
        return false;
    }
    
	if ( *pchSteamID == '[' )
    {
		pchSteamID++;
        bBracket = true;
    }

    chPrefix = *pchSteamID;
    switch( chPrefix )
    {
    case 'A':
		// This is test only
		eAccountType = k_EAccountTypeAnonGameServer;
        unMaxVal = 3;
        break;

    case 'G':
		eAccountType = k_EAccountTypeGameServer;
        break;

    case 'C':
		eAccountType = k_EAccountTypeContentServer;
        break;

    case 'g':
		eAccountType = k_EAccountTypeClan;
		nInstance = 0;
        break;

    case 'c':
		eAccountType = k_EAccountTypeChat;
		nInstance = k_EChatInstanceFlagClan;
        break;

    case 'L':
		eAccountType = k_EAccountTypeChat;
		nInstance = k_EChatInstanceFlagLobby;
        break;

    case 'T':
		eAccountType = k_EAccountTypeChat;
		nInstance = 0;	// Anon chat
        break;

    case 'U':
		eAccountType = k_EAccountTypeIndividual;
		nInstance = 1;
        break;

    case 'i':
		eAccountType = k_EAccountTypeInvalid;
		nInstance = 1;
        break;

    default:
        // We're reserving other leading characters so
        // this should only be the plain-digits case.
        if (chPrefix < '0' || chPrefix > '9')
        {
            bValid = false;
        }
        chPrefix = 0;
        break;
    }

    if ( chPrefix )
    {
        pchSteamID++; // skip the prefix
		if (*pchSteamID == '-' || *pchSteamID == ':')
			pchSteamID++; // skip the optional - or :
    }

    uint unIdx = 0;

    for (;;)
    {
        pchEnd = DecimalToUint64( pchSteamID, UINT64_MAX, &unVal[unIdx] );
        if ( !pchEnd )
        {
            bValid = false;
            break;
        }

        unIdx++;

        // For 'A' we can have a trailing instance, which must
        // be the end of the string.
        if ( *pchEnd == '(' &&
             chPrefix == 'A' )
        {
            if ( unIdx > 2 )
            {
                // Two instance IDs provided.
                bValid = false;
            }
            
            pchEnd = DecimalToUint64( pchEnd + 1, k_unSteamAccountInstanceMask, &unVal[2] );
            if ( !pchEnd ||
                 *pchEnd != ')' )
            {
                bValid = false;
                break;
            }
            else
            {
                nInstance = (uint)unVal[2];

                pchEnd++;
                if ( *pchEnd == ':' )
                {
                    // Not expecting more values.
                    bValid = false;
                    break;
                }
            }
        }

        if ( *pchEnd != ':' )
        {
            if ( bBracket )
            {
                if ( *pchEnd != ']' ||
                     *(pchEnd + 1) != 0 )
                {
                    bValid = false;
                }
            }
            else if ( *pchEnd != 0 )
            {
                bValid = false;
            }

            break;
        }

        if ( unIdx >= unMaxVal )
        {
            bValid = false;
            break;
        }

        pchSteamID = pchEnd + 1;
    }

    if ( unIdx > 2 )
    {
        if ( unVal[2] <= k_unSteamAccountInstanceMask )
        {
            nInstance = (uint)unVal[2];
        }
        else
        {
            bValid = false;
        }
    }
    if ( unIdx > 1 )
    {
        if ( unVal[0] >= k_EUniverseInvalid &&
             unVal[0] < k_EUniverseMax )
        {
            eUniverse = (EUniverse)unVal[0];
            if ( eUniverse == k_EUniverseInvalid )
                eUniverse = eDefaultUniverse;
        }
        else
        {
            bValid = false;
        }

        if ( unVal[1] <= k_unSteamAccountIDMask )
        {
            nAccountID = (uint)unVal[1];
        }
        else
        {
            bValid = false;
        }
    }
    else if ( unIdx > 0 )
    {
        if ( unVal[0] <= k_unSteamAccountIDMask )
        {
            nAccountID = (uint)unVal[0];
        }
        else if ( !chPrefix )
        {
            if ( bValid )
            {
                SetFromUint64( unVal[0] );
            }
            return bValid;
        }
        else
        {
            bValid = false;
        }
    }
    else
    {
        bValid = false;
    }

    if ( bValid )
    {
        if ( chPrefix == 'A' )
        {
            if ( nAccountID == 0 )
            {
                // i dont care what number you entered
                CreateBlankAnonLogon(eUniverse);
                return bValid;
            }
        }

        InstancedSet( nAccountID, nInstance, eUniverse, eAccountType );
    }

    return bValid;
}