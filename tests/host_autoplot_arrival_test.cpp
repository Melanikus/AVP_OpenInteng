// host_autoplot_arrival_test.cpp: A DOORWAY B'S PLAN DOES NOT KNOW (src/host/autoplot_arrival.h AutoPlot_Arrivals, AutoPlot_AddArrival) on made AI modules: A -> B where B
// names A only at another doorway (Genshd1's doormp06 exit 0 -> office) gives B one arrival (A's point back as its point, A's exit point as its point back, A's middle, the
// way through turned round, A's head as its neighbour); a two-way doorway, B naming A nowhere (one-way: no arrival, measured worse), B naming A 200 mm off (the same doorway),
// a doorway with no way through on A's side, a neighbour not in the run, a neighbour of -1 or A itself give none; two modules each into the same B;
// the goals keep their numbers (the arrival's goal right after the exits', before the stand spots). Then the hostile inputs (short lists, a point of two numbers, a way
// through that is not one long or not a number). Written 2026-11-20 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "autoplot_arrival.h"

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

// a doorway on A's side as autoplot_command.cpp makes it: its middle, its way through (out of A), known
static host::AutoPlotDoor Door(int32_t x, int32_t y, int32_t z, double tx, double tz)
{
	host::AutoPlotDoor d;

	d.throughFound = true;
	d.middle[0] = x, d.middle[1] = y, d.middle[2] = z;
	d.through[0] = tx, d.through[1] = tz;
	return d;
}

// one exit of an AI module: its point, its neighbour, its doorway, the point back
static void AddExit(host::AutoPlotExits *m, std::vector<int32_t> const &point, int32_t to, host::AutoPlotDoor const &door, std::vector<int32_t> const &back)
{
	m->points.push_back(point);
	m->to.push_back(to);
	m->doors.push_back(door);
	m->backs.push_back(back);
}

static host::AutoPlotExits Module(int32_t head)
{
	host::AutoPlotExits m;

	m.head = head;
	return m;
}

// a doorway with no way through known (an exit point only)
static host::AutoPlotDoor NoDoor()
{
	return host::AutoPlotDoor();
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// GENSHD1'S CASE (doormp06 exit 0 -> office): A (head 113) has an exit to B (head 120) along +x; B's exits go to 71 and 130, and back to A only at another doorway 7.8 m
	// off (its way through not known: so no arrival back into A from it). A's doorway as the mirrored loop makes it: the exit point 100 past the middle, the point back 100
	// short of it
	host::AutoPlotExits a = Module(113), b = Module(120), c = Module(71);

	AddExit(&a, { -60000, 1900, 38900 }, 71, Door(-60000, 1900, 39000, 0.0, -1.0), { -60000, 1900, 39100 });   // A -> 71, two-way (71 lists A)
	AddExit(&a, { -50100, 1900, 40000 }, 120, Door(-50200, 1900, 40000, 1.0, 0.0), { -50300, 1900, 40000 });   // A -> B, one-way
	AddExit(&b, { -45000, 1900, 41000 }, 130, Door(-45000, 1900, 41100, 0.0, 1.0), { -45000, 1900, 41200 });
	AddExit(&b, { -47000, 1900, 38000 }, 71, Door(-47000, 1900, 38100, 0.0, -1.0), { -47000, 1900, 38200 });
	AddExit(&c, { -60000, 1900, 39100 }, 113, Door(-60000, 1900, 39000, 0.0, 1.0), { -60000, 1900, 38900 });   // 71 -> A: A's first doorway is two-way
	AddExit(&c, { -47000, 1900, 38200 }, 120, Door(-47000, 1900, 38100, 0.0, 1.0), { -47000, 1900, 38000 });   // 71 -> B: B's doorway to 71 is two-way too

	host::AutoPlotExits const bOneWay = b;   // B naming A at no doorway at all

	AddExit(&b, { -50100 + 7800, 1900, 40000 }, 113, NoDoor(), {});   // B -> A only at another doorway, 7.8 m off

	std::vector<host::AutoPlotExits> run = { a, b, c };
	std::vector<host::AutoPlotArrival> got = host::AutoPlot_Arrivals(run);

	Check(got.size() == 1, "B names A only at another doorway: one arrival (" + std::to_string(got.size()) + ")");

	if (got.size() == 1)
	{
		host::AutoPlotArrival const &r = got[0];

		Check(r.into == 1 && r.from == 113 && r.fromExit == 1, "it is B's (place 1), from A (head 113), A's exit 1");
		Check(r.point == std::vector<int32_t>({ -50300, 1900, 40000 }), "its point: A's point back (inside A, where an alien of B heading for A would aim)");
		Check(r.back == std::vector<int32_t>({ -50100, 1900, 40000 }), "its point back: A's exit point (inside B, where an alien from A lands)");
		Check(r.door.throughFound && r.door.middle[0] == -50200 && r.door.middle[1] == 1900 && r.door.middle[2] == 40000, "its doorway's middle: A's");
		Check(r.door.through[0] == -1.0 && r.door.through[1] == 0.0, "its way through turned round: out of B, into A");
		Check(!r.door.soffitFound && !r.door.stationFound, "no soffit and no climb station yet (looked up later, as for every exit)");
	}

	// THE TWO-WAY DOORWAY, seen from the other side too: 71 -> A and A -> 71 list each other, so neither gets an arrival
	bool twoWay = false;

	for (host::AutoPlotArrival const &r : got)
	{
		twoWay = twoWay || r.from == 71 || (r.from == 113 && r.fromExit == 0);
	}

	Check(!twoWay, "a two-way doorway gives no arrival on either side");

	// B's exit to 130, not in the run above; with 130 in the run, listing B back (two-way): still the one arrival
	host::AutoPlotExits d = Module(130);

	AddExit(&d, { -45000, 1900, 41300 }, 120, Door(-45000, 1900, 41100, 0.0, -1.0), { -45000, 1900, 40900 });   // 130 -> B: two-way with B -> 130
	run = { a, b, c, d };
	got = host::AutoPlot_Arrivals(run);
	Check(got.size() == 1 && got[0].into == 1 && got[0].from == 113, "with 130 in the run (two-way with B): still only A's arrival into B (" + std::to_string(got.size()) + ")");

	// B BACK TO A AT ANOTHER DOORWAY (Genshd1's doormp06 exit 0 -> office, 11-20): B lists A, but its exit point is 7.8 m from A's exit point: no exit back at this doorway, so
	// B still gets A's arrival; listed 200 mm off (the same doorway, as exit points across one doorway lie), none
	host::AutoPlotExits far = bOneWay, near = bOneWay;

	AddExit(&far, { -50100 + 7800, 1900, 40000 }, 113, Door(-50100 + 7700, 1900, 40000, -1.0, 0.0), { -50100 + 7600, 1900, 40000 });
	AddExit(&near, { -50300, 1900, 40000 }, 113, Door(-50200, 1900, 40000, -1.0, 0.0), { -50100, 1900, 40000 });
	run = { a, far, c };
	got = host::AutoPlot_Arrivals(run);

	bool intoB = false, intoA = false;

	for (host::AutoPlotArrival const &r : got)
	{
		intoB = intoB || (r.into == 1 && r.from == 113 && r.fromExit == 1);
		intoA = intoA || (r.into == 0 && r.from == 120 && r.fromExit == 2);   // and the same seen from B: A lists B only at the other doorway
	}

	Check(got.size() == 2 && intoB && intoA, "B back to A at a doorway 7.8 m off: A's arrival into B all the same, and B's far exit's arrival into A (" +
		std::to_string(got.size()) + ")");
	run = { a, near, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "B back to A 200 mm off (the same doorway): no arrival (" + std::to_string(got.size()) + ")");

	// A ONE-WAY DOORWAY (B names A at no doorway at all): no arrival (measured worse twice on the whole level: the gate takes it from A's side)
	run = { a, bOneWay, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "B naming A nowhere (one-way): no arrival (" + std::to_string(got.size()) + ")");

	// AND THE OTHER WAY ROUND: 71 lists no exit to B any more, so B -> 71 is one-way: 71 gets nothing; A's arrival into B stays
	host::AutoPlotExits c1 = c;

	c1.points.pop_back(), c1.to.pop_back(), c1.doors.pop_back(), c1.backs.pop_back();
	run = { a, b, c1 };
	got = host::AutoPlot_Arrivals(run);

	size_t intoC = 0;

	for (host::AutoPlotArrival const &r : got)
	{
		intoC += r.into == 2 ? 1 : 0;
	}

	Check(got.size() == 1 && intoC == 0 && got[0].into == 1, "71 without its exit to B (one-way B -> 71): no arrival into 71, A's into B stays (" + std::to_string(got.size()) +
		" arrivals)");

	// A NEIGHBOUR NOT IN THE RUN (a selected run): no arrival
	run = { a, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "A -> B with B not in the run: no arrival (" + std::to_string(got.size()) + ")");

	// NO WAY THROUGH ON A's SIDE (the mirrored loop found no stand spot): no arrival
	host::AutoPlotExits blind = a;

	blind.doors[1].throughFound = false;
	run = { blind, b, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "A's doorway with no way through: no arrival (" + std::to_string(got.size()) + ")");

	blind = a;
	blind.backs[1].clear();
	run = { blind, b, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "A's doorway with no point back: no arrival");

	// A NEIGHBOUR OF -1 (not known) OR A ITSELF
	host::AutoPlotExits odd = Module(200), self = Module(201);

	AddExit(&odd, { 0, 0, 100 }, -1, Door(0, 0, 0, 0.0, 1.0), { 0, 0, -100 });
	AddExit(&self, { 0, 0, 100 }, 201, Door(0, 0, 0, 0.0, 1.0), { 0, 0, -100 });
	run = { odd, self };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "a neighbour of -1 or the module itself: no arrival");

	// TWO MODULES INTO THE SAME B, which names each only at another doorway: B gets one arrival from each, in the order of the modules
	host::AutoPlotExits n1 = Module(10), n2 = Module(11), hub = Module(12);

	AddExit(&n1, { 1100, 0, 0 }, 12, Door(1000, 0, 0, 1.0, 0.0), { 900, 0, 0 });
	AddExit(&n2, { -1100, 0, 0 }, 12, Door(-1000, 0, 0, -1.0, 0.0), { -900, 0, 0 });
	AddExit(&hub, { 0, 0, 5000 }, 13, Door(0, 0, 4900, 0.0, 1.0), { 0, 0, 4800 });
	AddExit(&hub, { 9000, 0, 0 }, 10, NoDoor(), {});    // back to n1 and n2 only far off (7.9 m and 9.9 m)
	AddExit(&hub, { -11000, 0, 0 }, 11, NoDoor(), {});
	run = { n1, n2, hub };
	got = host::AutoPlot_Arrivals(run);
	Check(got.size() == 2 && got[0].from == 10 && got[1].from == 11 && got[0].into == 2 && got[1].into == 2, "two modules into the same B: two arrivals, in their order");

	// ADDING THEM: B's lists get each after its own exits, and the goals keep their numbers (the exits' goals first, the stand spots after)
	std::vector<std::vector<int32_t> > goals = hub.points;

	goals.push_back({ 0, 0, 2500 });   // two stand spots
	goals.push_back({ 500, 0, 2500 });

	bool added = true;

	for (host::AutoPlotArrival const &r : got)
	{
		added = host::AutoPlot_AddArrival(r, &hub.points, &hub.to, &hub.doors, &hub.backs, &goals) && added;
	}

	Check(added && hub.points.size() == 5 && hub.to.size() == 5 && hub.doors.size() == 5 && hub.backs.size() == 5, "both added: B's four lists hold its 3 exits and the two arrivals");
	Check(hub.to[3] == 10 && hub.to[4] == 11, "each arrival's neighbour is the module it comes from");
	Check(hub.points[3] == std::vector<int32_t>({ 900, 0, 0 }) && hub.backs[3] == std::vector<int32_t>({ 1100, 0, 0 }), "the first's point is n1's point back, its point back n1's exit");
	Check(hub.doors[4].through[0] == 1.0 && hub.doors[4].through[1] == 0.0 && hub.doors[4].middle[0] == -1000, "the second's doorway: n2's middle, its way through turned round");
	Check(goals.size() == 7, "the goals: 5 exits and 2 stand spots (" + std::to_string(goals.size()) + ")");

	bool numbered = goals.size() == 7;

	for (size_t g = 0; g < 5 && numbered; g++)
	{
		numbered = goals[g] == hub.points[g];
	}

	Check(numbered && goals[5] == std::vector<int32_t>({ 0, 0, 2500 }) && goals[6] == std::vector<int32_t>({ 500, 0, 2500 }),
		"each exit's goal has its exit's number; the stand spots after them, as they were");

	// HOSTILE: lists shorter than the exits, a point of two numbers, a way through that is not one long, or not a number
	host::AutoPlotExits shortA = a;

	shortA.to.pop_back();
	run = { shortA, b, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "A's neighbours list shorter than its exits: nothing read from A");

	host::AutoPlotExits twoNumbers = a;

	twoNumbers.points[1] = { 1, 2 };
	run = { twoNumbers, b, c };
	got = host::AutoPlot_Arrivals(run);
	Check(got.empty(), "A's exit point of two numbers: no arrival");

	host::AutoPlotExits longWay = a, notNumber = a;

	longWay.doors[1].through[0] = 3.0;
	notNumber.doors[1].through[0] = nan("");
	run = { longWay, b, c };
	got = host::AutoPlot_Arrivals(run);
	run = { notNumber, b, c };
	Check(got.empty() && host::AutoPlot_Arrivals(run).empty(), "a way through 3 long, or not a number: no arrival");

	host::AutoPlotArrival one = host::AutoPlot_Arrivals({ a, b, c }).at(0);
	std::vector<std::vector<int32_t> > fewGoals;   // fewer goals than exits
	host::AutoPlotExits keep = b;

	Check(!host::AutoPlot_AddArrival(one, &keep.points, &keep.to, &keep.doors, &keep.backs, &fewGoals) && keep.points.size() == 3 && fewGoals.empty(),
		"goals fewer than the exits: nothing added, the lists as they were");

	keep.to.pop_back();
	goals = keep.points;
	Check(!host::AutoPlot_AddArrival(one, &keep.points, &keep.to, &keep.doors, &keep.backs, &goals) && keep.points.size() == 3 && goals.size() == 3,
		"B's lists not as long as its exits: nothing added");
	Check(!host::AutoPlot_AddArrival(one, nullptr, &keep.to, &keep.doors, &keep.backs, &goals), "a list missing: nothing added");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
