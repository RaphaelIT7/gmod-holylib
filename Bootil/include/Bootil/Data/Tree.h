#pragma once

namespace Bootil
{
	namespace Data
	{
		//
		// The tree is a simple recursive Name-Value data structure.
		// Each node can have a key, a value and multiple children nodes
		//
		template <typename TString>
		class BOOTIL_EXPORT TreeT
		{
			public:

				typedef TreeT<TString>						ThisClass;
				typedef typename std::list< ThisClass >		List;

			public:

				TreeT()
				{
					m_Info = 0;
				}

				const TString & Name() const;
				void Name( const TString & name );

				const TString & Value() const;
				void Value( const TString & value );

				//
				// Returns true if we have some children
				//
				// BOOTIL_FOREACH[_CONST]( child, mytree.Children(), Bootil::Data::Tree::List )
				// {
				//		// Do stuff to 'child'
				// }
				//
				bool HasChildren() const;

				//
				// Returns a list of children.
				//
				const List & Children() const { return m_Children; }
				List & Children() { return m_Children; }

				//
				// Adding and setting children
				// mychild = AddChild();					[adds an unnamed child]
				// mychild = AddChild( "Name" );			[adds a named child]
				// mychild = SetChild( "Name", "Value" );	[adds a named child with value]
				// mychild = GetChild( "Name" );			[returns child, or creates child if not exists]
				// bool b  = HasChild( "Name" );			[returns true if child exists]
				// mychild = GetChildNum( 7 );				[returns 7th child, or creates 7 child if not exists (if empty, will create 7 children and return the 7th etc)]
				TreeT<TString> & AddChild();
				TreeT<TString> & AddChild( TString name );
				TreeT<TString> & SetChild( TString strKey, TString strValue );
                TreeT<TString> & SetChild( TString strValue ) { return SetChild( "", strValue ); }
				TreeT<TString> & GetChild( const TString & name );
				bool			HasChild( const TString & name ) const;
				TreeT<TString> & GetChildNum( int iNum );

				//
				// Getting Child Value
				// value = Value( "Name", "Default" ) [returns 'Default' if 'Name' isn't found]
				//
				TString ChildValue( const TString & name, const TString & Default = "" ) const;


				//
				// Setting non-string values
				//
				template <typename TValue> TreeT<TString> & SetChildVar( TString strKey, TValue strValue );
				template <typename TValue> TValue ChildVar( TString strKey, TValue Default ) const;
				template <typename TValue> void Var( TValue strValue );
				template <typename TValue> TValue Var() const;
				template <typename TValue> bool IsVar() const;

				// Utility methods
				//
				template <typename TValue> TString VarToString( TValue var ) const;
				template <typename TValue> TValue StringToVar( const TString & str ) const;
				template <typename TValue> unsigned char VarID() const;
				bool IsBranch() const { return m_Info == 0; }

				//
				// Clean up
				//
				void Clear() { m_Children.clear(); }

			protected:

				TString			m_Name;
				TString			m_Value;
				unsigned char	m_Info;

				List		m_Children;
		};


		//
		// We have a normal version and a wide version by default
		// Although the wide version is probably a waste of time since
		// nothing supports it right now.
		//
		typedef TreeT<Bootil::BString> Tree;
		typedef TreeT<Bootil::WString> TreeW;


		//
		// Template function definitions
		//
		template <typename TString>
		const TString & TreeT<TString>::Name() const
		{
			return m_Name;
		}

		template <typename TString>
		void TreeT<TString>::Name( const TString & name )
		{
			m_Name = name;
		}

		template <typename TString>
		const TString & TreeT<TString>::Value() const
		{
			return m_Value;
		}

		template <typename TString>
		void TreeT<TString>::Value( const TString & value )
		{
			m_Info = 1;
			m_Value = value;
		}

		template <typename TString>
		TreeT<TString> & TreeT<TString>::AddChild()
		{
			{
				TreeT<TString> t;
				m_Children.push_back( t );
			}
			TreeT<TString> & t = m_Children.back();
			return t;
		}

		template <typename TString>
		TreeT<TString> & TreeT<TString>::AddChild( TString name )
		{
			TreeT<TString> & tree = AddChild();
			tree.Name( name );
			return tree;
		}

		template <typename TString>
		TreeT<TString> & TreeT<TString>::SetChild( TString strKey, TString strValue )
		{
			TreeT<TString> & tchild = AddChild( strKey );
			tchild.Value( strValue );
			return tchild;
		}

		template <typename TString>
		TString TreeT<TString>::ChildValue( const TString & name, const TString & Default ) const
		{
			BOOTIL_FOREACH_CONST( a, Children(), typename List )
			{
				if ( a->Name() == name ) { return a->Value(); }
			}
			return Default;
		}

		template <typename TString>
		bool TreeT<TString>::HasChild( const TString & name ) const
		{
			BOOTIL_FOREACH_CONST( a, Children(), typename List )
			{
				if ( a->Name() == name ) { return true; }
			}
			return false;
		}

		template <typename TString>
		TreeT<TString> & TreeT<TString>::GetChild( const TString & name )
		{
			BOOTIL_FOREACH( a, Children(), typename List )
			{
				if ( a->Name() == name ) { return ( *a ); }
			}
			return AddChild( name );
		}

		template <typename TString>
		TreeT<TString> & TreeT<TString>::GetChildNum( int iNum )
		{
			BOOTIL_FOREACH( a, Children(), typename List )
			{
				if ( iNum == 0 ) { return ( *a ); }

				iNum--;
			}

			while ( iNum > 0 )
			{
				AddChild();
			}

			return AddChild();
		}

		template <typename TString>
		bool TreeT<TString>::HasChildren() const
		{
			return !m_Children.empty();
		}

		template <typename TString> template <typename TValue> TreeT<TString> & TreeT<TString>::SetChildVar( TString strKey, TValue var )
		{
			TreeT<TString> & tchild = AddChild( strKey );
			tchild.Var<TValue>( var );
			return tchild;
		}

		template <typename TString> template <typename TValue> TValue TreeT<TString>::ChildVar( TString strKey, TValue varDefault ) const
		{
			BOOTIL_FOREACH_CONST( a, Children(), typename List )
			{
				if ( a->Name() == strKey ) { return a->template Var<TValue>(); }
			}
			return varDefault;
		}

		template <typename TString> template <typename TValue> void TreeT<TString>::Var( TValue var )
		{
			m_Info = VarID<TValue>();
			m_Value = VarToString<TValue>( var );
		}

		template <typename TString> template <typename TValue> TValue TreeT<TString>::Var() const
		{
			return StringToVar<TValue>( Value() );
		}

		template <typename TString> template <typename TValue> bool TreeT<TString>::IsVar() const
		{
			return m_Info == VarID<TValue>();
		}

	}

}
