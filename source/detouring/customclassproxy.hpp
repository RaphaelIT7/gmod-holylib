/*************************************************************************
* Detouring::ClassProxy
* A C++ class that allows you to "proxy" virtual tables and receive
* calls in substitute classes. Contains helpers for detouring regular
* member functions as well.
*------------------------------------------------------------------------
* Copyright (c) 2017-2022, Daniel Almeida
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************/

#pragma once

#include "detouring/hook.hpp"
#include "detouring/helpers.hpp"
#include "Platform.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace Detouring
{
	using CacheMap = std::unordered_map<void *, Member>;
	using HookMap = std::unordered_map<void *, Detouring::Hook>;

	template<typename Target, typename Substitute>
	class ClassProxy
	{
	protected:
		ClassProxy( ) = default;

		ClassProxy( Target *instance )
		{
			Initialize( instance );
		}

		virtual ~ClassProxy( ) = default;

	public:
		bool Initialize( Target *instance, Substitute *substitute )
		{
			auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			return shared_state->Initialize( instance, substitute );
		}

		inline bool Initialize( Target *instance )
		{
			return Initialize( instance, static_cast<Substitute *>( this ) );
		}

		inline Target *This( )
		{
			return reinterpret_cast<Target *>( this );
		}

		template<
			typename Definition,
			typename Traits = FunctionTraits<Definition>,
			std::enable_if_t<!Traits::IsMemberFunctionPointer, int> = 0
		>
		bool IsHooked( Definition original )
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			return shared_state->hooks.find( reinterpret_cast<void*>( original ) ) != shared_state->hooks.end( );
		}

		template<
			typename Definition,
			typename Traits = FunctionTraits<Definition>,
			std::enable_if_t<Traits::IsMemberFunctionPointer, int> = 0
		>
		bool IsHooked( Definition original )
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			const auto it = shared_state->hooks.find( GetAddress( original ) );
			if( it != shared_state->hooks.end( ) )
				return true;

			Member vtarget = GetVirtualAddress( shared_state->target_vtable, original );
			if( !vtarget.IsValid( ) )
				return false;

			return shared_state->target_vtable.pointer[vtarget.index] != shared_state->original_vtable[vtarget.index];
		}

		template<
			typename DefinitionOriginal,
			typename DefinitionSubstitute,
			typename TraitsOriginal = FunctionTraits<DefinitionOriginal>,
			typename = FunctionTraits<DefinitionSubstitute>,
			std::enable_if_t<!TraitsOriginal::IsMemberFunctionPointer, int> = 0
		>
		bool Hook(
			DefinitionOriginal original,
			DefinitionSubstitute substitute
		)
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			void *address = reinterpret_cast<void *>( original );
			if( address == nullptr )
				return false;

			const auto it = shared_state->hooks.find( address );
			if( it != shared_state->hooks.end( ) )
				return true;

			void *subst = GetAddress( substitute );
			if( subst == nullptr )
				return false;

			Detouring::Hook &hook = shared_state->hooks[address];
			if( !hook.Create( address, subst ) )
			{
				shared_state->hooks.erase( address );
				return false;
			}

			return hook.Enable( );
		}

		template<
			typename DefinitionOriginal,
			typename DefinitionSubstitute,
			typename TraitsOriginal = FunctionTraits<DefinitionOriginal>,
			typename = FunctionTraits<DefinitionSubstitute>,
			std::enable_if_t<TraitsOriginal::IsMemberFunctionPointer, int> = 0
		>
		bool Hook(
			DefinitionOriginal original,
			DefinitionSubstitute substitute
		)
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			Member target = GetVirtualAddress( shared_state->target_vtable, original );
			if( target.IsValid( ) )
			{
				if( shared_state->target_vtable.pointer[target.index] != shared_state->original_vtable[target.index] )
					return true;

				Member subst = GetVirtualAddress( shared_state->substitute_vtable, substitute );
				if( !subst.IsValid( ) )
					return false;

				ProtectMemory( shared_state->target_vtable.pointer + target.index, sizeof( void * ), false );
				shared_state->target_vtable.pointer[target.index] = subst.address;
				ProtectMemory( shared_state->target_vtable.pointer + target.index, sizeof( void * ), true );

				return true;
			}

			void *address = GetAddress( original );
			if( address == nullptr )
				return false;

			const auto it = shared_state->hooks.find( address );
			if( it != shared_state->hooks.end( ) )
				return true;

			void *subst = GetAddress( substitute );
			if( subst == nullptr )
				return false;

			Detouring::Hook &hook = shared_state->hooks[address];
			if( !hook.Create( address, subst ) )
			{
				shared_state->hooks.erase( address );
				return false;
			}

			return hook.Enable( );
		}

		template<
			typename Definition,
			typename Traits = FunctionTraits<Definition>,
			std::enable_if_t<!Traits::IsMemberFunctionPointer, int> = 0
		>
		bool UnHook( Definition original )
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			const auto it = shared_state->hooks.find( reinterpret_cast<void *>( original ) );
			if( it != shared_state->hooks.end( ) )
			{
				shared_state->hooks.erase( it );
				return true;
			}

			return false;
		}

		template<
			typename Definition,
			typename Traits = FunctionTraits<Definition>,
			std::enable_if_t<Traits::IsMemberFunctionPointer, int> = 0
		>
		bool UnHook( Definition original )
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return false;

			const auto it = shared_state->hooks.find( GetAddress( original ) );
			if( it != shared_state->hooks.end( ) )
			{
				shared_state->hooks.erase( it );
				return true;
			}

			Member target = GetVirtualAddress( shared_state->target_vtable, original );
			if( !target.IsValid( ) )
				return false;

			void *vfunction = shared_state->original_vtable[target.index];
			if( shared_state->target_vtable.pointer[target.index] == vfunction )
				return false;

			ProtectMemory( shared_state->target_vtable.pointer + target.index, sizeof( void * ), false );
			shared_state->target_vtable.pointer[target.index] = vfunction;
			ProtectMemory( shared_state->target_vtable.pointer + target.index, sizeof( void * ), true );

			return true;
		}

		template<
			typename Definition,
			typename... Args,
			typename Traits = FunctionTraits<Definition>,
			typename ReturnType = typename Traits::ReturnType,
			std::enable_if_t<!Traits::IsMemberFunctionPointer, int> = 0
		>
		ReturnType Call(
			Target *instance,
			Definition original,
			Args &&... args
		)
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return ReturnType( );

			void *address = reinterpret_cast<void *>( original ), *target = nullptr;
			const auto it = shared_state->hooks.find( address );
			if( it != shared_state->hooks.end( ) )
				target = it->second.GetTrampoline( );

			if( target == nullptr )
				target = address;

			if( target == nullptr )
				return ReturnType( );

			auto method = reinterpret_cast<Definition>( target );
			return method( instance, std::forward<Args>( args )... );
		}

		template<
			typename Definition,
			typename... Args,
			typename Traits = FunctionTraits<Definition>,
			typename ReturnType = typename Traits::ReturnType,
			std::enable_if_t<Traits::IsMemberFunctionPointer, int> = 0
		>
		ReturnType Call(
			Target *instance,
			Definition original,
			Args &&... args
		)
		{
			const auto shared_state = GetSharedState( );
			if( !shared_state )
				return ReturnType( );

			void *address = GetAddress( original );
			void *final_address = nullptr;
			const auto it = shared_state->hooks.find( address );
			if( it != shared_state->hooks.end( ) )
				final_address = it->second.GetTrampoline( );

			if( final_address == nullptr )
			{
				Member member = GetVirtualAddress( shared_state->target_vtable, original );
				if( member.IsValid( ) )
					final_address = shared_state->original_vtable[member.index];
			}

			if( final_address == nullptr )
			{
				if( address == nullptr )
					return ReturnType( );

				final_address = address;
			}

			struct CallMagic
			{
				const void *address = nullptr;
				const size_t offset = 0;
				const size_t unused[2] = { 0, 0 };
			} func = { final_address };
			auto typedfunc = reinterpret_cast<Definition *>( &func );
			return ( instance->**typedfunc )( std::forward<Args>( args )... );
		}

		template<
			typename Definition,
			typename... Args,
			typename Traits = FunctionTraits<Definition>,
			typename ReturnType = typename Traits::ReturnType
		>
		inline ReturnType Call( Definition original, Args &&... args )
		{
			return Call( This( ), original, std::forward<Args>( args )... );
		}

	private:
		struct VTable
		{
			size_t size = 0;
			void **pointer = nullptr;
			CacheMap cache;
		};

		template<
			typename Definition,
			typename Traits = FunctionTraits<Definition>,
			std::enable_if_t<Traits::IsMemberFunctionPointer, int> = 0
		>
		Member GetVirtualAddress(
			VTable &vtable,
			Definition method
		)
		{
			void *member = GetAddress( method );
			const auto it = vtable.cache.find( member );
			if( it != vtable.cache.end( ) )
				return it->second;

			Member address = Detouring::GetVirtualAddress( vtable.pointer, vtable.size, method );

			if( address.IsValid( ) )
				vtable.cache[member] = address;

			return address;
		}

		class SharedState
		{
		public:
			~SharedState( )
			{
				if( target_vtable.pointer == nullptr || target_vtable.size == 0 )
					return;

				ProtectMemory( target_vtable.pointer, target_vtable.size * sizeof( void * ), false );

				void **vtable = target_vtable.pointer;
				for(
					auto it = original_vtable.begin( );
					it != original_vtable.end( );
					++vtable, ++it
				)
					if( *vtable != *it )
						*vtable = *it;

				ProtectMemory( target_vtable.pointer, target_vtable.size * sizeof( void * ), true );
			}

			bool Initialize( Target *instance, Substitute *substitute )
			{
				if( target_vtable.pointer != nullptr )
					return true;

				target_vtable.pointer = GetVirtualTable( instance );
				if( target_vtable.pointer == nullptr )
					return false;

				std::vector<void *> ovtable;
				for( void **vtable = target_vtable.pointer; ovtable.size( ) < ovtable.max_size( ) && *vtable != nullptr; ++vtable )
				{
					if( !IsExecutableAddress( *vtable ) )
						break;

					ovtable.push_back( *vtable );
				}

				if( ovtable.empty( ) )
				{
					target_vtable.pointer = nullptr;
					return false;
				}

				original_vtable = std::move( ovtable );
				target_vtable.size = original_vtable.size( );

				substitute_vtable.pointer = GetVirtualTable( substitute );

				for( ; substitute_vtable.pointer[substitute_vtable.size] != nullptr; ++substitute_vtable.size );

				return true;
			}

			VTable target_vtable;
			std::vector<void *> original_vtable;
			VTable substitute_vtable;
			HookMap hooks;
		};

		std::shared_ptr<SharedState> GetSharedState()
		{
			return state;
		}

		const std::shared_ptr<SharedState> state = std::make_shared<SharedState>();
	};
}