// verify_autoplot_measure.cpp: the alien auto-plot design's two open points, measured on stock levels (AvP-docs/engine/inteng/reassess/alien-autoplot-design.md, "Open points"):
//   1. EXITS: does each exit point (an AI module's first ADJMDLEP entry to a neighbouring AI module, as the loader takes it) lie about 100 units inside the module it leads to? For each, its
//      depth inside that module's box (the module's place plus its shape's lowest and highest points: modules do not turn): the least distance to the box's faces when inside, or minus the
//      distance to the box when outside; and how far it lies outside its own module's box.
//   2. THE STRADDLE: how near each box's centre (where aliens aim) lies to a box it is linked to: 0 when the centre is inside that box. An alien standing at a centre that near reaches into
//      the other box with half its width; the design wants half an alien width at least.
// Read as the loader reads it (waypoint_net_level.h). It prints numbers and examples only; its exit is 0 whenever the levels can be read. Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_autoplot_measure <level file> [<level file> ...]

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"

#include "entry_markers.h"
#include "level_file.h"
#include "object_edit.h"
#include "waypoint_model.h"
#include "waypoint_net_level.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

struct Box
{
	double lo[3], hi[3];
};

// Every module's box, by its object index
static std::map<int32_t, Box> ModuleBoxes(OpenLevelFile *open)
{
	std::map<int32_t, Box> boxes;
	List<Object_Chunk *> all;

	open->Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && object->get_assoc_shape())
		{
			ChunkShape const &data = object->get_assoc_shape()->shape_data;
			Box box;

			box.lo[0] = object->object_data.location.x + data.min.x; box.hi[0] = object->object_data.location.x + data.max.x;
			box.lo[1] = object->object_data.location.y + data.min.y; box.hi[1] = object->object_data.location.y + data.max.y;
			box.lo[2] = object->object_data.location.z + data.min.z; box.hi[2] = object->object_data.location.z + data.max.z;
			boxes[static_cast<int32_t>(object->object_data.index_num)] = box;
		}
	}

	return boxes;
}

// Inside: the least distance to a face (above 0); outside: minus the distance to the box
static double Depth(Box const &box, int32_t const p[3])
{
	double inside = 1e30, outside2 = 0.0;
	bool in = true;

	for (int a = 0; a < 3; a++)
	{
		if (p[a] < box.lo[a])
		{
			in = false;
			outside2 += (box.lo[a] - p[a]) * (box.lo[a] - p[a]);
		}
		else if (p[a] > box.hi[a])
		{
			in = false;
			outside2 += (p[a] - box.hi[a]) * (p[a] - box.hi[a]);
		}
		else
		{
			double const d = p[a] - box.lo[a] < box.hi[a] - p[a] ? p[a] - box.lo[a] : box.hi[a] - p[a];
			inside = d < inside ? d : inside;
		}
	}

	return in ? inside : -sqrt(outside2);
}

// How far a point lies from a waypoint box (the game's edges: min .. max - 1), 0 inside
static double Away(host::WaypointRec const &box, int32_t const p[3])
{
	double d2 = 0.0;

	for (int a = 0; a < 3; a++)
	{
		double const lo = box.min[a], hi = (double)box.max[a] - 1.0;

		if (p[a] < lo)
			d2 += (lo - p[a]) * (lo - p[a]);
		else if (p[a] > hi)
			d2 += (p[a] - hi) * (p[a] - hi);
	}

	return sqrt(d2);
}

struct Spread
{
	std::vector<double> edges;   // a bucket ends below each edge; the last takes the rest
	std::vector<size_t> counts;
	std::vector<std::string> names;

	Spread(std::vector<double> const &e, std::vector<std::string> const &n) : edges(e), counts(e.size() + 1, 0), names(n) {}

	void Add(double v)
	{
		size_t i = 0;

		while (i < edges.size() && v >= edges[i])
			i++;

		counts[i]++;
	}

	std::string Words() const
	{
		std::string out;

		for (size_t i = 0; i < counts.size(); i++)
			out += (i ? ", " : "") + names[i] + " " + std::to_string(counts[i]);

		return out;
	}
};

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	// 1. exits: depth inside the module they lead to
	Spread depth({ 0.0, 50.0, 90.0, 111.0, 200.0, 500.0 }, { "outside it", "0-49 inside", "50-89", "90-110", "111-199", "200-499", "500 or more" });
	// how far past its own module's box an exit lies: the depth through the doorway (a distance to the nearest face of the module it leads to is no depth: an exit near the floor is
	// near the floor's face whatever its depth)
	Spread ownOut({ 0.5, 50.0, 90.0, 99.5, 100.5, 111.0, 150.0, 500.0 },
		{ "inside its own module", "under 50 out of its own", "50-89 out", "90-99 out", "100 out", "101-110 out", "111-149 out", "150-499 out", "500 or more out" });
	std::vector<double> depths;
	std::vector<std::string> outsideExamples;

	// 2. the straddle: each centre's distance to the nearest box linked to it
	Spread closeness({ 0.5, 250.0, 500.0, 1000.0 }, { "inside a linked box", "under 250 mm", "250-499", "500-999", "1000 or more" });
	size_t boxes = 0, unlinked = 0, centresOutside = 0, levels = 0;

	for (int a = 1; a < argc; a++)
	{
		OpenLevelFile open;

		if (open.Open(argv[a]) != LevelOpenResult::Opened)
		{
			printf("  %s: the game's loader refused it\n", argv[a]);
			continue;
		}

		levels++;

		std::string const whole = argv[a];
		size_t const slash = whole.find_last_of("/\\");
		std::string const name = slash == std::string::npos ? whole : whole.substr(slash + 1);
		std::map<int32_t, Box> const moduleBoxes = ModuleBoxes(&open);
		std::vector<host::WaypointNetModule> nets;
		std::string why;

		if (!host::WaypointNets_CheckLevel(&open, &nets, &why))
		{
			printf("  %s: %s\n", name.c_str(), why.c_str());
			continue;
		}

		std::vector<host::ModuleSnapshot> snapshots;

		host::ReadModuleSnapshots(&open, &snapshots);

		std::map<int32_t, int32_t> masterOf;

		for (host::ModuleSnapshot const &s : snapshots)
		{
			masterOf[s.index] = s.masterIndex >= 0 ? s.masterIndex : s.index;
		}

		for (host::ModuleSnapshot const &s : snapshots)
		{
			if (!s.module || s.masterIndex >= 0)
			{
				continue;
			}

			// the exits as the loader takes them: the first entry to each neighbouring AI module
			std::vector<host::EntryMarker> markers;
			std::vector<int32_t> taken;

			host::EntryMarkers_Of(&open, s.index, &markers);

			for (host::EntryMarker const &m : markers)
			{
				int32_t const neighbour = masterOf.count(m.adjacent) ? masterOf[m.adjacent] : m.adjacent;
				bool seen = false;

				for (int32_t const t : taken)
					seen = seen || t == neighbour;

				if (seen)
					continue;

				taken.push_back(neighbour);

				auto const into = moduleBoxes.find(m.adjacent);
				auto const from = moduleBoxes.find(m.main);

				if (into == moduleBoxes.end() || from == moduleBoxes.end())
					continue;

				double const d = Depth(into->second, m.point);

				depth.Add(d);
				depths.push_back(d);

				double const own = Depth(from->second, m.point);

				ownOut.Add(own >= 0.0 ? 0.0 : -own);

				if (d < 0.0 && outsideExamples.size() < 6)
					outsideExamples.push_back(name + " module " + std::to_string(m.main) + " to " + std::to_string(m.adjacent) + ": " + std::to_string(static_cast<int>(-d)) + " mm outside it");
			}
		}

		// the straddle, over every network
		for (host::ModuleSnapshot const &s : snapshots)
		{
			if (!s.module || s.masterIndex >= 0 || !s.hasWaypoint)
				continue;

			host::WaypointSet set;
			std::string bad;

			if (!host::Waypoints_Parse(s.waypoint.data(), s.waypoint.size(), &set, &bad))
				continue;

			for (size_t b = 0; b < set.alien.size(); b++)
			{
				host::WaypointRec const &box = set.alien[b];
				double least = 1e30;

				boxes++;
				centresOutside += Away(box, box.centre) > 0.0 ? 1 : 0;

				for (host::WaypointLinkRec const &link : box.links)
				{
					if (link.index >= 0 && static_cast<size_t>(link.index) < set.alien.size() && static_cast<size_t>(link.index) != b)
					{
						double const d = Away(set.alien[static_cast<size_t>(link.index)], box.centre);
						least = d < least ? d : least;
					}
				}

				if (least > 1e29)
					unlinked++;
				else
					closeness.Add(least);
			}
		}
	}

	double sum = 0.0;
	size_t insideCount = 0;

	for (double const d : depths)
	{
		if (d >= 0.0)
		{
			sum += d;
			insideCount++;
		}
	}

	printf("LEVELS %u\n", static_cast<unsigned>(levels));
	printf("EXITS %u (the first per neighbouring AI module): depth inside the module they lead to: %s; mean depth of those inside %.0f mm\n", static_cast<unsigned>(depths.size()), depth.Words().c_str(),
		insideCount ? sum / (double)insideCount : 0.0);
	printf("EXITS outside their own module's box: %s\n", ownOut.Words().c_str());

	for (std::string const &e : outsideExamples)
		printf("  e.g. %s\n", e.c_str());

	printf("STRADDLE: %u boxes (%u with no link, %u with their centre outside themselves): a centre's distance to the nearest box linked to it: %s\n", static_cast<unsigned>(boxes), static_cast<unsigned>(unlinked),
		static_cast<unsigned>(centresOutside), closeness.Words().c_str());
	return 0;
}
