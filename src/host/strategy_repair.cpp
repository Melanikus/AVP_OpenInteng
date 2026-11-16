// strategy_repair.cpp: see strategy_repair.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "strategy_repair.h"

#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "field_edit.h"
#include "level_file.h"
#include "level_store.h"
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

				if (FindObjectStrategy(level, record.id, &now) == StrategyFound::Found && now.payload.size() >= 12)
				{
					record.hasStrategy = true;
					record.kind = IntAt(now.payload, 8);
					record.payload = now.payload;
				}

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

					if (FindObjectStrategy(level, record.id, &now) == StrategyFound::Found && now.payload.size() >= 12)
					{
						record.hasStrategy = true;
						record.kind = IntAt(now.payload, 8);
						record.payload = now.payload;
					}

					out->push_back(record);
				}
			}
		}

		// FUN_005219f0 (without its last branch, see the header): a target ID that a record answers to
		bool Exists(std::map<std::string, size_t> const &index, std::vector<Record> const &records, uint8_t const *id)
		{
			if (IsNull(id))
			{
				return false;
			}

			auto const found = index.find(Key(id));

			return found != index.end() && (records[found->second].virt || records[found->second].hasStrategy);
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

	size_t StrategyRepair_Run(OpenLevelFile *open)
	{
		std::vector<Record> records;

		Collect(open, &records);

		std::map<std::string, size_t> index;

		for (size_t i = 0; i < records.size(); i++)
		{
			index[Key(records[i].id)] = i;
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
			index[Key(records[i].id)] = i;
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
