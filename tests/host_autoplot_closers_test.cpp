// host_autoplot_closers_test.cpp: THE GAP CLOSERS' packing (src/host/autoplot_closers.h; the owner 11-21: "fitter boxes as large as possible within the space we have and
// 'tetris together'", "no smaller than .9 right, need to hold the alien"): the fewest pieces of at most 2.4 m along a free stretch, split evenly, never under 0.9 m; the
// free stretches left between the boxes already standing; the sizes clamped (they may come from the dialog); and the switch, on only for an exact 1.
// Written 2026-11-21 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include "autoplot_closers.h"
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

static std::string Said(std::vector<std::pair<double, double> > const &v)
{
	std::string s;

	for (auto const &p : v)
	{
		s += " " + std::to_string((long long)floor(p.first + 0.5)) + ".." + std::to_string((long long)floor(p.second + 0.5));
	}

	return s.empty() ? " (none)" : s;
}

// the pieces touch end to end and fill from..to exactly
static bool Fills(std::vector<std::pair<double, double> > const &v, double from, double to)
{
	if (v.empty() || v.front().first != from || v.back().second != to)
	{
		return false;
	}

	for (size_t k = 1; k < v.size(); k++)
	{
		if (v[k].first != v[k - 1].second)
		{
			return false;
		}
	}

	return true;
}

int main()
{
	printf("host_autoplot_closers_test\n");

	// ---- THE PIECES: the fewest of at most 2.4 m, split evenly
	{
		auto const p = host::AutoPlot_CloserPieces(-300.0, 4800.0);   // the tall corridor's climb side west of the climb-in: 5.1 m

		Check(p.size() == 3 && Fills(p, -300.0, 4800.0) && fabs((p[0].second - p[0].first) - 1700.0) < 0.001, "5.1 m: three of 1.7 m, never 2.4 + 2.4 + 0.3 (" + Said(p) + ")");
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 2400.0);

		Check(p.size() == 1 && Fills(p, 0.0, 2400.0), "2.4 m: one piece, the whole stretch" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 2401.0);

		Check(p.size() == 2 && Fills(p, 0.0, 2401.0), "2401 mm: two of 1200.5" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 900.0);

		Check(p.size() == 1 && Fills(p, 0.0, 900.0), "900 mm, a body's width: one piece" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 899.0);

		Check(p.empty(), "899 mm: none, under a body (a box a body could swallow whole)" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 14100.0);   // the walls' 90%: 14.1 m

		bool each = true;

		for (auto const &q : p)
		{
			each = each && q.second - q.first >= 900.0 && q.second - q.first <= 2400.0;
		}

		Check(p.size() == 6 && Fills(p, 0.0, 14100.0) && each, "14.1 m: six of 2.35 m" + Said(p));
	}

	// ---- THE SIZES CLAMPED (from the dialog they are input from outside): the least never under the 900 body, the most never under the least, NaN safe
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 1300.0, 2400.0, 100.0);

		Check(p.size() == 1, "a least of 100 is taken as 900: 1.3 m stays one piece" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 1300.0, 1200.0, 900.0);

		Check(p.size() == 1 && Fills(p, 0.0, 1300.0), "a most of 1.2 m on a 1.3 m stretch: one piece of 1.3, not two under the least" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 3000.0, 500.0, 900.0);

		bool each = true;

		for (auto const &q : p)
		{
			each = each && q.second - q.first >= 900.0;
		}

		Check(!p.empty() && each && Fills(p, 0.0, 3000.0), "a most under the least is taken as the least: no piece under 900" + Said(p));
	}
	{
		auto const p = host::AutoPlot_CloserPieces(0.0, 3000.0, NAN, NAN);
		auto const q = host::AutoPlot_CloserPieces(0.0, NAN);

		Check(!p.empty() && Fills(p, 0.0, 3000.0) && q.empty(), "NaN sizes fall back to the least; a NaN stretch gives none");
	}

	// ---- THE FREE STRETCHES between the blocked ones (any order, overlapping, past the ends)
	{
		std::vector<std::pair<double, double> > const blocked = { { 7201.0, 4800.0 }, { 5000.0, 6000.0 }, { -900.0, -200.0 }, { 12000.0, 13000.0 } };
		auto const f = host::AutoPlot_CloserFree(-300.0, 12301.0, blocked);

		Check(f.size() == 2 && f[0].first == -200.0 && f[0].second == 4800.0 && f[1].first == 7201.0 && f[1].second == 12000.0, "free: -200..4800 and 7201..12000" + Said(f));
	}
	{
		auto const f = host::AutoPlot_CloserFree(0.0, 1000.0, {});

		Check(f.size() == 1 && f[0].first == 0.0 && f[0].second == 1000.0, "nothing blocked: the whole stretch" + Said(f));
	}
	{
		auto const f = host::AutoPlot_CloserFree(0.0, 1000.0, { { -5.0, 2000.0 } });

		Check(f.empty(), "all of it blocked: none" + Said(f));
	}

	// ---- THE SWITCH: on only for an exact 1 in [Editor] AutoPlotClosers (the ini is input from outside)
	{
		char folder[MAX_PATH] = "", file[MAX_PATH] = "";

		GetTempPathA(MAX_PATH, folder);
		GetTempFileNameA(folder, "apc", 0, file);

		struct Case
		{
			char const *value;
			bool on;
		} const cases[] = { { "1", true }, { "0", false }, { "2", false }, { "-1", false }, { "yes", false }, { "4294967297", false } };

		for (Case const &c : cases)
		{
			WritePrivateProfileStringA("Editor", "AutoPlotClosers", c.value, file);
			host::AutoPlotClosers_Load(file);
			Check(host::AutoPlotClosers_On() == c.on, std::string("AutoPlotClosers=") + c.value + (c.on ? ": on" : ": off"));
		}

		WritePrivateProfileStringA("Editor", "AutoPlotClosers", nullptr, file);
		host::AutoPlotClosers_Load(file);
		Check(!host::AutoPlotClosers_On(), "no AutoPlotClosers line: off (a scored candidate, off unless asked)");
		host::AutoPlotClosers_Load(nullptr);
		Check(!host::AutoPlotClosers_On(), "no settings file: off");
		DeleteFileA(file);
	}

	// ---- THE BODY AT ANOTHER BOX'S POINT (the closers' rounds on Hudson's runs: two stalls, one rule; Bishop 11-21: "a closer must not reach into the body of an alien
	// standing at ANY other box's point within reach, above it (an entry's aim) or below it (a floor's point), whatever that box's role", a 100 mm margin, never under the
	// 0.9 m least, and any closer it can't keep named). In the file's numbers, scale 1 (millimetres). The game's hold test of a place: min <= p <= max - 1, every way
	auto const box = [](int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1, int32_t cx, int32_t cy, int32_t cz)
	{
		host::WaypointRec b = {};

		b.min[0] = x0; b.min[1] = y0; b.min[2] = z0;
		b.max[0] = x1; b.max[1] = y1; b.max[2] = z1;
		b.centre[0] = cx; b.centre[1] = cy; b.centre[2] = cz;
		return b;
	};
	auto const bodyInABox = [](host::AutoPlotPlan const &p, int32_t x, int32_t y, int32_t z)
	{
		// the game's body round an alien's position: x and z 450 each way, from 900 above it down to it (y is down); in a box when any of its 8 corners is
		for (int32_t const dx : { -450, 450 })
		{
			for (int32_t const dy : { -900, 0 })
			{
				for (int32_t const dz : { -450, 450 })
				{
					for (host::WaypointRec const &b : p.list)
					{
						int32_t const q[3] = { x + dx, y + dy, z + dz };
						bool in = true;

						for (int a = 0; a < 3 && in; a++)
						{
							in = q[a] >= b.min[a] && q[a] <= b.max[a] - 1;
						}

						if (in)
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	};

	// (1) AT THE POINT: opscen01's entry 4, 651 mm deep, its point 300 under its top, a closer right under it (the alien a12 hung 398 frames there, held by the entry and
	// the closer: on a ceiling an alien's position is 900 under it, so its body runs from the point down a body). Box 0 the entry, box 1 the closer
	{
		auto const plan = [&box](char const *role0, int32_t closerBottom)
		{
			host::AutoPlotPlan p;

			p.list.push_back(box(0, -2000, 0, 6000, -1349, 3000, 5300, -1700, 2600));             // the entry (or what role0 says), its point 300 under its top
			p.list.push_back(box(4800, -1350, 2100, 7200, closerBottom, 3201, 6000, -375, 2650));  // the closer, from the entry's bottom down
			p.volumeRoles = { role0, "closer" };
			return p;
		};

		host::AutoPlotPlan p = plan("entry", 600);
		int n = host::AutoPlot_CloserClearAims(&p, 1.0);

		// the body at the entry's point reaches a body under it (y -800), and the margin 100 more: the closer's top goes to y -700; everything else stays
		Check(n == 1 && p.list[1].min[1] == -700, "a closer under an entry's point: its top lowered to a body and the margin under the point (y -700: " + std::to_string((long long)p.list[1].min[1]) + ")");
		Check(p.list[1].max[1] == 600 && p.list[1].min[0] == 4800 && p.list[1].max[0] == 7200 && p.list[1].min[2] == 2100 && p.list[1].max[2] == 3201,
			"only its top moves: its bottom and its sides stay");
		Check(p.list[1].centre[0] == 6000 && p.list[1].centre[1] == -50 && p.list[1].centre[2] == 2650, "its own aim point stays in its middle, up and down (y -50)");
		Check(p.list[0].min[1] == -2000 && p.list[0].max[1] == -1349 && p.list[0].centre[1] == -1700, "the entry is untouched");
		Check(p.notes.size() == 1 && p.notes[0] == "closers cleared of the body at another box's point: 1 (tops lowered 1, bottoms raised 0)", "the notes say it (" +
			(p.notes.empty() ? std::string() : p.notes[0]) + ")");

		// THE SLOT left over the lowered closer, from the entry's bottom (y -1349) to its new top (y -700): 649 mm, shorter than a body, so a body there is in one of them.
		// Tried from the highest an alien can be (hanging, its position 900 under the ceiling, the entry's top 200 into it: y -900) down to a body inside the closer
		bool slotHeld = true;

		for (int32_t y = -900; y <= -700 + 900; y += 25)
		{
			slotHeld = slotHeld && bodyInABox(p, 6000, y, 2650);
		}

		Check(slotHeld, "a body anywhere in the slot over it is in the entry or the closer: the wall stays closed");

		// a second pass finds nothing more to do
		Check(host::AutoPlot_CloserClearAims(&p, 1.0) == 0 && p.list[1].min[1] == -700 && p.notes.size() == 1, "run again: nothing changes");

		// a closer that would be left under a body's height is left as it is, and named
		p = plan("entry", -200);
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 0 && p.list[1].min[1] == -1350, "a closer 1150 tall, 500 of it clear of a body there: left as it is (a box under the least is never made)");
		Check(p.notes.size() == 1 && p.notes[0].find("left as they are: closer 1 by box 0's point (only 500 mm of it would be left)") != std::string::npos,
			"and named in the notes (" + (p.notes.empty() ? std::string() : p.notes[0]) + ")");

		// WHATEVER THE ROLE of the box whose point it is (Bishop 11-21): a floor's and an exit's points too; only another closer's own point is not (the planner keeps each
		// centre in its own box alone, and closers along a wall meet end to end, a body at one's point touching the next)
		for (char const *role : { "entry", "climb", "get-out", "LOW", "HIGH", "floor", "exit", "stair ceiling" })
		{
			p = plan(role, 600);
			n = host::AutoPlot_CloserClearAims(&p, 1.0);
			Check(n == 1 && p.list[1].min[1] == -700, std::string("box 0 a ") + role + ": the closer lowered under its point");
		}

		p = plan("closer", 600);
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 0 && p.list[1].min[1] == -1350, "box 0 another closer: the closer stays as it is");
		// (box 0, a "closer" 6000 by 3000 across the floor, has no side 1100 across: its wall can't be read, and the note counts it, so no foot goes unlooked-at unsaid)
		Check(p.notes.size() == 1 && p.notes[0] == "closers cleared of the body at another box's point: 0 (tops lowered 0, bottoms raised 0); closers whose wall could not be read "
			"(no side 1100 across: their points' feet not looked at): 1", "and the note counts the closer whose wall can't be read (" + (p.notes.empty() ? std::string() : p.notes[0]) + ")");

		// THE MARGIN reaches 100 past the body across too: a point 450 short of the closer's side along x (its body only touching) is a reason, 550 + 1 short is not
		p = plan("entry", 600);
		p.list[0].centre[0] = 4800 - 450;
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 1 && p.list[1].min[1] == -700, "a body that only touches the closer's side: within the margin, the closer lowered");
		p = plan("entry", 600);
		p.list[0].centre[0] = 4800 - 550 - 1;
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 0 && p.list[1].min[1] == -1350, "a body and its margin short of the closer's side: the closer stays");

		// THE MARGIN NEVER OPENS A GAP: a point 51 over the entry's bottom would want the top at y -400, a slot of 950 under the entry, where a body could have its top
		// corners under the entry and its bottom ones over the closer, in neither; the top goes only as far as leaves the slot under a body (y -452: 898), which still
		// clears the body itself (it reaches y -500), and the note says the margin was cut
		p = plan("entry", 600);
		p.list[0].centre[1] = -1400;
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 1 && p.list[1].min[1] == -452, "a point near the entry's bottom: the top lowered only to a slot under a body (y -452: " + std::to_string((long long)p.list[1].min[1]) + ")");
		Check(p.notes.size() == 1 && p.notes[0].find("margin cut 1") != std::string::npos, "and the notes say the margin was cut (" + (p.notes.empty() ? std::string() : p.notes[0]) + ")");
		slotHeld = true;

		for (int32_t y = -900; y <= -452 + 900; y += 25)
		{
			slotHeld = slotHeld && bodyInABox(p, 6000, y, 2650);
		}

		Check(slotHeld, "and a body anywhere in that slot is in a box");

		// THE LEVEL'S SCALE: the file's numbers are millimetres divided by it, so at scale 2 the body is 225 across and 450 tall in them, the margin 50
		p = plan("entry", 600);
		for (host::WaypointRec &b : p.list)
		{
			for (int a = 0; a < 3; a++)
			{
				b.min[a] /= 2; b.max[a] /= 2; b.centre[a] /= 2;
			}
		}
		n = host::AutoPlot_CloserClearAims(&p, 2.0);
		Check(n == 1 && p.list[1].min[1] == -350, "at scale 2 the same place in the file's numbers: top -350 (" + std::to_string((long long)p.list[1].min[1]) + ")");

		// hostile or empty input changes nothing
		p = plan("entry", 600);
		Check(host::AutoPlot_CloserClearAims(nullptr, 1.0) == 0, "no plan: 0");
		Check(host::AutoPlot_CloserClearAims(&p, 0.0) == 0 && host::AutoPlot_CloserClearAims(&p, -1.0) == 0 && host::AutoPlot_CloserClearAims(&p, nan("")) == 0 && p.list[1].min[1] == -1350,
			"a scale of 0, under 0 or not a number: nothing measured, nothing changed");
		p.volumeRoles.pop_back();
		Check(host::AutoPlot_CloserClearAims(&p, 1.0) == 0 && p.list[1].min[1] == -1350, "a plan whose roles run short of its boxes: only the boxes with a role are read");
	}

	// (2) AT THE POINT'S FOOT: med18 (batch 18, run 115116: the alien a2 stood 88 s on the wall straight across from the floor's point, held by the floor and two
	// closers; the game sent it to the floor's point, straight out from the wall, so it could not move along the wall). Its shape: the floor box 1701 tall, its point
	// 750 under its top, two closers on its -x wall (the face at x 0, 200 into the wall and 900 out) meeting at the point's z, their bottoms 1 into the floor's top.
	// The body at the point's foot (on the wall, at the point's height and its place along the wall) reaches 151 mm into both
	{
		auto const plan = [&box](int32_t floorX0, int32_t floorZ1, int32_t pointZ, int32_t closerTop)
		{
			host::AutoPlotPlan p;

			p.list.push_back(box(floorX0, -1701, 0, 15187, 0, floorZ1, 7837, -951, pointZ));   // the floor, its point 750 under its top
			p.list.push_back(box(-200, closerTop, 0, 901, -1700, 1351, 450, (closerTop - 1700) / 2, 675));       // closer A, z 0..1350
			p.list.push_back(box(-200, closerTop, 1350, 901, -1700, 2701, 450, (closerTop - 1700) / 2, 2025));   // closer B, z 1350..2700
			p.volumeRoles = { "floor", "closer", "closer" };
			return p;
		};

		host::AutoPlotPlan p = plan(487, 2701, 1350, -3650);
		int n = host::AutoPlot_CloserClearAims(&p, 1.0);

		// the body at the foot reaches up to y -1851, and the margin 100 more: both closers' bottoms go to y -1951
		Check(n == 2 && p.list[1].max[1] == -1951 && p.list[2].max[1] == -1951, "two closers over the floor's point's foot: their bottoms raised clear of the body and the margin (y -1951: " +
			std::to_string((long long)p.list[1].max[1]) + ", " + std::to_string((long long)p.list[2].max[1]) + ")");
		Check(p.list[1].min[1] == -3650 && p.list[2].min[1] == -3650 && p.list[1].min[0] == -200 && p.list[1].max[0] == 901 && p.list[2].min[2] == 1350 && p.list[2].max[2] == 2701,
			"only their bottoms move: their tops and their sides stay");
		Check(p.list[1].centre[1] == -2800 && p.list[2].centre[1] == -2800 && p.list[1].centre[0] == 450 && p.list[2].centre[2] == 2025, "their own aim points stay in their middles (y -2800)");
		Check(p.list[0].min[1] == -1701 && p.list[0].max[1] == 0 && p.list[0].centre[1] == -951, "the floor is untouched");
		Check(p.notes.size() == 1 && p.notes[0] == "closers cleared of the body at another box's point: 2 (tops lowered 0, bottoms raised 2)", "the notes say it (" +
			(p.notes.empty() ? std::string() : p.notes[0]) + ")");

		// THE SLOT under them, from their new bottom (y -1951) to the floor's top (y -1701): 250 mm, shorter than a body: a body anywhere up the wall is in a box
		bool wallHeld = true;

		for (int32_t y = -3650 + 900; y <= 0; y += 25)
		{
			wallHeld = wallHeld && bodyInABox(p, 450, y, 675) && bodyInABox(p, 450, y, 2025);
		}

		Check(wallHeld, "a body anywhere up the wall, from the closers' tops down to the floor, is still in a box (no new gap)");
		host::AutoPlotPlan closersOnly;

		closersOnly.list = { p.list[1], p.list[2] };
		Check(!bodyInABox(closersOnly, 450, -951, 1350), "and the body at the foot is in no closer now: the game sends it on along its route");
		Check(host::AutoPlot_CloserClearAims(&p, 1.0) == 0 && p.list[1].max[1] == -1951 && p.notes.size() == 1, "run again: nothing changes");

		// THE WALL'S WAY read from the closer itself (1100 across its wall: 900 out and 200 in; the plan keeps no wall): the same shape turned a quarter (x and z swapped)
		p = plan(487, 2701, 1350, -3650);
		for (host::WaypointRec &b : p.list)
		{
			std::swap(b.min[0], b.min[2]);
			std::swap(b.max[0], b.max[2]);
			std::swap(b.centre[0], b.centre[2]);
		}
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 2 && p.list[1].max[1] == -1951 && p.list[2].max[1] == -1951, "the same turned a quarter (the wall across z): the same raise");

		// NOT WITHIN REACH: a floor that stops 1400 short of the face (a body on the wall is not in it, so the game never sends one there to its point): the closers stay
		p = plan(1400, 2701, 1350, -3650);
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 0 && p.list[1].max[1] == -1700 && p.list[2].max[1] == -1700 && p.notes.empty(), "a floor out of the wall body's reach: the closers stay");

		// along the wall past the closers' ends: the floor's point at z 3361, its foot's body and margin clear of closer B's end (z 2701): the closers stay
		p = plan(487, 4000, 2701 + 550 + 110, -3650);
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 0 && p.list[1].max[1] == -1700 && p.list[2].max[1] == -1700, "a foot along the wall past the closers: they stay");

		// THE 900 LEAST: closers 1100 tall would keep 849: left as they are, and named
		p = plan(487, 2701, 1350, -2800);
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 0 && p.list[1].max[1] == -1700 && p.list[2].max[1] == -1700, "closers 1100 tall, 849 of them would be left: they stay as they are");
		Check(p.notes.size() == 1 && p.notes[0].find("left as they are: closer 1 by box 0's point (only 849 mm of it would be left)") != std::string::npos,
			"and named in the notes (" + (p.notes.empty() ? std::string() : p.notes[0]) + ")");

		// BOTH WAYS ON ONE CLOSER: an entry over closer A, its point 300 under its top (A's top lowered) and the floor's foot under it (A's bottom raised): both, as it
		// keeps more than the least (1050)
		p = plan(487, 2701, 1350, -3650);
		p.list.push_back(box(-200, -4301, 0, 901, -3649, 1351, 450, -4001, 675));
		p.volumeRoles.push_back("entry");
		n = host::AutoPlot_CloserClearAims(&p, 1.0);
		Check(n == 2 && p.list[1].min[1] == -3001 && p.list[1].max[1] == -1951 && p.list[2].min[1] == -3650 && p.list[2].max[1] == -1951,
			"an entry over closer A and the floor's foot under both: A lowered and raised (y -3001..-1951), B raised");
		Check(p.notes.size() == 1 && p.notes[0] == "closers cleared of the body at another box's point: 2 (tops lowered 1, bottoms raised 2)", "the notes count each way (" +
			(p.notes.empty() ? std::string() : p.notes[0]) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_autoplot_closers_test\n");
		return 1;
	}

	printf("passed host_autoplot_closers_test\n");
	return 0;
}
