// The test for tests/drive_scenario/editor_ended.h, how the harness knows the editor it started is dead and says so. Written 2026-11-23 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no editor, no process; the log's bytes and
// the exit code are given.

#include <stdio.h>
#include <string.h>

#include <string>

#include "drive_scenario/editor_ended.h"

using namespace editor_ended;

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

static void TakeText(Reader *r, std::string const &text)
{
	Take(r, text.data(), text.size());
}

static bool Has(std::string const &words, std::string const &part)
{
	return words.find(part) != std::string::npos;
}

int main()
{
	puts("the harness knows a dead editor");

	// THE CASE THAT WENT RED: the alpha.6 final round's log as Caverns plotted (Dietrich, b0e2f932), its lines ending in \r\n; the editor still
	// running (Windows' crash report) and then ended
	{
		Reader r;
		TakeText(&r, "stall: frame 13390 took 62.3 ms: messages 0.0, game 0.0, views 0.0 of which shapes 4.1\r\n"
			"stall: and 110 more that the list had no room for\r\n"
			"card: 9.3 ms: scene 9.3, surface pictures 0.0, light total 0.0\r\n"
			"crash: code C0000005 at AVP_OpenInteng.exe+1C29F0\r\n"
			"crash: on the stack: AVP_OpenInteng.exe+47E3EC\r\n");
		std::string const alive = Words(false, 0, r);
		std::string const ended = Words(true, 0xC0000005u, r);

		Check(alive == "CRASHED: the editor's log says \"crash: code C0000005 at AVP_OpenInteng.exe+1C29F0\" / \"crash: on the stack: AVP_OpenInteng.exe+47E3EC\" "
			"(it is still running: Windows' crash report holds it)", "the final round's crash, the editor still running: CRASHED, both crash lines quoted, no \\r (\"" + alive + "\")");
		Check(Has(ended, "CRASHED: the editor's log says \"crash: code C0000005 at AVP_OpenInteng.exe+1C29F0\"") && Has(ended, "(it has ended, exit code C0000005)"),
			"and once it has ended: CRASHED, the lines, and its exit code (\"" + ended + "\")");
	}

	// A LIVE EDITOR is not dead: no crash line, its process running. A line with "crash:" anywhere but its start is not a crash line
	{
		Reader r;
		TakeText(&r, "the data folder: %USERPROFILE%\\source\\AvP-builds-excluded\\shards\\s1\\lv\\hicks-data\r\n"
			"All stock maps: level 3 of 22, sulaco.RIF: the last run's crash: code C0000005 is not this one\r\n"
			" crash: code C0000005 (a space first)\r\n"
			"Crash: code C0000005 (another letter's size)\r\n");
		Check(Words(false, 0, r).empty() && r.crashCount == 0, "running, and no line that STARTS with \"crash: \": alive (\"\")");
	}

	// A CRASH LINE IN TWO PIECES (the log read while the crash writer was writing it): judged once, whole, when its end comes
	{
		Reader r;
		TakeText(&r, "card: 9.3 ms\r\ncrash: code C0000005 at AVP_Open");
		Check(r.crashCount == 0 && Words(false, 0, r).empty(), "half a crash line, its end not yet written: not judged yet");
		TakeText(&r, "Inteng.exe+1C29F0\r");
		Check(r.crashCount == 0, "its \\r come, its \\n not yet: still not judged");
		TakeText(&r, "\n");
		Check(r.crashCount == 1 && r.crash[0] == "crash: code C0000005 at AVP_OpenInteng.exe+1C29F0", "its end come: one crash line, whole (\"" +
			(r.crash.empty() ? std::string() : r.crash[0]) + "\")");
	}

	// THE PROCESS'S OWN END, with no crash line in the log
	{
		Reader r;
		TakeText(&r, "All stock maps: the run ended: 22 of 22\r\n");
		Check(Words(true, 0, r) == "ENDED: the editor ended by itself, exit code 0, with no crash line in its log", "ended with 0, no crash line: ENDED, exit code 0");
		Check(Words(true, 1, r) == "ENDED: the editor ended by itself, exit code 1, with no crash line in its log", "ended with 1: ENDED, exit code 1");
		Check(Words(true, 0xC00000FDu, r) == "CRASHED: the editor ended with exit code C00000FD, Windows' code for a crash, and its log has no crash line",
			"ended with C00000FD (the stack ran out) and no crash line: CRASHED, by its code");
		Check(Words(true, 0xC0000409u, r).compare(0, 8, "CRASHED:") == 0, "ended with C0000409 (a buffer overrun caught): CRASHED");
		Check(Words(true, 0x80000003u, r).compare(0, 6, "ENDED:") == 0 && Has(Words(true, 0x80000003u, r), "exit code 80000003"),
			"ended with 80000003 (a breakpoint, a warning's code, not a crash's): ENDED, in hex");
	}

	// THE EXIT CODE IN WORDS
	{
		Check(CodeWords(0) == "0" && CodeWords(98) == "98" && CodeWords(0x7FFFFFFFu) == "2147483647", "an exit code under 80000000 is a plain number (0, 98, 2147483647)");
		Check(CodeWords(0xC0000005u) == "C0000005" && CodeWords(0x80000000u) == "80000000" && CodeWords(0xFFFFFFFFu) == "FFFFFFFF",
			"from 80000000 up: Windows' 8 hex digits (C0000005, 80000000, FFFFFFFF)");
		Check(IsCrashCode(0xC0000000u) && !IsCrashCode(0xBFFFFFFFu) && !IsCrashCode(98), "a crash's code has its top two bits set (C0000000 is, BFFFFFFF and 98 aren't)");
	}

	// THE BOUNDS: at most kMostCrashLines quoted, the rest counted; a long line kept to kMostLetters, marked
	{
		Reader r;

		for (int n = 1; n <= 20; n++)
		{
			TakeText(&r, "crash: line " + std::to_string(n) + "\r\n");
		}

		std::string const words = Words(true, 0xC0000005u, r);
		Check(r.crashCount == 20 && r.crash.size() == kMostCrashLines && r.crash.back() == "crash: line 8", "20 crash lines: all 20 counted, the first 8 kept");
		Check(Has(words, "\"crash: line 8\" and 12 more crash lines (it has ended") && !Has(words, "line 9\""), "the words quote 8 and say \"and 12 more crash lines\"");
	}

	{
		Reader r;
		std::string const long1 = "crash: on the stack:" + std::string(2000, 'x');
		TakeText(&r, long1 + "\r\n");
		Check(r.crash.size() == 1 && r.crash[0] == long1.substr(0, kMostLetters) + "...", "a crash line of 2,020 letters: kept to 300, then \"...\"");

		std::string const exact = "crash: " + std::string(kMostLetters - 7, 'y');
		TakeText(&r, exact + "\r\n");
		Check(r.crash.size() == 2 && r.crash[1] == exact, "a crash line of exactly 300 letters and its \\r\\n: kept whole, no \"...\" (its \\r is no letter)");

		TakeText(&r, std::string(100000, 'z') + "\r\ncrash: after a long line\r\n");
		Check(r.crashCount == 3 && r.crash[2] == "crash: after a long line" && r.part.empty(), "a line of 100,000 letters, then a crash line: the long one let go, the crash line whole");
	}

	printf(failed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failed ? 1 : 0;
}
