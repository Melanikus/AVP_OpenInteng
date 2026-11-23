// The test for the scenario harness's idle gate (tests/drive_scenario/idle_gate.h): IDLE_SECONDS read strictly, the start allowed only after that long with
// no input, the clock's wrap, and a touch seen. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". It reads no clock and no file: the two Windows clocks are given as numbers. The program gives back a non-zero result if any
// check failed. Each check's words say what it proves.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "drive_scenario/idle_gate.h"

using namespace idle_gate;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	printf("harness idle gate\n");

	// IDLE_SECONDS: unset or empty is no gate; a whole number 1 to a day is taken; anything else refuses the run (a typo never turns the gate off)
	Check(ParseSeconds(nullptr) == 0 && ParseSeconds("") == 0, "IDLE_SECONDS unset or empty: no gate (0)");
	Check(ParseSeconds("120") == 120 && ParseSeconds("1") == 1 && ParseSeconds("86400") == 86400, "120, 1 and 86400 (a day) are taken as they are");
	Check(ParseSeconds("0") == -1, "0 is refused (-1): a gate of no time is no gate, so it must be said by leaving IDLE_SECONDS out");
	Check(ParseSeconds("86401") == -1 && ParseSeconds("9999999") == -1, "more than a day is refused, and a 7-digit number too");
	Check(ParseSeconds("12a") == -1 && ParseSeconds("-5") == -1 && ParseSeconds(" 120") == -1 && ParseSeconds("120 ") == -1 && ParseSeconds("1.5") == -1,
		"letters, a sign, spaces and a decimal point are refused");
	Check(ParseSeconds("0120") == 120, "a leading 0 is a plain whole number (120)");

	// the start: only after that long with no input
	Check(MayStart(0, 0) && MayStart(5, 0), "no gate: the editor may always start");
	Check(!MayStart(0, 120), "touched just now (0 ms ago), 120 s asked: NOT started");
	Check(!MayStart(119999, 120), "touched 119.999 s ago, 120 s asked: NOT started");
	Check(MayStart(120000, 120) && MayStart(3600000, 120), "touched 120 s ago, or an hour ago: may start");
	Check(!MayStart(86399999u, 86400) && MayStart(86400000u, 86400), "a day asked: 1 ms short refuses, the day itself starts (no overflow at 86400 x 1000)");

	// the two 32-bit millisecond clocks wrap every 49.7 days: the difference stays right across the wrap
	Check(IdleMs(5000, 2000) == 3000, "now 5000 ms, last input 2000 ms: 3000 ms idle");
	Check(IdleMs(1000, 0xFFFFF000u) == 1000 + 0x1000, "the clock wrapped between the last input (0xFFFFF000) and now (1000): 5096 ms idle, not 49 days");
	Check(!MayStart(IdleMs(1000, 0xFFFFF000u), 120), "and that run, touched 5 s before the wrap's other side, is NOT started");

	// a touch during the run: the last input's time moved between the start and the end
	Check(!Touched(123456, 123456), "the last input's time the same at the start and the end: nobody touched the machine");
	Check(Touched(123456, 123999), "it moved: someone touched the machine during the run (never green)");
	Check(kIdleNotStarted == 24, "a run not started gives back 24, run-hudson.ps1's own number, which run-shards takes as 'try again later'");

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
