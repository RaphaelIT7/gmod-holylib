#pragma once

namespace Bootil
{
	using BString = std::string;
	using WString = std::wstring;
}

#define BOOTIL_FOREACH( varname, arrayname, arraytype ) for ( arraytype::iterator varname = arrayname.begin(); varname != arrayname.end(); ++varname )

#define BOOTIL_FOREACH_CONST( varname, arrayname, arraytype ) for ( arraytype::const_iterator varname = arrayname.begin(); varname != arrayname.end(); ++varname )

