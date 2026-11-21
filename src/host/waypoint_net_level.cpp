// See waypoint_net_level.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// NAMED: the loader takes an AI module's slaves in its AIMODMAS list's order; entry_markers.h takes them in the file's order after the master. The first exit to a neighbour can differ
// only when two slaves both lead to it; the master's own entries come first either way.

#include "waypoint_net_level.h"

#include <map>

#include "chunk.hpp"
#include "environment_chunk.hpp"
#include "entry_markers.h"
#include "level_file.h"
#include "object_edit.h"
#include "waypoint_model.h"

namespace host
{
	namespace
	{
		// The AI module a module belongs to: its master, or itself when it is no slave
		int32_t MasterOf(std::map<int32_t, ModuleSnapshot const *> const &byIndex, int32_t index)
		{
			auto const found = byIndex.find(index);

			return found != byIndex.end() && found->second->masterIndex >= 0 ? found->second->masterIndex : index;
		}
	}

	double WaypointNets_Scale(OpenLevelFile *open)
	{
		if (!open || !open->Level())
		{
			return 1.0;
		}

		Chunk_With_Children *const environment = static_cast<Chunk_With_Children *>(open->Level()->lookup_single_child("REBENVDT"));
		Environment_Scale_Chunk *const scale = environment ? dynamic_cast<Environment_Scale_Chunk *>(environment->lookup_single_child("ENVSDSCL")) : nullptr;

		return scale ? scale->scale : 1.0;
	}

	bool WaypointNets_CheckLevel(OpenLevelFile *open, std::vector<WaypointNetModule> *out, std::string *why)
	{
		out->clear();

		std::vector<ModuleSnapshot> snapshots;

		if (!open || !open->Level() || ReadModuleSnapshots(open, &snapshots) != ObjectEditResult::Done)
		{
			*why = "the level's modules could not be read";
			return false;
		}

		std::map<int32_t, ModuleSnapshot const *> byIndex;

		for (ModuleSnapshot const &one : snapshots)
		{
			byIndex[one.index] = &one;
		}

		double const scale = WaypointNets_Scale(open);

		for (ModuleSnapshot const &one : snapshots)
		{
			// an AI module's network is its master's (or a lone module's); a slave's WAYPOINT is not read
			if (!one.module || one.masterIndex >= 0 || !one.hasWaypoint)
			{
				continue;
			}

			WaypointSet set;
			std::string bad;

			if (!Waypoints_Parse(one.waypoint.data(), one.waypoint.size(), &set, &bad))
			{
				*why = "module " + std::to_string(one.index) + "'s WAYPOINT: " + bad;
				return false;
			}

			if (set.alien.empty())
			{
				continue;
			}

			WaypointNetModule module;
			std::vector<EntryMarker> markers;

			module.master = one.index;
			module.boxes = set.alien.size();
			module.waypointHash = 14695981039346656037ull;

			for (uint8_t const byte : one.waypoint)
			{
				module.waypointHash = (module.waypointHash ^ byte) * 1099511628211ull;
			}

			if (!EntryMarkers_Of(open, one.index, &markers))
			{
				*why = "module " + std::to_string(one.index) + "'s entry points could not be read";
				return false;
			}

			// the first entry to each neighbouring AI module
			for (EntryMarker const &marker : markers)
			{
				int32_t const neighbour = MasterOf(byIndex, marker.adjacent);
				bool seen = false;

				for (int32_t const already : module.exitTo)
				{
					seen = seen || already == neighbour;
				}

				if (!seen)
				{
					WaypointPoint point = { { marker.point[0], marker.point[1], marker.point[2] } };

					module.exits.push_back(point);
					module.exitTo.push_back(neighbour);
				}
			}

			module.problems = Waypoints_Check(set.alien, module.exits, scale, 1000, &module.more);
			out->push_back(module);
		}

		return true;
	}

	std::string WaypointNets_ReportWords(std::vector<WaypointProblem> const &problems, size_t more)
	{
		size_t const found = problems.size() + more;

		if (found == 0)
		{
			return "checker: nothing found";
		}

		std::string words = found == 1 ? "checker: " : "checker: " + std::to_string(found) + " found: ";

		for (size_t p = 0; p < problems.size() && p < kWaypointReportNamed; p++)
		{
			words += (p ? "; " : "") + problems[p].words;
		}

		size_t const named = problems.size() < kWaypointReportNamed ? problems.size() : kWaypointReportNamed;

		if (found > named)
		{
			words += "; and " + std::to_string(found - named) + " more";
		}

		return words;
	}

	std::string WaypointNets_ReportModule(OpenLevelFile *open, int32_t master)
	{
		std::vector<WaypointNetModule> modules;
		std::string why;

		if (!WaypointNets_CheckLevel(open, &modules, &why))
		{
			return "checker: the level could not be read: " + why;
		}

		for (WaypointNetModule const &module : modules)
		{
			if (module.master == master)
			{
				return WaypointNets_ReportWords(module.problems, module.more);
			}
		}

		return "checker: no alien waypoints";
	}
}
