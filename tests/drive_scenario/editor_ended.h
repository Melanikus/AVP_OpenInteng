// editor_ended.h: how the scenario harness knows the editor it started is DEAD, and says so in words. Written 2026-11-23 by Melanikus (Frost) for
// AVP_OpenInteng, on Bishop 2's card. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: in the alpha.6 final round (Dietrich, on the cut b0e2f932) the editor CRASHED while All stock maps plotted Caverns: its log, avp_log.txt, says
// "crash: code C0000005 at AVP_OpenInteng.exe+1C29F0". But wait-moving counts a state the editor doesn't answer as no change, so it sat out its whole
// 300 s and then said STALLED: the crash read as a stuck run, and the crash's own lines were left in a log nobody quoted.
// WHAT IT DOES: the editor is dead when its process has ended, OR when its log has a crash line written since the harness started it. (The game's crash
// writer, avpx 3dc/logString.cpp WriteLogBeforeCrash, writes its lines and then hands the crash on to Windows, whose crash report can keep the process
// alive a while longer: so the log can say it before the process has ended.) Every wait then fails AT ONCE, in one of these:
//   CRASHED: the editor's log says "crash: code C0000005 at AVP_OpenInteng.exe+1C29F0" / "crash: on the stack: ..." (it has ended, exit code C0000005)
//   CRASHED: the editor ended with exit code C0000005, Windows' code for a crash, and its log has no crash line
//   ENDED: the editor ended by itself, exit code 0, with no crash line in its log
// The log is read a piece at a time as it grows (Take), and only whole lines are judged: a crash line is one that starts with "crash: ", as the
// crash writer starts each of its lines (avpx 3dc/log_files.h LogFiles_DescribeCrash). Nothing else in the editor writes a line that starts so.
// These are the pure parts (no Windows), so tests/harness_editor_ended_test.cpp can check them; drive_scenario.cpp reads the process and the file.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace editor_ended
{
	size_t const kMostCrashLines = 8;    // the first 8 crash lines are kept and quoted (the code and the place, then the stack): enough to look it up
	size_t const kMostLetters = 300;     // each one cut to this many letters; a longer line is kept to here and marked with "..."

	// The log, as it is read: the crash lines seen so far, and the end of the last piece, a line not yet ended
	struct Reader
	{
		std::string part;                  // the line being read, at most kMostLetters letters of it
		bool partCut = false;              // more came than kMostLetters: the rest of that line is let go
		std::vector<std::string> crash;    // the crash lines, in order, at most kMostCrashLines
		size_t crashCount = 0;             // every crash line seen (it can be more than are kept: the words say so)
	};

	// One whole line of the log: kept when it is a crash line
	inline void Line(Reader *r, std::string const &line, bool cut)
	{
		if (line.compare(0, 7, "crash: ") != 0)
		{
			return;
		}

		r->crashCount++;

		if (r->crash.size() < kMostCrashLines)
		{
			r->crash.push_back(cut ? line + "..." : line);
		}
	}

	// The next piece of the log, as many bytes as were read: whole lines are judged, a part line waits for its end in the next piece. (The log's
	// lines end in \r\n: the \r is let go wherever it is, so it never counts as a letter of the line.)
	inline void Take(Reader *r, char const *bytes, size_t size)
	{
		for (size_t at = 0; at < size; at++)
		{
			char const letter = bytes[at];

			if (letter == '\r')
			{
				continue;
			}

			if (letter == '\n')
			{
				Line(r, r->part, r->partCut);
				r->part.clear();
				r->partCut = false;
			}
			else if (r->part.size() < kMostLetters)
			{
				r->part.push_back(letter);
			}
			else
			{
				r->partCut = true;
			}
		}
	}

	// Windows' codes for a crash have their top two bits set (C0000005 a bad memory read or write, C00000FD the stack ran out, C0000409 a buffer
	// overrun the program caught itself): an exit code that high is a crash's, never a number a program gives back on its own
	inline bool IsCrashCode(uint32_t code)
	{
		return code >= 0xC0000000u;
	}

	// An exit code as people read it: a crash's in Windows' 8 hex digits (C0000005), any other as a plain number (0, 1, 98)
	inline std::string CodeWords(uint32_t code)
	{
		if (code < 0x80000000u)
		{
			return std::to_string(code);
		}

		char const digits[] = "0123456789ABCDEF";
		std::string hex(8, '0');

		for (int at = 7; at >= 0; at--)
		{
			hex[(size_t)at] = digits[code & 0xFu];
			code >>= 4;
		}

		return hex;
	}

	// The words for an editor that is dead, or "" while it lives (its process hasn't ended and its log has no crash line). ended and exitCode are the
	// process's; the reader holds what the log said since the harness started it.
	inline std::string Words(bool ended, uint32_t exitCode, Reader const &r)
	{
		if (r.crashCount > 0)
		{
			std::string quoted;

			for (std::string const &line : r.crash)
			{
				quoted += (quoted.empty() ? "\"" : " / \"") + line + "\"";
			}

			if (r.crashCount > r.crash.size())
			{
				quoted += " and " + std::to_string(r.crashCount - r.crash.size()) + " more crash lines";
			}

			return "CRASHED: the editor's log says " + quoted + (ended ? " (it has ended, exit code " + CodeWords(exitCode) + ")" :
				" (it is still running: Windows' crash report holds it)");
		}

		if (!ended)
		{
			return std::string();
		}

		return IsCrashCode(exitCode) ? "CRASHED: the editor ended with exit code " + CodeWords(exitCode) + ", Windows' code for a crash, and its log has no crash line" :
			"ENDED: the editor ended by itself, exit code " + CodeWords(exitCode) + ", with no crash line in its log";
	}
}
