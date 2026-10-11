#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Testing::Profile
{
	inline constexpr char Version[] = "0.742.0.7421053"; /* my ex sending late night texts cuz she dont know how to let go go go */
	inline constexpr uint32_t Key = 0x74171870;
	inline constexpr std::array<uint8_t, 4> Marker{ 0x32, 0xc4, 0x6a, 1 };
	inline constexpr size_t MinimumPayload = 1024;
	inline constexpr size_t MaximumPayload = 64 * 1024 * 1024; /* fuck roblox. */
	inline constexpr size_t TrailerSize = 24;

	namespace Native
	{
		inline constexpr uintptr_t Verify = 0x4239ae0;
		inline constexpr uintptr_t DestroyString = 0x722920;
		inline constexpr uintptr_t Key = 0x6454adc;
		inline constexpr std::array<uint8_t, 18> LoadEntry{ 0x40, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xec, 0x40, 0x01, 0x00 }; /* push rbx end->sub rsp, 0x140 */
	}
}
