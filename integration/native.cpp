#include "native.h"
#include "client/Environment/Env.h"
#include "core/Roblox/Update.h"
#include "core/Scheduler/Helper.h"
#include "profile.h"
#include "signer.h"
#include <Luau/Bytecode.h>
#include <Luau/BytecodeBuilder.h>
#include <Luau/BytecodeUtils.h>
#include <Luau/Compiler.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace Testing
{
	namespace
	{
		struct NativeString
		{
			std::array<uint8_t, 16> Buffer{ };
			size_t Length = 0;
			size_t Capacity = 15;
		};

		struct Request
		{
			const char* Name;
			const char* Data;
			size_t Size;
			int Environment = 0;
		};

		struct Details
		{
			int Version = -1;
			uint32_t Key = 0;
			NativeString Trailer;
		};

		static_assert( sizeof( NativeString ) == 32 );
		static_assert( sizeof( Details ) == 40 );

		class Encoder : public Luau::BytecodeEncoder
		{
			void encode( uint32_t* Data, size_t Count ) override
			{
				for ( size_t Index = 0; Index < Count; )
				{
					const uint8_t Opcode = LUAU_INSN_OP( Data[ Index ] );
					Data[ Index ] = ( Data[ Index ] & ~0xffu ) | uint8_t( Opcode * 227 );
					Index += Luau::getOpLength( static_cast<LuauOpcode>( Opcode ) );
				}
			}
		};

		uintptr_t Address( uintptr_t Rva )
		{
			return reinterpret_cast<uintptr_t>( GetModuleHandleW( nullptr ) ) + Rva;
		}

		uint32_t VerifyNative( const std::vector<uint8_t>& Bytes )
		{
			Request Input{ "=SignerValidation", reinterpret_cast<const char*>( Bytes.data( ) ), Bytes.size( ) };
			Details Result;
			const auto Verify = reinterpret_cast<uint32_t( __fastcall* )( uintptr_t, const Request*, Details* )>( Address( Profile::Native::Verify ) );
			const auto Destroy = reinterpret_cast<void( __fastcall* )( NativeString* )>( Address( Profile::Native::DestroyString ) );
			const uint32_t Status = Verify( 0, &Input, &Result );
			Destroy( &Result.Trailer );
			return Status;
		}

		std::string Container( const std::vector<uint8_t>& Bytes )
		{
			std::string Result( "RSB1", 4 );
			Result.append( 4, 0 );
			Result.append( reinterpret_cast<const char*>( Bytes.data( ) ), Bytes.size( ) );
			const uint32_t Key = Update::Roblox::Functions::BytecodeHash( Result.data( ), int( Result.size( ) ), 42 );
			for ( size_t Index = 0; Index < Result.size( ); ++Index )
				Result[ Index ] ^= uint8_t( ( Key >> ( ( Index & 3 ) * 8 ) ) + 41 * Index );
			return Result;
		}

		int Load( lua_State* L, const std::vector<uint8_t>& Bytes )
		{
			std::string Encoded = Container( Bytes );
			NativeString Input;
			const char* Pointer = Encoded.data( );
			std::memcpy( Input.Buffer.data( ), &Pointer, sizeof( Pointer ) );
			Input.Length = Encoded.size( );
			Input.Capacity = Encoded.size( );
			const auto Loader = reinterpret_cast<int( __fastcall* )( lua_State*, const NativeString*, const char*, int )>( Update::LuaU::LuaVMLoad );
			return Loader( L, &Input, "=SignerValidation", 0 );
		}

		void CheckProfile( )
		{
			std::array<uint8_t, 18> Entry{ };
			if ( !ReadProcessMemory( GetCurrentProcess( ), reinterpret_cast<const void*>( Update::LuaU::LuaVMLoad ), Entry.data( ), Entry.size( ), nullptr ) || Entry != Profile::Native::LoadEntry )
				throw std::runtime_error( "native loader does not match the signing profile" );
			uint32_t Key = 0;
			if ( !ReadProcessMemory( GetCurrentProcess( ), reinterpret_cast<const void*>( Address( Profile::Native::Key ) ), &Key, sizeof( Key ), nullptr ) || Key != Profile::Key )
				throw std::runtime_error( "native signing key does not match the profile" );
		}

		int Run( lua_State* L )
		{
			luaL_checktype( L, 1, LUA_TSTRING );
			luaL_checktype( L, 2, LUA_TSTRING );
			const std::string Directory = lua_tostring( L, 1 );
			size_t Length = 0;
			const char* Source = lua_tolstring( L, 2, &Length );
			auto Compile = [ Directory, Source = std::string( Source, Length ) ]( ) -> Yielded
			{
				CheckProfile( );
				Encoder Encoding;
				Luau::CompileOptions Options;
				Options.optimizationLevel = 1;
				Options.debugLevel = 2;
				const std::string Bytecode = Luau::compile( Source, Options, { }, &Encoding );
				if ( Bytecode.empty( ) || !Bytecode.front( ) )
					throw std::runtime_error( "test source failed compilation" );
				Signer Signing( Directory );
				auto Signed = Signing.Sign( std::span( reinterpret_cast<const uint8_t*>( Bytecode.data( ) ), Bytecode.size( ) ) );
				return [ Signed = std::move( Signed ) ]( lua_State* State )
				{
					CheckProfile( );
					const uint32_t Valid = VerifyNative( Signed );
					auto Changed = Signed;
					Changed[ Changed.size( ) - Profile::TrailerSize - 1 ] ^= 1;
					const uint32_t Invalid = VerifyNative( Changed );
					Update::Roblox::Functions::Print( 0, "SignerValidation native valid=%u modified=%u", Valid, Invalid );
					if ( Valid != 0 || Invalid != 7 )
						throw std::runtime_error( "native verifier did not accept/reject the expected inputs" );
					const int Top = lua_gettop( State );
					const int Status = Load( State, Signed );
					Update::Roblox::Functions::Print( 0, "SignerValidation LvmLoad status=%d type=%s", Status, lua_typename( State, lua_type( State, -1 ) ) );
					if ( Status != LUA_OK || lua_gettop( State ) != Top + 1 || !lua_isfunction( State, -1 ) )
						throw std::runtime_error( "native LvmLoad did not return a closure" );
					lua_pushinteger( State, Valid );
					lua_pushinteger( State, Invalid );
					lua_pushinteger( State, Status );
					return 4;
				};
			};
			return Yielding::YieldExec( L, Compile );
		}
	}

	void Initialize( )
	{
		Env::RegisterGlobal( "testsigner", Run );
	}
}
