// See entry_markers.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "entry_markers.h"

#include <algorithm>
#include <map>
#include <set>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "object_edit.h"

namespace host
{
	namespace
	{
		struct Box
		{
			int64_t lo[3];
			int64_t hi[3];
			bool known;
		};

		// The AI module of a module: its master and its slaves (a module with none of the AI chunks is its own AI module)
		std::vector<int32_t> GroupOf(std::map<int32_t, ModuleSnapshot> const &modules, int32_t module)
		{
			std::vector<int32_t> out;
			std::map<int32_t, ModuleSnapshot>::const_iterator const found = modules.find(module);

			if (found == modules.end())
			{
				return out;
			}

			int32_t const master = found->second.masterIndex >= 0 ? found->second.masterIndex : module;

			out.push_back(master);

			for (std::map<int32_t, ModuleSnapshot>::const_iterator it = modules.begin(); it != modules.end(); ++it)
			{
				if (it->first != master && it->second.masterIndex == master)
				{
					out.push_back(it->first);
				}
			}

			return out;
		}

		bool In(std::vector<int32_t> const &list, int32_t value)
		{
			return std::find(list.begin(), list.end(), value) != list.end();
		}

		bool Snapshots(OpenLevelFile *open, std::map<int32_t, ModuleSnapshot> *out)
		{
			std::vector<ModuleSnapshot> all;

			if (ReadModuleSnapshots(open, &all) != ObjectEditResult::Done)
			{
				return false;
			}

			for (ModuleSnapshot const &snapshot : all)
			{
				if (snapshot.module && !out->count(snapshot.index))
				{
					(*out)[snapshot.index] = snapshot;
				}
			}

			return true;
		}

		// A module's box: its place and its shape's extents (inteng's +0x10 and +0x104..+0x118 of the module record)
		Box BoxOf(OpenLevelFile *open, int32_t index)
		{
			Box box = { { 0, 0, 0 }, { 0, 0, 0 }, false };
			::List<Object_Chunk *> all;

			open->Level()->list_objects(&all);

			for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object || (int32_t)object->object_data.index_num != index || !object->get_assoc_shape())
				{
					continue;
				}

				ChunkShape const &data = object->get_assoc_shape()->shape_data;
				int64_t const at[3] = { object->object_data.location.x, object->object_data.location.y, object->object_data.location.z };

				box.lo[0] = at[0] + data.min.x; box.hi[0] = at[0] + data.max.x;
				box.lo[1] = at[1] + data.min.y; box.hi[1] = at[1] + data.max.y;
				box.lo[2] = at[2] + data.min.z; box.hi[2] = at[2] + data.max.z;
				box.known = true;
				return box;
			}

			return box;
		}

		// The biggest way the place is outside the box, over the three axes (0 inside): FUN_0058a6b0 0x58a920-0x58a9aa
		int64_t Overshoot(Box const &box, int64_t const place[3])
		{
			int64_t worst = 0;

			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const over = place[axis] - box.hi[axis];
				int64_t const under = box.lo[axis] - place[axis];

				worst = over > worst ? over : worst;
				worst = under > worst ? under : worst;
			}

			return worst;
		}

		// One side of FUN_0058a6b0: the entries of the modules of `from` that lead into `into` get the place held inside the nearest box of `into`. Changes entries in *edits, counts them in *count.
		void MoveEntries(OpenLevelFile *open, std::map<int32_t, ModuleSnapshot> const &modules, std::vector<int32_t> const &from, std::vector<int32_t> const &into, int64_t const place[3],
			std::map<int32_t, ModuleAdjacencyFor> *edits, size_t *count)
		{
			int32_t best = -1;
			int64_t bestOvershoot = 2000000000;
			Box bestBox = { { 0, 0, 0 }, { 0, 0, 0 }, false };

			for (int32_t const module : from)
			{
				std::map<int32_t, ModuleSnapshot>::const_iterator const found = modules.find(module);

				if (found == modules.end() || !found->second.hasAdjacency)
				{
					continue;
				}

				for (ModuleAdjacencyEntry const &entry : found->second.adjacency)
				{
					if (!In(into, entry.objectIndex))
					{
						continue;
					}

					Box const box = BoxOf(open, entry.objectIndex);

					if (!box.known)
					{
						continue;
					}

					int64_t const over = Overshoot(box, place);

					if (over < bestOvershoot)
					{
						best = entry.objectIndex;
						bestOvershoot = over;
						bestBox = box;
					}
				}
			}

			if (best < 0)
			{
				return;
			}

			// the box shrunk by 100 each side, a box under 200 thick is its middle (0x58aa95 and the two like it), the place held inside
			int64_t held[3];

			for (int axis = 0; axis < 3; axis++)
			{
				int64_t lo = bestBox.lo[axis];
				int64_t hi = bestBox.hi[axis];

				if (hi - lo < 200 + 1)
				{
					lo = hi = (lo + hi) / 2;
				}
				else
				{
					lo += 100;
					hi -= 100;
				}

				held[axis] = place[axis] > hi ? hi : (place[axis] < lo ? lo : place[axis]);
			}

			for (int32_t const module : from)
			{
				std::map<int32_t, ModuleSnapshot>::const_iterator const found = modules.find(module);

				if (found == modules.end() || !found->second.hasAdjacency)
				{
					continue;
				}

				bool any = false;

				for (ModuleAdjacencyEntry const &entry : found->second.adjacency)
				{
					any = any || In(into, entry.objectIndex);
				}

				if (!any)
				{
					continue;
				}

				ModuleAdjacencyFor &edit = (*edits)[module];

				if (edit.entries.empty())
				{
					edit.objectIndex = module;
					edit.remove = false;
					edit.entries = found->second.adjacency;
				}

				for (ModuleAdjacencyEntry &entry : edit.entries)
				{
					if (In(into, entry.objectIndex))
					{
						entry.point[0] = (int32_t)held[0];
						entry.point[1] = (int32_t)held[1];
						entry.point[2] = (int32_t)held[2];
						entry.flags |= 1;   // the hand-set bit
						(*count)++;
					}
				}
			}
		}
	}

	bool EntryMarkers_Of(OpenLevelFile *open, int32_t module, std::vector<EntryMarker> *out)
	{
		out->clear();

		std::map<int32_t, ModuleSnapshot> modules;

		if (!open || !open->Level() || !Snapshots(open, &modules))
		{
			return false;
		}

		std::vector<int32_t> const group = GroupOf(modules, module);

		for (int32_t const member : group)
		{
			std::map<int32_t, ModuleSnapshot>::const_iterator const found = modules.find(member);

			if (found == modules.end() || !found->second.hasAdjacency)
			{
				continue;
			}

			for (ModuleAdjacencyEntry const &entry : found->second.adjacency)
			{
				// leads outside the AI module, and neither bit 2 nor bit 4 of its flags (FUN_00588260 0x5884f6 / 0x58852d)
				if (In(group, entry.objectIndex) || (entry.flags & 2) || (entry.flags & 4) || !modules.count(entry.objectIndex))
				{
					continue;
				}

				EntryMarker marker;

				marker.main = member;
				marker.adjacent = entry.objectIndex;
				marker.point[0] = entry.point[0];
				marker.point[1] = entry.point[1];
				marker.point[2] = entry.point[2];
				marker.flags = entry.flags;
				out->push_back(marker);
			}
		}

		return true;
	}

	EntryMoveResult EntryMarkers_Move(OpenLevelFile *open, EntryMarker const &marker, int32_t const place[3], size_t *written)
	{
		size_t none = 0;

		if (!written)
		{
			written = &none;
		}

		*written = 0;

		if (!open || !open->Level())
		{
			return EntryMoveResult::NoLevel;
		}

		if (place[0] == marker.point[0] && place[1] == marker.point[1] && place[2] == marker.point[2])
		{
			return EntryMoveResult::NothingMoved;
		}

		std::map<int32_t, ModuleSnapshot> modules;

		if (!Snapshots(open, &modules))
		{
			return EntryMoveResult::WriteFailed;
		}

		std::vector<int32_t> const mainGroup = GroupOf(modules, marker.main);
		std::vector<int32_t> const otherGroup = GroupOf(modules, marker.adjacent);
		int64_t const at[3] = { place[0], place[1], place[2] };
		std::map<int32_t, ModuleAdjacencyFor> edits;
		size_t count = 0;

		MoveEntries(open, modules, mainGroup, otherGroup, at, &edits, &count);
		MoveEntries(open, modules, otherGroup, mainGroup, at, &edits, &count);

		if (edits.empty())
		{
			return EntryMoveResult::NothingMoved;
		}

		std::vector<ModuleAdjacencyFor> list;

		for (std::map<int32_t, ModuleAdjacencyFor>::const_iterator it = edits.begin(); it != edits.end(); ++it)
		{
			list.push_back(it->second);
		}

		std::vector<bool> wrote;

		if (SetModuleAdjacencyMany(open, list, &wrote) != ObjectEditResult::Done)
		{
			return EntryMoveResult::WriteFailed;
		}

		*written = count;
		return EntryMoveResult::Done;
	}
}
