#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace URL
		{
			BOOTIL_EXPORT void URI( BString & strURL )
			{
				const BString Protocol( "://" );
				std::size_t pos = strURL.find( Protocol );

				if ( pos != std::string::npos )
				{ pos = strURL.find_first_of( "/", pos + Protocol.length() ); }
				else
				{ pos = strURL.find_first_of( "/" ); }

				if ( pos != std::string::npos )
				{
					strURL = strURL.substr( pos );
					return;
				}

				strURL = "/";
			}

			BOOTIL_EXPORT void Query( BString & strURL )
			{
				URI( strURL );

				if ( strURL.size() < 3 )
				{
					strURL = "";
					return;
				}

				std::size_t pos = strURL.find_first_of( "?" );

				if ( pos != std::string::npos )
				{
					strURL = strURL.substr( pos );
					return;
				}

				strURL = "";
			}

			BOOTIL_EXPORT int ParseArguments( BString strQuery, String::Map & map )
			{
				if ( strQuery[0] == '?' ) { strQuery = strQuery.substr( 1 ); }

				int i = 0;
				String::List lst;
				String::Util::Split( strQuery, "&", lst );
				BOOTIL_FOREACH( str, lst, String::List )
				{
					std::size_t pos = str->find_first_of( "=" );

					if ( pos == std::string::npos ) { continue; }

					if ( pos == 0 ) { continue; }

					if ( pos == str->length() ) { continue; }

					if ( str->find_last_of( "=" ) != pos ) { continue; }

					map[ str->substr( 0, pos ) ] = str->substr( pos + 1 );
					i++;
				}
				return i;
				/*
				int iArguments = 0;

				url_parse::Parsed parsed;
				url_parse::ParseStandardURL( strURL.c_str(), strURL.length(), &parsed );

				url_parse::Component query = parsed.query;
				url_parse::Component key, value;

				while ( url_parse::ExtractQueryKeyValue( strURL.c_str(), &query, &key, &value) )
				{
					map[ strURL.substr( key.begin, key.begin + key.len ) ] = strURL.substr( value.begin, value.begin + value.len );
					iArguments++;
				}

				return iArguments;
				*/
			}
		}
	}
}
