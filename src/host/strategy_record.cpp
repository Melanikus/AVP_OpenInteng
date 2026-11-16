// See strategy_record.h. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "strategy_record.h"

#include <stdio.h>
#include <string.h>

#include <exception>

#include "field_descriptor_from_model.h"
#include "field_model.h"
#include "field_panel_host.h"

namespace host
{
	namespace
	{
		// The words a refusal gives that aren't the model's own. Static, so a reader of *why can keep the pointer.
		char const NoRecordWords[] = "there's no record to fill";
		char const NoPayloadWords[] = "there's no strategy to show";
		char const UnknownKindWords[] = "the editor doesn't know this kind of strategy";
		char const TooManyFieldsWords[] = "the strategy has more fields than the panel shows";
		char const NoMemoryWords[] = "there isn't memory enough to show the strategy";

		// A flags field: one Bit for each named bit, but not the bits the editor always sets when it saves. The whole word is each Bit's
		// value, as the panel takes it. Gives back false when the field can't be read.
		bool AddBits(model::StrategyKind const &kind, model::Field const &field, uint8_t const *payload, size_t length, FieldRecord *record)
		{
			double word = 0;

			if (!model::ReadField(kind, field, payload, length, &word, 0))
			{
				return false;
			}

			char const *help = 0;
			bool helpKept = false;

			for (size_t i = 0; i < model::StrategyBitCount; i++)
			{
				model::BitName const &named = model::StrategyBits[i];

				if (named.kind != kind.number || strcmp(named.field, field.name) != 0 || named.alwaysSet)
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
		bool AddChoice(model::StrategyKind const &kind, model::Field const &field, std::string const &label, uint8_t const *payload,
		               size_t length, FieldRecord *record, bool *added)
		{
			*added = false;
			double stored = 0;

			if (!model::ReadField(kind, field, payload, length, &stored, 0))
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

			for (size_t i = 0; i < model::StrategyChoiceCount; i++)
			{
				model::ChoiceName const &named = model::StrategyChoices[i];

				if (named.kind == kind.number && strcmp(named.field, field.name) == 0)
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
		bool AddTriple(model::StrategyKind const &kind, model::Field const &field, uint8_t const *payload, size_t length,
		               FieldRecord *record)
		{
			int32_t axes[3] = { 0, 0, 0 };

			for (int axis = 0; axis < 3; axis++)
			{
				if (!model::ReadFieldAxis(kind, field, payload, length, axis, &axes[axis], 0))
				{
					return false;
				}
			}

			PushTriple(record, SafeWords(field.label, ui::kFieldNameMost - 2), HelpOf(record, field), field, axes);
			return true;
		}

		// An object field: its two halves in hex, or "none" for 0 and 0, in a Text no longer than that. Gives back false when the field
		// can't be read.
		bool AddObject(model::StrategyKind const &kind, model::Field const &field, std::string const &label, uint8_t const *payload,
		               size_t length, FieldRecord *record)
		{
			uint32_t first = 0;
			uint32_t second = 0;

			if (!model::ReadFieldObject(kind, field, payload, length, &first, &second))
			{
				return false;
			}

			PushObject(record, Keep(record, label), HelpOf(record, field), first, second, &field);
			return true;
		}

		// Adds what one field shows, if anything. Gives back false only when a field the kind has can't be read from the payload.
		bool AddField(model::StrategyKind const &kind, model::Field const &field, uint8_t const *payload, size_t length, FieldRecord *record)
		{
			// An orientation isn't shown yet: orientations come with the edit path.
			if (field.storage == model::Storage::IntMatrix)
			{
				return true;
			}

			// A flags field has no label of its own: each of its bits has one.
			if (field.unit == model::Unit::Flags)
			{
				return AddBits(kind, field, payload, length, record);
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

				if (!AddChoice(kind, field, label, payload, length, record, &added))
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

			if (!model::ReadField(kind, field, payload, length, &number, 0))
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

		bool Build(model::StrategyKind const &kind, uint8_t const *payload, size_t length, FieldRecord *record, char const **why)
		{
			EmptyRecord(record);

			char const *const label = (kind.offered && kind.label && kind.label[0]) ? kind.label : kind.name;
			record->title = SafeWords(label, ui::kFieldNameMost - 8) + " (" + std::to_string(kind.number) + ")";

			// The kind's own fields first, then the ones every strategy has. The kind's own number isn't a field to show.
			for (size_t i = 0; i < kind.fieldCount; i++)
			{
				if (!AddField(kind, kind.fields[i], payload, length, record))
				{
					return RefuseWith(record, why, "the strategy's own field " + SafeWords(kind.fields[i].name, ui::kFieldNameMost) +
					                                   " couldn't be read");
				}
			}

			for (size_t i = 0; i < model::StrategyBaseFieldCount; i++)
			{
				if (strcmp(model::StrategyBaseFields[i].name, "StrategyType") == 0)
				{
					continue;
				}

				if (!AddField(kind, model::StrategyBaseFields[i], payload, length, record))
				{
					return RefuseWith(record, why, "the strategy's own field " +
					                                   SafeWords(model::StrategyBaseFields[i].name, ui::kFieldNameMost) + " couldn't be read");
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

	bool BuildStrategyRecordOfKind(const model::StrategyKind &kind, const uint8_t *payload, size_t length, FieldRecord *record,
	                               const char **why)
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
			return Build(kind, payload, length, record, why);
		}
		catch (std::exception const &)
		{
			return Refuse(record, why, NoMemoryWords);
		}
	}

	bool BuildStrategyRecord(const uint8_t *payload, size_t length, FieldRecord *record, const char **why)
	{
		if (!record)
		{
			return Refuse(0, why, NoRecordWords);
		}

		if (!payload)
		{
			return Refuse(record, why, NoPayloadWords);
		}

		model::SizeCheck const check = model::CheckStrategySize(payload, length, 0);

		if (check != model::SizeCheck::Right)
		{
			return Refuse(record, why, model::SizeCheckWords(check));
		}

		// The kind's number: an int32, little-endian, at +8 (the size check has found at least 20 bytes)
		uint32_t const bits = (uint32_t)payload[8] | ((uint32_t)payload[9] << 8) | ((uint32_t)payload[10] << 16) | ((uint32_t)payload[11] << 24);
		int32_t number = 0;
		memcpy(&number, &bits, sizeof(number));

		model::StrategyKind const *const kind = model::FindStrategyKind(number);

		if (!kind)
		{
			return Refuse(record, why, UnknownKindWords);
		}

		return BuildStrategyRecordOfKind(*kind, payload, length, record, why);
	}
}
