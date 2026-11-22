// waypoint_delete.cpp: DELETE WAYPOINTS, the pure part (waypoint_delete.h). Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
#include "waypoint_delete.h"

#include <algorithm>
#include <map>

#include "waypoint_model.h"

namespace host
{
	namespace
	{
		std::string Number(size_t n)
		{
			// with a thousands comma, as the owner writes counts ("1,240")
			std::string const digits = std::to_string((unsigned long long)n);
			std::string out;

			for (size_t i = 0; i < digits.size(); i++)
			{
				out += digits[i];
				out += (digits.size() - 1 - i) % 3 == 0 && i + 1 < digits.size() ? "," : "";
			}

			return out;
		}

		std::string Waypoints(size_t n)
		{
			return Number(n) + (n == 1 ? " alien waypoint" : " alien waypoints");
		}

		std::string Modules(size_t n)
		{
			return Number(n) + (n == 1 ? " AI module" : " AI modules");
		}
	}

	WaypointDeletePlan WaypointDelete_Plan(std::vector<ModuleSnapshot> const &snapshots, std::vector<int32_t> const &modules, bool all)
	{
		WaypointDeletePlan plan;
		std::map<int32_t, int32_t> headOf;                     // each module's AI module head (its master, or itself)
		std::map<int32_t, ModuleSnapshot const *> byIndex;   // the heads' snapshots, by object index
		std::vector<int32_t> order;                          // the heads in the file's order

		for (ModuleSnapshot const &s : snapshots)
		{
			if (!s.module)
			{
				continue;
			}

			int32_t const head = s.masterIndex >= 0 ? s.masterIndex : s.index;

			headOf[s.index] = head;

			if (head == s.index)
			{
				byIndex[s.index] = &s;
				order.push_back(s.index);
			}
		}

		// the heads asked for: every one, or each named module's, once, in the order named
		std::vector<int32_t> asked;

		if (all)
		{
			asked = order;
		}
		else
		{
			for (int32_t const m : modules)
			{
				auto const h = headOf.find(m);

				if (h != headOf.end() && std::find(asked.begin(), asked.end(), h->second) == asked.end())
				{
					asked.push_back(h->second);
				}
			}
		}

		for (int32_t const head : asked)
		{
			auto const s = byIndex.find(head);

			if (s == byIndex.end() || !s->second->hasWaypoint)
			{
				continue;   // no WAYPOINT chunk: nothing to delete (most of Rebellion's modules)
			}

			WaypointSet set;
			std::string why;

			if (!Waypoints_Parse(s->second->waypoint.data(), s->second->waypoint.size(), &set, &why))
			{
				plan.unreadable++;
				continue;
			}

			if (set.alien.empty())
			{
				continue;   // ground waypoints only: those are kept, so nothing to delete
			}

			WaypointDeleteHead one;

			one.head = head;
			one.alien = set.alien.size();
			one.ground = set.ground.size();
			plan.heads.push_back(one);
			plan.alien += one.alien;
		}

		return plan;
	}

	std::string WaypointDelete_Question(WaypointDeletePlan const &plan, bool all)
	{
		size_t ground = 0;

		for (WaypointDeleteHead const &one : plan.heads)
		{
			ground += one.ground;
		}

		std::string const what = plan.heads.size() == 1 && !all ? "AI module " + std::to_string((long long)plan.heads[0].head) : Modules(plan.heads.size());

		return std::string(all ? "Delete All Waypoints" : "Delete Waypoints") + ": delete the " + Waypoints(plan.alien) + " of " + what + "? This includes any made by hand " +
			"(most likely Rebellion's)." + (ground > 0 ? " Their " + Number(ground) + " ground waypoints (the marines' and predators') are kept." : std::string()) +
			" Nothing is lost until the file is saved.";
	}

	std::string WaypointDelete_Done(size_t waypoints, size_t modules)
	{
		return "Deleted " + Waypoints(waypoints) + " of " + Modules(modules) + "; not saved yet (File > Save keeps it)";
	}
}
