// See placement_hierarchy_fly.h. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "placement_hierarchy_fly.h"

#include <vector>

#include "special_objects_edit.h"

bool PlacedHierarchyFly_World(OpenLevelFile *open, size_t number, double scale, int32_t world[3])
{
	std::vector<host::PlacedHierarchyInfo> all;

	if (!open || !world || host::PlacedHierarchies_Read(open, &all) != host::SpecialResult::Done || number >= all.size())
	{
		return false;
	}

	double const s = scale > 0.0 ? scale : 1.0;

	for (int axis = 0; axis < 3; axis++)
	{
		world[axis] = (int32_t)(all[number].place[axis] * s);
	}

	return true;
}

std::string PlacedHierarchyFly_Stop(OpenLevelFile *open, size_t number, int32_t const place[3])
{
	std::vector<host::PlacedHierarchyInfo> all;

	if (!open || !place || host::PlacedHierarchies_Read(open, &all) != host::SpecialResult::Done || number >= all.size())
	{
		return "Fly selected object: not moved: the placed hierarchy is not in the level";
	}

	host::SpecialResult const written = host::PlacedHierarchy_WritePlace(open, number, place, all[number].turn);

	return written == host::SpecialResult::Done ? "Fly selected object: off, the object is where it flew to" : std::string("Fly selected object: not moved: ") + host::SpecialWords(written);
}
