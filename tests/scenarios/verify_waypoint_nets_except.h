// verify_waypoint_nets_except.h: verify_waypoint_nets' "except" option, the exits it is told by name to report as EXCEPTIONS instead of failures (Bishop 2026-11-20, his rule-6(a)
// ruling: on Genshd1 air06 (364) exit 3 and apcend (367) exit 3 lie in no box, because their areas have no alien way to the rest, and the auto-plot writes those networks anyway; the
// stock game drops the waypoints for a target in no box, bh_waypoint.cpp NPCGetWaypointDirection, D4). Only the exits named, only for EXIT OUTSIDE; anything not named fails as before.
// Written 2026-11-20 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "waypoint_net_level.h"

namespace verify_nets
{
	// what an excepted exit prints, after the checker's own words
	char const kExceptionWhy[] = "no alien route to this exit: an alien heading there walks the floor way (D4)";
	// what a "<master>:*" exception prints: the module keeps stock's network (refused or not shaped by the auto-plot; Frost's rule 11-20: it holds only then)
	char const kStockWhy[] = "keeps stock's network: refused / not shaped";

	struct ExceptedExit
	{
		int32_t master = -1;   // the AI module's master's object index, as the verifier's lines name it
		int32_t exit = -1;     // the exit's number in that AI module, as "exit N (...) lies in no box" names it
		bool whole = false;    // "<master>:*": the whole module, which keeps stock's network (exit -1); it excepts only a module in the stock set (Count, FindExcepted)
	};

	namespace detail
	{
		// 1 to 9 plain decimal digits, from text to end (no sign, no space, no other base)
		inline bool Number(char const *text, char const *end, int32_t *out)
		{
			if (end - text < 1 || end - text > 9)
			{
				return false;
			}

			int32_t value = 0;

			for (char const *c = text; c < end; c++)
			{
				if (*c < '0' || *c > '9')
				{
					return false;
				}

				value = value * 10 + (*c - '0');
			}

			*out = value;
			return true;
		}
	}

	// one "<master>:<exit>" word, e.g. "364:3": two plain decimal numbers of 1 to 9 digits, one colon between them, nothing else
	inline bool ParseExceptedExit(char const *word, ExceptedExit *out)
	{
		if (!word || !out)
		{
			return false;
		}

		char const *colon = nullptr;
		char const *end = word;

		for (; *end; end++)
		{
			if (*end == ':')
			{
				if (colon)
				{
					return false;
				}

				colon = end;
			}

			if (end - word > 20)
			{
				return false;
			}
		}

		ExceptedExit e;

		if (colon && colon[1] == '*' && colon + 2 == end && detail::Number(word, colon, &e.master))
		{
			e.whole = true;
			e.exit = -1;
			*out = e;
			return true;
		}

		if (!colon || !detail::Number(word, colon, &e.master) || !detail::Number(colon + 1, end, &e.exit))
		{
			return false;
		}

		*out = e;
		return true;
	}

	// whether a problem of the AI module 'master' is one the names except: an EXIT OUTSIDE of that AI module, of the exit named
	inline bool IsExcepted(std::vector<ExceptedExit> const &excepted, int32_t master, host::WaypointProblem const &problem, std::set<int32_t> const *stock = nullptr)
	{
		// a whole module named: any finding of it, when it keeps stock's network
		for (ExceptedExit const &e : excepted)
		{
			if (e.whole && e.master == master && stock && stock->count(master))
			{
				return true;
			}
		}

		if (problem.kind != host::WaypointProblemKind::ExitOutside)
		{
			return false;
		}

		for (ExceptedExit const &e : excepted)
		{
			if (!e.whole && e.master == master && e.exit == problem.other)
			{
				return true;
			}
		}

		return false;
	}

	// the copy's finding a name excepts, or null when it names none (the verifier fails such a name: a named exception must name a real finding)
	inline host::WaypointProblem const *FindExcepted(std::vector<host::WaypointNetModule> const &nets, ExceptedExit const &e, std::set<int32_t> const *stock = nullptr)
	{
		std::vector<ExceptedExit> const one(1, e);

		for (host::WaypointNetModule const &net : nets)
		{
			if (net.master != e.master)
			{
				continue;
			}

			for (host::WaypointProblem const &p : net.problems)
			{
				if (IsExcepted(one, net.master, p, stock))
				{
					return &p;
				}
			}
		}

		return nullptr;
	}

	struct Tally
	{
		size_t found = 0;      // the problems that count: the check fails when there is one
		size_t excepted = 0;   // the named exceptions among the problems the check looks at
		std::string first;     // the first that counts, in the checker's words
	};

	// what one check finds in one AI module's network: the problems of the kinds given, or of every kind when clean (and then the problems past the kept ones too), less the
	// named exceptions, which are counted apart
	inline Tally Count(host::WaypointNetModule const &net, std::vector<host::WaypointProblemKind> const &kinds, bool clean, std::vector<ExceptedExit> const &excepted,
		std::set<int32_t> const *stock = nullptr)
	{
		Tally t;

		for (host::WaypointProblem const &p : net.problems)
		{
			bool counts = clean;

			for (host::WaypointProblemKind const k : kinds)
			{
				counts = counts || p.kind == k;
			}

			if (!counts)
			{
				continue;
			}

			if (IsExcepted(excepted, net.master, p, stock))
			{
				t.excepted++;
				continue;
			}

			t.found++;
			t.first = t.first.empty() ? p.words : t.first;
		}

		t.found += clean ? net.more : 0;
		return t;
	}
}
