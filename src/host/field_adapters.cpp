// See field_adapters.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".

#include "field_adapters.h"
#include "level_objects.h"
#include "light_edit.h"
#include "object_header.h"
#include "field_panel_host.h"
#include "chunk_record.h"
#include "level_chunks.h"
#include "record_edit.h"
#include "record_join.h"
#include "strategy_edit.h"
#include "strategy_lookup.h"
#include "strategy_record.h"

#include <string.h>

#include <string>
#include <vector>

namespace host
{
	static bool JoinChunk(char const *name, Chunk *chunk, PayloadStore const &store, FieldRecord *record, ShownEdit *edit, bool readOnly = false);   // below

	// An object's ID as 16 hex digits, in capital letters, as the status line says it (Selection_Words)
	static std::string IdInHex(const uint8_t *id)
	{
		static const char kDigits[] = "0123456789ABCDEF";
		std::string hex;

		for (size_t i = 0; i < kSelectionIdBytes; i++)
		{
			hex += kDigits[id[i] >> 4];
			hex += kDigits[id[i] & 0x0F];
		}

		return hex;
	}

	// Whether every byte of an object's ID is 0: the game's own "no name"
	static bool IdIsNull(const uint8_t *id)
	{
		for (size_t i = 0; i < kSelectionIdBytes; i++)
		{
			if (id[i] != 0)
			{
				return false;
			}
		}

		return true;
	}

	// The record for the selection itself (the public builders below add the module's flags to it)
	static bool BuildSelected(const Selected &selected, FieldRecord *record, PayloadStore const *store, ShownEdit *edit)
	{
		if (edit)
		{
			*edit = ShownEdit();
		}

		if (!record)
		{
			return false;
		}

		*record = FieldRecord();

		switch (selected.kind)
		{
			case SelectedKind::Object:
				if (IdIsNull(selected.objectId))
				{
					return false;
				}

				// What the open level holds for this object. D2: an object with a strategy shows its strategy's fields first (strategy_record.h,
				// dialog 129's for a placed object), then its header's six boxes (D1), under "<name>: <kind> (<number>)". The boxes are added
				// only while the record has room for them (the host shows at most kFieldPanelHostMostFields). D1: an object with no strategy
				// (scenery, most of them), or one whose strategy can't be read, shows its header alone, its name as the title; with no name it
				// keeps its boxes and gets the stand-in's title below, its ID. An object the level doesn't hold, or no level, keeps the
				// stand-in alone.
				{
					ObjectHeader header;
					bool const hasHeader = LevelObjects_FindHeader(selected.objectId, &header);
					ObjectStrategy strategy;
					char const *why = 0;

					// With a store, the strategy's bytes are the store's as they are now, and the fields the model can set are opened
					bool const editable = store && edit;
					bool const found = editable ? LookUpStrategyBytes(LevelObjects_Level(), *store, selected.objectId, &strategy) == StrategyFound::Found
					                            : FindSelectedStrategy(LevelObjects_Level(), selected, &strategy) == StrategyFound::Found;

					if (found && (editable ? BuildEditableStrategyRecord(strategy.payload.data(), strategy.payload.size(), record, &why)
					                       : BuildStrategyRecord(strategy.payload.data(), strategy.payload.size(), record, &why)))
					{
						if (editable)
						{
							// The record was built for a payload of at least 20 bytes with a kind the model knows: its number is the int32 at +8
							uint32_t const bits = (uint32_t)strategy.payload[8] | ((uint32_t)strategy.payload[9] << 8) |
								((uint32_t)strategy.payload[10] << 16) | ((uint32_t)strategy.payload[11] << 24);
							int32_t number = 0;

							memcpy(&number, &bits, sizeof(number));

							EditPart part;

							part.first = 0;
							part.count = record->list.size();
							part.chunk = strategy.chunk;
							part.strategyKind = model::FindStrategyKind(number);
							edit->parts.push_back(part);
						}

						std::string const kind = record->title;
						bool const boxes = hasHeader && record->list.size() + 6 <= kFieldPanelHostMostFields && AddObjectHeaderBoxes(header, record);

						record->title = boxes && !header.name.empty() ? header.name + ": " + kind : kind;
						return true;
					}

					if (hasHeader && BuildObjectHeaderRecord(header, record) && !record->title.empty())
					{
						return true;
					}
				}

				record->title = "Object " + IdInHex(selected.objectId);
				return true;

			case SelectedKind::Light:
			{
				// One light of the level's light set (Light Sourcing, light_edit.h): its numbers are a STDLIGHT chunk, edited through the same
				// editable record as any chunk (M-3, H1)
				if (selected.lightIndex < 0)
				{
					return false;
				}

				std::string const name = "Light " + std::to_string((long long)selected.lightIndex + 1);
				Chunk *const light = FindLightChunk(LevelObjects_Level(), (size_t)selected.lightIndex);

				if (store && edit && light)
				{
					JoinChunk("STDLIGHT", light, *store, record, edit);   // a light is one STDLIGHT run of fields
				}

				record->title = name;
				return true;
			}

			case SelectedKind::Module:
				if (selected.moduleIndex < 0)
				{
					return false;
				}

				record->title = "Module " + std::to_string(selected.moduleIndex);
				return true;

			default:
				// Nothing selected, or a kind this doesn't know
				return false;
		}
	}

	// Joins the record of one chunk the model has a table for to *record, EDITABLE, from the bytes the store holds, and notes its run of fields
	// in *edit. A chunk that isn't there, whose bytes the store hasn't, or that the model can't build a record for, is left out: it is not a
	// mistake, as most chunks of a level are optional. Gives back whether the chunk's fields were added.
	static bool JoinChunk(char const *name, Chunk *chunk, PayloadStore const &store, FieldRecord *record, ShownEdit *edit, bool readOnly)
	{
		std::vector<unsigned char> bytes;
		FieldRecord more;
		char const *why = 0;
		model::ChunkKind const *const kind = model::FindAnyChunkKind(name, 0);

		if (!chunk || !kind || !store.PayloadOf(chunk, &bytes) || !BuildEditableChunkRecord(name, bytes.data(), bytes.size(), &more, &why))
		{
			return false;
		}

		// A chunk inteng never lets you change (the level's settings: inteng has no screen for them, they were the old CENCON tool's dialogs) is shown
		// and never edited: every descriptor is read-only, so the panel takes no value for it and EditFromPanel refuses it (Bishop's audit 2026-11-15)
		if (readOnly)
		{
			for (size_t i = 0; i < more.list.size(); i++)
			{
				more.list[i].readOnly = true;
			}
		}

		EditPart part;

		part.first = record->list.size();
		part.count = more.list.size();
		part.chunk = chunk;
		part.chunkKind = kind;

		if (!JoinRecord(record, &more, kFieldPanelHostMostFields))
		{
			return false;
		}

		edit->parts.push_back(part);
		return true;
	}

	bool FieldAdapters_Build(const Selected &selected, FieldRecord *record)
	{
		return FieldAdapters_Build(selected, record, 0, 0);
	}

	bool FieldAdapters_BuildPlacedLight(uint8_t const *objectId, FieldRecord *record, PayloadStore const *store, ShownEdit *edit)
	{
		*record = FieldRecord();

		Chunk *const placed = objectId && LevelObjects_Level() ? FindPlacedLightChunk(LevelObjects_Level(), objectId) : nullptr;

		if (!placed || !store || !edit)
		{
			return false;
		}

		FieldRecord whole;
		ShownEdit wholeEdit;

		if (!JoinChunk("PLOBJLIT", placed, *store, &whole, &wholeEdit) || wholeEdit.parts.empty())
		{
			return false;
		}

		// inteng's dialog (FUN_004b0470, "Light properties") has only: the five times in milliseconds (Fade up time, Time up, Fade down time, Time down, Timer start), "On/off type", "Light type", and the two
		// checkboxes "Start on" (flags bit 1) and "Use down colour for off" (flags bit 2), in that order. The descriptors are picked out of the whole record, in that order, with their values and sources.
		struct Wanted { char const *name; uint32_t bit; };
		Wanted const order[] = { { "fade_up_time", 0 }, { "up_time", 0 }, { "fade_down_time", 0 }, { "down_time", 0 }, { "start_time", 0 }, { "on_off_type", 0 }, { "light_type", 0 }, { "flags", 1 }, { "flags", 2 } };
		std::vector<size_t> keep;

		for (Wanted const &want : order)
		{
			for (size_t i = 0; i < whole.list.size() && i < whole.sources.size(); i++)
			{
				FieldSource const &source = whole.sources[i];

				if (source.field && strcmp(source.field->name, want.name) == 0 && (want.bit == 0 || (source.what == SourceKind::Bit && source.bit == want.bit)))
				{
					keep.push_back(i);
					break;
				}
			}
		}

		if (keep.size() != sizeof(order) / sizeof(order[0]))
		{
			return false;
		}

		std::vector<ui::FieldDescriptor> list;
		std::vector<ui::FieldValue> values;
		std::deque<FieldSource> sources;

		for (size_t i : keep)
		{
			list.push_back(whole.list[i]);
			values.push_back(whole.values[i]);
			sources.push_back(whole.sources[i]);
		}

		whole.list = list;
		whole.values = values;
		whole.sources = sources;

		EditPart part = wholeEdit.parts[0];

		part.first = 0;
		part.count = whole.list.size();
		wholeEdit.parts.assign(1, part);

		*record = std::move(whole);
		*edit = wholeEdit;
		record->title = "Light properties";
		return true;
	}

	bool FieldAdapters_Build(const Selected &selected, FieldRecord *record, PayloadStore const *store, ShownEdit *edit)
	{
		if (!BuildSelected(selected, record, store, edit))
		{
			return false;
		}

		// G: an object that is a module shows its flags after the rest, editable
		if (store && edit && selected.kind == SelectedKind::Object)
		{
			Chunk *flags = 0;

			if (FindModuleFlags(LevelObjects_Level(), selected.objectId, &flags) == LevelChunkFound::Found)
			{
				JoinChunk("MODFLAGS", flags, *store, record, edit);
			}
		}

		return true;
	}

	bool FieldAdapters_BuildLevelSettings(PayloadStore const *store, FieldRecord *record, ShownEdit *edit)
	{
		if (!store || !record || !edit)
		{
			return false;
		}

		*record = FieldRecord();
		*edit = ShownEdit();

		// L: the chunks of the level's settings the model has a table for, in the order of the old editor's dialog. The names a SOUNDDIR or a
		// RANTEXID holds aren't shown yet (chunk_record.h), so what each shows is its numbers.
		char const *const names[] = { "AVPENVIR", "GLOGENDC", "ENVSDSCL", "SOUNDDIR" };
		bool any = false;

		for (char const *const name : names)
		{
			Chunk *found = 0;

			if (FindEnvironmentChild(LevelObjects_Level(), name, &found) == LevelChunkFound::Found && JoinChunk(name, found, *store, record, edit, true))
			{
				any = true;
			}
		}

		if (!any)
		{
			*record = FieldRecord();
			*edit = ShownEdit();
			return false;
		}

		record->title = "Level settings";
		return true;
	}
}
