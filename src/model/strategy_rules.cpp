// The rules for strategies (the AVPSTRAT chunk): how big each one has to be, which values a field may take, the bits that are always
// set, the upgrades of old files, and how a new one starts. Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// A level can come from anyone, so everything here reads a payload as hostile: every read is measured against the payload's length
// first, and a count from the file is never trusted to size anything until it's been checked against the bytes that are really there.

#include "field_bytes.h"
#include "field_model.h"

// IsNaNOrEndless: the editor's one test for "not a real number", by the bits, since the editor is built with the game's /fp:fast.
#include "real_numbers.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

namespace model
{
	// The byte reads and writes and the setters at a known place are shared with the other chunks' rules (field_bytes.h).
	using namespace detail;

	namespace detail
	{
		SizeCheck ReadCount(uint8_t const *payload, size_t length, size_t at, size_t item_bytes, size_t *count)
		{
			if (at > length || length - at < 4)
			{
				return SizeCheck::CountCutOff;
			}

			// A list whose items take no bytes can't be measured against the bytes left. No table has one; it's refused all the same.
			if (item_bytes == 0)
			{
				return SizeCheck::CountTooBig;
			}

			// Compared as 64-bit signed numbers, so nothing wraps around before the count has been measured.
			int64_t const stored = LoadInt32(payload + at);
			int64_t const most = (int64_t)((length - at - 4) / item_bytes);

			if (stored < 0)
			{
				return SizeCheck::CountNegative;
			}

			if (stored > most)
			{
				return SizeCheck::CountTooBig;
			}

			*count = (size_t)stored;
			return SizeCheck::Right;
		}
	}

	namespace
	{
		// The size every strategy has before its own fields: the chunk's index and spare2, then the kind, Type and ExtraData.
		size_t const SmallestStrategy = 20;

		// The game's bits that the editor always sets when it saves (strategy_chunk.hpp).
		uint32_t const SwitchUpdated = 0x80000000u;        // MultiSwitchFlag_SwitchUpdated: targets' requests are stored one by one
		uint32_t const SparesHoldValues = 0x80000000u;     // SimpleStrategy_SparesDontContainJunk: the words after integrity are real

		// Where a kind's first list ends: the list whose count is at a fixed place, followed by its items.
		bool FirstListEnd(StrategyKind const &kind, uint8_t const *payload, size_t length, size_t *end)
		{
			size_t at = 0;
			size_t item_bytes = 0;

			switch (kind.size)
			{
			case SizeRule::Lift: at = 20; item_bytes = 8; break;               // the linked lifts
			case SizeRule::MultiSwitch: at = 36; item_bytes = 12; break;       // the targets
			case SizeRule::MissionObjective: at = 40; item_bytes = 12; break;  // the objectives it alters
			default: return false;
			}

			size_t count = 0;

			if (ReadCount(payload, length, at, item_bytes, &count) != SizeCheck::Right)
			{
				return false;
			}

			*end = at + 4 + count * item_bytes;
			return true;
		}

		Field const *FindField(StrategyKind const &kind, char const *name)
		{
			for (size_t i = 0; i < kind.fieldCount; i++)
			{
				if (strcmp(kind.fields[i].name, name) == 0)
				{
					return &kind.fields[i];
				}
			}

			return 0;
		}

		bool IsWholeNumberStorage(Storage storage)
		{
			return storage != Storage::Float32 && storage != Storage::Float64;
		}

		// Writes a field's value at its place. The value has already been checked to fit. A masked field changes only its own bits.
		// One number can't describe a field of three numbers, an object or an orientation, so those are refused here (false); the
		// first two have their own setters (SetAxisAt, SetObjectAt), and an orientation is set by turning the thing it belongs to.
		bool StoreField(Field const &field, uint8_t *at, double value)
		{
			switch (field.storage)
			{
			case Storage::Byte:
				at[0] = (uint8_t)value;
				break;

			case Storage::Int16:
			case Storage::UInt16:
				{
					uint16_t bits;

					if (field.storage == Storage::Int16)
					{
						int16_t const signedValue = (int16_t)value;
						memcpy(&bits, &signedValue, sizeof(bits));
					}
					else
					{
						bits = (uint16_t)value;
					}

					at[0] = (uint8_t)(bits & 0xFF);
					at[1] = (uint8_t)(bits >> 8);
				}
				break;

			case Storage::UInt32:
				if (field.mask)
				{
					uint32_t const kept = LoadUInt32(at) & ~field.mask;
					StoreUInt32(at, kept | (((uint32_t)value << field.shift) & field.mask));
				}
				else
				{
					StoreUInt32(at, (uint32_t)value);
				}
				break;

			case Storage::IntMatrix:
			case Storage::ObjectId:
			case Storage::IntTriple:
				return false;

			case Storage::Float32:
				{
					float const single = (float)value;
					uint32_t bits;
					memcpy(&bits, &single, sizeof(bits));
					StoreUInt32(at, bits);
				}
				break;

			case Storage::Float64:
				{
					uint64_t bits;
					memcpy(&bits, &value, sizeof(bits));
					StoreUInt32(at, (uint32_t)(bits & 0xFFFFFFFFu));
					StoreUInt32(at + 4, (uint32_t)(bits >> 32));
				}
				break;

			default:
				// a plain number with a shift (a fan's times: whole seconds, kept in 16.16) is stored shifted up
				StoreInt32(at, field.shift && !field.mask ? (int32_t)((int64_t)value << field.shift) : (int32_t)value);
				break;
			}

			return true;
		}

		// Reads a field's number from its place, as the file holds it: the inverse of StoreField, one byte at a time and little-endian.
		// A masked whole number gives only its masked bits, not shifted (the way StoreField puts them back). Nothing is fixed, checked or
		// named here: a fraction may be "not a number", and a choice may be one that nobody named, for the panel to show and flag. One
		// number can't describe a field of three numbers, an object or an orientation, so those are refused (false), with *value left
		// alone; the first two have their own readers (ReadFieldAxis, ReadFieldObject). The place has already been checked to fit.
		// (Added 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng.)
		bool LoadFieldNumber(Field const &field, uint8_t const *at, double *value)
		{
			switch (field.storage)
			{
			case Storage::Byte:
				*value = at[0];
				return true;

			case Storage::Int16:
				{
					uint16_t const bits = (uint16_t)(at[0] | (at[1] << 8));
					int16_t signedValue;
					memcpy(&signedValue, &bits, sizeof(signedValue));
					*value = signedValue;
				}
				return true;

			case Storage::UInt16:
				*value = (uint16_t)(at[0] | (at[1] << 8));
				return true;

			case Storage::UInt32:
				{
					uint32_t const word = LoadUInt32(at);
					*value = (double)(field.mask ? ((word & field.mask) >> field.shift) : word);
				}
				return true;

			case Storage::IntMatrix:
			case Storage::ObjectId:
			case Storage::IntTriple:
				return false;

			case Storage::Float32:
				{
					uint32_t const bits = LoadUInt32(at);
					float single;
					memcpy(&single, &bits, sizeof(single));
					*value = single;
				}
				return true;

			case Storage::Float64:
				{
					uint64_t const bits = (uint64_t)LoadUInt32(at) | ((uint64_t)LoadUInt32(at + 4) << 32);
					memcpy(value, &bits, sizeof(*value));
				}
				return true;

			default:
				// a plain number with a shift (a fan's times, 16.16): the word / 2^shift ROUNDED TOWARD 0, as inteng's view divides (S-K75-1, 0x5356b0-0x53575a); a shift would round a negative word down
				*value = field.shift && !field.mask && field.shift < 31 ? (double)(LoadInt32(at) / (int32_t)(1 << field.shift)) : (double)LoadInt32(at);
				return true;
			}
		}

		// Whether the field's storage fits in the payload at that place. Worked out without adding, so a huge place can't wrap.
		bool FitsAt(Field const &field, size_t length, size_t at)
		{
			return at <= length && length - at >= StorageBytes(field.storage);
		}
	}

	namespace detail
	{
		bool SetFieldAt(Field const &field, uint8_t *payload, size_t length, size_t at, double value, char const **why)
		{
			// The value is checked first, so its reason is given even when the place is wrong too.
			if (!ValueIsAllowed(field, value, why))
			{
				return false;
			}

			// A list's count would no longer match its items: it changes only with them.
			if (field.unit == Unit::ListCount)
			{
				if (why)
				{
					*why = ListCountReason;
				}

				return false;
			}

			if (!FitsAt(field, length, at))
			{
				return false;
			}

			// StoreField refuses a field of three numbers or an object: one number can't set those (SetAxisAt and SetObjectAt do).
			return StoreField(field, payload + at, value);
		}

		bool SetAxisAt(Field const &field, uint8_t *payload, size_t length, size_t at, int axis, int32_t value, char const **why)
		{
			if (field.storage != Storage::IntTriple || axis < 0 || axis > 2)
			{
				return false;
			}

			if (!ValueIsAllowed(field, value, why) || !FitsAt(field, length, at))
			{
				return false;
			}

			StoreInt32(payload + at + 4 * (size_t)axis, value);
			return true;
		}

		bool SetObjectAt(Field const &field, uint8_t *payload, size_t length, size_t at, uint32_t first, uint32_t second)
		{
			if (field.storage != Storage::ObjectId || !FitsAt(field, length, at))
			{
				return false;
			}

			StoreUInt32(payload + at, first);
			StoreUInt32(payload + at + 4, second);
			return true;
		}

		bool ReadFieldAt(Field const &field, uint8_t const *payload, size_t length, size_t at, double *value)
		{
			if (!FitsAt(field, length, at) || !payload || !value)
			{
				return false;
			}

			return LoadFieldNumber(field, payload + at, value);
		}

		bool ReadAxisAt(Field const &field, uint8_t const *payload, size_t length, size_t at, int axis, int32_t *value)
		{
			if ((unsigned)axis > 2 || field.storage != Storage::IntTriple || !FitsAt(field, length, at) || !payload || !value)
			{
				return false;
			}

			*value = LoadInt32(payload + at + 4 * (size_t)axis);
			return true;
		}

		bool ReadObjectAt(Field const &field, uint8_t const *payload, size_t length, size_t at, uint32_t *first, uint32_t *second)
		{
			if (field.storage != Storage::ObjectId || !FitsAt(field, length, at) || !payload || !first || !second)
			{
				return false;
			}

			*first = LoadUInt32(payload + at);
			*second = LoadUInt32(payload + at + 4);
			return true;
		}

		bool WriteCreationAt(Field const &field, uint8_t *payload, size_t length, size_t at)
		{
			if (!FitsAt(field, length, at))
			{
				return false;
			}

			if (field.storage == Storage::IntTriple)
			{
				// A field of three numbers starts with each of them at its creation value.
				for (size_t axis = 0; axis < 3; axis++)
				{
					StoreInt32(payload + at + 4 * axis, (int32_t)field.creation);
				}
			}
			else if (field.storage == Storage::ObjectId)
			{
				// A new chunk's object fields name no object (the test checks every table says 0 for them).
				StoreUInt32(payload + at, 0);
				StoreUInt32(payload + at + 4, 0);
			}
			else
			{
				// StoreField refuses an orientation, which has no single creation value (the tables give it none).
				return StoreField(field, payload + at, field.creation);
			}

			return true;
		}
	}

	unsigned StorageBytes(Storage storage)
	{
		switch (storage)
		{
		case Storage::Byte: return 1;
		case Storage::Int16: case Storage::UInt16: return 2;
		case Storage::ObjectId: case Storage::Float64: return 8;
		case Storage::IntTriple: return 12;
		case Storage::IntMatrix: return 36;
		default: return 4;
		}
	}

	void StorageRange(Storage storage, double *lowest, double *highest)
	{
		switch (storage)
		{
		case Storage::Byte: *lowest = 0; *highest = UCHAR_MAX; break;
		case Storage::Int16: *lowest = SHRT_MIN; *highest = SHRT_MAX; break;
		case Storage::UInt16: *lowest = 0; *highest = USHRT_MAX; break;
		case Storage::UInt32: *lowest = 0; *highest = UINT_MAX; break;
		case Storage::ObjectId: *lowest = 0; *highest = UINT_MAX; break;   // each of its two halves
		case Storage::Float32: *lowest = -FLT_MAX; *highest = FLT_MAX; break;
		case Storage::Float64: *lowest = -DBL_MAX; *highest = DBL_MAX; break;
		default: *lowest = INT_MIN; *highest = INT_MAX; break;             // Int32, and each number of an IntTriple or IntMatrix
		}
	}

	SizeCheck CheckStrategySize(uint8_t const *payload, size_t length, size_t *expected)
	{
		// The game refuses a strategy too short to hold the numbers every one has (strategy_chunk.cpp, AVP_Strategy_Chunk).
		if (length < SmallestStrategy)
		{
			return SizeCheck::TooShort;
		}

		StrategyKind const *const kind = FindStrategyKind(LoadInt32(payload + 8));

		// A kind the game doesn't read field by field is kept as a block of whatever size it has.
		if (!kind || kind->size == SizeRule::AnySize)
		{
			if (expected)
			{
				*expected = length;
			}

			return SizeCheck::Right;
		}

		size_t size = kind->fixedBytes;
		size_t first = 0;
		size_t second = 0;
		SizeCheck check = SizeCheck::Right;

		switch (kind->size)
		{
		case SizeRule::Fixed:
			break;

		case SizeRule::Lift:
			// The linked lifts (8 bytes each), then the door, the two switches, the flags and the floor (32 bytes), then the lifts
			// elsewhere (28 bytes each), then the facing.
			check = ReadCount(payload, length, 20, 8, &first);

			if (check == SizeCheck::Right)
			{
				check = ReadCount(payload, length, 24 + first * 8 + 32, 28, &second);
			}

			size += first * 8 + second * 28;
			break;

		case SizeRule::LinkSwitch:
			check = ReadCount(payload, length, 48, 8, &first);
			size += first * 8;
			break;

		case SizeRule::MultiSwitch:
			// The targets (an object and its request, 12 bytes each), then the linked switches (8 bytes each).
			check = ReadCount(payload, length, 36, 12, &first);

			if (check == SizeCheck::Right)
			{
				check = ReadCount(payload, length, 40 + first * 12, 8, &second);
			}

			size += first * 12 + second * 8;
			break;

		case SizeRule::MissionObjective:
			// The objectives it alters (12 bytes each), then the request targets (12 bytes each).
			check = ReadCount(payload, length, 40, 12, &first);

			if (check == SizeCheck::Right)
			{
				check = ReadCount(payload, length, 44 + first * 12, 12, &second);
			}

			size += first * 12 + second * 12;
			break;

		case SizeRule::PointEffects:
			{
				// Each point effect is its point's number, a pad and a count, then that many targets of 16 bytes (an object, its
				// request and when it fires), read as the game's ReadPointEffects does.
				size_t effects = 0;
				check = ReadCount(payload, length, 20, 12, &effects);

				size_t at = 24;

				for (size_t i = 0; check == SizeCheck::Right && i < effects; i++)
				{
					size_t targets = 0;

					// The count is the effect's third number, so reading it also proves the effect's whole 12 bytes are there.
					check = ReadCount(payload, length, at + 8, 16, &targets);
					at += 12 + targets * 16;
					size += 12 + targets * 16;
				}
			}
			break;

		default:
			return SizeCheck::WrongSize;
		}

		// A count that couldn't be read is the reason; no size is worked out from it.
		if (check != SizeCheck::Right)
		{
			return check;
		}

		if (expected)
		{
			*expected = size;
		}

		return size == length ? SizeCheck::Right : SizeCheck::WrongSize;
	}

	char const *SizeCheckWords(SizeCheck check)
	{
		switch (check)
		{
		case SizeCheck::Right: return "the strategy is the size its kind and its lists add up to";
		case SizeCheck::TooShort: return "the strategy is shorter than the 20 bytes every strategy has";
		case SizeCheck::CountCutOff: return "the strategy ends in the middle of one of its lists' counts";
		case SizeCheck::CountNegative: return "one of the strategy's lists has a count below zero";
		case SizeCheck::CountTooBig: return "one of the strategy's lists counts more items than the strategy holds";
		case SizeCheck::WrongSize: return "the strategy isn't the size its kind and its lists add up to";
		case SizeCheck::NameCutOff: return "a name in the strategy has no ending zero inside it";
		}

		return "the strategy's size can't be checked";
	}

	bool StrategySizeIsRight(uint8_t const *payload, size_t length)
	{
		return CheckStrategySize(payload, length, 0) == SizeCheck::Right;
	}

	bool FieldPlace(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, size_t *place)
	{
		size_t at = 0;

		switch (field.anchor)
		{
		case Anchor::Start:
			at = field.offset;
			break;

		case Anchor::AfterFirstList:
			{
				size_t end = 0;

				if (!FirstListEnd(kind, payload, length, &end))
				{
					return false;
				}

				at = end + field.offset;
			}
			break;

		case Anchor::End:
			if (field.offset > length)
			{
				return false;
			}
			at = length - field.offset;
			break;
		}

		if (at > length || length - at < StorageBytes(field.storage))
		{
			return false;
		}

		*place = at;
		return true;
	}

	char const NotANumberReason[] = "a value has to be a real number";
	char const NotWholeReason[] = "this value is a whole number";
	char const DoesNotFitReason[] = "the file stores this value in fewer bits than that needs";
	char const ListCountReason[] = "a list's count changes only when its items are added or taken away";
	char const UnnamedChoiceReason[] = "this value isn't one of the field's named choices";
	char const NotThisKindsReason[] = "this field isn't one this kind of strategy has, or it's the kind's own number";

	bool ValueIsAllowed(Field const &field, double value, char const **why)
	{
		char const *reason = 0;
		double lowest = 0;
		double highest = 0;
		StorageRange(field.storage, &lowest, &highest);

		if (IsNaNOrEndless(value))
		{
			reason = NotANumberReason;
		}
		else if (IsWholeNumberStorage(field.storage) && value != floor(value))
		{
			reason = NotWholeReason;
		}
		else if (value < lowest || value > highest)
		{
			reason = DoesNotFitReason;
		}
		// The value is known to be within its storage here. Every masked field is stored unsigned (UInt32, which the test checks), so
		// value isn't negative, and the conversion below is defined.
		else if (field.mask && (((uint32_t)value << field.shift) & ~field.mask) != 0)
		{
			reason = DoesNotFitReason;
		}
		else if (field.limit != Limit::None && (value < field.lowest || value > field.highest))
		{
			reason = field.why;
		}
		else if (field.step && fmod(value, (double)field.step) != 0.0)
		{
			reason = field.why && field.why[0] ? field.why : NotWholeReason;
		}

		if (reason && why)
		{
			*why = reason;
		}

		return reason == 0;
	}

	bool UpgradeOldStrategy(uint8_t *payload, size_t length)
	{
		if (!StrategySizeIsRight(payload, length))
		{
			return false;
		}

		int32_t const number = LoadInt32(payload + 8);
		StrategyKind const *const kind = FindStrategyKind(number);

		if (!kind)
		{
			return false;
		}

		// A many-target switch from before its targets' requests were stored one by one. Then pad1 said how many of the first
		// targets get "off" (0) and the rest got "on" (1). The game's own reader turns it into the new form (strategy_chunk.cpp,
		// MultiSwitchStrategy), and so did the old editor; this does the same, so the level reads the same in all three.
		if (number == 66 || number == 68)
		{
			Field const *const flags = FindField(*kind, "flags");
			Field const *const pad1 = FindField(*kind, "pad1");
			size_t flagsAt = 0;
			size_t pad1At = 0;
			size_t targets = 0;

			if (!flags || !pad1 || !FieldPlace(*kind, *flags, payload, length, &flagsAt) ||
			    !FieldPlace(*kind, *pad1, payload, length, &pad1At) ||
			    ReadCount(payload, length, 36, 12, &targets) != SizeCheck::Right)
			{
				return false;
			}

			uint32_t const flagBits = LoadUInt32(payload + flagsAt);

			if (flagBits & SwitchUpdated)
			{
				return false;
			}

			int32_t const firstOn = LoadInt32(payload + pad1At);

			for (size_t i = 0; i < targets; i++)
			{
				// i is below the payload's length, so it fits an int; the comparison is the game's own, signed.
				int32_t const request = ((int32_t)i >= firstOn) ? 1 : 0;
				StoreInt32(payload + 40 + i * 12 + 8, request);
			}

			StoreInt32(payload + pad1At, 0);
			StoreUInt32(payload + flagsAt, flagBits | SwitchUpdated);
			return true;
		}

		// A placed object from before the words after its integrity held real values. The game's own reader clears them, as junk
		// (strategy_chunk.cpp, SimpleStrategy), and so did the old editor; this does the same.
		if (number == 62)
		{
			uint32_t const integrity = LoadUInt32(payload + 32);

			if (integrity & SparesHoldValues)
			{
				return false;
			}

			StoreUInt32(payload + 32, integrity | SparesHoldValues);
			StoreUInt32(payload + 36, 0);   // flags
			StoreUInt32(payload + 40, 0);   // target_request
			StoreUInt32(payload + 44, 0);   // targetID, its two halves
			StoreUInt32(payload + 48, 0);
			return true;
		}

		return false;
	}

	bool SetAlwaysSetStrategyBits(uint8_t *payload, size_t length)
	{
		if (!StrategySizeIsRight(payload, length))
		{
			return false;
		}

		int32_t const number = LoadInt32(payload + 8);
		StrategyKind const *const kind = FindStrategyKind(number);

		if (!kind)
		{
			return false;
		}

		size_t at = 0;
		uint32_t bit = 0;

		if (number == 66 || number == 68)
		{
			Field const *const flags = FindField(*kind, "flags");

			if (!flags || !FieldPlace(*kind, *flags, payload, length, &at))
			{
				return false;
			}

			bit = SwitchUpdated;
		}
		else if (number == 62)
		{
			at = 32;
			bit = SparesHoldValues;
		}
		else
		{
			return false;
		}

		uint32_t const before = LoadUInt32(payload + at);

		if (before & bit)
		{
			return false;
		}

		StoreUInt32(payload + at, before | bit);
		return true;
	}

	// A place no payload has, for a field that has none in this one: the setters at a place still check the value first, then refuse.
	static size_t const NoPlace = (size_t)-1;

	namespace
	{
		// Whether a field may be set in a strategy of this kind: one of the kind's own, or one every strategy has, but not the kind's
		// own number, which would make the payload another kind's. A field of another kind would be written at its own offset here.
		bool IsFieldOfStrategy(StrategyKind const &kind, Field const &field)
		{
			for (size_t i = 0; i < kind.fieldCount; i++)
			{
				if (&kind.fields[i] == &field)
				{
					return true;
				}
			}

			for (size_t i = 0; i < StrategyBaseFieldCount; i++)
			{
				if (&StrategyBaseFields[i] == &field)
				{
					return strcmp(field.name, "StrategyType") != 0;
				}
			}

			return false;
		}

		bool RefuseFieldOfAnotherKind(char const **why)
		{
			if (why)
			{
				*why = NotThisKindsReason;
			}

			return false;
		}
	}

	bool SetField(StrategyKind const &kind, Field const &field, uint8_t *payload, size_t length, double value, char const **why)
	{
		if (!IsFieldOfStrategy(kind, field))
		{
			return RefuseFieldOfAnotherKind(why);
		}

		// A choice field takes only the values its choices name. A value the field can't hold at all gets that reason first.
		if (field.unit == Unit::Choice && ValueIsAllowed(field, value, 0) && !ChoiceIsNamed(kind.number, field.name, (int32_t)value))
		{
			if (why)
			{
				*why = UnnamedChoiceReason;
			}

			return false;
		}

		size_t at = 0;

		if (!FieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return SetFieldAt(field, payload, length, at, value, why);
	}

	bool SetFieldAxis(StrategyKind const &kind, Field const &field, uint8_t *payload, size_t length, int axis, int32_t value,
	                  char const **why)
	{
		if (!IsFieldOfStrategy(kind, field))
		{
			return RefuseFieldOfAnotherKind(why);
		}

		size_t at = 0;

		if (!FieldPlace(kind, field, payload, length, &at))
		{
			at = NoPlace;
		}

		return SetAxisAt(field, payload, length, at, axis, value, why);
	}

	bool SetFieldObject(StrategyKind const &kind, Field const &field, uint8_t *payload, size_t length, uint32_t first,
	                    uint32_t second)
	{
		size_t at = 0;

		if (!IsFieldOfStrategy(kind, field) || !FieldPlace(kind, field, payload, length, &at))
		{
			return false;
		}

		return SetObjectAt(field, payload, length, at, first, second);
	}

	// The readers: the setters' inverses, held to the same rules before a byte is read. The field has to be one of the kind's own, or
	// one every strategy has but not the kind's own number (IsFieldOfStrategy), and it has to fit in the payload at its place
	// (FieldPlace, which measures a list's count against the bytes there before it trusts it). A payload or a place to put the value
	// that isn't there is refused too. On any refusal the value is left alone, and *why is set only for a field of another kind, as
	// SetField sets it. What's read is the level's, so it's hostile, and it's given back exactly as the file holds it. (Added
	// 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng.)
	bool ReadField(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, double *value, char const **why)
	{
		if (!IsFieldOfStrategy(kind, field))
		{
			return RefuseFieldOfAnotherKind(why);
		}

		size_t at = 0;

		if (!payload || !value || !FieldPlace(kind, field, payload, length, &at))
		{
			return false;
		}

		return LoadFieldNumber(field, payload + at, value);
	}

	bool ReadFieldAxis(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, int axis, int32_t *value,
	                   char const **why)
	{
		if (!IsFieldOfStrategy(kind, field))
		{
			return RefuseFieldOfAnotherKind(why);
		}

		size_t at = 0;

		if (!payload || !value || field.storage != Storage::IntTriple || axis < 0 || axis > 2 ||
		    !FieldPlace(kind, field, payload, length, &at))
		{
			return false;
		}

		*value = LoadInt32(payload + at + 4 * (size_t)axis);
		return true;
	}

	bool ReadFieldObject(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, uint32_t *first,
	                     uint32_t *second)
	{
		size_t at = 0;

		if (!payload || !first || !second || !IsFieldOfStrategy(kind, field) || field.storage != Storage::ObjectId ||
		    !FieldPlace(kind, field, payload, length, &at))
		{
			return false;
		}

		*first = LoadUInt32(payload + at);
		*second = LoadUInt32(payload + at + 4);
		return true;
	}

	bool MakeNewStrategy(int32_t number, uint8_t *buffer, size_t capacity, size_t *length)
	{
		StrategyKind const *const asked = FindStrategyKind(number);

		if (!asked || !asked->offered)
		{
			return false;
		}

		// Airlock and LiftNoTel were made as a Lift, with the flag that makes it one (the old editor's own way).
		StrategyKind const *const kind = FindStrategyKind(asked->writtenAs);
		uint32_t liftFlag = 0;

		if (number == 57)
		{
			liftFlag = 0x2;
		}
		else if (number == 60)
		{
			liftFlag = 0x4;
		}

		if (!kind || kind->size == SizeRule::AnySize || kind->fixedBytes > capacity)
		{
			return false;
		}

		size_t const size = kind->fixedBytes;   // every list is empty
		memset(buffer, 0, size);

		for (size_t i = 0; i < StrategyBaseFieldCount; i++)
		{
			Field const &field = StrategyBaseFields[i];

			if (field.hasCreation)
			{
				StoreField(field, buffer + field.offset, field.creation);
			}
		}

		StoreInt32(buffer + 8, kind->number);

		for (size_t i = 0; i < kind->fieldCount; i++)
		{
			Field const &field = kind->fields[i];
			size_t at = 0;

			if (!field.hasCreation || !FieldPlace(*kind, field, buffer, size, &at))
			{
				continue;
			}

			WriteCreationAt(field, buffer, size, at);
		}

		if (liftFlag)
		{
			Field const *const flags = FindField(*kind, "LiftFlags");
			size_t at = 0;

			if (!flags || !FieldPlace(*kind, *flags, buffer, size, &at))
			{
				return false;
			}

			StoreUInt32(buffer + at, LoadUInt32(buffer + at) | liftFlag);
		}

		SetAlwaysSetStrategyBits(buffer, size);
		*length = size;
		return true;
	}

	// Copy Strategy's copy (the old editor's FUN_00528340 calls the source kind's method +0xc for each target; reassess/strategy-data-kinds.md
	// S-KC-7 and the S-K<kind>-C rows, close-out F-2). Each kind's copy makes a new strategy on the target and copies some of the source:
	//   50 Door (FUN_0052cfb0), 62 Placed Object (FUN_00530e70) and 64 Lighting (FUN_0052f080 -> FUN_0052f0f0): every field after the first
	//      20 bytes; the head's Type and ExtraData (payload +12, +16) are left 0 by the new strategy's constructor (FUN_0052b120).
	//   52 Binary Switch (FUN_0052d630 -> FUN_0052d6a0): the seven words after the head, and the head's Type and ExtraData: all of it.
	//   61 Linked Switch: the old editor's 61 has 52's copy, which makes the copy a KIND 52 and leaves its links behind. INTENG BUG, FIXED: the
	//      copy stays a 61 (52's words, Type and ExtraData), with no links (NumLinks 0), as 66 and 68 copy theirs.
	//   66 Multi Switch (FUN_005306d0 -> FUN_00530740) and 68 Area Switch (FUN_00530a40 -> FUN_00530ab0): every field, the targets and the
	//      head's Type and ExtraData; the LINKS are emptied (NumLinks 0, no link IDs). A switch's links are the switches that target it, made
	//      again when its view opens and when the level is saved (S-K61-3).
	// The payload's first two words (the chunk's index and spare2) are not in the old editor's strategy record; the copy keeps the source's
	// (every strategy of the 38 stock levels has 0 in both).
	bool CopyStrategyAsTheOldEditor(uint8_t const *source, size_t sourceLength, uint8_t *buffer, size_t capacity, size_t *length)
	{
		if (!source || !buffer || !length || sourceLength < 20 || !StrategySizeIsRight(source, sourceLength))
		{
			return false;
		}

		int32_t const number = LoadInt32(source + 8);
		size_t keep = sourceLength;    // the source's bytes copied as they are, from the start
		size_t tail = sourceLength;    // and from here to the end, after a NumLinks of 0 (66 and 68)
		bool zeroHead = false;
		bool emptyLinks = false;

		switch (number)
		{
		case 50:
		case 62:
		case 64:
			zeroHead = true;
			break;

		case 52:
			break;

		case 61:
			keep = 48;   // NumLinks is at 48, the links after it
			tail = sourceLength;
			emptyLinks = true;
			break;

		case 66:
		case 68:
			{
				// the targets' count at 36 (12 bytes each), then NumLinks and the links (8 bytes each), then the rest; the size check above has
				// measured both lists against the payload
				int32_t const targets = LoadInt32(source + 36);
				size_t const linksAt = 40 + (size_t)targets * 12;
				int32_t const links = LoadInt32(source + linksAt);

				keep = linksAt;
				tail = linksAt + 4 + (size_t)links * 8;
				emptyLinks = true;
			}
			break;

		default:
			return false;   // the old editor's +0x10 says "Copying not supported for this strategy"
		}

		size_t const size = emptyLinks ? keep + 4 + (sourceLength - tail) : sourceLength;

		if (keep > sourceLength || tail > sourceLength || size > capacity)
		{
			return false;
		}

		memcpy(buffer, source, keep);

		if (emptyLinks)
		{
			StoreInt32(buffer + keep, 0);
			memcpy(buffer + keep + 4, source + tail, sourceLength - tail);
		}

		if (zeroHead)
		{
			StoreInt32(buffer + 12, 0);
			StoreInt32(buffer + 16, 0);
		}

		*length = size;
		return true;
	}
}
