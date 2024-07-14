#include "Bootil/Bootil.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"

namespace Bootil
{
	namespace Data
	{
		namespace Json
		{
			template <typename Writer>
			void WriteValue( Writer & writer, const Bootil::Data::Tree & tree )
			{
				if ( tree.IsVar<float>() )
				{
					writer.Double( tree.Var<float>() );
					return;
				}

				if ( tree.IsVar<double>() )
				{
					writer.Double( tree.Var<double>() );
					return;
				}

				if ( tree.IsVar<int>() )
				{
					writer.Int( tree.Var<int>() );
					return;
				}

				if ( tree.IsVar<bool>() )
				{
					writer.Bool( tree.Var<bool>() );
					return;
				}

				writer.String( tree.Value().c_str() );
			}

			template <typename Writer>
			void DoJsonExport( Writer & writer, const Bootil::Data::Tree & tree )
			{
				//
				// If none of the children have keys then it's an array
				//
				bool IsArray = true;
				BOOTIL_FOREACH_CONST( a, tree.Children(), Bootil::Data::Tree::List )
				{
					if ( !a->Name().empty() )
					{
						IsArray = false;
						break;
					}
				}

				if ( IsArray )
				{ writer.StartArray(); }
				else
				{ writer.StartObject(); }

				int iUniqueKey = 1;
				BOOTIL_FOREACH_CONST( a, tree.Children(), Bootil::Data::Tree::List )
				{
					const Bootil::BString & strKey = a->Name();

					// If we're not an array then write the key
					if ( !IsArray )
					{
						if ( strKey.empty() )
						{
							writer.String( Bootil::String::Format::Print( "_%i_", iUniqueKey ).c_str() );
							iUniqueKey++;
						}
						else
						{
							writer.String( strKey.c_str() );
						}
					}

					if ( a->HasChildren() || a->IsBranch() )
					{
						// If it's a parent we ignore the value and
						// just treat it like an object/array
						DoJsonExport<Writer>( writer, *a );
					}
					else
					{
						WriteValue( writer, *a );
					}
				}

				if ( IsArray )
				{ writer.EndArray(); }
				else
				{ writer.EndObject(); }
			}

			bool Export( const Bootil::Data::Tree & tree, Bootil::BString & output, bool bPretty )
			{
				rapidjson::StringBuffer f;

				if ( bPretty )
				{
					rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( f );
					writer.SetIndent( '\t', 1 );
					DoJsonExport< rapidjson::PrettyWriter<rapidjson::StringBuffer> >( writer, tree );
				}
				else
				{
					rapidjson::Writer<rapidjson::StringBuffer> writer( f );
					DoJsonExport< rapidjson::Writer<rapidjson::StringBuffer> >( writer, tree );
				}

				output = f.GetString();
				return true;
			}


			void DoImport( Bootil::Data::Tree & tree, rapidjson::Document::ValueType & doc )
			{
				if ( doc.IsArray() )
				{
					rapidjson::Document::ValueType::ValueIterator it = doc.Begin();

					while ( it != doc.End() )
					{
						if ( it->IsObject() || it->IsArray() )
						{
							Bootil::Data::Tree & child = tree.AddChild();
							DoImport( child, *it );
						}
						else if ( it->IsString() )
						{
							Bootil::Data::Tree & child = tree.AddChild();
							child.Value( it->GetString() );
						}
						else if ( it->IsBool() )
						{
							Bootil::Data::Tree & child = tree.AddChild();
							child.Var<bool>( it->GetBool() );
						}
						else if ( it->IsNumber() )
						{
							Bootil::Data::Tree & child = tree.AddChild();
							child.Var<double>( it->GetDouble() );
						}
						else if ( it->IsInt() )
						{
							Bootil::Data::Tree & child = tree.AddChild();
							child.Var<int>( it->GetInt() );
						}

						++it;
					}
				}

				if ( doc.IsObject() )
				{
					rapidjson::Document::MemberIterator it = doc.MemberBegin();

					while ( it != doc.MemberEnd() )
					{
						if ( it->value.IsObject() || it->value.IsArray() )
						{
							Bootil::Data::Tree & child = tree.AddChild( it->name.GetString() );
							DoImport( child, it->value );
						}
						else if ( it->value.IsString() )
						{
							tree.SetChild( it->name.GetString(), it->value.GetString() );
						}
						else if ( it->value.IsBool() )
						{
							tree.SetChildVar<bool>( it->name.GetString(), it->value.GetBool() );
						}
						else if ( it->value.IsNumber() )
						{
							tree.SetChildVar<double>( it->name.GetString(), it->value.GetDouble() );
						}
						else if ( it->value.IsInt() )
						{
							tree.SetChildVar<int>( it->name.GetString(), it->value.GetInt() );
						}

						++it;
					}
				}
			}

			bool Import( Bootil::Data::Tree & tree, const char* strInput )
			{
				rapidjson::Document doc;

				{
					if ( doc.Parse<0>( strInput ).HasParseError() )
					{ return false; }
				}

				if ( doc.IsObject() || doc.IsArray() )
				{
					DoImport( tree, doc );
				}

				return true;
			}
		}
	}
}
