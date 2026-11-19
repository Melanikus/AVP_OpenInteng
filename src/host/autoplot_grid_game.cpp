// autoplot_grid_game.cpp: AutoPlotGrid_Start (autoplot_grid.h), the part that reads the GAME: the open level's polygons as the game holds them (AvPx's
// AlienWaysLevel_List: every module's, and every still object's where it stands), and each one's game module: its module's range, else, for a still object's, the
// module the game finds at the polygon's middle (ModuleFromPosition). On the editor's main thread, with the level open in the view; the build that follows is
// autoplot_grid.cpp's, on its own thread. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "pvisible.h"   // ModuleFromPosition (after the strategy and game headers it needs, as level_view.cpp has them)

#include "alien_ways_guide.h"
#include "alien_ways_level.h"
#include "alien_ways_map.h"

#include "autoplot_grid.h"
#include "autoplot_grid_feed.h"
#include "state_line.h"
#include "entry_markers.h"
#include "level_objects.h"
#include "object_list.h"

#include <windows.h>

#include <math.h>

#include <algorithm>
#include <map>
#include <set>

extern double local_scale;   // the level's scale (projload.cpp): the world's units are the file's times it

namespace host
{
	bool AutoPlotGrid_Start(OpenLevelFile *open, std::string *why)
	{
		if (!open)
		{
			*why = "no grid: no level is open";
			AutoPlotGrid_StartRefused(*why);
			return false;
		}

		std::vector<AlienWaysPolygon> polygons;
		std::vector<AlienWaysRange> ranges;
		std::vector<AlienWaysLevelDoor> doors;
		AlienWaysLevelCounts counts = {};
		char const *const refused = AlienWaysLevel_List(&polygons, &ranges, &doors, &counts);

		if (refused)
		{
			*why = std::string("no grid: the level's polygons: ") + refused;
			AutoPlotGrid_StartRefused(*why);
			return false;
		}

		// each module's range filled in once (a range is the module's own number, its m_index: the listing's), so a level of 400,000 polygons costs no scan per polygon
		std::vector<int32_t> listedModule(polygons.size(), -1);

		for (size_t m = 0; m < ranges.size(); m++)
		{
			for (size_t i = ranges[m].first; i < ranges[m].end && i < polygons.size(); i++)
			{
				listedModule[i] = (int32_t)m;
			}
		}

		for (size_t i = 0; i < polygons.size(); i++)
		{
			int32_t module = listedModule[i];

			// a still object's polygon is in no module's range: the module the game finds at its middle (the listing has checked its corners)
			if (module < 0 && polygons[i].corners > 0 && polygons[i].corners <= kAlienWaysMostCorners)
			{
				double sum[3] = { 0.0, 0.0, 0.0 };

				for (int c = 0; c < polygons[i].corners; c++)
				{
					sum[0] += polygons[i].x[c];
					sum[1] += polygons[i].y[c];
					sum[2] += polygons[i].z[c];
				}

				VECTORCH middle;

				middle.vx = (int)floor(sum[0] / polygons[i].corners + 0.5);
				middle.vy = (int)floor(sum[1] / polygons[i].corners + 0.5);
				middle.vz = (int)floor(sum[2] / polygons[i].corners + 0.5);

				MODULE *const room = ModuleFromPosition(&middle, (MODULE *)0);

				module = room ? (int32_t)room->m_index : -1;
			}

			listedModule[i] = module;
		}

		return AutoPlotGrid_StartFrom(std::move(polygons), std::move(listedModule), why);
	}
}

// THE HARNESS'S PROBE (autoplot_grid.h)
namespace
{
	bool probeAsked = false, probeStarted = false, probeDone = false;
	int probeMode = 0;   // 1: two stand spots of one module; 2: every AI module's exits, pair by pair
	std::string probeExits = "done 0";
	std::string probeFails;   // the pairs no alien way joined, named
	DWORD probeStartTick = 0;
	std::string probeGrid = "done 0 ready 0 ms 0", probeWays;

	std::string WayWords(char const *body, bool found, std::vector<host::AutoPlotPoint> const &way, DWORD ms)
	{
		return std::string(body) + " found " + (found ? "1" : "0") + " points " + std::to_string(way.size()) + " ms " + std::to_string((unsigned long long)ms) + " from " +
			std::to_string(way.empty() ? -1 : (int)way.front().module) + " to " + std::to_string(way.empty() ? -1 : (int)way.back().module);
	}
}

namespace host
{
	// AutoPlotProbe=2: what the auto-plot will search: for each AI module (its exits: ADJMDLEP entry points into the neighbours, EntryMarkers_Of, each AI module once), an
	// alien's way and a marine's between each pair of its exits (at most 12 pairs a module, 300 in all), the exits' points in the world's units (the file's times
	// local_scale). "climbing": a way found that is on a wall or the ceiling somewhere.
	static void ExitsProbe(OpenLevelFile *open)
	{
		std::vector<host::LevelObject> objects;
		std::set<std::string> seen;
		double const scale = local_scale > 0.0 ? local_scale : 1.0;
		int modules = 0, pairs = 0, found = 0, climbing = 0, tooFar = 0, noWay = 0, floorFound = 0;
		DWORD total = 0, slowest = 0;
		std::map<std::string, int> whys;   // each no-way reason, counted

		LevelObjects_Read(&objects);

		for (size_t i = 0; i < objects.size() && pairs < 300; i++)
		{
			std::vector<EntryMarker> exits;

			if (!EntryMarkers_Of(open, (int32_t)i, &exits) || exits.size() < 2)
			{
				continue;
			}

			std::string key;

			for (EntryMarker const &e : exits)
			{
				key += std::to_string(e.main) + "," + std::to_string(e.adjacent) + ";";
			}

			if (!seen.insert(key).second)
			{
				continue;   // this AI module's exits, met again from another of its modules
			}

			modules++;

			int inModule = 0;

			for (size_t a = 0; a < exits.size() && inModule < 12 && pairs < 300; a++)
			{
				for (size_t b = a + 1; b < exits.size() && inModule < 12 && pairs < 300; b++)
				{
					int32_t from[3], to[3];

					for (int k = 0; k < 3; k++)
					{
						from[k] = (int32_t)floor((double)exits[a].point[k] * scale + 0.5);
						to[k] = (int32_t)floor((double)exits[b].point[k] * scale + 0.5);
					}

					std::vector<AutoPlotPoint> way, ground;
					std::string why, groundWhy;
					DWORD const t0 = GetTickCount();
					bool const ok = AutoPlotGrid_Way(from, to, true, &way, &why);
					DWORD const took = GetTickCount() - t0;

					pairs++;
					inModule++;
					total += took;
					slowest = took > slowest ? took : slowest;

					if (ok)
					{
						found++;

						for (AutoPlotPoint const &p : way)
						{
							if (p.surface != AutoPlotPoint::Surface::Floor)
							{
								climbing++;
								break;
							}
						}
					}
					else if (why.find("as far as it may") != std::string::npos)
					{
						tooFar++;
					}
					else
					{
						noWay++;
						whys[why.substr(0, 48)]++;
					}

					if (!ok)
					{
						auto const nameOf = [&objects](int32_t index) { return index >= 0 && (size_t)index < objects.size() && objects[(size_t)index].name ? std::string(objects[(size_t)index].name) : std::string("?"); };
						int const failsListed = (int)std::count(probeFails.begin(), probeFails.end(), '|');

						if (failsListed < 24)
						{
							probeFails += (probeFails.empty() ? "" : " | ") + std::string("AI ") + std::to_string(exits[a].main) + " " + nameOf(exits[a].main) + " exits to " +
								std::to_string(exits[a].adjacent) + " " + nameOf(exits[a].adjacent) + " and " + std::to_string(exits[b].adjacent) + " " + nameOf(exits[b].adjacent) + ": " +
								(why.find("pieces") != std::string::npos ? "pieces that don't join" : why.substr(8));
						}
					}

					floorFound += AutoPlotGrid_Way(from, to, false, &ground, &groundWhy) ? 1 : 0;
				}
			}
		}

		probeExits = "done 1 modules " + std::to_string(modules) + " pairs " + std::to_string(pairs) + " found " + std::to_string(found) + " climbing " + std::to_string(climbing) +
			" toofar " + std::to_string(tooFar) + " noway " + std::to_string(noWay) + " floorfound " + std::to_string(floorFound) + " ms " +
			std::to_string((unsigned long long)total) + " max " + std::to_string((unsigned long long)slowest);

		for (auto const &one : whys)
		{
			probeExits += " | " + std::to_string(one.second) + " x " + one.first;
		}
	}

	void AutoPlotGrid_ProbeLoad(char const *settingsFile)
	{
		probeMode = settingsFile ? (int)GetPrivateProfileIntA("Editor", "AutoPlotProbe", 0, settingsFile) : 0;
		probeAsked = probeMode == 1 || probeMode == 2;
	}

	void AutoPlotGrid_ProbeFrame(OpenLevelFile *open)
	{
		if (!probeAsked || probeDone || !open)
		{
			return;
		}

		std::string why;

		if (!probeStarted)
		{
			probeStarted = true;
			probeStartTick = GetTickCount();

			if (!AutoPlotGrid_Start(open, &why))
			{
				probeGrid = "done 1 ready 0 ms 0 " + why;
				probeDone = true;
			}

			return;
		}

		if (!AutoPlotGrid_Ready(&why))
		{
			if (why != "building")
			{
				probeGrid = "done 1 ready 0 ms " + std::to_string((unsigned long long)(GetTickCount() - probeStartTick)) + " " + why;
				probeDone = true;
			}

			return;
		}

		probeDone = true;
		size_t cells = 0, kept = 0;

		AutoPlotGrid_Size(&cells, &kept);
		probeGrid = "done 1 ready 1 ms " + std::to_string((unsigned long long)(GetTickCount() - probeStartTick)) + " cells " + std::to_string((unsigned long long)cells) +
			" kb " + std::to_string((unsigned long long)(kept / 1024));

		if (probeMode == 2)
		{
			ExitsProbe(open);
			return;
		}

		// what the auto-plot asks: ways INSIDE one AI module (between its goals; the design's exits lie just inside the neighbours). Two stand spots, spread apart, of the first
		// game modules that have two, up to 8 modules, until a way joins them (a module may hold a vent or a ledge a marine can't reach). A way across the whole level can look
		// past the search's cap (Derelict, 11-18: 200,000 cells in 0.2 s), which the auto-plot never asks.
		std::string tried = "no module with two stand spots";
		int tries = 0;

		for (int32_t module = 0; module < 64 && tries < 8; module++)
		{
			std::vector<AutoPlotPoint> const spots = AutoPlotGrid_StandSpots(module, 2);

			if (spots.size() < 2)
			{
				continue;
			}

			tries++;

			std::vector<AutoPlotPoint> alien, marine;
			std::string alienWhy, marineWhy;
			DWORD const a0 = GetTickCount();
			bool const alienFound = AutoPlotGrid_Way(spots[0].place, spots[1].place, true, &alien, &alienWhy);
			DWORD const a1 = GetTickCount();
			bool const marineFound = AutoPlotGrid_Way(spots[0].place, spots[1].place, false, &marine, &marineWhy);
			DWORD const a2 = GetTickCount();

			tried = WayWords("alien", alienFound, alien, a1 - a0) + (alienFound ? "" : " (" + alienWhy + ")") + "; " + WayWords("floor", marineFound, marine, a2 - a1) +
				(marineFound ? "" : " (" + marineWhy + ")") + "; module " + std::to_string(module) + " tries " + std::to_string(tries);

			if (alienFound && marineFound)
			{
				break;
			}
		}

		probeWays = tried;
	}

	void AutoPlotGrid_ProbeStateLine()
	{
		if (probeAsked)
		{
			StateLine_Add("autoplotgrid", probeGrid);
			StateLine_Add("autoplotways", probeWays);
			StateLine_Add("autoplotexits", probeMode == 2 ? probeExits : std::string());
			StateLine_Add("autoplotfails", probeMode == 2 ? probeFails : std::string());
		}
	}
}
