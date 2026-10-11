#include "signer.h"
#include "profile.h"
#include "solver.h"
#include "vendor/blake3/blake3.h"
#include <algorithm>
#include <stdexcept>

namespace Testing
{
	namespace
	{
		Detail::Bits ReadBits( std::span<const uint8_t> Bytes )
		{
			Detail::Bits Value;
			for ( size_t Index = 0; Index < Bytes.size( ); ++Index )
				( Index < 8 ? Value.Low : Value.High ) |= uint64_t( Bytes[ Index ] ) << ( ( Index & 7 ) * 8 );
			return Value;
		}

		template <size_t Size>
		std::array<uint8_t, Size> WriteBits( Detail::Bits Value )
		{
			std::array<uint8_t, Size> Bytes{ };
			for ( size_t Index = 0; Index < Size; ++Index )
				Bytes[ Index ] = uint8_t( ( Index < 8 ? Value.Low : Value.High ) >> ( ( Index & 7 ) * 8 ) );
			return Bytes;
		}

		std::array<uint8_t, 12> Digest( std::span<const uint8_t> Bytes )
		{
			blake3_hasher State;
			blake3_hasher_init( &State );
			blake3_hasher_update( &State, Bytes.data( ), Bytes.size( ) );
			std::array<uint8_t, 32> Hash{ };
			blake3_hasher_finalize( &State, Hash.data( ), Hash.size( ) );
			std::array<uint8_t, 12> Result{ };
			std::copy_n( Hash.begin( ) + 4, Result.size( ), Result.begin( ) );
			return Result;
		}
	}

	class Signer::Implementation
	{
	  public:
		explicit Implementation( const std::filesystem::path& Directory ) : Model( Directory )
		{
		}

		Detail::Solver Model;
	};

	Signer::Signer( const std::filesystem::path& Directory ) : m_Impl( std::make_unique<Implementation>( Directory ) )
	{
	}

	Signer::~Signer( ) = default;
	Signer::Signer( Signer&& ) noexcept = default;
	Signer& Signer::operator=( Signer&& ) noexcept = default;

	std::array<uint8_t, 16> Signer::Solve( const std::array<uint8_t, 12>& Target ) const
	{
		return WriteBits<16>( m_Impl->Model.Solve( ReadBits( Target ) ) );
	}

	std::array<uint8_t, 12> Signer::Evaluate( const std::array<uint8_t, 16>& Signature ) const
	{
		return WriteBits<12>( m_Impl->Model.Evaluate( ReadBits( Signature ) ) );
	}

	std::vector<uint8_t> Signer::Sign( std::span<const uint8_t> Bytecode ) const
	{
		if ( Bytecode.empty( ) || !Bytecode.front( ) || Bytecode.size( ) > Profile::MaximumPayload )
			throw std::invalid_argument( "expected nonempty compiled bytecode within the size limit" );

		std::vector<uint8_t> Result( Bytecode.begin( ), Bytecode.end( ) );
		Result.resize( std::max( Result.size( ), Profile::MinimumPayload ), 0 );
		const auto Signature = Solve( Digest( Result ) );
		Result.insert( Result.end( ), Signature.begin( ), Signature.end( ) );
		for ( int Index = 0; Index < 4; ++Index )
			Result.push_back( Signature[ Index ] ^ uint8_t( Profile::Key >> ( Index * 8 ) ) );
		for ( int Index = 0; Index < 4; ++Index )
			Result.push_back( Signature[ Index ] ^ Profile::Marker[ Index ] );
		if ( !Verify( Result ) )
			throw std::runtime_error( "signed bytecode failed local verification" );
		return Result;
	}

	bool Signer::Verify( std::span<const uint8_t> Bytecode ) const
	{
		if ( Bytecode.size( ) < Profile::MinimumPayload + Profile::TrailerSize ||
			 Bytecode.size( ) > Profile::MaximumPayload + Profile::TrailerSize || !Bytecode.front( ) )
			return false;

		const auto Trailer = Bytecode.last( Profile::TrailerSize );
		for ( int Index = 0; Index < 4; ++Index )
		{
			if ( uint8_t( Trailer[ 16 + Index ] ^ Trailer[ Index ] ) != uint8_t( Profile::Key >> ( Index * 8 ) ) ||
				 uint8_t( Trailer[ 20 + Index ] ^ Trailer[ Index ] ) != Profile::Marker[ Index ] )
				return false;
		}

		std::array<uint8_t, 16> Signature{ };
		std::copy_n( Trailer.begin( ), Signature.size( ), Signature.begin( ) );
		return Evaluate( Signature ) == Digest( Bytecode.first( Bytecode.size( ) - Profile::TrailerSize ) );
	}
}
