// idle_gate.h: the scenario harness's IDLE GATE. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng, on Bishop 2's order after the owner was at
// his PC during a run that took the screen lane (the harness's own cursor watch saw him). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// WHAT IT DOES: a run is started only when nobody has touched the machine (mouse, keyboard, any input Windows counts) for the seconds asked in the
// environment's IDLE_SECONDS (run-shards.sh sets it), as AvPx's Hudson does for itself (avpx 3dc/avp/hudson.cpp, GetLastInputInfo). Too recent a
// touch: the editor is not started, and the harness gives back kIdleNotStarted, which run-shards takes as "try again later", as it takes run-hudson's 24.
// A touch while the run goes makes it "touched": a FAILED check, never green. Without IDLE_SECONDS the harness runs as it always did.
// The harness itself never makes input (it only posts messages to the editor's own windows), so its own steps never count as a touch.
// These are the pure parts (no Windows), so tests/harness_idle_gate_test.cpp can check them; drive_scenario.cpp asks Windows for the two clocks.
#pragma once

#include <stdint.h>

namespace idle_gate
{
	int const kIdleNotStarted = 24;      // the harness's exit when it didn't start the editor because of a touch (run-hudson.ps1's own number)
	int const kMostSeconds = 86400;      // a day: more than that is a typo, and is refused rather than taken

	// IDLE_SECONDS as given: 0 when it isn't set or is empty (no gate); the seconds, 1 to kMostSeconds; -1 for anything else (not a plain whole number,
	// 0, too big): the run refuses then, as a typo must never quietly turn the gate off
	inline int ParseSeconds(char const *text)
	{
		if (!text || !*text)
		{
			return 0;
		}

		long long value = 0;

		for (char const *c = text; *c; c++)
		{
			if (*c < '0' || *c > '9' || c - text >= 6)
			{
				return -1;
			}

			value = value * 10 + (*c - '0');
		}

		return value >= 1 && value <= kMostSeconds ? (int)value : -1;
	}

	// How long ago the last input was, in ms, from Windows' two 32-bit millisecond clocks (GetTickCount now, LASTINPUTINFO.dwTime). Both wrap every
	// 49.7 days, and the difference taken in unsigned 32-bit numbers stays right across the wrap.
	inline uint32_t IdleMs(uint32_t now, uint32_t lastInput)
	{
		return now - lastInput;
	}

	// The editor may start: no gate, or nobody has touched the machine for the seconds asked
	inline bool MayStart(uint32_t idleMs, int seconds)
	{
		return seconds <= 0 || idleMs >= (uint32_t)seconds * 1000u;
	}

	// Someone touched the machine between the two readings of the last input's time (at the start and at the end of the run)
	inline bool Touched(uint32_t lastInputAtStart, uint32_t lastInputAtEnd)
	{
		return lastInputAtEnd != lastInputAtStart;
	}
}
