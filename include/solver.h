#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Testing::Detail
{
	struct Bits
	{
		uint64_t Low = 0;
		uint64_t High = 0;

		bool Test( int Index ) const;
		void Set( int Index );
		int Top( ) const;
		bool Empty( ) const;
		Bits& operator^=( Bits Other );
		friend Bits operator^( Bits Left, Bits Right );
		bool operator==( const Bits& ) const = default;
	};

	class Basis
	{
	  public:
		std::array<Bits, 128> Rows{ };
		int Count = 0;

		void Insert( Bits Value );
		Bits Reduce( Bits Value ) const;
		std::vector<Bits> Kernel( ) const;
	};

	class Solver
	{
	  public:
		explicit Solver( const std::filesystem::path& Directory );
		Bits Evaluate( Bits Value ) const;
		Bits Solve( Bits Target ) const;

	  private:
		struct Layer
		{
			Basis Previous;
			std::array<Bits, 96> Images{ };
			std::array<Bits, 96> Directions{ };
		};

		Bits Polynomial( Bits Value ) const;
		Bits ByteTerm( int Index, uint8_t Value ) const;
		void Prepare( );

		Bits m_Constant;
		std::vector<Bits> m_LinearA;
		std::vector<Bits> m_LinearB;
		std::vector<Bits> m_Table;
		std::array<Bits, 128> m_Linear{ };
		std::vector<Bits> m_Quadratic;
		std::vector<Layer> m_Layers;
	};
}
