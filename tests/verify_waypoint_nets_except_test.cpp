// Tests VERIFY_WAYPOINT_NETS' "except" OPTION (tests/scenarios/verify_waypoint_nets_except.h; Bishop 2026-11-20, to carry out his rule-6(a) ruling without hiding anything: on Genshd1 two
// AI modules, air06 (364) and apcend (367), each have an exit whose area has no alien way to the rest, and the auto-plot writes their networks anyway). Written 2026-11-20 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT: an exit named "<master>:<exit>" after the word except is an EXCEPTION, not a failure, and only that exit of only that AI module, only for EXIT OUTSIDE; an exit not named still fails
// as before, in "exits" and in "clean"; an exception that names no such finding of the copy is found as such (the verifier fails it: a named exception must name a real finding); the
// words of the exception; and the "<master>:<exit>" word read strictly (two plain decimal numbers, one colon, nothing else).

#include <stdio.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "waypoint_net_level.h"
#include "verify_waypoint_nets_except.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

namespace
{
	using host::WaypointProblem;
	using host::WaypointProblemKind;
	using host::WaypointNetModule;
	using verify_nets::ExceptedExit;

	WaypointProblem Problem(WaypointProblemKind kind, int32_t box, int32_t other, std::string const &words)
	{
		WaypointProblem p;

		p.kind = kind;
		p.box = box;
		p.other = other;
		p.words = words;
		return p;
	}

	ExceptedExit Named(int32_t master, int32_t exit)
	{
		ExceptedExit e;

		e.master = master;
		e.exit = exit;
		return e;
	}

	std::vector<WaypointProblemKind> const kExits = { WaypointProblemKind::ExitOutside };
	std::vector<WaypointProblemKind> const kConnected = { WaypointProblemKind::CannotReach };

	bool Parses(char const *word, int32_t master, int32_t exit)
	{
		ExceptedExit e;

		return verify_nets::ParseExceptedExit(word, &e) && e.master == master && e.exit == exit;
	}

	bool Refused(char const *word)
	{
		ExceptedExit e;

		return !verify_nets::ParseExceptedExit(word, &e);
	}
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// ---- the word
	Check(Parses("364:3", 364, 3), "\"364:3\" names AI module 364's exit 3");
	Check(Parses("0:0", 0, 0), "\"0:0\" names AI module 0's exit 0");
	Check(Parses("123456789:987654321", 123456789, 987654321), "nine digits each are read");

	char const *const refused[] = { "", ":", "364", "364:", ":3", "364:3:1", "-1:3", "364:-3", "364:3x", " 364:3", "364:3 ", "+364:3", "364:+3", "0x16:3", "1234567890:3",
		"364:1234567890", "x:3", "364,3", "364;3" };

	for (char const *word : refused)
	{
		Check(Refused(word), std::string("\"") + word + "\" is refused");
	}

	Check(Refused(nullptr), "no word at all is refused");

	// ---- one AI module (364) with two exits in no box, 1 and 3, and a box that cannot be reached
	WaypointNetModule air06;

	air06.master = 364;
	air06.boxes = 12;
	air06.exits.resize(4);
	air06.problems.push_back(Problem(WaypointProblemKind::ExitOutside, -1, 3, "exit 3 (-1 2 3) lies in no box"));
	air06.problems.push_back(Problem(WaypointProblemKind::CannotReach, 0, 2, "box 0 cannot reach box 2"));
	air06.problems.push_back(Problem(WaypointProblemKind::ExitOutside, -1, 1, "exit 1 (4 5 6) lies in no box"));

	// nothing named: both exits fail, as before
	{
		std::vector<ExceptedExit> none;
		verify_nets::Tally const t = verify_nets::Count(air06, kExits, false, none);

		Check(t.found == 2 && t.excepted == 0, "with nothing named, both exits in no box count (" + std::to_string(t.found) + " found, " + std::to_string(t.excepted) + " excepted)");
		Check(t.first == "exit 3 (-1 2 3) lies in no box", "and the first is said in the checker's words (" + t.first + ")");
	}

	// exit 3 named: it is an exception; exit 1, not named, still fails
	{
		std::vector<ExceptedExit> named = { Named(364, 3) };
		verify_nets::Tally const t = verify_nets::Count(air06, kExits, false, named);

		Check(t.excepted == 1, "364:3 named: exit 3 is an exception (" + std::to_string(t.excepted) + " excepted)");
		Check(t.found == 1 && t.first == "exit 1 (4 5 6) lies in no box", "and exit 1, not named, still fails (" + std::to_string(t.found) + " found, " + t.first + ")");
	}

	// both named: the check passes, with both excepted
	{
		std::vector<ExceptedExit> named = { Named(364, 3), Named(364, 1) };
		verify_nets::Tally const t = verify_nets::Count(air06, kExits, false, named);

		Check(t.found == 0 && t.excepted == 2, "364:3 and 364:1 named: nothing fails, both excepted (" + std::to_string(t.found) + " found, " + std::to_string(t.excepted) + " excepted)");
	}

	// another AI module's exit 3 named: it covers nothing here
	{
		std::vector<ExceptedExit> named = { Named(367, 3) };
		verify_nets::Tally const t = verify_nets::Count(air06, kExits, false, named);

		Check(t.found == 2 && t.excepted == 0, "367:3 named: AI module 364's exits still both fail (" + std::to_string(t.found) + " found)");
	}

	// an exit number names an exit, never a box: 364:2 does not cover "box 0 cannot reach box 2"
	{
		std::vector<ExceptedExit> named = { Named(364, 2), Named(364, 0) };
		verify_nets::Tally const c = verify_nets::Count(air06, kConnected, false, named);
		verify_nets::Tally const e = verify_nets::Count(air06, kExits, false, named);

		Check(c.found == 1 && c.excepted == 0, "364:2 and 364:0 named: the box that cannot be reached still fails connected (" + std::to_string(c.found) + " found)");
		Check(e.found == 2 && e.excepted == 0, "and they cover neither exit in no box (" + std::to_string(e.found) + " found)");
	}

	// clean: every kind, the problems past the kept ones too; a named exit is an exception there as well
	{
		WaypointNetModule capped = air06;

		capped.more = 5;

		std::vector<ExceptedExit> none;
		std::vector<ExceptedExit> named = { Named(364, 3) };
		verify_nets::Tally const all = verify_nets::Count(capped, {}, true, none);
		verify_nets::Tally const t = verify_nets::Count(capped, {}, true, named);

		Check(all.found == 8 && all.excepted == 0, "clean, nothing named: 3 problems and 5 past the kept ones (" + std::to_string(all.found) + " found)");
		Check(t.found == 7 && t.excepted == 1, "clean, 364:3 named: exit 3 excepted, the rest still fail (" + std::to_string(t.found) + " found, " + std::to_string(t.excepted) + " excepted)");
	}

	// ---- the finding a named exception covers, over the whole copy
	{
		WaypointNetModule apcend;

		apcend.master = 367;
		apcend.boxes = 9;
		apcend.exits.resize(4);
		apcend.problems.push_back(Problem(WaypointProblemKind::ExitOutside, -1, 3, "exit 3 (7 8 9) lies in no box"));

		std::vector<WaypointNetModule> const nets = { air06, apcend };
		WaypointProblem const *a = verify_nets::FindExcepted(nets, Named(367, 3));
		WaypointProblem const *b = verify_nets::FindExcepted(nets, Named(364, 1));

		Check(a && a->words == "exit 3 (7 8 9) lies in no box", "367:3 finds apcend's exit 3 in no box");
		Check(b && b->words == "exit 1 (4 5 6) lies in no box", "364:1 finds air06's exit 1 in no box");
		Check(!verify_nets::FindExcepted(nets, Named(367, 1)), "367:1 finds nothing: apcend's exit 1 lies in a box (the verifier fails such a stale name)");
		Check(!verify_nets::FindExcepted(nets, Named(364, 2)), "364:2 finds nothing: box 2's problem is no exit's");
		Check(!verify_nets::FindExcepted(nets, Named(999, 3)), "999:3 finds nothing: no such AI module");
	}

	// ---- the words
	// ---- A MODULE THAT KEEPS STOCK'S NETWORK (Bishop 11-20, Frost's rule for "<master>:*": the whole-level plot leaves its refused and not-shaped AI modules with
	// stock's own network, which the checker faults, strwll01 (149) and shft21 (305) on Genshd1). "<master>:*" names the whole module, and excepts every finding of
	// it only when the module really keeps stock's network (its master in the stock set: its WAYPOINT the same as the stock file's); one the planner wrote still fails
	{
		ExceptedExit whole;

		Check(verify_nets::ParseExceptedExit("305:*", &whole) && whole.master == 305 && whole.whole, "\"305:*\" names AI module 305 whole");

		std::vector<ExceptedExit> const named = { whole };
		std::set<int32_t> const stock = { 305 }, written;
		WaypointNetModule shft21 = air06;

		shft21.master = 305;

		verify_nets::Tally const kept = verify_nets::Count(shft21, kExits, true, named, &stock);
		verify_nets::Tally const ours = verify_nets::Count(shft21, kExits, true, named, &written);
		std::vector<WaypointNetModule> const nets = { shft21 };

		Check(kept.found == 0 && kept.excepted == 3, "305:* on a module that keeps stock's network: every finding an exception (" + std::to_string(kept.found) + " found, " +
			std::to_string(kept.excepted) + " excepted)");
		Check(ours.found == 3 && ours.excepted == 0, "305:* on a network the planner wrote: every finding still fails (" + std::to_string(ours.found) + " found)");
		Check(verify_nets::FindExcepted(nets, whole, &stock) != nullptr && verify_nets::FindExcepted(nets, whole, &written) == nullptr,
			"the finding 305:* names: there when it keeps stock's network, none when the planner wrote it (so the verifier fails that name)");
		Check(strcmp(verify_nets::kStockWhy, "keeps stock's network: refused / not shaped") == 0, std::string("such an exception says why: ") + verify_nets::kStockWhy);
	}

	Check(strcmp(verify_nets::kExceptionWhy, "no alien route to this exit: an alien heading there walks the floor way (D4)") == 0, std::string("an exception says why: ") +
		verify_nets::kExceptionWhy);

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
