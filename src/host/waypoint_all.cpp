// See waypoint_all.h. Written 2026-11-19 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_all.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "level_file.h"
#include "object_edit.h"

namespace host
{
	namespace
	{
		// The state of the level the list was read from: a different open, a different level (a new Open), a level swapped in, or a same-size edit reads it again. A new Open
		// starts Generation() and Changes() at 0 again and its level may be made where the last one was, so the objects' fingerprint is part of it too
		struct ReadFrom
		{
			OpenLevelFile const *open = nullptr;
			File_Chunk const *level = nullptr;
			unsigned generation = 0;
			unsigned changes = 0;
			uint64_t objects = 0;
			bool valid = false;
		};

		// The level's objects, in the file's order: their count, each one's index and name, folded into one number (FNV-1a)
		uint64_t ObjectsFingerprint(OpenLevelFile *open)
		{
			uint64_t hash = 1469598103934665603ull;
			uint64_t count = 0;
			List<Object_Chunk *> all;

			auto const fold = [&hash](uint64_t value)
			{
				for (int b = 0; b < 8; b++)
				{
					hash = (hash ^ ((value >> (8 * b)) & 0xffu)) * 1099511628211ull;
				}
			};

			open->Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object)
				{
					continue;
				}

				count++;
				fold((uint64_t)(uint32_t)object->object_data.index_num);

				for (size_t k = 0; k < sizeof(object->object_data.o_name) && object->object_data.o_name[k] != 0; k++)
				{
					fold((uint64_t)(unsigned char)object->object_data.o_name[k]);
				}
			}

			fold(count);
			return hash;
		}

		ReadFrom readFrom;
		std::vector<AlienNet> nets;
		size_t unreadableCount = 0;
		size_t reads = 0;

		// Every object's name by its index (the first object of an index), each name read only up to its array's end
		std::map<int32_t, std::string> NamesByIndex(OpenLevelFile *open)
		{
			std::map<int32_t, std::string> names;
			List<Object_Chunk *> all;

			open->Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object)
				{
					continue;
				}

				int32_t const index = (int32_t)object->object_data.index_num;
				size_t length = 0;

				while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
				{
					length++;
				}

				if (!names.count(index))
				{
					names[index] = std::string(object->object_data.o_name, length);
				}
			}

			return names;
		}

		void Read(OpenLevelFile *open)
		{
			nets.clear();
			unreadableCount = 0;
			reads++;

			std::vector<ModuleSnapshot> snapshots;

			if (ReadModuleSnapshots(open, &snapshots) != ObjectEditResult::Done)
			{
				return;   // nothing shown, as with no level (the same state is not read again)
			}

			std::map<int32_t, std::string> const names = NamesByIndex(open);

			for (ModuleSnapshot const &one : snapshots)
			{
				// a master or a lone module (projload.cpp 2497: a slave's WAYPOINT is not read) with a WAYPOINT
				if (!one.module || one.masterIndex >= 0 || !one.hasWaypoint)
				{
					continue;
				}

				WaypointSet set;
				std::string why;

				if (!Waypoints_Parse(one.waypoint.data(), one.waypoint.size(), &set, &why))
				{
					unreadableCount++;
					continue;
				}

				if (set.alien.empty())
				{
					continue;
				}

				AlienNet net;
				std::map<int32_t, std::string>::const_iterator name = names.find(one.index);   // never "iterator const": Snyk skips such a file (Vasquez 11-19)

				net.master = one.index;
				net.name = name != names.end() ? name->second : std::string();
				net.alien.swap(set.alien);
				nets.push_back(std::move(net));
			}
		}
	}

	std::vector<AlienNet> const &WaypointAll_AlienNets(OpenLevelFile *open, size_t *unreadable)
	{
		if (!open || !open->Level())
		{
			readFrom = ReadFrom();
			nets.clear();
			unreadableCount = 0;
		}
		else
		{
			uint64_t const objects = ObjectsFingerprint(open);

			if (!readFrom.valid || readFrom.open != open || readFrom.level != open->Level() || readFrom.generation != open->Generation() || readFrom.changes != open->Changes() ||
				readFrom.objects != objects)
			{
				Read(open);
				readFrom.open = open;
				readFrom.level = open->Level();
				readFrom.generation = open->Generation();
				readFrom.changes = open->Changes();
				readFrom.objects = objects;
				readFrom.valid = true;
			}
		}

		if (unreadable)
		{
			*unreadable = unreadableCount;
		}

		return nets;
	}

	size_t WaypointAll_Reads()
	{
		return reads;
	}
}
