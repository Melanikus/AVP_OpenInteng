// Telling a real number from "not a number" (NaN) and from endless (infinity), by its bits. Written 2026-11-14 by Melanikus for
// AVP_OpenInteng.
//
// The editor is built with the game's own settings, which include the compiler's fast rules for fractions (/fp:fast). Under those, the
// compiler is allowed to assume no value is NaN or endless, so isnan(), isfinite() and comparisons aren't promised to catch one. A value
// read from a level, or worked out from one, can be either. So every check in the editor that has to know asks these, which look at
// the bits themselves and can't be optimised away. The game's own level-scale check does the same (avpx/3dc/win95/environment_chunk.cpp).
//
// A double's 11 exponent bits (52 to 62) are all set exactly when it's NaN or endless; a float's 8 (23 to 30) the same.

#ifndef AVP_OPENINTENG_REAL_NUMBERS_H
#define AVP_OPENINTENG_REAL_NUMBERS_H

#include <stdint.h>
#include <string.h>

inline bool IsNaNOrEndless(double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 52) & 0x7FF) == 0x7FF;
}

inline bool IsNaNOrEndless(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 23) & 0xFF) == 0xFF;
}

// NaN alone: all exponent bits set, and some fraction bit set. (With no fraction bit set, it's endless instead.)
inline bool IsNaN(double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 52) & 0x7FF) == 0x7FF && (bits & 0x000FFFFFFFFFFFFFull) != 0;
}

inline bool IsNaN(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return ((bits >> 23) & 0xFF) == 0xFF && (bits & 0x007FFFFFu) != 0;
}

// True for a real number: not NaN and not endless.
inline bool IsRealNumber(double value)
{
	return !IsNaNOrEndless(value);
}

inline bool IsRealNumber(float value)
{
	return !IsNaNOrEndless(value);
}

#endif
