#include "solver.h"
#include <fstream>
#include <random>
#include <stdexcept>

namespace Testing::Detail
{
	namespace
	{
		std::vector<Bits> ReadTable( const std::filesystem::path& Path, size_t Count )
		{
			std::ifstream File( Path, std::ios::binary | std::ios::ate );
			if ( !File || File.tellg( ) != static_cast<std::streamoff>( Count * 16 ) )
				throw std::runtime_error( "invalid table: " + Path.string( ) );

			File.seekg( 0 );
			std::vector<Bits> Result( Count );
			for ( Bits& Value : Result )
			{
				std::array<uint8_t, 16> Bytes{ };
				if ( !File.read( reinterpret_cast<char*>( Bytes.data( ) ), Bytes.size( ) ) )
					throw std::runtime_error( "truncated table: " + Path.string( ) );

				for ( int Index = 0; Index < 8; ++Index )
					Value.Low |= uint64_t( Bytes[ Index ] ) << ( Index * 8 );
				for ( int Index = 0; Index < 4; ++Index )
					Value.High |= uint64_t( Bytes[ Index + 8 ] ) << ( Index * 8 );
			}
			return Result;
		}

		int PairIndex( int Left, int Right )
		{
			return Left + 15 * Right - Right * ( Right + 1 ) / 2 - 1;
		}
	}

	Solver::Solver( const std::filesystem::path& Directory )
		: m_Constant( ReadTable( Directory / "signature-constant.bin", 1 ).front( ) ),
		  m_LinearA( ReadTable( Directory / "signature-linear-a.bin", 4096 ) ),
		  m_LinearB( ReadTable( Directory / "signature-linear-b.bin", 4096 ) ),
		  m_Table( ReadTable( Directory / "signature-quadratic.bin", 245760 ) ),
		  m_Quadratic( 128 * 128 )
	{
		for ( int Left = 0; Left < 128; ++Left )
		{
			m_Linear[ Left ] = ByteTerm( Left / 8, uint8_t( 1 << ( Left % 8 ) ) );
			for ( int Right = 0; Right < Left; ++Right )
			{
				Bits Coefficient;
				if ( Left / 8 == Right / 8 )
					Coefficient = ByteTerm( Left / 8, uint8_t( ( 1 << ( Left % 8 ) ) | ( 1 << ( Right % 8 ) ) ) ) ^ m_Linear[ Left ] ^ m_Linear[ Right ];
				else
					Coefficient = m_Table[ ( PairIndex( Left / 8, Right / 8 ) * 8 + 7 - Left % 8 ) * 256 + ( 1 << ( Right % 8 ) ) ];

				m_Quadratic[ Left * 128 + Right ] = Coefficient;
				m_Quadratic[ Right * 128 + Left ] = Coefficient;
			}
		}

		std::mt19937_64 Random( 0xa17e );
		for ( int Index = 0; Index < 64; ++Index )
		{
			const Bits Value{ Random( ), Random( ) };
			if ( Evaluate( Value ) != Polynomial( Value ) )
				throw std::runtime_error( "tables do not match the quadratic model" );
		}
		Prepare( );
	}

	Bits Solver::ByteTerm( int Index, uint8_t Value ) const
	{
		return Value ? m_LinearA[ Index * 256 + Value ] ^ m_LinearB[ Index * 256 + Value ] : Bits{ };
	}

	Bits Solver::Evaluate( Bits Value ) const
	{
		std::array<uint8_t, 16> Bytes{ };
		for ( int Index = 0; Index < 16; ++Index )
			Bytes[ Index ] = uint8_t( ( Index < 8 ? Value.Low : Value.High ) >> ( ( Index & 7 ) * 8 ) );

		Bits Result = m_Constant;
		for ( int Left = 0; Left < 16; ++Left )
		{
			Result ^= ByteTerm( Left, Bytes[ Left ] );
			for ( int Right = 0; Right < Left; ++Right )
			{
				if ( !Bytes[ Right ] )
					continue;
				for ( int Bit = 0; Bit < 8; ++Bit )
				{
					if ( Bytes[ Left ] & ( 128 >> Bit ) )
						Result ^= m_Table[ ( PairIndex( Left, Right ) * 8 + Bit ) * 256 + Bytes[ Right ] ];
				}
			}
		}
		return Result;
	}

	Bits Solver::Polynomial( Bits Value ) const
	{
		Bits Result = m_Constant;
		for ( int Left = 0; Left < 128; ++Left )
		{
			if ( !Value.Test( Left ) )
				continue;
			Result ^= m_Linear[ Left ];
			for ( int Right = 0; Right < Left; ++Right )
			{
				if ( Value.Test( Right ) )
					Result ^= m_Quadratic[ Left * 128 + Right ];
			}
		}
		return Result;
	}

	void Solver::Prepare( )
	{
		Basis Images;
		for ( int Step = 0; Step < 16 && Images.Count < 96; ++Step )
		{
			std::vector<Bits> Projected( m_Quadratic.size( ) );
			for ( size_t Index = 0; Index < Projected.size( ); ++Index )
				Projected[ Index ] = Images.Reduce( m_Quadratic[ Index ] );

			Basis Equations;
			for ( int Output = 0; Output < 96; ++Output )
			{
				for ( int Left = 0; Left < 128; ++Left )
				{
					Bits Row;
					for ( int Right = 0; Right < 128; ++Right )
					{
						if ( Projected[ Left * 128 + Right ].Test( Output ) )
							Row.Set( Right );
					}
					Equations.Insert( Row );
				}
			}

			const auto Kernel = Equations.Kernel( );
			Basis Expanded = Images;
			Layer Current;
			Current.Previous = Images;
			for ( Bits Direction : Kernel )
			{
				Bits Image = Polynomial( Direction ) ^ m_Constant;
				Expanded.Insert( Image );
				Image = Images.Reduce( Image );
				while ( !Image.Empty( ) )
				{
					const int Pivot = Image.Top( );
					if ( Current.Images[ Pivot ].Empty( ) )
					{
						Current.Images[ Pivot ] = Image;
						Current.Directions[ Pivot ] = Direction;
						break;
					}
					Image ^= Current.Images[ Pivot ];
					Direction ^= Current.Directions[ Pivot ];
				}
			}

			if ( Expanded.Count <= Images.Count )
				throw std::runtime_error( "table structure does not admit the expected inversion" );
			m_Layers.push_back( std::move( Current ) );
			Images = Expanded;
		}
		if ( Images.Count != 96 )
			throw std::runtime_error( "incomplete signature image" );
	}

	Bits Solver::Solve( Bits Target ) const
	{
		if ( Target.High >> 32 )
			throw std::invalid_argument( "target exceeds 96 bits" );
		Bits Value;
		for ( auto It = m_Layers.rbegin( ); It != m_Layers.rend( ); ++It )
		{
			Bits Remaining = It->Previous.Reduce( Target ^ Polynomial( Value ) );
			for ( int Pivot = 95; Pivot >= 0; --Pivot )
			{
				if ( Remaining.Test( Pivot ) )
				{
					Remaining ^= It->Images[ Pivot ];
					Value ^= It->Directions[ Pivot ];
				}
			}
			if ( !Remaining.Empty( ) )
				throw std::runtime_error( "target is outside the layer image" );
		}
		if ( Evaluate( Value ) != Target )
			throw std::runtime_error( "signature preimage verification failed" );
		return Value;
	}
}
