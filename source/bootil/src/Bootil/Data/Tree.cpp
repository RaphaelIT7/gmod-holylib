#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace Data
	{
		// String (generic value)
		template<> template<> unsigned char Tree::VarID<Bootil::BString>() const			{ return 1; }

		// Float
		template<> template<> unsigned char Tree::VarID<float>() const						{ return 2; }
		template<> template<> Bootil::BString Tree::VarToString( float var ) const			{ return Bootil::String::Format::NiceFloat( var );}
		template<> template<> float Tree::StringToVar( const Bootil::BString & var ) const	{ return Bootil::String::To::Float( var ); }

		// int
		template<> template<> unsigned char Tree::VarID<int>() const						{ return 3; }
		template<> template<> Bootil::BString Tree::VarToString( int var ) const			{ return Bootil::String::Format::Int( var );}
		template<> template<> int Tree::StringToVar( const Bootil::BString & var ) const		{ return Bootil::String::To::Int( var ); }

		// Bool
		template<> template<> unsigned char Tree::VarID<bool>() const						{ return 4; }
		template<> template<> Bootil::BString Tree::VarToString( bool var ) const			{ return var ? "true" : "false";}
		template<> template<> bool Tree::StringToVar( const Bootil::BString & var ) const	{ return Bootil::String::To::Bool( var ); }

		// Double
		template<> template<> unsigned char Tree::VarID<double>() const						{ return 5; }
		template<> template<> Bootil::BString Tree::VarToString( double var ) const			{ return Bootil::String::Format::NiceDouble( var );}
		template<> template<> double Tree::StringToVar( const Bootil::BString & var ) const	{ return Bootil::String::To::Double( var ); }
	}
}
