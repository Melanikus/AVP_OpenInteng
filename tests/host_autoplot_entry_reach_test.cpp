// host_autoplot_entry_reach_test.cpp: AN ENTRY'S REACH TOWARD ITS DOORWAY'S TOP NEVER HOLDS ANOTHER BOX'S AIM (autoplot_entry_reach.h; Bishop's spec (a) 11-21).
// The three refused Genshd1 modules whose tall entry swallowed another box's aim, from their own plot (dietrich-proof/floor-trenchb-closers-whole-on, dc953952):
// each entry's box (the plot's max minus 1: the loader's), every other box's aim, and the entry's ceiling band from the real roofs (lanes33 cells on the stock
// level: the flat roof over all its floor cells, the band from 200 into it to 450 under it). And strwll01's door0 entry, 16.2 m tall, which holds no aim: untouched.
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_entry_reach.h"

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

static std::string N(double v)
{
	return std::to_string((long long)v);
}

// whether the body at the aim (450 each way across, from 900 over its place to its place) and the margin round it reaches into the box
static bool BodyIn(double const lo[3], double const hi[3], std::vector<double> const &a)
{
	double const m = (double)host::kAutoPlotEntryReachClear;
	double const bLo[3] = { a[0] - 450.0 - m, a[1] - 900.0 - m, a[2] - 450.0 - m }, bHi[3] = { a[0] + 450.0 + m, a[1] + m, a[2] + 450.0 + m };

	for (int k = 0; k < 3; k++)
	{
		if (bHi[k] < lo[k] || bLo[k] > hi[k])
		{
			return false;
		}
	}

	return true;
}

static bool Holds(double const lo[3], double const hi[3], std::vector<double> const &a)
{
	return a[0] >= lo[0] && a[0] <= hi[0] && a[1] >= lo[1] && a[1] <= hi[1] && a[2] >= lo[2] && a[2] <= hi[2];
}

// one entry: its box, its band, the module's other aims; the y it should end with (its top and bottom), and which aims' bodies must then be clear of it
static void Entry(char const *name, double const lo0[3], double const hi0[3], double bandLo, double bandHi, std::vector<std::vector<double> > const &aims,
	double wantLo, double wantHi, size_t wantCuts, std::vector<size_t> const &clear)
{
	double lo[3] = { lo0[0], lo0[1], lo0[2] }, hi[3] = { hi0[0], hi0[1], hi0[2] };
	std::vector<host::AutoPlotEntryReachCut> const cuts = host::AutoPlot_EntryReachClear(lo, hi, bandLo, bandHi, aims);

	printf("%s: y %s..%s -> %s..%s, %zu cut(s)\n", name, N(lo0[1]).c_str(), N(hi0[1]).c_str(), N(lo[1]).c_str(), N(hi[1]).c_str(), cuts.size());
	Check(lo[1] == wantLo && hi[1] == wantHi, std::string(name) + ": the stretch stops short of the held aim's body: y " + N(lo[1]) + ".." + N(hi[1]) + " (wanted " +
		N(wantLo) + ".." + N(wantHi) + ")");
	Check(cuts.size() == wantCuts, std::string(name) + ": " + std::to_string((unsigned long long)cuts.size()) + " cut(s) named (wanted " + std::to_string((unsigned long long)wantCuts) + ")");
	Check(lo[0] == lo0[0] && hi[0] == hi0[0] && lo[2] == lo0[2] && hi[2] == hi0[2] && lo[1] <= bandLo && hi[1] >= bandHi, std::string(name) + ": x and z as they were, the band kept");

	bool bodies = true, none = true;

	for (size_t const k : clear)
	{
		bodies = bodies && !BodyIn(lo, hi, aims[k]);
	}

	for (std::vector<double> const &a : aims)
	{
		none = none && (!Holds(lo, hi, a) || (a[1] >= bandLo && a[1] <= bandHi));
	}

	Check(bodies, std::string(name) + ": the body at each held aim, and 100 mm round it, clear of the entry");
	Check(none, std::string(name) + ": no other box's aim held by the entry but in its band");
}

// ONLY WHERE THE REPAIR CAN'T (Bishop's (a') 11-21): the boxes of a module as the planner has them when the stretch is cut (the entries whole, each aim the planner's
// own: dc953952's plot, an aim the repair moved there put back where the planner had it)
struct Row
{
	int32_t min[3], max[3], centre[3];
};

static std::vector<host::WaypointRec> List(std::vector<Row> const &rows)
{
	std::vector<host::WaypointRec> list;

	for (Row const &r : rows)
	{
		host::WaypointRec w = {};

		for (int a = 0; a < 3; a++)
		{
			w.min[a] = r.min[a];
			w.max[a] = r.max[a];
			w.centre[a] = r.centre[a];
		}

		list.push_back(w);
	}

	return list;
}

static void Place(char const *name, std::vector<Row> const &rows, size_t b, bool want)
{
	bool const got = host::AutoPlot_PlaceOfItsOwn(List(rows), b, 1.0);

	Check(got == want, std::string(name) + (want ? ": has a place of its own the repair would move its aim to" : ": has no place of its own (the repair leaves it, the module is refused)") +
		" (" + (got ? "has" : "none") + ")");
}

static void Places()
{
	// opsstr01 (AI 150): floor 5, all of it under the door0 entry's stretch at its aim's height
	std::vector<Row> const opsstr01 = {
		{ { -28800, -15138, 97500 }, { -21899, -13437, 116401 }, { -25350, -14388, 102225 } }, { { -24900, -15138, 116400 }, { -18299, -13437, 120301 }, { -21600, -14388, 118350 } },
		{ { -22800, -17735, 120200 }, { -20399, -13437, 120997 }, { -21600, -17235, 120700 } }, { { -24900, -16534, 116400 }, { -21899, -15137, 119851 }, { -23400, -15139, 118125 } },
		{ { -30000, -17735, 106800 }, { -17705, -9290, 117301 }, { -18450, -17435, 115650 } }, { { -21900, -15138, 113400 }, { -18299, -13437, 116401 }, { -20100, -14388, 114900 } },
		{ { -30000, -17735, 96816 }, { -21899, -17084, 109801 }, { -25650, -17435, 97650 } }, { { -27600, -17758, 116299 }, { -17610, -17084, 120301 }, { -17705, -17435, 116850 } },
		{ { -18906, -8690, 115326 }, { -17105, -5639, 116527 }, { -17306, -8689, 115926 } }, { { -26213, -16484, 96216 }, { -25012, -13437, 98017 }, { -25613, -15587, 96416 } },
		{ { -18811, -16507, 116299 }, { -17010, -13437, 117500 }, { -17211, -15598, 116899 } }, { { -24900, -17085, 119896 }, { -22799, -15137, 120997 }, { -23850, -16112, 120346 } },
		{ { -20400, -17085, 119896 }, { -18299, -15137, 120997 }, { -19350, -16112, 120346 } } };

	Place("opsstr01 (AI 150) floor 5", opsstr01, 5, false);

	// liftshft02 (AI 385): its floor and its get-out, both all under the door0 entry's stretch
	std::vector<Row> const liftshft02 = {
		{ { -107400, -5872, -26700 }, { -99447, -1497, -23699 }, { -104400, -5572, -24400 } }, { { -107400, -3449, -26700 }, { -103799, -1748, -23699 }, { -105600, -2699, -25200 } },
		{ { -106800, -5872, -23800 }, { -104399, -1748, -23179 }, { -105600, -5372, -23300 } }, { { -106800, -4671, -26700 }, { -104399, -3448, -24149 }, { -105600, -3450, -25425 } },
		{ { -100648, -897, -25928 }, { -98847, 2139, -24727 }, { -99048, -896, -25328 } } };

	Place("liftshft02 (AI 385) floor 1", liftshft02, 1, false);
	Place("liftshft02 (AI 385) get-out 3", liftshft02, 3, false);

	// strwll01 (AI 149): exit 12, under its door1 entry's stretch up (89%), exit 11 and the door0 entry the rest
	std::vector<Row> const strwll01 = {
		{ { -2325, -3448, 113382 }, { 4876, -1747, 124183 }, { 1275, -2698, 118782 } }, { { -17925, 448, 113382 }, { -10424, 2149, 118483 }, { -14175, 1198, 115932 } },
		{ { -10525, -400, 114732 }, { -8276, 2149, 117133 }, { -8377, 0, 115932 } }, { { -17925, -947, 114732 }, { -10874, 449, 117133 }, { -14400, 447, 115932 } },
		{ { -18815, -17735, 115482 }, { -10424, -1497, 116427 }, { -17175, -1848, 116232 } }, { { -18815, -9941, 116382 }, { -10424, -1497, 120283 }, { -17475, -1848, 117132 } },
		{ { -18810, -2148, 111882 }, { -10424, -1497, 115553 }, { -18075, -1848, 114732 } }, { { -10725, -19683, 118249 }, { -2123, -1497, 124183 }, { -7875, -4223, 121932 } },
		{ { -10525, -2148, 113382 }, { -8276, 0, 115483 }, { -9397, -495, 114432 } }, { { -10525, -2148, 115482 }, { -8276, 0, 116383 }, { -9397, -495, 115932 } },
		{ { -10525, -2148, 116382 }, { -8276, 0, 118250 }, { -9397, -495, 117315 } }, { { -18815, -16484, 115226 }, { -17614, 2149, 117027 }, { -18215, -7794, 115481 } },
		{ { -18815, -8690, 116199 }, { -17614, 2149, 118000 }, { -18215, -3897, 117799 } }, { { -19410, -897, 114352 }, { -17609, 2149, 115553 }, { -19210, 0, 114952 } },
		{ { -1375, -12587, 104605 }, { 426, -1747, 105806 }, { 225, -7793, 105205 } }, { { -3324, -18432, 118249 }, { -1523, -1747, 119450 }, { -1724, -10716, 118849 } },
		{ { -1275, -18432, 109761 }, { -74, -1747, 111562 }, { -675, -10716, 109961 } } };

	Place("strwll01 (AI 149) exit 12", strwll01, 12, false);

	// STEPS04 (AI 215): its get-out's aim 1 mm inside the door0 entry (the planner's z 10360; the repair moved it to 10361): a place just past the entry's end
	std::vector<Row> const steps04 = {
		{ { 18900, 4734, 9910 }, { 26401, 6435, 10811 }, { 22650, 5484, 10360 } }, { { 26300, 3922, 9910 }, { 28512, 6435, 10811 }, { 28411, 4322, 10360 } },
		{ { 18900, 2211, 7193 }, { 26401, 4737, 10361 }, { 25700, 2511, 9910 } }, { { 18900, 3412, 9910 }, { 25951, 4735, 10811 }, { 22425, 4733, 10360 } },
		{ { 18900, 108, 10360 }, { 26401, 2862, 16188 }, { 25700, 408, 10810 } }, { { 22104, 5337, 6593 }, { 23305, 7984, 8394 }, { 22704, 5844, 6793 } },
		{ { 22104, 1359, 14987 }, { 23305, 4095, 16788 }, { 22704, 2101, 16587 } } };

	Place("STEPS04 (AI 215) get-out 3", steps04, 3, true);

	// genstrs (AI 227): the same, the get-out's planner aim at z 19050
	std::vector<Row> const genstrs = {
		{ { 18900, 448, 18600 }, { 26701, 2149, 19501 }, { 22800, 1198, 18825 } }, { { 26600, -367, 18600 }, { 28816, 2149, 19501 }, { 28715, 33, 19050 } },
		{ { 18900, -2082, 14987 }, { 26701, 893, 19051 }, { 26000, -1782, 18600 } }, { { 18900, -881, 18600 }, { 26251, 449, 19501 }, { 22103, 447, 19050 } },
		{ { 18900, -2082, 19050 }, { 26701, -1430, 20084 }, { 26000, -1782, 19500 } }, { { 22104, 1493, 14387 }, { 23305, 4085, 16188 }, { 22704, 1974, 14587 } },
		{ { 22104, -830, 18883 }, { 23305, 2149, 20684 }, { 22704, 33, 20483 } } };

	Place("genstrs (AI 227) get-out 3", genstrs, 3, true);

	// yard19 (AI 101): a HIGH whose aim the planner puts inside the door0 entry (its V3 slides it out past the entry's end, z -20005): its box runs on past the
	// entry's -20399 end, so V2's places are there too
	std::vector<Row> const yard19 = {
		{ { -95700, 448, -30000 }, { -84899, 2149, -20399 }, { -90300, 1198, -25200 } }, { { -99600, 448, -26700 }, { -95699, 2149, -23699 }, { -97650, 1198, -25200 } },
		{ { -91500, -2349, -20500 }, { -89099, 2149, -19282 }, { -90300, -1949, -19383 } }, { { -95700, -4844, -26700 }, { -84899, 449, -24149 }, { -90300, 447, -25425 } },
		{ { -99600, -6045, -30000 }, { -83852, -1497, -20399 }, { -84300, -5745, -21100 } }, { { -100258, -2148, -26700 }, { -84899, -1497, -23699 }, { -100200, -1848, -24400 } },
		{ { -95700, -6045, -20500 }, { -84899, -1949, -19282 }, { -90300, -2854, -20401 } }, { { -95700, -6045, -31373 }, { -84899, -1949, -29899 }, { -90300, -5745, -29999 } },
		{ { -85053, -897, -25928 }, { -83252, 2149, -24727 }, { -83453, 0, -25328 } }, { { -100858, -897, -25928 }, { -99057, 2149, -24727 }, { -100658, 0, -25328 } } };

	Place("yard19 (AI 101) HIGH 6", yard19, 6, true);
	Place("yard19 (AI 101) HIGH 7", yard19, 7, true);
}

int main()
{
	// opsstr01 (AI 150): door0 entry box 4; the band under the flat roof -17535: -17735..-17085. Box 5 (floor)'s aim at y -14388 is in its stretch down to the
	// vertical doorway's top (-9740): the bottom comes up to 1001 over that body's top (-14388 - 900 - 100 - 1)
	{
		double const lo[3] = { -30000, -17735, 106800 }, hi[3] = { -17706, -9291, 117300 };
		std::vector<std::vector<double> > const aims = { { -25350, -14388, 102225 }, { -21600, -14388, 118350 }, { -21600, -17235, 120700 }, { -23400, -15139, 118125 },
			{ -20100, -14388, 114900 }, { -25650, -17435, 97650 }, { -17705, -17435, 116850 }, { -17306, -8689, 115926 }, { -25613, -15587, 96416 }, { -17211, -15598, 116899 },
			{ -23850, -16112, 120346 }, { -19350, -16112, 120346 } };

		Entry("opsstr01 (AI 150) door0 entry", lo, hi, -17735, -17085, aims, -17735, -15389, 1, { 4 });
	}

	// liftshft02 (AI 385): door0 entry box 0; the band under the flat roof -5672: -5872..-5222. Box 1 (floor)'s aim at -2699 and box 3 (get-out)'s at -3450 are in
	// its stretch down to the doorway at the shaft's foot: the bottom comes up over BOTH bodies (the get-out's, the higher: -3450 - 1001)
	{
		double const lo[3] = { -107400, -5872, -26700 }, hi[3] = { -99448, -1498, -23700 };
		std::vector<std::vector<double> > const aims = { { -105600, -2699, -25200 }, { -105600, -5372, -23300 }, { -105600, -3450, -25425 }, { -99048, -896, -25328 } };

		Entry("liftshft02 (AI 385) door0 entry", lo, hi, -5872, -5222, aims, -5872, -4451, 2, { 0, 2 });
	}

	// strwll01 (AI 149): door1 entry box 5; the band under the roof -1948: -2148..-1498. Its vertical doorway's top lies 7.8 m OVER it (-9741): box 12 (exit)'s aim
	// at -3897 is in that stretch up, so the top comes down to 101 under that body's foot (-3897 + 100 + 1)
	{
		double const lo[3] = { -18815, -9941, 116382 }, hi[3] = { -10425, -1498, 120282 };
		std::vector<std::vector<double> > const aims = { { 1275, -2698, 118782 }, { -14175, 1198, 115932 }, { -8377, 0, 115932 }, { -14400, 447, 115932 },
			{ -17175, -1848, 116232 }, { -18075, -1848, 114732 }, { -7875, -4223, 121932 }, { -9397, -495, 114432 }, { -9397, -495, 115932 }, { -9397, -495, 117315 },
			{ -18215, -7794, 115481 }, { -18215, -3897, 117799 }, { -19210, 0, 114952 }, { 225, -7793, 105205 }, { -1724, -10716, 118849 }, { -675, -10716, 109961 } };

		Entry("strwll01 (AI 149) door1 entry", lo, hi, -2148, -1498, aims, -3796, -1498, 1, { 11 });

		// its door0 entry box 4, 16.2 m tall up to the vertical doorway to opsstr01: it holds no aim (exit 11's lies 1 mm past its -z side), so it is left whole
		// (as the 30 other tall entries of the level: Object11's carry its 9 lanes)
		double const lo4[3] = { -18815, -17735, 115482 }, hi4[3] = { -10425, -1498, 116426 };
		std::vector<std::vector<double> > aims4 = aims;

		aims4[4] = std::vector<double>{ -17475, -1848, 117132 };   // (its own aim, box 4's, out of the list: box 5's in its place)
		Entry("strwll01 (AI 149) door0 entry, holding no aim", lo4, hi4, -2148, -1498, aims4, -17735, -1498, 0, {});
	}

	// THE BAND IS NEVER CUT: an aim inside the band is the band's (a climb's top under a ceiling), and the stretch stops at the band's edge even when the body at a
	// held aim reaches into the band
	{
		double const lo[3] = { 0, -6000, 0 }, hi[3] = { 4000, -1000, 4000 };
		std::vector<std::vector<double> > const inBand = { { 2000, -5500, 2000 } }, nearBand = { { 2000, -4800, 2000 } };

		Entry("a made-up entry, an aim in its band", lo, hi, -6000, -5300, inBand, -6000, -1000, 0, {});
		Entry("a made-up entry, an aim just under its band", lo, hi, -6000, -5300, nearBand, -6000, -5300, 1, {});
	}

	Places();

	printf("host_autoplot_entry_reach_test: %d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
