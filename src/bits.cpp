#include "solver.h"
#include <bit>

namespace Testing::Detail
{
	bool Bits::Test( int Index ) const
	{
		return ( ( Index < 64 ? Low : High ) >> ( Index & 63 ) ) & 1;
	}

	void Bits::Set( int Index )
	{
		( Index < 64 ? Low : High ) |= uint64_t( 1 ) << ( Index & 63 );
	}

	int Bits::Top( ) const
	{
		return High ? 127 - std::countl_zero( High ) : 63 - std::countl_zero( Low );
	}

	bool Bits::Empty( ) const
	{
		return !( Low | High );
	}

	Bits& Bits::operator^=( Bits Other )
	{
		Low ^= Other.Low;
		High ^= Other.High;
		return *this;
	}

	Bits operator^( Bits Left, Bits Right )
	{
		return Left ^= Right;
	}

	void Basis::Insert( Bits Value )
	{
		while ( !Value.Empty( ) )
		{
			const int Pivot = Value.Top( );
			if ( Rows[ Pivot ].Empty( ) )
			{
				Rows[ Pivot ] = Value;
				++Count;
				return;
			}
			Value ^= Rows[ Pivot ];
		}
	}

	Bits Basis::Reduce( Bits Value ) const
	{
		for ( int Pivot = 127; Pivot >= 0; --Pivot )
		{
			if ( Value.Test( Pivot ) )
				Value ^= Rows[ Pivot ];
		}
		return Value;
	}

	std::vector<Bits> Basis::Kernel( ) const
	{
		std::vector<Bits> Result;
		for ( int Free = 0; Free < 128; ++Free )
		{
			if ( !Rows[ Free ].Empty( ) )
				continue;

			Bits Value;
			Value.Set( Free );
			for ( int Pivot = 0; Pivot < 128; ++Pivot )
			{
				const Bits& Row = Rows[ Pivot ];
				if ( ( std::popcount( Row.Low & Value.Low ) + std::popcount( Row.High & Value.High ) ) & 1 )
					Value.Set( Pivot );
			}
			Result.push_back( Value );
		}
		return Result;
	}
}
