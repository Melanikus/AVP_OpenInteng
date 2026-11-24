// sha256_hex.h: the SHA-256 of some bytes, as 64 lowercase hex digits, made with the game's own SHA-256 (avpx 3dc/sha256_stream.h, header only, the one its loader uses).
// Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// It takes the place of IntengExe_Sha256, which went with the inteng.exe reader (nothing of inteng's is read from inteng.exe any more: the owner's ruling 11-24). The same
// answers: tests/host_sha256_hex_test checks the standard's own examples.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "sha256_stream.h"

namespace host
{
	inline std::string Sha256Hex(uint8_t const *bytes, size_t size)
	{
		Sha256Stream stream;
		uint8_t sum[32];
		static char const digits[] = "0123456789abcdef";
		std::string out;

		Sha256_Start(&stream);
		Sha256_Add(&stream, bytes, size);
		Sha256_Finish(&stream, sum);

		for (uint8_t b : sum)
		{
			out += digits[b >> 4];
			out += digits[b & 15];
		}

		return out;
	}
}
