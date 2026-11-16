// See chunk_record.h. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "chunk_record.h"

#include <string.h>

#include <exception>

#include "field_descriptor_from_model.h"
#include "field_panel_host.h"

namespace host
{
	namespace
	{
		// The words a refusal gives that aren't the model's own. Static, so a reader of *why can keep the pointer.
		char const NoRecordWords[] = "there's no record to fill";
		char const NoPayloadWords[] = "there's no chunk to show";
		char const UnknownChunkWords[] = "the editor doesn't know this kind of chunk";
		char const ChunkOfItemsWords[] = "this kind of chunk has items, and is shown one item at a time";
		char const TooManyFieldsWords[] = "the chunk has more fields than the panel shows";
		char const NoMemoryWords[] = "there isn't memory enough to show the chunk";

		// A flags field: one Bit for each named bit of this chunk's field. The whole word is each Bit's value, as the panel takes it.
		// Gives back false when the field can't be read.
		bool AddBits(model::ChunkKind const &kind, model::ChunkNames const &names, model::Field const &field, uint8_t const *payload,
		             size_t length, FieldRecord *record)
		{
			double word = 0;

			if (!model::ReadChunkField(kind, field, payload, length, &word, 0))
			{
				return false;
			}

			char const *help = 0;
			bool helpKept = false;

			for (size_t i = 0; i < names.bitCount; i++)
			{
				model::ChunkBitName const &named = names.bits[i];

				if (strcmp(named.chunk, kind.id) != 0 || strcmp(named.field, field.name) != 0)
				{
					continue;
				}

				std::string const label = SafeWords(named.label, ui::kFieldNameMost);

				if (label.empty())
				{
					continue;   // a bit the old editor showed no label for isn't shown
				}

				if (!helpKept)
				{
					help = HelpOf(record, field);
					helpKept = true;
				}

				PushBit(record, Keep(record, label), help, named.bit, (uint32_t)word, &field);
			}

			return true;
		}

		// A choice: one Choice box with the field's named values, and the value as the file holds it, named or not. Gives back false when
		// the field can't be read, and sets *added to whether a box was made: a choice with no named value at all can't be a dropdown (the
		// panel's check refuses a list of none), so it's shown as a number instead.
		bool AddChoice(model::ChunkKind const &kind, model::ChunkNames const &names, model::Field const &field, std::string const &label,
		               uint8_t const *payload, size_t length, FieldRecord *record, bool *added)
		{
			*added = false;
			double stored = 0;

			if (!model::ReadChunkField(kind, field, payload, length, &stored, 0))
			{
				return false;
			}

			// The panel holds a choice as an int32. A value outside that (an unsigned field's, from 2147483648 up) can't be one, so it's
			// shown as a Number, exactly as the file holds it: *added stays false, which is what AddField does for a choice it can't box.
			if (!FitsAChoice(stored))
			{
				return true;
			}

			std::vector<ui::Choice> choices;

			for (size_t i = 0; i < names.choiceCount; i++)
			{
				model::ChunkChoiceName const &named = names.choices[i];

				if (strcmp(named.chunk, kind.id) == 0 && strcmp(named.field, field.name) == 0)
				{
					std::string words = SafeWords(named.label, ui::kFieldNameMost);

					if (words.empty())
					{
						words = "?";   // every choice has words, or the list is refused
					}

					ui::Choice const choice = { named.value, Keep(record, words) };
					choices.push_back(choice);
				}
			}

			if (choices.empty())
			{
				return true;
			}

			PushChoice(record, Keep(record, label), HelpOf(record, field), choices, (int32_t)stored, &field);
			*added = true;
			return true;
		}

		// A field of three numbers: three Numbers, named for their axis. The label is cut two letters short so the axis fits.
		bool AddTriple(model::ChunkKind const &kind, model::Field const &field, uint8_t const *payload, size_t length, FieldRecord *record)
		{
			int32_t axes[3] = { 0, 0, 0 };

			for (int axis = 0; axis < 3; axis++)
			{
				if (!model::ReadChunkFieldAxis(kind, field, payload, length, axis, &axes[axis], 0))
				{
					return false;
				}
			}

			PushTriple(record, SafeWords(field.label, ui::kFieldNameMost - 2), HelpOf(record, field), field, axes);
			return true;
		}

		// An object field: its two halves in hex, or "none" for 0 and 0. Gives back false when the field can't be read.
		bool AddObject(model::ChunkKind const &kind, model::Field const &field, std::string const &label, uint8_t const *payload,
		               size_t length, FieldRecord *record)
		{
			uint32_t first = 0;
			uint32_t second = 0;

			if (!model::ReadChunkFieldObject(kind, field, payload, length, &first, &second))
			{
				return false;
			}

			PushObject(record, Keep(record, label), HelpOf(record, field), first, second, &field);
			return true;
		}

		// Adds what one field shows, if anything. Gives back false only when a field the kind has can't be read from the payload.
		bool AddField(model::ChunkKind const &kind, model::ChunkNames const &names, model::Field const &field, uint8_t const *payload,
		              size_t length, FieldRecord *record)
		{
			// An orientation isn't shown yet: orientations come with the edit path.
			if (field.storage == model::Storage::IntMatrix)
			{
				return true;
			}

			// A flags field has no label of its own: each of its bits has one.
			if (field.unit == model::Unit::Flags)
			{
				return AddBits(kind, names, field, payload, length, record);
			}

			// A field the old editor showed no label for isn't shown, and its bytes are left alone.
			std::string const label = SafeWords(field.label, ui::kFieldNameMost);

			if (label.empty())
			{
				return true;
			}

			if (field.unit == model::Unit::Choice)
			{
				bool added = false;

				if (!AddChoice(kind, names, field, label, payload, length, record, &added))
				{
					return false;
				}

				if (added)
				{
					return true;
				}
			}

			if (field.storage == model::Storage::IntTriple)
			{
				return AddTriple(kind, field, payload, length, record);
			}

			if (field.storage == model::Storage::ObjectId)
			{
				return AddObject(kind, field, label, payload, length, record);
			}

			double number = 0;

			if (!model::ReadChunkField(kind, field, payload, length, &number, 0))
			{
				return false;
			}

			PushNumber(record, Keep(record, label), HelpOf(record, field), field, number);
			return true;
		}

		bool Refuse(FieldRecord *record, char const **why, char const *words)
		{
			if (record)
			{
				EmptyRecord(record);
			}

			if (why)
			{
				*why = words;
			}

			return false;
		}

		// A refusal whose words are made here: kept in the record (which is empty otherwise), so *why stays good as long as the record.
		bool RefuseWith(FieldRecord *record, char const **why, std::string const &words)
		{
			EmptyRecord(record);
			record->text.push_back(words);

			if (why)
			{
				*why = record->text.back().c_str();
			}

			return false;
		}

		bool Build(model::ChunkKind const &kind, model::ChunkNames const &names, uint8_t const *payload, size_t length, FieldRecord *record,
		           char const **why)
		{
			EmptyRecord(record);
			record->title = SafeWords(kind.label, 31) + " (" + SafeWords(kind.id, 8) + ")";

			for (size_t i = 0; i < kind.fieldCount; i++)
			{
				if (!AddField(kind, names, kind.fields[i], payload, length, record))
				{
					return RefuseWith(record, why, "the chunk's own field " + SafeWords(kind.fields[i].name, ui::kFieldNameMost) +
					                                   " couldn't be read");
				}
			}

			if (record->list.size() > kFieldPanelHostMostFields)
			{
				return Refuse(record, why, TooManyFieldsWords);
			}

			ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

			if (!ui::CheckFieldList(record->list.data(), record->list.size(), &problem))
			{
				char reason[96];
				snprintf(reason, sizeof(reason), "the record didn't pass the panel's check (field %u)", (unsigned)problem.field);
				return RefuseWith(record, why, reason);
			}

			return true;
		}
	}

	bool BuildChunkRecordOfKind(model::ChunkKind const &kind, model::ChunkNames const &names, uint8_t const *payload, size_t length,
	                            FieldRecord *record, char const **why)
	{
		if (!record)
		{
			return Refuse(0, why, NoRecordWords);
		}

		if (!payload)
		{
			return Refuse(record, why, NoPayloadWords);
		}

		// Running out of memory refuses the record, with nothing shown, as any other mistake does
		try
		{
			return Build(kind, names, payload, length, record, why);
		}
		catch (std::exception const &)
		{
			return Refuse(record, why, NoMemoryWords);
		}
	}

	bool BuildChunkRecord(char const *chunk, uint8_t const *payload, size_t length, FieldRecord *record, char const **why)
	{
		if (!record)
		{
			return Refuse(0, why, NoRecordWords);
		}

		if (!payload)
		{
			return Refuse(record, why, NoPayloadWords);
		}

		model::ChunkNames names = { 0, 0, 0, 0 };
		model::ChunkKind const *const kind = chunk ? model::FindAnyChunkKind(chunk, &names) : 0;

		if (!kind)
		{
			// A chunk the editor describes in parts (a track, alternate locations, the AI chunks) has no ChunkKind to build from here: it's
			// shown one item at a time, by code of its own, so it isn't called unknown
			bool const hasItems = chunk && model::FindChunkFamily(chunk) != model::ChunkFamily::None;

			return Refuse(record, why, hasItems ? ChunkOfItemsWords : UnknownChunkWords);
		}

		model::SizeCheck const check = model::CheckChunkSize(*kind, payload, length);

		if (check != model::SizeCheck::Right)
		{
			return Refuse(record, why, model::ChunkSizeCheckWords(check));
		}

		return BuildChunkRecordOfKind(*kind, names, payload, length, record, why);
	}
}
