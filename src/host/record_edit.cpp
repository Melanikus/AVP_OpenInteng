// See record_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "record_edit.h"

#include <math.h>
#include <string.h>

#include "field_descriptor_from_model.h"

#include "chunk_record.h"
#include "field_descriptor_from_model.h"
#include "real_numbers.h"   // IsNaNOrEndless, by the bits: the editor is built with /fp:fast
#include "strategy_record.h"

namespace host
{
	// The whole number a field holds for a number shown in its display unit
	double StoredFromShown(model::Field const &field, double shown)
	{
		double const stored = shown / DisplayFactor(field);

		return stored >= 0 ? floor(stored + 0.5) : -floor(-stored + 0.5);
	}

	char const NotAnObjectReason[] = "an object is \"none\", the name of an object of the level, or two groups of 8 digits and letters A to F with a colon between, such as 7334ED09:EF81A000";

	namespace
	{
		// The reason for a value an axis can't take, which the panel's own checks never let through but a caller might
		char const NotAWholeNumberWords[] = "this value has to be a whole number that fits the field";

		// The field spec's check of a number, run by the panel before it keeps one: the model's own, and for a choice the values it names.
		// The context is the descriptor's FieldSource, which lives in the record.
		bool NumberFieldCheck(void const *context, double value, char const **why)
		{
			FieldSource const *const source = (FieldSource const *)context;

			if (!source || !source->field)
			{
				return false;
			}

			// a number shown in another unit (a light's percent, degrees and metres) is checked as the whole number it comes to in the field
			if (DisplayFactor(*source->field) != 1.0)
			{
				value = StoredFromShown(*source->field, value);
			}

			if (!model::ValueIsAllowed(*source->field, value, why))
			{
				return false;
			}

			if (source->field->unit != model::Unit::Choice)
			{
				return true;
			}

			// A choice field is set to a value its choices name, and the names are int32 (turning a number above that into one is undefined)
			bool const named = FitsAChoice(value) && (source->ofChunk ? model::ChunkChoiceIsNamed(source->chunk, source->field->name, (int32_t)value)
			                                                          : model::ChoiceIsNamed(source->strategyKind, source->field->name, (int32_t)value));

			if (!named && why)
			{
				*why = model::UnnamedChoiceReason;
			}

			return named;
		}

		bool ObjectTextCheck(void const *, unsigned char const *bytes, size_t length, unsigned char const *, size_t, char const **why)
		{
			uint32_t first = 0;
			uint32_t second = 0;

			if (ParseObjectText(bytes, length, &first, &second))
			{
				return true;
			}

			if (why)
			{
				*why = NotAnObjectReason;
			}

			return false;
		}

		// Whether the model can set the field the descriptor shows, in the way the descriptor shows it
		bool CanOpen(FieldSource const &source, ui::FieldDescriptor const &descriptor)
		{
			if (!source.field || source.field->unit == model::Unit::ListCount)
			{
				return false;
			}

			model::Storage const storage = source.field->storage;
			bool const plainNumber = storage != model::Storage::IntTriple && storage != model::Storage::ObjectId && storage != model::Storage::IntMatrix;

			switch (source.what)
			{
			case SourceKind::Number: return descriptor.kind == ui::FieldKind::Number && plainNumber;
			case SourceKind::Axis: return descriptor.kind == ui::FieldKind::Number && storage == model::Storage::IntTriple;
			case SourceKind::Bit: return descriptor.kind == ui::FieldKind::Bit && source.field->unit == model::Unit::Flags;
			case SourceKind::Choice: return descriptor.kind == ui::FieldKind::Choice && source.field->unit == model::Unit::Choice;
			case SourceKind::Object: return descriptor.kind == ui::FieldKind::Text && storage == model::Storage::ObjectId;
			default: return false;
			}
		}

		// Opens every descriptor the model can set, and says whose fields they are
		void Open(FieldRecord *record, bool ofChunk, int32_t strategyKind, char const *chunk)
		{
			if (!record)
			{
				return;
			}

			size_t const count = record->sources.size() < record->list.size() ? record->sources.size() : record->list.size();

			for (size_t i = 0; i < count; i++)
			{
				FieldSource &source = record->sources[i];
				ui::FieldDescriptor &descriptor = record->list[i];

				if (!CanOpen(source, descriptor))
				{
					continue;
				}

				source.ofChunk = ofChunk;
				source.strategyKind = strategyKind;

				for (int letter = 0; letter < 8; letter++)
				{
					source.chunk[letter] = (ofChunk && chunk) ? chunk[letter] : 0;
				}

				source.chunk[8] = 0;
				descriptor.readOnly = false;

				if (descriptor.kind == ui::FieldKind::Number)
				{
					descriptor.numberCheck = &NumberFieldCheck;
					descriptor.checkContext = &source;
				}
				else if (descriptor.kind == ui::FieldKind::Text)
				{
					descriptor.textCheck = &ObjectTextCheck;
				}
			}
		}

		int HexDigit(unsigned char letter)
		{
			if (letter >= '0' && letter <= '9')
			{
				return letter - '0';
			}

			if (letter >= 'A' && letter <= 'F')
			{
				return letter - 'A' + 10;
			}

			if (letter >= 'a' && letter <= 'f')
			{
				return letter - 'a' + 10;
			}

			return -1;
		}

		bool ReadEightDigits(unsigned char const *bytes, uint32_t *value)
		{
			uint32_t word = 0;

			for (int i = 0; i < 8; i++)
			{
				int const digit = HexDigit(bytes[i]);

				if (digit < 0)
				{
					return false;
				}

				word = (word << 4) | (uint32_t)digit;
			}

			*value = word;
			return true;
		}

		// A whole number that an int32 holds, as an axis is
		bool IsAnAxisValue(double value)
		{
			return FitsAChoice(value) && value == (double)(int32_t)value;
		}
	}

	void MakeStrategyRecordEditable(FieldRecord *record, model::StrategyKind const &kind)
	{
		Open(record, false, kind.number, 0);
	}

	void MakeChunkRecordEditable(FieldRecord *record, model::ChunkKind const &kind)
	{
		Open(record, true, 0, kind.id);
	}

	bool BuildEditableStrategyRecord(uint8_t const *payload, size_t length, FieldRecord *record, char const **why)
	{
		if (!BuildStrategyRecord(payload, length, record, why))
		{
			return false;
		}

		// BuildStrategyRecord found at least 20 bytes and a kind it knows: the kind's number is the int32 at +8
		uint32_t const bits = (uint32_t)payload[8] | ((uint32_t)payload[9] << 8) | ((uint32_t)payload[10] << 16) | ((uint32_t)payload[11] << 24);
		int32_t number = 0;
		memcpy(&number, &bits, sizeof(number));
		model::StrategyKind const *const kind = model::FindStrategyKind(number);

		if (kind)
		{
			MakeStrategyRecordEditable(record, *kind);
		}

		return true;
	}

	bool BuildEditableChunkRecord(char const *chunk, uint8_t const *payload, size_t length, FieldRecord *record, char const **why)
	{
		if (!BuildChunkRecord(chunk, payload, length, record, why))
		{
			return false;
		}

		// BuildChunkRecord found the chunk's kind, which is why it gave a record
		model::ChunkKind const *const kind = chunk ? model::FindAnyChunkKind(chunk, 0) : 0;

		if (kind)
		{
			MakeChunkRecordEditable(record, *kind);
		}

		return true;
	}

	static ObjectNameResolver nameResolver = 0;

	void SetObjectNameResolver(ObjectNameResolver resolver)
	{
		nameResolver = resolver;
	}

	bool ParseObjectText(unsigned char const *bytes, size_t length, uint32_t *first, uint32_t *second)
	{
		if (!bytes || !first || !second)
		{
			return false;
		}

		if (length == 4 && (bytes[0] | 0x20) == 'n' && (bytes[1] | 0x20) == 'o' && (bytes[2] | 0x20) == 'n' && (bytes[3] | 0x20) == 'e')
		{
			*first = 0;
			*second = 0;
			return true;
		}

		uint32_t one = 0;
		uint32_t two = 0;

		if (length != 17 || bytes[8] != ':' || !ReadEightDigits(bytes, &one) || !ReadEightDigits(bytes + 9, &two))
		{
			// not the hex form: the name of an object, when the editor can look one up
			if (nameResolver && length > 0 && length <= 63)
			{
				char name[64];

				memcpy(name, bytes, length);
				name[length] = 0;
				return nameResolver(name, length, first, second);
			}

			return false;
		}

		*first = one;
		*second = two;
		return true;
	}

	PanelEditAnswer EditFromPanel(FieldRecord const &record, ui::FieldPanel const &panel, size_t field, uint32_t wordNow, FieldEdit *edit,
	                               char const **why)
	{
		if (!edit || field >= record.list.size() || field >= panel.Count())
		{
			return PanelEditAnswer::BadCall;
		}

		if (field >= record.sources.size())
		{
			return PanelEditAnswer::NotEditable;
		}

		FieldSource const &source = record.sources[field];

		if (record.list[field].readOnly || !CanOpen(source, record.list[field]))
		{
			return PanelEditAnswer::NotEditable;
		}

		FieldEdit made;
		made.field = source.field->name;

		switch (source.what)
		{
		case SourceKind::Number:
			made.what = EditWhat::Number;
			made.number = DisplayFactor(*source.field) != 1.0 ? StoredFromShown(*source.field, panel.NumberValue(field)) : panel.NumberValue(field);
			break;

		case SourceKind::Choice:
			made.what = EditWhat::Number;
			made.number = (double)panel.ChoiceValue(field);
			break;

		case SourceKind::Bit:
			made.what = EditWhat::Number;
			made.number = (double)(panel.BitChecked(field) ? (wordNow | source.bit) : (wordNow & ~source.bit));
			break;

		case SourceKind::Axis:
		{
			double const value = panel.NumberValue(field);

			if (!IsAnAxisValue(value))
			{
				if (why)
				{
					*why = NotAWholeNumberWords;
				}

				return PanelEditAnswer::ValueRefused;
			}

			made.what = EditWhat::Axis;
			made.axis = source.axis;
			made.axisValue = (int32_t)value;
			break;
		}

		case SourceKind::Object:
		{
			size_t length = 0;
			unsigned char const *const text = panel.TextValue(field, &length);

			if (!ParseObjectText(text, length, &made.first, &made.second))
			{
				if (why)
				{
					*why = NotAnObjectReason;
				}

				return PanelEditAnswer::ValueRefused;
			}

			made.what = EditWhat::Object;
			break;
		}

		default:
			return PanelEditAnswer::NotEditable;
		}

		*edit = made;
		return PanelEditAnswer::Made;
	}

	namespace
	{
		// What the edit's answer is as the caller is told it
		KeptAnswer Told(PanelEditAnswer answer)
		{
			switch (answer)
			{
			case PanelEditAnswer::Made: return KeptAnswer::Applied;
			case PanelEditAnswer::NotEditable: return KeptAnswer::NotEditable;
			case PanelEditAnswer::ValueRefused: return KeptAnswer::ValueRefused;
			default: return KeptAnswer::BadCall;
			}
		}

		// The flags word a bit's field holds now, from the number the model read
		bool AsWord(double number, uint32_t *word)
		{
			if (IsNaNOrEndless(number) || number < 0 || number > 4294967295.0)
			{
				return false;
			}

			*word = (uint32_t)number;
			return true;
		}
	}

	uint32_t InteingFlagRules(int32_t kind, char const *field, uint32_t bit, uint32_t newWord)
	{
		if (kind == 50 && field && strcmp(field, "DoorFlags") == 0 && (bit == 0x2 || bit == 0x8))
		{
			uint32_t const other = bit == 0x2 ? 0x8u : 0x2u;
			bool const on = (newWord & bit) != 0;

			// on: the other goes off; off: the other comes on
			return on ? (newWord & ~other) : (newWord | other);
		}

		return newWord;
	}

	KeptAnswer ApplyKeptStrategyField(PayloadStore *store, Chunk *chunk, model::StrategyKind const &kind, FieldRecord const &record,
	                                  ui::FieldPanel const &panel, size_t field, EditAnswer *editAnswer, char const **why)
	{
		if (!store || !chunk || field >= record.list.size())
		{
			return KeptAnswer::BadCall;
		}

		if (field >= record.sources.size())
		{
			return KeptAnswer::NotEditable;
		}

		FieldSource const &source = record.sources[field];

		if (!source.field || source.strategyKind != kind.number)
		{
			return KeptAnswer::NotEditable;
		}

		std::vector<unsigned char> bytes;
		uint32_t wordNow = 0;

		if (!store->PayloadOf(chunk, &bytes))
		{
			return KeptAnswer::NoPayload;
		}

		if (source.what == SourceKind::Bit)
		{
			double number = 0;

			if (!model::ReadField(kind, *source.field, bytes.data(), bytes.size(), &number, 0) || !AsWord(number, &wordNow))
			{
				return KeptAnswer::NoPayload;
			}
		}

		FieldEdit edit;
		PanelEditAnswer const made = EditFromPanel(record, panel, field, wordNow, &edit, why);

		if (made != PanelEditAnswer::Made)
		{
			return Told(made);
		}

		// a flag that depends on another one (inteng's click): the word is corrected as inteng leaves it
		if (source.what == SourceKind::Bit && edit.what == EditWhat::Number && edit.number >= 0 && edit.number <= 4294967295.0)
		{
			edit.number = (double)InteingFlagRules(kind.number, source.field->name, source.bit, (uint32_t)edit.number);
		}

		EditAnswer const answer = ApplyStrategyPayloadEdit(store, chunk, kind.number, edit, why);

		if (editAnswer)
		{
			*editAnswer = answer;
		}

		return answer == EditAnswer::Applied ? KeptAnswer::Applied : KeptAnswer::EditRefused;
	}

	KeptAnswer ApplyKeptChunkField(PayloadStore *store, Chunk *chunk, model::ChunkKind const &kind, FieldRecord const &record,
	                               ui::FieldPanel const &panel, size_t field, EditAnswer *editAnswer, char const **why)
	{
		if (!store || !chunk || field >= record.list.size())
		{
			return KeptAnswer::BadCall;
		}

		if (field >= record.sources.size())
		{
			return KeptAnswer::NotEditable;
		}

		FieldSource const &source = record.sources[field];

		if (!source.field || memcmp(source.chunk, kind.id, 8) != 0)
		{
			return KeptAnswer::NotEditable;
		}

		std::vector<unsigned char> bytes;
		uint32_t wordNow = 0;

		if (!store->PayloadOf(chunk, &bytes))
		{
			return KeptAnswer::NoPayload;
		}

		if (source.what == SourceKind::Bit)
		{
			double number = 0;

			if (!model::ReadChunkField(kind, *source.field, bytes.data(), bytes.size(), &number, 0) || !AsWord(number, &wordNow))
			{
				return KeptAnswer::NoPayload;
			}
		}

		FieldEdit edit;
		PanelEditAnswer const made = EditFromPanel(record, panel, field, wordNow, &edit, why);

		if (made != PanelEditAnswer::Made)
		{
			return Told(made);
		}

		EditAnswer const answer = ApplyChunkEdit(store, chunk, kind, edit, why);

		if (editAnswer)
		{
			*editAnswer = answer;
		}

		return answer == EditAnswer::Applied ? KeptAnswer::Applied : KeptAnswer::EditRefused;
	}
}

namespace host
{
	// inteng's light mode: the Shadows, Runtime and No PreLight items and the engine's bits are changed in the light's master record by FUN_004acf30 (out\lightmode.txt:68-206).
	// Turning No PreLight (local 0x40) ON also sets 0x304 in the engine word, and the light save (FUN_004ad630) writes the engine word without 0x300: what is saved
	// is the engine word with the Omni bit (0x4) set. Turning it OFF clears only 0x300, which the save never writes: nothing saved changes. This does the same after the panel
	// has changed that bit of a STDLIGHT: the engine word gets 0x4. (Added 2026-11-15 by Melanikus (Drake), Bishop's audit: F-LIGHT-COUPLING of frost-deviations.md copied.)
	ApplyLightCouplingAnswer ApplyLightCoupling(PayloadStore *store, Chunk *chunk, model::ChunkKind const &kind, FieldRecord const &record, size_t field)
	{
		if (!store || !chunk || field >= record.sources.size() || memcmp(kind.id, "STDLIGHT", 8) != 0)
		{
			return ApplyLightCouplingAnswer::NotCoupled;
		}

		FieldSource const &source = record.sources[field];

		if (!source.field || source.what != SourceKind::Bit || source.bit != 0x40 || strcmp(source.field->name, "local_light_flags") != 0)
		{
			return ApplyLightCouplingAnswer::NotCoupled;
		}

		model::Field const *engine = 0;
		model::Field const *local = 0;

		for (size_t i = 0; i < kind.fieldCount; i++)
		{
			if (strcmp(kind.fields[i].name, "engine_light_flags") == 0)
			{
				engine = &kind.fields[i];
			}
			else if (strcmp(kind.fields[i].name, "local_light_flags") == 0)
			{
				local = &kind.fields[i];
			}
		}

		std::vector<unsigned char> bytes;
		double localNow = 0;
		double engineNow = 0;

		if (!engine || !local || !store->PayloadOf(chunk, &bytes) ||
			!model::ReadChunkField(kind, *local, bytes.data(), bytes.size(), &localNow, 0) ||
			!model::ReadChunkField(kind, *engine, bytes.data(), bytes.size(), &engineNow, 0))
		{
			return ApplyLightCouplingAnswer::Refused;
		}

		uint32_t localWord = 0;
		uint32_t engineWord = 0;

		if (!AsWord(localNow, &localWord) || !AsWord(engineNow, &engineWord))
		{
			return ApplyLightCouplingAnswer::Refused;
		}

		if ((localWord & 0x40) == 0 || (engineWord & 0x4) != 0)
		{
			return ApplyLightCouplingAnswer::NotCoupled;   // turned off, or the Omni bit is on already: nothing more is saved
		}

		FieldEdit edit;
		edit.field = engine->name;
		edit.what = EditWhat::Number;
		edit.number = (double)(engineWord | 0x4);
		char const *why = 0;
		return ApplyChunkEdit(store, chunk, kind, edit, &why) == EditAnswer::Applied ? ApplyLightCouplingAnswer::Applied : ApplyLightCouplingAnswer::Refused;
	}
}
