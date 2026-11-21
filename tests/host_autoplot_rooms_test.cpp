// host_autoplot_rooms_test.cpp: THE ROOMS' WALL TEST (autoplot_rooms.h, Drake 11-20). A room's floor rectangle 10 x 5 cells of 300 mm under a flat ceiling at y -2000:
// a side is a wall where the ceiling stops at it, and not where the ceiling goes on past it (med16 on 6342c5de: 3.9 m past its floor's south edge). Each side's answer
// is the mm of it along which the ceiling goes on within kAutoPlotRoomWallLook (600 mm, two cells); a ceiling lower than the flat's tolerance does not count.
// Written 2026-11-20 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <map>
#include <string>
#include <utility>

#include "autoplot_rooms.h"

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

int main()
{
	typedef std::map<std::pair<int, int>, double> Ceilings;
	double const top = -2000.0, flat = 1000.0, e = 300.0;
	Ceilings room;

	// the floor rectangle's own ceiling: cells x 0..9, z 0..4
	for (int x = 0; x <= 9; x++)
	{
		for (int z = 0; z <= 4; z++)
		{
			room[std::make_pair(x, z)] = top;
		}
	}

	printf("A RECTANGLE WHOSE CEILING STOPS AT ITS EDGES: every side is a wall (0 mm goes on past)\n");
	{
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 0, 1, top, flat, e) == 0.0, "the +x side: 0");
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 0, -1, top, flat, e) == 0.0, "the -x side: 0");
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 2, 1, top, flat, e) == 0.0, "the +z side: 0");
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 2, -1, top, flat, e) == 0.0, "the -z side: 0");
	}

	printf("THE CEILING GOES ON PAST THE -z SIDE (a band the floor leaves out, as med16's south): that side's whole 3000 mm, the others 0\n");
	{
		Ceilings c = room;

		for (int x = 0; x <= 9; x++)
		{
			for (int z = -13; z <= -1; z++)
			{
				c[std::make_pair(x, z)] = top + 10.0;
			}
		}

		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 2, -1, top, flat, e) == 3000.0, "the -z side: 3000");
		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 2, 1, top, flat, e) == 0.0, "the +z side: 0");
		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 0, 1, top, flat, e) == 0.0, "the +x side: 0 (the band past -z is not past +x)");
	}

	printf("ONLY WITHIN 600 mm: a ceiling starting 3 cells (900 mm) past the side does not make it no wall\n");
	{
		Ceilings c = room;

		for (int x = 0; x <= 9; x++)
		{
			c[std::make_pair(x, -3)] = top;
		}

		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 2, -1, top, flat, e) == 0.0, "the -z side: 0");

		for (int x = 0; x <= 9; x++)
		{
			c[std::make_pair(x, -2)] = top;
		}

		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 2, -1, top, flat, e) == 3000.0, "with one at 2 cells (600 mm): 3000");
	}

	printf("A LOWER CEILING PAST THE SIDE (a soffit, a duct's underside more than the flat's 1000 mm under the room's) does not count; part of the side counts its part\n");
	{
		Ceilings c = room;

		for (int z = 0; z <= 4; z++)
		{
			c[std::make_pair(10, z)] = top + 1500.0;
		}

		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 0, 1, top, flat, e) == 0.0, "the +x side, 1500 lower past it: 0");

		for (int z = 0; z <= 1; z++)
		{
			c[std::make_pair(10, z)] = top + 500.0;
		}

		Check(host::AutoPlot_CeilingBeyond(c, 0, 9, 0, 4, 0, 1, top, flat, e) == 600.0, "two of its five cells under a ceiling 500 lower: 600");
	}

	printf("NOTHING TO MEASURE: a bad axis, sign or cell edge gives 0, and no crash\n");
	{
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 1, 1, top, flat, e) == 0.0, "axis 1 (y): 0");
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 0, 0, top, flat, e) == 0.0, "sign 0: 0");
		Check(host::AutoPlot_CeilingBeyond(room, 0, 9, 0, 4, 0, 1, top, flat, 0.0) == 0.0, "cell edge 0: 0");
		Check(host::AutoPlot_CeilingBeyond(room, 9, 0, 0, 4, 0, 1, top, flat, e) == 0.0, "an empty rectangle: 0");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
