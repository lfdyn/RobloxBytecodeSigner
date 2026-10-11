#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace Testing
{
	class Signer
	{
	  public:
		explicit Signer( const std::filesystem::path& Directory );
		~Signer( );
		Signer( Signer&& ) noexcept;
		Signer& operator=( Signer&& ) noexcept;

		std::vector<uint8_t> Sign( std::span<const uint8_t> Bytecode ) const;
		bool Verify( std::span<const uint8_t> Bytecode ) const;
		std::array<uint8_t, 16> Solve( const std::array<uint8_t, 12>& Target ) const;
		std::array<uint8_t, 12> Evaluate( const std::array<uint8_t, 16>& Signature ) const;

	  private:
		class Implementation;
		std::unique_ptr<Implementation> m_Impl;
	};
}
