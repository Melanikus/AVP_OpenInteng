// See field_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".

#include "field_edit.h"

#include <string.h>

#include <exception>
#include <string>

namespace host
{
	namespace
	{
		char const ObjectRefusedWords[] = "that object can't be put in that field";
		char const NoReasonWords[] = "the field can't hold that value";

		// The field's name from outside: read to its first 0, at most kFieldEditNameMost bytes. False for no name, an empty one, or one that
		// doesn't end by then (no field has so long a name).
		bool NameOf(char const *name, std::string *out)
		{
			out->clear();

			if (!name)
			{
				return false;
			}

			size_t length = 0;

			while (length < kFieldEditNameMost && name[length] != 0)
			{
				out->push_back(name[length]);
				length++;
			}

			return length > 0 && name[length] == 0;
		}

		// The kind's own field of that name, then one every strategy has (but not the kind's own number)
		model::Field const *StrategyFieldNamed(model::StrategyKind const &kind, std::string const &name)
		{
			for (size_t i = 0; i < kind.fieldCount; i++)
			{
				if (name == kind.fields[i].name)
				{
					return &kind.fields[i];
				}
			}

			for (size_t i = 0; i < model::StrategyBaseFieldCount; i++)
			{
				if (strcmp(model::StrategyBaseFields[i].name, "StrategyType") != 0 && name == model::StrategyBaseFields[i].name)
				{
					return &model::StrategyBaseFields[i];
				}
			}

			return 0;
		}

		int32_t KindAt8(std::vector<unsigned char> const &bytes)
		{
			uint32_t const bits = (uint32_t)bytes[8] | ((uint32_t)bytes[9] << 8) | ((uint32_t)bytes[10] << 16) | ((uint32_t)bytes[11] << 24);
			int32_t number;
			memcpy(&number, &bits, sizeof(number));
			return number;
		}

		EditAnswer Refused(char const **why, char const *reason, char const *otherwise)
		{
			if (why)
			{
				*why = reason ? reason : otherwise;
			}

			return EditAnswer::ValueRefused;
		}
	}

	char const *EditAnswerWords(EditAnswer answer)
	{
		switch (answer)
		{
		case EditAnswer::Applied: return "the value was put in";
		case EditAnswer::BadCall: return "there was no store, no chunk or no field to edit";
		case EditAnswer::NoPayload: return "the editor has no bytes for that chunk";
		case EditAnswer::PayloadRefused: return "the chunk isn't a size the game reads, so it isn't edited";
		case EditAnswer::KindChanged: return "the strategy isn't the kind this edit was made for";
		case EditAnswer::UnknownKind: return "the editor doesn't know this kind of strategy";
		case EditAnswer::NoSuchField: return "this kind has no field of that name";
		case EditAnswer::ValueRefused: return "the field can't hold that value";
		case EditAnswer::NotStored: return "the editor couldn't keep the edited chunk";
		case EditAnswer::NoMemory: return "there isn't memory enough to edit";
		}

		return "the edit couldn't be made";
	}

	EditAnswer ApplyStrategyPayloadEdit(PayloadStore *store, Chunk *chunk, int32_t kind, FieldEdit const &edit, char const **why)
	{
		if (why)
		{
			*why = 0;
		}

		std::string name;

		if (!store || !chunk || !NameOf(edit.field, &name))
		{
			return EditAnswer::BadCall;
		}

		// Running out of memory refuses the edit, with nothing changed
		try
		{
			std::vector<unsigned char> bytes;

			if (!store->PayloadOf(chunk, &bytes))
			{
				return EditAnswer::NoPayload;
			}

			// The size first, as the game's loader checks it: that also says the 20 bytes every strategy has are there to read the kind from
			if (model::CheckStrategySize(bytes.data(), bytes.size(), 0) != model::SizeCheck::Right)
			{
				return EditAnswer::PayloadRefused;
			}

			if (KindAt8(bytes) != kind)
			{
				return EditAnswer::KindChanged;
			}

			model::StrategyKind const *const found = model::FindStrategyKind(kind);

			if (!found)
			{
				return EditAnswer::UnknownKind;
			}

			model::Field const *const field = StrategyFieldNamed(*found, name);

			if (!field)
			{
				return EditAnswer::NoSuchField;
			}

			// Padding is not a field anyone types into: inteng's Edit Strategy screen has no line for a pad or a spare number (inteng-edit-rules.md)
			if (strncmp(field->name, "pad", 3) == 0 || strncmp(field->name, "spare", 5) == 0)
			{
				if (why)
				{
					*why = "this is padding, which the old editor never shows or changes";
				}

				return EditAnswer::ValueRefused;
			}

			bool taken = false;
			char const *reason = 0;

			switch (edit.what)
			{
			case EditWhat::Number:
				taken = model::SetField(*found, *field, bytes.data(), bytes.size(), edit.number, &reason);
				break;

			case EditWhat::Axis:
				taken = model::SetFieldAxis(*found, *field, bytes.data(), bytes.size(), edit.axis, edit.axisValue, &reason);
				break;

			case EditWhat::Object:
				taken = model::SetFieldObject(*found, *field, bytes.data(), bytes.size(), edit.first, edit.second);
				reason = ObjectRefusedWords;
				break;
			}

			if (!taken)
			{
				return Refused(why, reason, NoReasonWords);
			}

			return store->SetPayload(chunk, bytes) ? EditAnswer::Applied : EditAnswer::NotStored;
		}
		catch (std::exception const &)
		{
			return EditAnswer::NoMemory;
		}
	}

	EditAnswer ApplyChunkEdit(PayloadStore *store, Chunk *chunk, model::ChunkKind const &kind, FieldEdit const &edit, char const **why)
	{
		if (why)
		{
			*why = 0;
		}

		std::string name;

		if (!store || !chunk || !NameOf(edit.field, &name))
		{
			return EditAnswer::BadCall;
		}

		try
		{
			std::vector<unsigned char> bytes;

			if (!store->PayloadOf(chunk, &bytes))
			{
				return EditAnswer::NoPayload;
			}

			if (model::CheckChunkSize(kind, bytes.data(), bytes.size()) != model::SizeCheck::Right)
			{
				return EditAnswer::PayloadRefused;
			}

			model::Field const *const field = model::FindChunkField(kind, name.c_str());

			if (!field)
			{
				return EditAnswer::NoSuchField;
			}

			bool taken = false;
			char const *reason = 0;

			switch (edit.what)
			{
			case EditWhat::Number:
				taken = model::SetChunkField(kind, *field, bytes.data(), bytes.size(), edit.number, &reason);
				break;

			case EditWhat::Axis:
				taken = model::SetChunkFieldAxis(kind, *field, bytes.data(), bytes.size(), edit.axis, edit.axisValue, &reason);
				break;

			case EditWhat::Object:
				taken = model::SetChunkFieldObject(kind, *field, bytes.data(), bytes.size(), edit.first, edit.second);
				reason = ObjectRefusedWords;
				break;
			}

			if (!taken)
			{
				return Refused(why, reason, NoReasonWords);
			}

			return store->SetPayload(chunk, bytes) ? EditAnswer::Applied : EditAnswer::NotStored;
		}
		catch (std::exception const &)
		{
			return EditAnswer::NoMemory;
		}
	}
}
