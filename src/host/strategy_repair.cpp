// strategy_repair.cpp: see strategy_repair.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_repair.h"

#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk (its SHPMORPH child: FUN_005219f0's last case)
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "chunk_sound.hpp"    // Sound_Object_Chunk::CalculateID
#include "avp_chunk.hpp"      // AVP_Particle_Generator_Data_Chunk

#include "field_edit.h"
#include "level_file.h"
#include "level_store.h"
#include "object_edit.h"   // SetStrategyBytes
#include "placed_hierarchy_strategy.h"   // PlacedHierarchyStrategy_Put
#include "strategy_edit.h"
#include "strategy_lookup.h"
#include "virtual_object.h"

namespace host
{
	namespace
	{
		struct Record
		{
			uint8_t id[8];
			bool virt;           // a virtual object (record type 1); else a placed object (type 0)
			bool hasStrategy;
			int32_t kind;
			std::vector<uint8_t> payload;
			bool other = false;  // a record of another class: an enemy or a generator (types 3, 2), a sound (5), a placed hierarchy (7), a particle generator (8)
			bool placedHierarchy = false;   // type 7: its strategy (kind 76) is the PLACHIER's AVPSTRAT
			bool morph = false;  // a placed object (type 0) whose OWN shape has a SHPMORPH chunk (FUN_005219f0 0x521a28-0x521a43: ECX = the record's +0x28, its object; OBJHEAD1 +0x4c, the shape)
		};

		std::string Key(uint8_t const *id)
		{
			return std::string((char const *)id, 8);
		}

		bool IsNull(uint8_t const *id)
		{
			for (int i = 0; i < 8; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		int32_t IntAt(std::vector<uint8_t> const &bytes, size_t at)
		{
			int32_t value = 0;

			if (at + 4 <= bytes.size())
			{
				memcpy(&value, bytes.data() + at, 4);
			}

			return value;
		}

		// The records: the level's placed objects, then its virtual objects (file order)
		void Collect(OpenLevelFile *open, std::vector<Record> *out)
		{
			out->clear();

			if (!open || !open->Level())
			{
				return;
			}

			File_Chunk *const level = open->Level();
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object)
				{
					continue;
				}

				Record record;
				ObjectStrategy now;

				memset(&record, 0, sizeof(record.id));
				memcpy(record.id, &object->object_data.ID, 8);
				record.virt = false;
				record.hasStrategy = false;
				record.kind = 0;

				if (LookUpStrategyBytes(level, LevelStore(open), record.id, &now) == StrategyFound::Found && now.payload.size() >= 12)
				{
					record.hasStrategy = true;
					record.kind = IntAt(now.payload, 8);
					record.payload = now.payload;
				}

				// FUN_005219f0's last case: with no strategy, the object is a target that exists when its own shape morphs (FUN_004d0fd0 on the record's object: its OBJHEAD1's shape, +0x4c;
				// then FUN_00431a00 "SHPMORPH"). get_assoc_shape is that header's associated_shape; a shape the library kept as plain bytes is none
				Shape_Chunk *const shape = object->get_assoc_shape();

				record.morph = shape && !shape->r_u_miscellaneous() && shape->lookup_single_child("SHPMORPH") != nullptr;
				out->push_back(record);
			}

			std::vector<VirtualObjectInfo> virtuals;

			if (VirtualObject_List(open, &virtuals))
			{
				for (VirtualObjectInfo const &one : virtuals)
				{
					Record record;
					ObjectStrategy now;

					memcpy(record.id, one.id, 8);
					record.virt = true;
					record.hasStrategy = false;
					record.kind = 0;

					if (LookUpStrategyBytes(level, LevelStore(open), record.id, &now) == StrategyFound::Found && now.payload.size() >= 12)
					{
						record.hasStrategy = true;
						record.kind = IntAt(now.payload, 8);
						record.payload = now.payload;
					}

					out->push_back(record);
				}
			}

			// The records inteng makes for the level's OTHER objects (FUN_00521cb0, after the virtual objects; each constructor's record type read 2026-11-17): the enemies and generators
			// (AVPGENER: FUN_0053a7a0 type 3, FUN_00539ff0 type 2), the sounds (SOUNDOB2, AvP's game number: FUN_0053ac30 type 5), the placed hierarchies (PLACHIER: FUN_0053b5a0 type 7) and the
			// particle generators (PARGENER: FUN_0053afa0 type 8). FUN_005219f0 answers YES for a target naming any record whose type is not 0, so a target that names one of these is never
			// a dead one; and the repair pass runs the strategy of each that holds one (an enemy's 69, a placed hierarchy's 76). Until 2026-11-17 these were left out, so a switch whose
			// target was a generator, a sound, a placed hierarchy or a particle generator lost it to Find Errors, and an enemy's death target was never looked at.
			// NAMED, left out: the cut-scene heads (CUTSHEAD, type 9) and traps (MUMMTRAP, type 10) of Rebellion's other games, which the game's library does not read.
			auto const addOther = [&](uint8_t const *id, bool placed)
			{
				Record record;
				ObjectStrategy now;

				memcpy(record.id, id, 8);
				record.virt = false;
				record.other = true;
				record.placedHierarchy = placed;
				record.hasStrategy = false;
				record.kind = 0;

				if (LookUpStrategyBytes(level, LevelStore(open), record.id, &now) == StrategyFound::Found && now.payload.size() >= 12)
				{
					record.hasStrategy = true;
					record.kind = IntAt(now.payload, 8);
					record.payload = now.payload;
				}

				out->push_back(record);
			};

			std::vector<GeneratorRef> generators;
			ListGenerators(level, &generators);

			for (GeneratorRef const &one : generators)
			{
				addOther(one.id, false);
			}

			std::vector<PlacedHierarchyRef> placed;
			ListPlacedHierarchies(level, &placed);

			for (PlacedHierarchyRef const &one : placed)
			{
				addOther(one.id, true);
			}

			// the sounds' and the particle generators' IDs, as the game's library makes and reads them (Sound_Object_Chunk::CalculateID: md5 of its name, the eighth byte 0, as inteng's
			// FUN_0051b770; a particle generator's PARGENDA holds its own); a chunk the library kept as plain bytes is none
			try
			{
				Chunk *const env = level->lookup_single_child("REBENVDT");
				Chunk *const spec = env && !env->r_u_miscellaneous() ? ((Chunk_With_Children *)env)->lookup_single_child("SPECLOBJ") : nullptr;

				if (spec && !spec->r_u_miscellaneous())
				{
					List<Chunk *> sounds;
					((Chunk_With_Children *)spec)->lookup_child("SOUNDOB2", sounds);

					for (LIF<Chunk *> each(&sounds); !each.done(); each.next())
					{
						Sound_Object_Chunk *const sound = (Sound_Object_Chunk *)each();

						if (sound && !sound->r_u_miscellaneous() && sound->snd_name)
						{
							ObjectID const id = sound->CalculateID();
							addOther((uint8_t const *)&id, false);
						}
					}

					List<Chunk *> particles;
					((Chunk_With_Children *)spec)->lookup_child("PARGENER", particles);

					for (LIF<Chunk *> each(&particles); !each.done(); each.next())
					{
						Chunk *const generator = each();
						Chunk *const data = generator && !generator->r_u_miscellaneous() ? ((Chunk_With_Children *)generator)->lookup_single_child("PARGENDA") : nullptr;

						if (data && !data->r_u_miscellaneous())
						{
							addOther((uint8_t const *)&((AVP_Particle_Generator_Data_Chunk *)data)->id, false);
						}
					}
				}
			}
			catch (std::exception const &)
			{
			}
		}

		// FUN_005219f0: a target ID that a record answers to (the first with the ID, FUN_00529570): any record whose type is not 0 (virtual, enemy, generator, sound, placed hierarchy, particle
		// generator), a placed object with a strategy, or one with none whose own shape has SHPMORPH (OUR BUG, FIXED 11-17, Vasquez: the last case was read as the SELECTED object's; the
		// listing passes the record's object in ECX)
		bool Exists(std::map<std::string, size_t> const &index, std::vector<Record> const &records, uint8_t const *id)
		{
			if (IsNull(id))
			{
				return false;
			}

			auto const found = index.find(Key(id));

			return found != index.end() && (records[found->second].virt || records[found->second].other || records[found->second].hasStrategy || records[found->second].morph);
		}

		bool IsSwitch(int32_t kind)
		{
			return kind == 52 || kind == 61 || kind == 66 || kind == 68;   // FUN_005218b0: 0x34, 0x3d, 0x42, 0x44
		}

		// One target reset: the ID field to the null ID, the request field to 0, each by its field's name; true when the bytes said something else before
		bool ResetTarget(OpenLevelFile *open, Record const &record, size_t idAt, size_t requestAt, char const *idField, char const *requestField)
		{
			bool const idSet = idAt + 8 <= record.payload.size() && !IsNull(record.payload.data() + idAt);
			bool const requestSet = IntAt(record.payload, requestAt) != 0;

			if (!idSet && !requestSet)
			{
				return false;
			}

			LevelStore store(open);
			char const *why = nullptr;
			StrategyFound lookup;
			bool changed = false;

			if (idSet)
			{
				FieldEdit edit;

				edit.field = idField;
				edit.what = EditWhat::Object;
				edit.first = 0;
				edit.second = 0;
				changed = ApplyStrategyEdit(open->Level(), record.id, record.kind, edit, &store, &why, &lookup) == EditAnswer::Applied || changed;
			}

			if (requestSet)
			{
				FieldEdit edit;

				edit.field = requestField;
				edit.what = EditWhat::Number;
				edit.number = 0;
				changed = ApplyStrategyEdit(open->Level(), record.id, record.kind, edit, &store, &why, &lookup) == EditAnswer::Applied || changed;
			}

			return changed;
		}
	}

	namespace
	{
		// A strategy's whole bytes written: a placed object's or a module's through SetStrategyBytes; a virtual object's (every stock Mission Objective) through VirtualObject_SetStrategyBytes,
		// which SetStrategyBytes cannot reach (it answers NoSuchObject for an ID that is not a placed object or a module); a placed hierarchy's through PlacedHierarchyStrategy_Put
		bool WriteBytes(OpenLevelFile *open, uint8_t const *id, std::vector<uint8_t> const &bytes)
		{
			ObjectEditResult const placed = SetStrategyBytes(open, id, bytes.data(), bytes.size());

			if (placed == ObjectEditResult::Done)
			{
				return true;
			}

			if (placed != ObjectEditResult::NoSuchObject)
			{
				return false;
			}

			VirtualObjectResult const virt = VirtualObject_SetStrategyBytes(open, id, bytes.data(), bytes.size());

			// a placed hierarchy's (its 76): the PLACHIER's AVPSTRAT, which neither of the two reaches (added 2026-11-17)
			return virt == VirtualObjectResult::Done ||
			       (virt == VirtualObjectResult::NotFound && PlacedHierarchyStrategy_Put(open, id, bytes.data(), bytes.size()) == PlacedHierarchyStrategyResult::Done);
		}

		// FUN_005335f0 (row S-K70-R of reassess/strategy-data-kinds.md, SKD-70-7 of strategy-kinds-dietrich.md): a Mission Objective's alterations, the count at +40 and 12 bytes each from +44 (the target's ID, its word). Each
		// entry whose target FUN_005219f0 rejects is removed: the count goes down and, when the entry was not the last, the LAST entry is moved into its place and looked at in turn (so the
		// order changes); a kept entry moves the walk on. The list is shrunk (or freed at 0). The request-target list after it is not checked (inteng does not). True when it changed.
		bool RepairMissionList(OpenLevelFile *open, std::map<std::string, size_t> const &index, std::vector<Record> const &records, Record const &record)
		{
			std::vector<uint8_t> const &payload = record.payload;

			if (payload.size() < 44)
			{
				return false;
			}

			int32_t const count = IntAt(payload, 40);

			if (count <= 0 || (size_t)count > (payload.size() - 44) / 12)
			{
				return false;   // no list, or a count the bytes do not hold (a hostile file): left as it is
			}

			std::vector<uint8_t> entries(payload.begin() + 44, payload.begin() + 44 + 12 * (size_t)count);
			int32_t kept = count;
			int32_t at = 0;

			do
			{
				if (!Exists(index, records, entries.data() + 12 * (size_t)at))
				{
					kept--;

					if (at < kept)
					{
						memcpy(entries.data() + 12 * (size_t)at, entries.data() + 12 * (size_t)kept, 12);
					}
				}
				else
				{
					at++;
				}
			}
			while (at < kept);

			if (kept == count)
			{
				return false;
			}

			std::vector<uint8_t> bytes(payload.begin(), payload.begin() + 44);

			memcpy(bytes.data() + 40, &kept, 4);
			bytes.insert(bytes.end(), entries.begin(), entries.begin() + 12 * (size_t)kept);
			bytes.insert(bytes.end(), payload.begin() + 44 + 12 * (size_t)count, payload.end());
			return WriteBytes(open, record.id, bytes);
		}

		// FUN_00534450 (rows S-K72-R, and S-K80-R through FUN_005354a0; dump-full-3 0x534456-0x534590): a Track Object's point effects, the count at +20, then each effect's point, a pad word,
		// its target count and 16 bytes per target (the ID, the request word, the direction word), then the kind's own words. In each effect every target FUN_005219f0 rejects is removed as
		// in the Mission Objective's list (the LAST target moved into its place and looked at in turn); then an effect with NO targets is deleted, whether or not one was removed (its count
		// is read after the walk, 0x5344d9: an effect that had none goes too), and the effects after it close up IN ORDER (0x534534) and that place is looked at again. With destruct (80,
		// FUN_005354a0 0x5354a8-0x5354ca) the destruct target, the last 8 bytes, becomes the null ID when FUN_005219f0 rejects it; its request word is left as it is. True when it changed.
		bool RepairTrackEffects(OpenLevelFile *open, std::map<std::string, size_t> const &index, std::vector<Record> const &records, Record const &record, bool destruct)
		{
			std::vector<uint8_t> const &payload = record.payload;

			if (payload.size() < 24)
			{
				return false;
			}

			int32_t const count = IntAt(payload, 20);

			if (count < 0 || (size_t)count > (payload.size() - 24) / 12)
			{
				return false;   // a count the bytes do not hold (a hostile file): left as it is
			}

			struct Effect
			{
				std::vector<uint8_t> head;      // the point and the pad word
				std::vector<uint8_t> targets;   // 16 bytes each
			};

			std::vector<Effect> effects;
			size_t at = 24;

			for (int32_t i = 0; i < count; i++)
			{
				if (payload.size() - at < 12)
				{
					return false;
				}

				int32_t const targets = IntAt(payload, at + 8);

				if (targets < 0 || (size_t)targets > (payload.size() - at - 12) / 16)
				{
					return false;   // a target count the bytes do not hold: left as it is
				}

				Effect one;

				one.head.assign(payload.begin() + (std::ptrdiff_t)at, payload.begin() + (std::ptrdiff_t)(at + 8));
				one.targets.assign(payload.begin() + (std::ptrdiff_t)(at + 12), payload.begin() + (std::ptrdiff_t)(at + 12 + 16 * (size_t)targets));
				effects.push_back(one);
				at += 12 + 16 * (size_t)targets;
			}

			std::vector<uint8_t> tail(payload.begin() + (std::ptrdiff_t)at, payload.end());

			for (size_t e = 0; e < effects.size();)
			{
				std::vector<uint8_t> &targets = effects[e].targets;
				size_t kept = targets.size() / 16;

				for (size_t t = 0; t < kept;)
				{
					if (!Exists(index, records, targets.data() + 16 * t))
					{
						kept--;

						if (t < kept)
						{
							memcpy(targets.data() + 16 * t, targets.data() + 16 * kept, 16);
						}
					}
					else
					{
						t++;
					}
				}

				targets.resize(16 * kept);

				if (kept == 0)
				{
					effects.erase(effects.begin() + (std::ptrdiff_t)e);
				}
				else
				{
					e++;
				}
			}

			if (destruct && tail.size() >= 8 && !Exists(index, records, tail.data() + tail.size() - 8))
			{
				memset(tail.data() + tail.size() - 8, 0, 8);   // the null ID (DAT_00914930 / 34)
			}

			std::vector<uint8_t> bytes(payload.begin(), payload.begin() + 24);
			int32_t const left = (int32_t)effects.size();

			memcpy(bytes.data() + 20, &left, 4);

			for (Effect const &one : effects)
			{
				int32_t const targets = (int32_t)(one.targets.size() / 16);

				bytes.insert(bytes.end(), one.head.begin(), one.head.end());
				bytes.insert(bytes.end(), (uint8_t const *)&targets, (uint8_t const *)&targets + 4);
				bytes.insert(bytes.end(), one.targets.begin(), one.targets.end());
			}

			bytes.insert(bytes.end(), tail.begin(), tail.end());

			if (bytes == payload)
			{
				return false;
			}

			return WriteBytes(open, record.id, bytes);
		}
	}

	size_t StrategyRepair_Run(OpenLevelFile *open)
	{
		std::vector<Record> records;

		Collect(open, &records);

		std::map<std::string, size_t> index;

		for (size_t i = 0; i < records.size(); i++)
		{
			index.emplace(Key(records[i].id), i);   // the FIRST record with an ID answers (FUN_00529570 walks the records from the start)
		}

		size_t changed = 0;

		for (Record const &record : records)
		{
			if (!record.hasStrategy)
			{
				continue;
			}

			size_t idAt = 0, requestAt = 0;
			char const *idField = nullptr, *requestField = nullptr;

			if (record.kind == 52 || record.kind == 61)
			{
				idAt = 36; requestAt = 44; idField = "Target"; requestField = "request";   // FUN_0052dc20
			}
			else if (record.kind == 62)
			{
				idAt = 44; requestAt = 40; idField = "targetID"; requestField = "target_request";   // FUN_005315b0
			}
			else if (record.kind == 69)
			{
				idAt = 28; requestAt = 24; idField = "DeathTarget"; requestField = "request";   // FUN_00531cd0
			}
			else if (record.kind == 70)
			{
				changed += RepairMissionList(open, index, records, record) ? 1 : 0;   // FUN_005335f0
				continue;
			}
			else if (record.kind == 72 || record.kind == 76 || record.kind == 80)
			{
				// 76 (a placed hierarchy's Hierarchy Object) has 72's method +0x1c, FUN_00534450 (vtable 0x5af6f4 +0x1c, read from the exe 2026-11-17): its flag effects are the point effects
				changed += RepairTrackEffects(open, index, records, record, record.kind == 80) ? 1 : 0;   // FUN_00534450; 80: FUN_005354a0
				continue;
			}
			else
			{
				continue;
			}

			if (idAt + 8 > record.payload.size() || Exists(index, records, record.payload.data() + idAt))
			{
				continue;
			}

			if (ResetTarget(open, record, idAt, requestAt, idField, requestField))
			{
				changed++;
			}
		}

		return changed;
	}

	size_t StrategyRepair_DeleteObsoleteSwitches(OpenLevelFile *open, bool *failed)
	{
		if (failed)
		{
			*failed = false;
		}

		// FUN_00537030 starts with the repair pass
		StrategyRepair_Run(open);

		std::vector<Record> records;

		Collect(open, &records);

		std::map<std::string, size_t> index;

		for (size_t i = 0; i < records.size(); i++)
		{
			index.emplace(Key(records[i].id), i);   // the FIRST record with an ID answers (FUN_00529570 walks the records from the start)
		}

		std::set<size_t> gone;
		size_t deleted = 0;

		auto const indexOf = [&](uint8_t const *id) -> long
		{
			if (IsNull(id))
			{
				return -1;
			}

			auto const found = index.find(Key(id));

			return found == index.end() || gone.count(found->second) ? -1 : (long)found->second;
		};

		for (size_t i = 0; i < records.size(); i++)
		{
			Record const &first = records[i];

			if (gone.count(i) || !first.virt || !first.hasStrategy || !IsSwitch(first.kind))
			{
				continue;
			}

			std::vector<size_t> closure;
			bool give = false;

			closure.push_back(i);

			for (size_t at = 0; at < closure.size() && !give; at++)
			{
				Record const &member = records[closure[at]];

				if (!member.virt || !member.hasStrategy || !IsSwitch(member.kind))
				{
					give = true;
					break;
				}

				auto const add = [&](uint8_t const *id)
				{
					long const found = indexOf(id);

					if (found < 0)
					{
						return;
					}

					for (size_t seen : closure)
					{
						if (seen == (size_t)found)
						{
							return;
						}
					}

					closure.push_back((size_t)found);
				};

				if ((member.kind == 52 || member.kind == 61) && member.payload.size() >= 44)
				{
					add(member.payload.data() + 36);
				}
				else if ((member.kind == 66 || member.kind == 68) && member.payload.size() >= 40)
				{
					int32_t const count = IntAt(member.payload, 36);

					for (int32_t t = 0; t < count && 40 + (size_t)(t + 1) * 12 <= member.payload.size(); t++)
					{
						add(member.payload.data() + 40 + (size_t)t * 12);
					}
				}
			}

			if (give)
			{
				continue;
			}

			if (VirtualObject_Destroy(open, first.id) != VirtualObjectResult::Done)
			{
				if (failed)
				{
					*failed = true;
				}

				return deleted;
			}

			gone.insert(i);
			deleted++;
		}

		return deleted;
	}
}
