#pragma once

#ifndef _HAC_MANIFEST_PUBLIC_KEY_H
#define _HAC_MANIFEST_PUBLIC_KEY_H

#include <array>
#include <cstdint>

// Override at build time: /D HAC_PUBKEY_HEX="\"<64 hex chars>\""
// Default is the checked-in dev public key at tools/keys/dev.public.key.hex.
#ifndef HAC_PUBKEY_HEX
#define HAC_PUBKEY_HEX "574e4292560fc97b57845943e9e325120ad23124362f90be73d2b34bb3a46d34"
#endif

namespace hac::manifest {

namespace detail
{
	constexpr uint8_t HexNib(char c)
	{
		return (c >= '0' && c <= '9') ? uint8_t(c - '0')
			 : (c >= 'a' && c <= 'f') ? uint8_t(10 + c - 'a')
			 : (c >= 'A' && c <= 'F') ? uint8_t(10 + c - 'A')
			 : uint8_t(0);
	}

	template <size_t N>
	constexpr auto DecodeHex(const char (&hex)[N])
	{
		static_assert(N == 65, "public key must be exactly 64 hex chars");
		std::array<uint8_t, 32> out{};
		for (size_t i = 0; i < 32; ++i)
		{
			out[i] = uint8_t((HexNib(hex[2 * i]) << 4) | HexNib(hex[2 * i + 1]));
		}
		return out;
	}
}

inline constexpr auto kEmbeddedPublicKey = detail::DecodeHex(HAC_PUBKEY_HEX);

} // namespace hac::manifest

#endif
