// See entry_recalc.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "entry_recalc.h"

#include <string.h>

#include <exception>
#include <map>
#include <set>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk, ChunkShape
#include "chunk_waypoint.hpp" // AI_Module_Master_Chunk, AI_Module_Slave_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "entry_points.h"
#include "field_edit.h"       // PayloadStore

namespace host
{
	namespace
	{
		size_t const CountBytes = 4;
		size_t const ItemBytes = 20;   // object number, flags, the point (3 numbers): win95/chunk_object.cpp:795

		int32_t Get32(std::vector<unsigned char> const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, 4);
			return value;
		}

		void Put32(std::vector<unsigned char> *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, 4);
		}

		// The whole items of an ADJMDLEP payload: its count, when the bytes hold that many items exactly (a longer or shorter chunk is not read)
		bool ItemsIn(std::vector<unsigned char> const &bytes, size_t *count)
		{
			if (bytes.size() < CountBytes)
			{
				return false;
			}

			int32_t const stated = Get32(bytes, 0);

			if (stated < 0 || (size_t)stated > (bytes.size() - CountBytes) / ItemBytes || bytes.size() != CountBytes + (size_t)stated * ItemBytes)
			{
				return false;
			}

			*count = (size_t)stated;
			return true;
		}

		bool IsHolder(Chunk *chunk)
		{
			return chunk && !chunk->r_u_miscellaneous() && dynamic_cast<Chunk_With_Children *>(chunk) != nullptr;
		}

		// The module data of the object, or null
		Chunk_With_Children *ModuleData(Object_Chunk *object)
		{
			Chunk *const found = object->lookup_single_child("MODULEDT");

			return IsHolder(found) ? static_cast<Chunk_With_Children *>(found) : nullptr;
		}

		// The object's place and its shape as the entry computation reads them. false when the object has no shape.
		bool ModuleOf(Object_Chunk *object, EntryModule *out)
		{
			Shape_Chunk *const shape = object->get_assoc_shape();

			if (!shape)
			{
				return false;
			}

			ChunkShape const &data = shape->shape_data;

			out->place[0] = object->object_data.location.x;
			out->place[1] = object->object_data.location.y;
			out->place[2] = object->object_data.location.z;
			out->maxExtent[0] = data.max.x;
			out->maxExtent[1] = data.max.y;
			out->maxExtent[2] = data.max.z;
			out->minExtent[0] = data.min.x;
			out->minExtent[1] = data.min.y;
			out->minExtent[2] = data.min.z;

			if (data.num_verts < 0 || data.num_polys < 0 || (data.num_verts > 0 && !data.v_list) || (data.num_polys > 0 && !data.poly_list))
			{
				return false;
			}

			for (int i = 0; i < data.num_verts; i++)
			{
				out->points.push_back(data.v_list[i].x);
				out->points.push_back(data.v_list[i].y);
				out->points.push_back(data.v_list[i].z);
			}

			for (int i = 0; i < data.num_polys; i++)
			{
				out->polygonCorners.push_back(data.poly_list[i].num_verts);

				for (int corner = 0; corner < 4; corner++)
				{
					out->polygonCorners.push_back(data.poly_list[i].vert_ind[corner]);
				}

				out->polygonNormalY.push_back(data.p_normal_list ? data.p_normal_list[i].y : 0.0f);
			}

			return true;
		}
	}

	namespace
	{
		typedef std::vector<Object_Chunk *> ObjectList;

		// A module's numbers, made when first asked for and kept; null when it has no shape
		EntryModule const *NumbersOf(Object_Chunk *object, std::map<Object_Chunk *, EntryModule> *shapes, std::set<Object_Chunk *> *withoutShape)
		{
			auto const found = shapes->find(object);

			if (found != shapes->end())
			{
				return &found->second;
			}

			if (withoutShape->count(object))
			{
				return nullptr;
			}

			EntryModule made;

			if (!ModuleOf(object, &made))
			{
				withoutShape->insert(object);
				return nullptr;
			}

			return &shapes->insert(std::make_pair(object, made)).first->second;
		}

		bool Contains(ObjectList const &list, Object_Chunk const *object)
		{
			for (size_t i = 0; i < list.size(); i++)
			{
				if (list[i] == object)
				{
					return true;
				}
			}

			return false;
		}

		// FUN_004038a0: the modules a module stands for. A module with no MODULEDT: itself. A slave stands for its master (and everything the master
		// stands for); a master for itself and all its slaves (the master first, then the slaves in the order the master holds them).
		ObjectList GroupOf(Object_Chunk *object)
		{
			ObjectList out;
			Chunk_With_Children *const module = ModuleData(object);

			if (!module)
			{
				out.push_back(object);
				return out;
			}

			Object_Chunk *start = object;
			Chunk *const slave = module->lookup_single_child("AIMODSLA");

			if (slave && !slave->r_u_miscellaneous())
			{
				AI_Module_Slave_Chunk const *const asSlave = dynamic_cast<AI_Module_Slave_Chunk const *>(slave);

				if (asSlave && asSlave->MasterModule)
				{
					start = asSlave->MasterModule;
				}
			}

			out.push_back(start);

			Chunk_With_Children *const startModule = start == object ? module : ModuleData(start);
			Chunk *const master = startModule ? startModule->lookup_single_child("AIMODMAS") : nullptr;

			if (master && !master->r_u_miscellaneous())
			{
				AI_Module_Master_Chunk *const asMaster = dynamic_cast<AI_Module_Master_Chunk *>(master);

				if (asMaster)
				{
					for (LIF<Object_Chunk *> each(&asMaster->ModuleList); !each.done(); each.next())
					{
						if (each())
						{
							out.push_back(each());
						}
					}
				}
			}

			return out;
		}

		// The numbers of the modules a module's ADJMDLEP names
		bool AdjacentNumbers(Chunk *adjacency, PayloadStore const &store, std::vector<int32_t> *numbers)
		{
			std::vector<unsigned char> bytes;
			size_t items = 0;

			numbers->clear();

			if (!adjacency || adjacency->r_u_miscellaneous() || !store.PayloadOf(adjacency, &bytes) || !ItemsIn(bytes, &items))
			{
				return false;
			}

			for (size_t i = 0; i < items; i++)
			{
				numbers->push_back(Get32(bytes, CountBytes + i * ItemBytes));
			}

			return true;
		}

		// FUN_004039d0 (a, b), as Vasquez read it in runs 33 and 36: both lists are emptied; then for each module M of a's old members that has an
		// ADJMDLEP, for each entry E of it, the first member X of b's old members whose object number is E's: X goes into b and M into a, unless
		// they are in already. So both lists end up with the members that really are adjacent to the other list. (That the lists put in are b's and
		// a's, as here, is read for b (EBP, run 36) and follows for a.)
		void MergeLists(ObjectList *a, ObjectList *b, PayloadStore const &store)
		{
			ObjectList const second = *b;
			ObjectList const first = *a;

			a->clear();
			b->clear();

			for (size_t m = 0; m < first.size(); m++)
			{
				Chunk_With_Children *const module = ModuleData(first[m]);
				Chunk *const adjacency = module ? module->lookup_single_child("ADJMDLEP") : nullptr;
				std::vector<int32_t> numbers;

				if (!adjacency || !AdjacentNumbers(adjacency, store, &numbers))
				{
					continue;
				}

				for (size_t e = 0; e < numbers.size(); e++)
				{
					for (size_t x = 0; x < second.size(); x++)
					{
						if ((int32_t)second[x]->object_data.index_num == numbers[e])
						{
							if (!Contains(*b, second[x]))
							{
								b->push_back(second[x]);
							}

							if (!Contains(*a, first[m]))
							{
								a->push_back(first[m]);
							}

							break;
						}
					}
				}
			}
		}
	}

	bool PlanEntryRecalc(File_Chunk *level, PayloadStore const *store, std::vector<EntryChange> *changes, EntryRecalcCounts *counts, std::vector<EntryDelete> *deletes, int32_t onlyModule)
	{
		EntryRecalcCounts none;
		EntryRecalcCounts &total = counts ? *counts : none;

		memset(&total, 0, sizeof(total));

		if (changes)
		{
			changes->clear();
		}

		if (deletes)
		{
			deletes->clear();
		}

		if (!level || !store || !changes)
		{
			return false;
		}

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			std::map<int32_t, Object_Chunk *> byNumber;
			std::map<Object_Chunk *, EntryModule> shapes;      // each module's numbers, made when first needed
			std::set<Object_Chunk *> withoutShape;

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				byNumber[(int32_t)each()->object_data.index_num] = each();
			}

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				Chunk_With_Children *const module = ModuleData(object);
				Chunk *const adjacency = module ? module->lookup_single_child("ADJMDLEP") : nullptr;
				std::vector<unsigned char> bytes;
				size_t items = 0;

				if (!adjacency || adjacency->r_u_miscellaneous() || !store->PayloadOf(adjacency, &bytes) || !ItemsIn(bytes, &items))
				{
					continue;
				}

				int32_t const mainIndex = (int32_t)object->object_data.index_num;

				if (onlyModule >= 0 && mainIndex != onlyModule)
				{
					continue;
				}

				for (size_t entry = 0; entry < items; entry++)
				{
					size_t const at = CountBytes + entry * ItemBytes;
					int32_t const adjacentIndex = Get32(bytes, at);
					uint32_t const flags = (uint32_t)Get32(bytes, at + 4);
					int32_t const point[3] = { Get32(bytes, at + 8), Get32(bytes, at + 12), Get32(bytes, at + 16) };

					total.entries++;

					auto const found = byNumber.find(adjacentIndex);

					if (found == byNumber.end())
					{
						if (deletes)
						{
							// asking mode: inteng takes the entry out (FUN_00403d50 0x403fb5-0x403fd4)
							EntryDelete drop = { adjacency, mainIndex, adjacentIndex, entry };

							deletes->push_back(drop);
							total.noObject++;
						}
						else
						{
							total.skippedNoObject++;
						}

						continue;
					}

					// The two lists, made as inteng makes them and merged through their ADJMDLEP
					ObjectList mainObjects = GroupOf(object);
					ObjectList adjacentObjects = GroupOf(found->second);

					if (mainObjects.size() > 1 || adjacentObjects.size() > 1)
					{
						total.inGroups++;
					}

					// Both modules of the pair are in one group: each stands for itself alone (inteng, FUN_00402ae0 at 0x402c46), and the pair is never asked about: the collapse
					// clears local_67c (0x402e6d) and the don't-touch question needs it (0x40327d-0x403293), so such a pair goes on as after a No (0x403456). Until 2026-11-17 ours
					// asked it too (AD-8 of reassess/adjacent-closeout-dietrich.md; Dietrich).
					bool const collapsed = Contains(mainObjects, found->second);

					if (collapsed)
					{
						mainObjects.assign(1, object);
						adjacentObjects.assign(1, found->second);
					}

					MergeLists(&mainObjects, &adjacentObjects, *store);

					if (mainObjects.empty() || adjacentObjects.empty())
					{
						// inteng's assertion (Adjmmode.cpp:0x1b0, 0x1b1): nothing to work out
						total.skippedNoOverlap++;
						continue;
					}

					// Every member's numbers; a member without a shape (or the pair's own) leaves the entry as it is
					std::vector<EntryModule const *> mainList;
					std::vector<EntryModule const *> adjacentList;
					EntryModule const *const ownMain = NumbersOf(object, &shapes, &withoutShape);
					EntryModule const *const ownAdjacent = NumbersOf(found->second, &shapes, &withoutShape);
					bool shapesOk = ownMain && ownAdjacent;

					for (size_t i = 0; i < mainObjects.size() && shapesOk; i++)
					{
						EntryModule const *const numbers = NumbersOf(mainObjects[i], &shapes, &withoutShape);

						shapesOk = numbers != nullptr;
						mainList.push_back(numbers);
					}

					for (size_t i = 0; i < adjacentObjects.size() && shapesOk; i++)
					{
						EntryModule const *const numbers = NumbersOf(adjacentObjects[i], &shapes, &withoutShape);

						shapesOk = numbers != nullptr;
						adjacentList.push_back(numbers);
					}

					if (!shapesOk)
					{
						total.skippedNoObject++;
						continue;
					}

					EntryResult const result = ComputeEntryPointOfLists(*ownMain, *ownAdjacent, mainList, adjacentList, flags, point, 0.0f, false);
					bool const asked = result.warnedNoTouch && !collapsed;   // the question's gate (local_67c)

					total.recalculated++;
					total.notTouching += asked ? 1 : 0;
					total.failedToPlace += result.failedToPlace ? 1 : 0;

					// The game's loader refuses a place farther than 2^24 from the middle (CheckPlace): never write one
					bool const outOfRange = result.point[0] > 16777216 || result.point[0] < -16777216 || result.point[1] > 16777216 || result.point[1] < -16777216 ||
						result.point[2] > 16777216 || result.point[2] < -16777216;

					if (outOfRange)
					{
						total.outOfRange++;
					}
					else if (result.flags == flags && result.point[0] == point[0] && result.point[1] == point[1] && result.point[2] == point[2])
					{
						total.unchanged++;
					}

					bool const nothingToWrite = outOfRange || (result.flags == flags && result.point[0] == point[0] && result.point[1] == point[1] && result.point[2] == point[2]);

					// asking mode keeps a pair that doesn't touch for its question (not a collapsed one, AD-8), and one that failed to place for its box (FUN_00402ae0 0x4036c9, AD-7), even with
					// nothing to write
					if (nothingToWrite && !(deletes && (asked || result.failedToPlace)))
					{
						continue;
					}

					EntryChange change;

					change.keep = nothingToWrite;
					// the names for the question, read within their fields (a name in a hostile file may have no zero)
					change.mainName.assign(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name)));
					change.adjacentName.assign(found->second->object_data.o_name, strnlen(found->second->object_data.o_name, sizeof(found->second->object_data.o_name)));

					change.adjacency = adjacency;
					change.mainIndex = mainIndex;
					change.adjacentIndex = adjacentIndex;
					change.entry = entry;
					change.flagsBefore = flags;
					change.flagsAfter = result.flags;

					for (int axis = 0; axis < 3; axis++)
					{
						change.pointBefore[axis] = point[axis];
						change.pointAfter[axis] = result.point[axis];
					}

					change.notTouching = asked;
					change.failedToPlace = result.failedToPlace;
					changes->push_back(change);
				}
			}
		}
		catch (std::exception const &)
		{
			// Out of memory: no plan
			changes->clear();
			memset(&total, 0, sizeof(total));
			return false;
		}

		return true;
	}

	size_t ApplyEntryRecalc(PayloadStore *store, std::vector<EntryChange> const &changes, size_t *refused)
	{
		size_t written = 0;
		size_t notWritten = 0;

		try
		{
			size_t at = 0;

			while (at < changes.size())
			{
				// All the changes of one chunk are one write
				Chunk *const chunk = changes[at].adjacency;
				size_t end = at;

				while (end < changes.size() && changes[end].adjacency == chunk)
				{
					end++;
				}

				std::vector<unsigned char> bytes;
				size_t items = 0;
				size_t put = 0;

				if (store && chunk && store->PayloadOf(chunk, &bytes) && ItemsIn(bytes, &items))
				{
					for (size_t i = at; i < end; i++)
					{
						EntryChange const &change = changes[i];
						size_t const place = CountBytes + change.entry * ItemBytes;

						if (change.keep)
						{
							continue;   // kept only for its question: nothing to write (and not a refusal)
						}

						if (change.entry < items && Get32(bytes, place) == change.adjacentIndex)
						{
							Put32(&bytes, place + 4, (int32_t)change.flagsAfter);

							for (int axis = 0; axis < 3; axis++)
							{
								Put32(&bytes, place + 8 + 4 * (size_t)axis, change.pointAfter[axis]);
							}

							put++;
						}
					}

					if (put > 0 && !store->SetPayload(chunk, bytes))
					{
						put = 0;
					}
				}

				size_t kept = 0;

				for (size_t i = at; i < end; i++)
				{
					kept += changes[i].keep ? 1 : 0;
				}

				written += put;
				notWritten += (end - at) - kept - put;
				at = end;
			}
		}
		catch (std::exception const &)
		{
			notWritten = changes.size() - written;
		}

		if (refused)
		{
			*refused = notWritten;
		}

		return written;
	}

	bool AdjacentModulesOf(File_Chunk *level, PayloadStore const *store, int32_t moduleNumber, std::vector<int32_t> *out)
	{
		if (out)
		{
			out->clear();
		}

		if (!level || !store || !out)
		{
			return false;
		}

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			Object_Chunk *module = nullptr;
			std::set<int32_t> numbers;

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				int32_t const number = (int32_t)each()->object_data.index_num;

				numbers.insert(number);

				if (number == moduleNumber && !module)
				{
					module = each();
				}
			}

			Chunk_With_Children *const data = module ? ModuleData(module) : nullptr;
			Chunk *const adjacency = data ? data->lookup_single_child("ADJMDLEP") : nullptr;
			std::vector<int32_t> entries;

			if (!adjacency || !AdjacentNumbers(adjacency, *store, &entries))
			{
				return false;
			}

			for (size_t i = 0; i < entries.size(); i++)
			{
				if (numbers.count(entries[i]) != 0)
				{
					out->push_back(entries[i]);
				}
			}

			return true;
		}
		catch (std::exception const &)
		{
			out->clear();
			return false;
		}
	}
}
