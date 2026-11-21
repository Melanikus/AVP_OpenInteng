// host_autoplot_nest_test.cpp: THE NESTED BOXES' ORDER (autoplot_nest.h). Bishop's spec (1) 11-21: wherever box A lies inside box B (A at most 600 mm out of B on every
// axis) and A comes before B in the list, the list is reordered so A comes after B: the game takes the LAST box holding an alien's body as its own (SweepWaypoints,
// bh_waypoint.cpp 425-446: a body is in a box when any of its 8 corners is) and goes on from a route's start box, once at its centre, only when that is the start
// (177-182). With A first, at A's centre the body is in B too and B is its own: an alien whose route starts in A never goes on. Links and every index are remapped once;
// nothing else changes (no box's extent, aim or link set). Hudson: MED05 (AI 183), its door0 entry inside its door2 entry: alien 082505:a16, ~4,000 stalled frames;
// doormp09 (AI 267), its door0 entry inside its door1 entry: 224 stalled frames "in 4" (070143:a6, 073445:a12).
// The boxes are the plan's own, as bishop-int 7c8d2f4c wrote them (its whole Genshd1 plot with the closers on, dietrich-proof/floor-base7c-closers-whole-on): extents,
// aims, roles and links ("3a": to box 3, aliens only; "8ao": one way; the one-way-back mark is not in the plot's words).
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "autoplot_nest.h"
#include "autoplot_plan.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

struct Box
{
	char const *role;
	int32_t min[3], max[3], centre[3];
	char const *links;
};

// the plan from the plot's words: each box, its role, its links (flags: a = aliens only, o = one way); each doorway's entry and exit by their roles
static host::AutoPlotPlan Plan(Box const *boxes, size_t count)
{
	host::AutoPlotPlan plan;

	for (size_t i = 0; i < count; i++)
	{
		host::WaypointRec r;

		for (int a = 0; a < 3; a++)
		{
			r.min[a] = boxes[i].min[a];
			r.max[a] = boxes[i].max[a];
			r.centre[a] = boxes[i].centre[a];
		}

		for (char const *p = boxes[i].links; *p != '\0';)
		{
			while (*p == ' ')
			{
				p++;
			}

			if (*p == '\0')
			{
				break;
			}

			host::WaypointLinkRec l;

			l.index = (int32_t)strtol(p, (char **)&p, 10);
			l.flags = 0;

			for (; *p != '\0' && *p != ' '; p++)
			{
				l.flags |= *p == 'a' ? host::kLinkAlien : (*p == 'o' ? host::kLinkOneWay : 0);
			}

			r.links.push_back(l);
		}

		std::string const role = boxes[i].role;

		plan.list.push_back(r);
		plan.climbing.push_back(role.compare(0, 5, "climb") == 0);
		plan.volumeRoles.push_back(role.substr(role.rfind(' ') + 1));   // the planner's own word, the last of the plot's ("entry", "closer", ...)

		// "climb door1 entry" holds doorway 1's entry; "floor exit2 exit" doorway 2's exit box
		for (size_t at = role.find("door"); at != std::string::npos; at = role.find("door", at + 4))
		{
			size_t const n = (size_t)atoi(role.c_str() + at + 4);

			if (plan.doorBox.size() <= n)
			{
				plan.doorBox.resize(n + 1, -1);
			}

			plan.doorBox[n] = (int32_t)i;
		}

		for (size_t at = role.find("exit"); at != std::string::npos; at = role.find("exit", at + 4))
		{
			if (role[at + 4] >= '0' && role[at + 4] <= '9')
			{
				size_t const n = (size_t)atoi(role.c_str() + at + 4);

				if (plan.exitBox.size() <= n)
				{
					plan.exitBox.resize(n + 1, -1);
				}

				plan.exitBox[n] = (int32_t)i;
			}
		}
	}

	return plan;
}

// the game's own: the last box of the list holding a body standing at the point (any of its 8 corners: 450 each way across, from 900 over the point to the point; the
// loader takes 1 off each max)
static int32_t OwnBoxAt(std::vector<host::WaypointRec> const &list, int32_t const p[3])
{
	int32_t own = -1;

	for (size_t j = 0; j < list.size(); j++)
	{
		bool held = false;

		for (int corner = 0; corner < 8 && !held; corner++)
		{
			int32_t const c[3] = { p[0] + ((corner & 1) ? 450 : -450), p[1] - ((corner & 2) ? 900 : 0), p[2] + ((corner & 4) ? 450 : -450) };

			held = c[0] >= list[j].min[0] && c[0] <= list[j].max[0] - 1 && c[1] >= list[j].min[1] && c[1] <= list[j].max[1] - 1 && c[2] >= list[j].min[2] && c[2] <= list[j].max[2] - 1;
		}

		own = held ? (int32_t)j : own;
	}

	return own;
}

// the box of the list with this role and these extents (after a reorder: where it went)
static int32_t Find(host::AutoPlotPlan const &plan, Box const &b)
{
	for (size_t i = 0; i < plan.list.size(); i++)
	{
		std::string const role = b.role;
		bool same = plan.volumeRoles[i] == role.substr(role.rfind(' ') + 1);

		for (int a = 0; a < 3 && same; a++)
		{
			same = plan.list[i].min[a] == b.min[a] && plan.list[i].max[a] == b.max[a] && plan.list[i].centre[a] == b.centre[a];
		}

		if (same)
		{
			return (int32_t)i;
		}
	}

	return -1;
}

static void Module(char const *name, Box const *boxes, size_t count, size_t inner, size_t outer)
{
	host::AutoPlotPlan plan = Plan(boxes, count);
	host::AutoPlotPlan const before = plan;
	size_t const moved = host::AutoPlot_OrderNested(&plan);

	printf("%s: %zu boxes, %zu moved\n", name, count, moved);

	// THE INNER BOX, AT ITS OWN CENTRE, IS THE ALIEN'S OWN (the outer one holds the body there too)
	int32_t const at = Find(plan, boxes[inner]), out = Find(plan, boxes[outer]);

	Check(at >= 0 && out >= 0 && out < at, std::string(name) + ": the inner box (" + boxes[inner].role + ") comes after the outer one (" + boxes[outer].role + "): " +
		std::to_string((long long)out) + " before " + std::to_string((long long)at));
	Check(at >= 0 && OwnBoxAt(plan.list, plan.list[(size_t)at].centre) == at, std::string(name) + ": at the inner box's centre the last box holding the body is the inner box (" +
		std::to_string((long long)(at >= 0 ? OwnBoxAt(plan.list, plan.list[(size_t)at].centre) : -1)) + ", the inner " + std::to_string((long long)at) + ")");

	// ONLY THE NUMBERING CHANGES: every box once, the same extents, aim and role; every link the same pair and flags; each doorway's entry and exit the same boxes
	bool same = plan.list.size() == before.list.size();
	std::vector<int32_t> to(count, -1);

	for (size_t i = 0; i < count && same; i++)
	{
		to[i] = Find(plan, boxes[i]);
		same = to[i] >= 0;
	}

	Check(same, std::string(name) + ": every box once, its extents, aim and role unchanged");

	bool links = same;

	for (size_t i = 0; i < count && links; i++)
	{
		std::map<int32_t, int32_t> mine, then;

		for (host::WaypointLinkRec const &l : plan.list[(size_t)to[i]].links)
		{
			mine[l.index] = l.flags;
		}

		for (host::WaypointLinkRec const &l : before.list[i].links)
		{
			then[to[(size_t)l.index]] = l.flags;
		}

		links = mine == then;
	}

	Check(links, std::string(name) + ": every link the same two boxes, the same flags");

	bool doors = same && plan.doorBox.size() == before.doorBox.size() && plan.exitBox.size() == before.exitBox.size();

	for (size_t d = 0; d < before.doorBox.size() && doors; d++)
	{
		doors = before.doorBox[d] < 0 ? plan.doorBox[d] < 0 : plan.doorBox[d] == to[(size_t)before.doorBox[d]];
	}

	for (size_t d = 0; d < before.exitBox.size() && doors; d++)
	{
		doors = before.exitBox[d] < 0 ? plan.exitBox[d] < 0 : plan.exitBox[d] == to[(size_t)before.exitBox[d]];
	}

	Check(doors, std::string(name) + ": each doorway's entry and exit box follow their boxes");

	// THE DESIGNED DESCENTS: at each exit box's centre (an alien coming down from the entry over it) the exit is still the alien's own
	bool exits = same;

	for (size_t d = 0; d < plan.exitBox.size() && exits; d++)
	{
		exits = plan.exitBox[d] < 0 || OwnBoxAt(plan.list, plan.list[(size_t)plan.exitBox[d]].centre) == plan.exitBox[d] ||
			OwnBoxAt(before.list, before.list[(size_t)before.exitBox[d]].centre) != before.exitBox[d];   // (one the game did not take as its own before either is not asked)
	}

	Check(exits, std::string(name) + ": at each exit box's centre the exit is still the alien's own");
}

// THE SPLIT (Bishop's (C') 11-21, the owner's tetris rule)
// whether a body standing at the point (the game's 8 corners, as OwnBoxAt; `round` mm further out every way: 0 for the game's own test) is in the box
static bool BodyIn(host::WaypointRec const &b, int32_t const p[3], int32_t round)
{
	for (int corner = 0; corner < 8; corner++)
	{
		int32_t const c[3] = { p[0] + ((corner & 1) ? 450 + round : -450 - round), p[1] + ((corner & 2) ? -900 - round : round), p[2] + ((corner & 4) ? 450 + round : -450 - round) };

		if (c[0] >= b.min[0] && c[0] <= b.max[0] - 1 && c[1] >= b.min[1] && c[1] <= b.max[1] - 1 && c[2] >= b.min[2] && c[2] <= b.max[2] - 1)
		{
			return true;
		}
	}

	return false;
}

// whether a place, looking down (x and z), is in the box (the loader takes 1 off each max)
static bool PlanIn(host::WaypointRec const &b, int32_t x, int32_t z)
{
	return x >= b.min[0] && x <= b.max[0] - 1 && z >= b.min[2] && z <= b.max[2] - 1;
}

// the box with this role and these extents, whatever its aim
static int32_t FindBox(host::AutoPlotPlan const &plan, std::string const &role, host::WaypointRec const &b)
{
	for (size_t i = 0; i < plan.list.size(); i++)
	{
		bool same = plan.volumeRoles[i] == role;

		for (int a = 0; a < 3 && same; a++)
		{
			same = plan.list[i].min[a] == b.min[a] && plan.list[i].max[a] == b.max[a];
		}

		if (same)
		{
			return (int32_t)i;
		}
	}

	return -1;
}

// the flags of the link from a box to another, -1 for none
static int32_t FlagsOf(host::WaypointRec const &from, int32_t to)
{
	for (host::WaypointLinkRec const &l : from.links)
	{
		if (l.index == to)
		{
			return l.flags;
		}
	}

	return -1;
}

// the entry `outer` (doorway `door`) with the entry `inner` inside it, cut into `pieces` rectangles; `aim`, when given: both entries' aim as the planner picked it,
// before the volume repair moved either (MED05: the same place, inside both)
static void Split(char const *name, Box const *boxes, size_t count, size_t inner, size_t outer, size_t door, size_t pieces, int32_t const *aim)
{
	host::AutoPlotPlan plan = Plan(boxes, count);

	for (int a = 0; a < 3 && aim; a++)
	{
		plan.list[inner].centre[a] = aim[a];
		plan.list[outer].centre[a] = aim[a];
	}

	host::AutoPlotPlan const before = plan;
	host::WaypointRec const &o = before.list[outer];

	// the doorways' exit points: stand-ins, each exit box's middle (the planner's exit volume is made round its point)
	std::vector<std::vector<int32_t> > exits(before.exitBox.size());

	for (size_t d = 0; d < exits.size(); d++)
	{
		if (before.exitBox[d] >= 0)
		{
			host::WaypointRec const &x = before.list[(size_t)before.exitBox[d]];

			exits[d] = std::vector<int32_t>{ (x.min[0] + x.max[0]) / 2, (x.min[1] + x.max[1]) / 2, (x.min[2] + x.max[2]) / 2 };
		}
	}

	size_t const split = host::AutoPlot_SplitNestedEntries(&plan, exits, 1.0);

	host::AutoPlot_OrderNested(&plan);
	printf("%s, split: %zu entry split, %zu -> %zu boxes\n", name, split, count, plan.list.size());
	Check(split == 1, std::string(name) + ": the outer entry (" + boxes[outer].role + ") is split (" + std::to_string((unsigned long long)split) + " split)");
	Check(plan.list.size() <= (size_t)host::kAutoPlotMostBoxes && plan.list.size() == plan.volumeRoles.size() && plan.list.size() == plan.climbing.size(),
		std::string(name) + ": at most 99 boxes, a role and a climbing mark each");

	// every box but the outer one as it was (extents, aim, role); the inner's top the outer's when that is higher (Bishop's (a): doormp09's door0 entry was sized
	// for its low ceiling, -1794, under a roof that ramps up to the outer's -1948)
	std::vector<int32_t> to(count, -1);
	bool kept = true;

	for (size_t i = 0; i < count; i++)
	{
		std::string const role = boxes[i].role;

		host::WaypointRec want = before.list[i];

		want.min[1] = i == inner ? std::min(want.min[1], o.min[1]) : want.min[1];
		to[i] = i == outer ? -1 : FindBox(plan, role.substr(role.rfind(' ') + 1), want);
		kept = kept && (i == outer || (to[i] >= 0 && plan.list[(size_t)to[i]].centre[0] == before.list[i].centre[0] &&
			plan.list[(size_t)to[i]].centre[1] == before.list[i].centre[1] && plan.list[(size_t)to[i]].centre[2] == before.list[i].centre[2]));
	}

	Check(kept, std::string(name) + ": every other box there, its extents, aim and role as they were (the inner's top the outer's: y " +
		std::to_string((long long)before.list[inner].min[1]) + " -> " + std::to_string((long long)std::min(before.list[inner].min[1], o.min[1])) + ")");

	// THE PIECES: the entries inside the outer's old area, but the inner; looking down they and the inner cover it once everywhere (no gap, no overlap)
	std::vector<size_t> piece;
	int32_t const in = to[inner];

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		host::WaypointRec const &b = plan.list[i];

		if ((int32_t)i != in && plan.volumeRoles[i] == "entry" && b.min[0] >= o.min[0] && b.max[0] <= o.max[0] && b.min[2] >= o.min[2] && b.max[2] <= o.max[2] &&
			b.min[1] == o.min[1] && b.max[1] == o.max[1])
		{
			piece.push_back(i);
		}
	}

	Check(piece.size() == pieces, std::string(name) + ": the outer entry is " + std::to_string((unsigned long long)piece.size()) + " pieces (" +
		std::to_string((unsigned long long)pieces) + " the fewest, largest), each its height");

	size_t once = 0, samples = 0;

	for (int32_t x = o.min[0] + 50; x < o.max[0] && in >= 0; x += 100)
	{
		for (int32_t z = o.min[2] + 50; z < o.max[2]; z += 100)
		{
			size_t holders = PlanIn(plan.list[(size_t)in], x, z) ? 1 : 0;

			for (size_t const p : piece)
			{
				holders += PlanIn(plan.list[p], x, z) ? 1 : 0;
			}

			once += holders == 1 ? 1 : 0;
			samples++;
		}
	}

	Check(samples > 0 && once == samples, std::string(name) + ": looking down, the pieces and the inner entry hold every place of the old entry once (" +
		std::to_string((unsigned long long)once) + " of " + std::to_string((unsigned long long)samples) + ")");

	bool wide = !piece.empty();

	for (size_t const p : piece)
	{
		wide = wide && plan.list[p].max[0] - plan.list[p].min[0] >= host::kAutoPlotNestLeast && plan.list[p].max[2] - plan.list[p].min[2] >= host::kAutoPlotNestLeast;
		printf("  piece %zu: x %d..%d, z %d..%d, aim %d %d %d\n", p, plan.list[p].min[0], plan.list[p].max[0], plan.list[p].min[2], plan.list[p].max[2], plan.list[p].centre[0],
			plan.list[p].centre[1], plan.list[p].centre[2]);
	}

	Check(wide, std::string(name) + ": every piece at least 0.9 m across both ways");

	// THE DOORWAY PIECE: the doorway's entry still, over its exit; every link of the outer kept, the same flags
	int32_t const dp = door < plan.doorBox.size() ? plan.doorBox[door] : -1;
	bool isPiece = false;

	for (size_t const p : piece)
	{
		isPiece = isPiece || (int32_t)p == dp;
	}

	Check(isPiece && !exits[door].empty() && PlanIn(plan.list[(size_t)dp], exits[door][0], exits[door][2]), std::string(name) +
		": the doorway's entry is the piece over its exit point (box " + std::to_string((long long)dp) + ")");

	bool links = isPiece;

	for (host::WaypointLinkRec const &l : o.links)
	{
		int32_t const b = l.index >= 0 && (size_t)l.index < count ? to[(size_t)l.index] : -1;

		links = links && b >= 0 && FlagsOf(plan.list[(size_t)dp], b) == l.flags;
	}

	Check(links, std::string(name) + ": the doorway piece keeps every link of the outer entry, the same flags");

	// THE OTHER PIECES: aliens only both ways to the doorway piece; every link of the list has its way back
	bool joined = isPiece;

	for (size_t const p : piece)
	{
		joined = joined && ((int32_t)p == dp || (FlagsOf(plan.list[p], dp) == host::kLinkAlien && FlagsOf(plan.list[(size_t)dp], (int32_t)p) == host::kLinkAlien));
	}

	Check(joined, std::string(name) + ": every other piece is linked aliens only both ways to the doorway piece");

	bool back = true;

	for (size_t i = 0; i < plan.list.size(); i++)
	{
		for (host::WaypointLinkRec const &l : plan.list[i].links)
		{
			back = back && l.index >= 0 && (size_t)l.index < plan.list.size() && FlagsOf(plan.list[(size_t)l.index], (int32_t)i) >= 0;
		}
	}

	Check(back, std::string(name) + ": every link has its way back, inside the list");

	// NO BARE CEILING (Bishop's cut-cover): a hanging alien (its place 900 under the ceiling, 200 under the entry's top) anywhere over the old entry is in a box
	size_t bare = 0, places = 0;

	for (int32_t x = o.min[0] + 150; x < o.max[0]; x += 300)
	{
		for (int32_t z = o.min[2] + 150; z < o.max[2]; z += 300)
		{
			int32_t const p[3] = { x, o.min[1] + 200 + 900, z };

			bare += OwnBoxAt(plan.list, p) < 0 ? 1 : 0;
			places++;
		}
	}

	Check(bare == 0, std::string(name) + ": a hanging alien anywhere over the old entry is in a box (" + std::to_string((unsigned long long)bare) + " of " +
		std::to_string((unsigned long long)places) + " places in none)");

	// BOTH ENTRIES GO ON: at the inner entry's aim and at each piece's the game's own box is that box; at each piece's aim the body is in no other box at all
	Check(in >= 0 && OwnBoxAt(plan.list, plan.list[(size_t)in].centre) == in, std::string(name) + ": at the inner entry's aim its own box is the inner entry (" +
		std::to_string((long long)(in >= 0 ? OwnBoxAt(plan.list, plan.list[(size_t)in].centre) : -1)) + ", the inner " + std::to_string((long long)in) + ")");

	bool own = !piece.empty(), alone = !piece.empty();

	for (size_t const p : piece)
	{
		own = own && OwnBoxAt(plan.list, plan.list[p].centre) == (int32_t)p;

		for (size_t j = 0; j < plan.list.size(); j++)
		{
			alone = alone && (j == p || !BodyIn(plan.list[j], plan.list[p].centre, 0));
		}
	}

	Check(own, std::string(name) + ": at each piece's aim its own box is that piece");
	Check(alone, std::string(name) + ": at each piece's aim the body is in no other box");

	// NO OTHER BOX LOSES ITS OWN BOX AT ITS AIM (Bishop 11-21: the whole module, not only the entries): every box but the outer one that the game took as its own at
	// its aim before the split still is (doormp09's closer under the west piece: with the pieces at the list's end the piece came after it, and won there)
	bool keepsOwn = kept;
	std::string lost;

	for (size_t i = 0; i < count && kept; i++)
	{
		if (i == outer || OwnBoxAt(before.list, before.list[i].centre) != (int32_t)i)
		{
			continue;
		}

		if (OwnBoxAt(plan.list, plan.list[(size_t)to[i]].centre) != to[i])
		{
			keepsOwn = false;
			lost += std::string(" ") + boxes[i].role + " (box " + std::to_string((unsigned long long)i) + ")";
		}
	}

	Check(keepsOwn, std::string(name) + ": every other box the game took as its own at its aim before the split still is" + (lost.empty() ? std::string() : ": NOT" + lost));

	// THE DESIGNED DESCENTS still: at each exit box's centre the exit is the alien's own
	bool exitsOwn = true;

	for (size_t d = 0; d < plan.exitBox.size(); d++)
	{
		exitsOwn = exitsOwn && (plan.exitBox[d] < 0 || OwnBoxAt(plan.list, plan.list[(size_t)plan.exitBox[d]].centre) == plan.exitBox[d]);
	}

	Check(exitsOwn, std::string(name) + ": at each exit box's centre the exit is still the alien's own");
	Check(split == 1 && plan.centresShared == 0, std::string(name) + ": no aim held by another box (" + std::to_string((unsigned long long)plan.centresShared) + " shared)");
}

// a piece that would be under 0.9 m: the entry left whole and named (Bishop: "name any piece that would fall under 0.9 m and stop there")
static void SplitRefused(char const *name, Box const *boxes, size_t count, size_t outer, int32_t outerWest)
{
	host::AutoPlotPlan plan = Plan(boxes, count);

	plan.list[outer].min[0] = outerWest;

	host::AutoPlotPlan const before = plan;
	size_t const split = host::AutoPlot_SplitNestedEntries(&plan, std::vector<std::vector<int32_t> >(), 1.0);
	bool named = false;

	for (std::string const &n : plan.notes)
	{
		named = named || (n.find("left whole") != std::string::npos && n.find("under 900 mm") != std::string::npos);
		printf("  note: %s\n", n.c_str());
	}

	Check(split == 0 && plan.list.size() == before.list.size() && plan.list[outer].min[0] == outerWest && plan.list[outer].max[0] == before.list[outer].max[0],
		std::string(name) + ": with a piece under 0.9 m the outer entry is left whole");
	Check(named, std::string(name) + ": and the note names it");
}

int main()
{
	static Box const med05[] = {
		{ "floor floor", { -73200, -7344, 85500 }, { -57899, -5643, 93001 }, { -65550, -6594, 89250 }, "1a 2a 3ao 4a 8a 11ao 12ao 13ao 14ao 15ao 16ao 17ao 18ao 19ao 20ao 21ao 22ao" },   // 0
		{ "floor floor", { -73200, -7344, 82200 }, { -66599, -5643, 85501 }, { -69900, -6594, 83850 }, "0a 9a 10a 23ao 24ao" },   // 1
		{ "floor floor", { -62400, -7344, 82200 }, { -57899, -5643, 85501 }, { -60150, -6594, 83850 }, "0a 25ao 26ao 27ao 28ao" },   // 2
		{ "climb climb", { -66750, -11889, 92900 }, { -64349, -5643, 93721 }, { -65550, -11389, 93400 }, "0a 5ao 7ao" },   // 3
		{ "climb get-out", { -73200, -10688, 85500 }, { -66599, -7343, 92551 }, { -69900, -7345, 89025 }, "0ao 7a" },   // 4
		{ "climb door0 entry", { -65550, -11889, 85500 }, { -57355, -11238, 93001 }, { -57899, -11589, 92300 }, "3a 6a 7a 8ao 17a 19a 21a" },   // 5
		{ "climb door1 entry", { -73200, -11889, 81035 }, { -65148, -9290, 85501 }, { -65850, -11589, 82350 }, "5a 9ao 23a 24a" },   // 6
		{ "climb door2 entry", { -73200, -11889, 83177 }, { -57899, -11238, 93001 }, { -65551, -11589, 92300 }, "3a 5a 10ao 4ao 11a 13a 15a 17a 19a 21a 25a 27a" },   // 7
		{ "floor exit0 exit", { -58556, -10638, 87075 }, { -56755, -5643, 88276 }, { -56956, -8767, 87675 }, "5a 0ao" },   // 8
		{ "floor exit1 exit", { -66349, -8690, 80435 }, { -65148, -5643, 82236 }, { -65749, -7793, 80635 }, "6a 1ao" },   // 9
		{ "floor exit2 exit", { -65680, -10638, 83177 }, { -63879, -5542, 84378 }, { -65480, -8767, 83777 }, "7a 1ao" },   // 10
		{ "climb closer", { -73200, -11239, 92620 }, { -71049, -9291, 93721 }, { -72125, -10266, 93070 }, "0a 7ao 12a 13a 14a" },   // 11
		{ "climb closer", { -73200, -9292, 92620 }, { -71049, -7343, 93721 }, { -72125, -8318, 93070 }, "11a 0a 13a 14a" },   // 12
		{ "climb closer", { -71050, -11239, 92620 }, { -68899, -9291, 93721 }, { -69975, -10266, 93070 }, "11a 12a 0a 7ao 14a 15a 16a" },   // 13
		{ "climb closer", { -71050, -9292, 92620 }, { -68899, -7343, 93721 }, { -69975, -8318, 93070 }, "11a 12a 13a 0a 15a 16a" },   // 14
		{ "climb closer", { -68900, -11239, 92620 }, { -66749, -9291, 93721 }, { -67825, -10266, 93070 }, "13a 14a 0a 7ao 16a" },   // 15
		{ "climb closer", { -68900, -9292, 92620 }, { -66749, -7343, 93721 }, { -67825, -8318, 93070 }, "13a 14a 15a 0a" },   // 16
		{ "climb closer", { -64350, -11239, 92620 }, { -62199, -9291, 93721 }, { -63275, -10266, 93070 }, "0a 5ao 7ao 18a 19a 20a" },   // 17
		{ "climb closer", { -64350, -9292, 92620 }, { -62199, -7343, 93721 }, { -63275, -8318, 93070 }, "17a 0a 19a 20a" },   // 18
		{ "climb closer", { -62200, -11239, 92620 }, { -60049, -9291, 93721 }, { -61125, -10266, 93070 }, "17a 18a 0a 5ao 7ao 20a 21a 22a" },   // 19
		{ "climb closer", { -62200, -9292, 92620 }, { -60049, -7343, 93721 }, { -61125, -8318, 93070 }, "17a 18a 19a 0a 21a 22a" },   // 20
		{ "climb closer", { -60050, -10589, 92620 }, { -57899, -9291, 93721 }, { -58975, -9940, 93070 }, "19a 20a 0a 5ao 7ao 22a" },   // 21
		{ "climb closer", { -60050, -9292, 92620 }, { -57899, -7343, 93721 }, { -58975, -8318, 93070 }, "19a 20a 21a 0a" },   // 22
		{ "climb closer", { -73743, -9291, 82200 }, { -72642, -7594, 83851 }, { -73093, -8442, 83025 }, "1a 6ao 24a" },   // 23
		{ "climb closer", { -73743, -9291, 83850 }, { -72642, -7594, 85501 }, { -73093, -8442, 84675 }, "23a 1a 6ao" },   // 24
		{ "climb closer", { -58800, -11239, 82200 }, { -57699, -9291, 83851 }, { -58350, -10266, 83025 }, "2a 7ao 26a 27a 28a" },   // 25
		{ "climb closer", { -58800, -9292, 82200 }, { -57699, -7594, 83851 }, { -58350, -8443, 83025 }, "25a 2a 27a 28a" },   // 26
		{ "climb closer", { -58800, -11239, 83850 }, { -57699, -9291, 85501 }, { -58350, -10266, 84675 }, "25a 26a 2a 7ao 28a" },   // 27
		{ "climb closer", { -58800, -9292, 83850 }, { -57699, -7594, 85501 }, { -58350, -8443, 84675 }, "25a 26a 27a 2a" },   // 28
	};

	static Box const doormp09[] = {
		{ "floor floor", { 13500, 233, 118800 }, { 24301, 2149, 120301 }, { 18900, 983, 119550 }, "1a 2ao 3a 8a 9ao" },   // 0
		{ "floor floor", { 16800, 12, 117300 }, { 21001, 1795, 118801 }, { 18900, 762, 118050 }, "0a 7a" },   // 1
		{ "climb climb", { 24200, -2148, 118800 }, { 24853, 2149, 120301 }, { 24700, -1648, 119550 }, "0a 5ao" },   // 2
		{ "climb get-out", { 16800, -947, 118800 }, { 20551, 234, 120301 }, { 18675, 232, 119550 }, "0ao 4a 5a" },   // 3
		{ "climb door0 entry", { 16800, -1794, 116787 }, { 21001, -1107, 120301 }, { 20300, -1494, 116787 }, "5a 7ao 3ao" },   // 4
		{ "climb door1 entry", { 13500, -2148, 117300 }, { 24301, -1497, 121396 }, { 23600, -1848, 120900 }, "2a 4a 8ao 3ao 9a 6a" },   // 5
		{ "climb HIGH", { 20900, -1794, 117300 }, { 22754, -41, 118801 }, { 21001, -1494, 118050 }, "5a" },   // 6
		{ "floor exit0 exit", { 18207, -507, 116187 }, { 19408, 1759, 117988 }, { 18807, 0, 116387 }, "4a 1ao" },   // 7
		{ "floor exit1 exit", { 18207, -897, 120195 }, { 19408, 2149, 121996 }, { 18807, 0, 121795 }, "5a 0ao" },   // 8
		{ "climb closer", { 12762, -1498, 118800 }, { 13863, -17, 120301 }, { 13412, -757, 119550 }, "0a 5ao" },   // 9
	};

	Module("MED05 (AI 183)", med05, sizeof(med05) / sizeof(med05[0]), 5, 7);
	Module("doormp09 (AI 267)", doormp09, sizeof(doormp09) / sizeof(doormp09[0]), 4, 5);

	static int32_t const med05Aim[3] = { -64350, -11589, 92300 };   // both entries' first aim, the planner's (the repair then moved them apart: "V2 moved box 5 ... 6451 mm")

	Split("MED05 (AI 183)", med05, sizeof(med05) / sizeof(med05[0]), 5, 7, 2, 2, med05Aim);
	Split("doormp09 (AI 267)", doormp09, sizeof(doormp09) / sizeof(doormp09[0]), 4, 5, 1, 3, nullptr);
	SplitRefused("doormp09 (AI 267), its door1 entry 500 mm past the inner one", doormp09, sizeof(doormp09) / sizeof(doormp09[0]), 5, 16300);

	printf("host_autoplot_nest_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
