// movable_placed_hierarchy.cpp: movable.h's adapter for a PLACED HIERARCHY (a PLACHIER > PLACHIDT of SPECLOBJ): its place and turn read with PlacedHierarchies_Read and written
// with PlacedHierarchy_WritePlace (special_objects_edit.h), the writer a flown hierarchy's stop already uses (placement_hierarchy_fly.cpp, V-6). That writer takes the turn as
// well as the place, so no new writer is needed. Its name, index, ID, extra numbers, sequences and sounds stay as the file has them, and the write goes through the game's loader,
// which checks the changed level; nothing is changed if it refuses. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHICH ONE: by the ref's ID (the PLACHIDT's 8 bytes) when it has one, else by its number in PlacedHierarchies_Read's list (the file's order).
// THE TURN: the PLACHIDT holds it x y z w as OBJHEAD1 does, and the game reads both alike (object_setup.cpp:1233-1236, x y z negated, as 899-902 for an object), so it is
// given and taken as it is.

#include "movable.h"

#include <string.h>

#include <vector>

#include "special_objects_edit.h"

namespace host
{
	namespace
	{
		bool NamesSomething(MovableRef const &ref)
		{
			for (size_t i = 0; i < kSelectionIdBytes; i++)
			{
				if (ref.id[i] != 0)
				{
					return true;
				}
			}

			return false;
		}

		// The placed hierarchies of the level and which of them the ref names. NothingOpen with no level; NoSuchThing when the level has none, or none is that one.
		MovableResult Find(OpenLevelFile *file, MovableRef const &ref, std::vector<PlacedHierarchyInfo> *all, size_t *at)
		{
			if (!file)
			{
				return MovableResult::NothingOpen;
			}

			SpecialResult const read = PlacedHierarchies_Read(file, all);

			if (read == SpecialResult::BadCall)
			{
				return MovableResult::NothingOpen;
			}

			if (read != SpecialResult::Done)
			{
				return MovableResult::NoSuchThing;
			}

			if (NamesSomething(ref))
			{
				for (size_t i = 0; i < all->size(); i++)
				{
					if (memcmp((*all)[i].id, ref.id, sizeof((*all)[i].id)) == 0)
					{
						*at = i;
						return MovableResult::Done;
					}
				}

				return MovableResult::NoSuchThing;
			}

			if (ref.number >= 0 && (size_t)ref.number < all->size())
			{
				*at = (size_t)ref.number;
				return MovableResult::Done;
			}

			return MovableResult::NoSuchThing;
		}
	}

	MovableResult MovablePlacedHierarchy_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		std::vector<PlacedHierarchyInfo> all;
		size_t at = 0;
		MovableResult const found = Find(file, ref, &all, &at);

		if (found != MovableResult::Done)
		{
			return found;
		}

		MovablePose read;

		memcpy(read.place, all[at].place, sizeof(read.place));
		memcpy(read.turn, all[at].turn, sizeof(read.turn));
		*pose = read;
		return MovableResult::Done;
	}

	MovableResult MovablePlacedHierarchy_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		std::vector<PlacedHierarchyInfo> all;
		size_t at = 0;
		MovableResult const found = Find(file, ref, &all, &at);

		if (found != MovableResult::Done)
		{
			return found;
		}

		switch (PlacedHierarchy_WritePlace(file, at, pose.place, pose.turn))
		{
			case SpecialResult::Done: return MovableResult::Done;
			case SpecialResult::BadCall: return MovableResult::NothingOpen;
			case SpecialResult::NotFound: case SpecialResult::NoEnvironment: return MovableResult::NoSuchThing;
			default: return MovableResult::WriterRefused;
		}
	}
}
