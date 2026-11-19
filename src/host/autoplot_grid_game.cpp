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

#include <windows.h>

#include <math.h>

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
	void AutoPlotGrid_ProbeLoad(char const *settingsFile)
	{
		probeAsked = settingsFile && GetPrivateProfileIntA("Editor", "AutoPlotProbe", 0, settingsFile) == 1;
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
		probeGrid = "done 1 ready 1 ms " + std::to_string((unsigned long long)(GetTickCount() - probeStartTick));

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
		}
	}
}
