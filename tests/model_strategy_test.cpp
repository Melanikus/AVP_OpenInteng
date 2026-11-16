// The test for src/model's strategy tables and rules. Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds strategy payloads in memory, some of them hostile, and checks what the model
// says about them. The program gives back a non-zero result if any check failed. Each check's words say what it proves.

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "field_bytes.h"
#include "crash_guard.h"
#include "field_model.h"

// IsNaN and IsNaNOrEndless, by the bits: this test is built with /fp:fast too, so a comparison of a number with itself can't tell.
#include "real_numbers.h"

using namespace model;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void PutInt(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

static int32_t GetInt(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t bits = 0;

	for (int i = 0; i < 4; i++)
	{
		bits |= (uint32_t)bytes.at(at + i) << (8 * i);
	}

	int32_t value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

// A strategy payload of that kind and size, all zero apart from its kind.
static std::vector<uint8_t> Payload(int32_t kind, size_t size)
{
	std::vector<uint8_t> bytes(size, 0);

	if (size >= 12)
	{
		PutInt(bytes, 8, kind);
	}

	return bytes;
}

static bool SizeIsRight(std::vector<uint8_t> const &bytes)
{
	return StrategySizeIsRight(bytes.data(), bytes.size());
}

// What the size check says about a payload. Every refusal test asserts its exact reason (the handbook's lesson): a later rule refusing
// the same payload for another reason would otherwise hide a missing one.
static SizeCheck SizeOf(std::vector<uint8_t> const &bytes)
{
	return CheckStrategySize(bytes.data(), bytes.size(), 0);
}

static Field const *FieldOf(StrategyKind const *kind, char const *name)
{
	if (!kind)
	{
		return 0;
	}

	for (size_t i = 0; i < kind->fieldCount; i++)
	{
		if (strcmp(kind->fields[i].name, name) == 0)
		{
			return &kind->fields[i];
		}
	}

	return 0;
}

static bool Allowed(int32_t kind, char const *field, double value, std::string *why = 0)
{
	Field const *const found = FieldOf(FindStrategyKind(kind), field);
	char const *reason = 0;

	if (!found)
	{
		return false;
	}

	bool const allowed = ValueIsAllowed(*found, value, &reason);

	if (why && reason)
	{
		*why = reason;
	}

	return allowed;
}

static std::vector<uint8_t> NewOne(int32_t kind)
{
	std::vector<uint8_t> bytes(512, 0xEE);
	size_t length = 0;

	if (!MakeNewStrategy(kind, bytes.data(), bytes.size(), &length))
	{
		return std::vector<uint8_t>();
	}

	bytes.resize(length);
	return bytes;
}

static void TablesHoldTogether()
{
	printf("The tables hold together (a mistyped offset, range or name shows up here)\n");

	bool numbersUnique = true;
	bool placesInside = true;
	bool creationsAllowed = true;
	bool reasonsGiven = true;
	bool choicesNamed = true;
	bool noOverlap = true;
	std::string firstProblem;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		StrategyKind const &kind = StrategyKinds[k];

		for (size_t other = k + 1; other < StrategyKindCount; other++)
		{
			if (StrategyKinds[other].number == kind.number)
			{
				numbersUnique = false;
			}
		}

		// Which bytes of the kind's smallest payload each field covers, to find two fields claiming the same bytes. Two masked
		// fields may share a word, if their masks don't overlap.
		std::vector<uint32_t> claimed(kind.fixedBytes, 0);

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			Field const &field = kind.fields[f];
			unsigned const bytes = StorageBytes(field.storage);

			if (field.anchor == Anchor::Start && (field.offset < 20 || field.offset + bytes > kind.fixedBytes))
			{
				placesInside = false;
				firstProblem = firstProblem.empty() ? std::string(kind.name) + "." + field.name : firstProblem;
			}

			if (field.anchor == Anchor::End && (field.offset < bytes || field.offset > kind.fixedBytes - 20))
			{
				placesInside = false;
				firstProblem = firstProblem.empty() ? std::string(kind.name) + "." + field.name : firstProblem;
			}

			if (field.limit != Limit::None && field.why[0] == 0)
			{
				reasonsGiven = false;
			}

			if (field.hasCreation && !ValueIsAllowed(field, field.creation, 0))
			{
				creationsAllowed = false;
				firstProblem = firstProblem.empty() ? std::string(kind.name) + "." + field.name : firstProblem;
			}

			if (field.unit == Unit::Choice && field.hasCreation && !ChoiceIsNamed(kind.number, field.name, (int32_t)field.creation))
			{
				choicesNamed = false;
				firstProblem = firstProblem.empty() ? std::string(kind.name) + "." + field.name : firstProblem;
			}

			// Places in the smallest payload: an AfterFirstList field sits right after the empty first list.
			if (kind.size == SizeRule::AnySize)
			{
				continue;
			}

			std::vector<uint8_t> empty = Payload(kind.number, kind.fixedBytes);
			size_t at = 0;

			if (!FieldPlace(kind, field, empty.data(), empty.size(), &at))
			{
				placesInside = false;
				firstProblem = firstProblem.empty() ? std::string(kind.name) + "." + field.name : firstProblem;
				continue;
			}

			uint32_t const mask = field.mask ? field.mask : 0xFFFFFFFFu;

			for (unsigned b = 0; b < bytes; b++)
			{
				// A masked field claims only its own bits; spread its mask over the bytes it covers.
				uint32_t const bits = field.mask ? ((mask >> (8 * b)) & 0xFF) : 0xFF;

				if (claimed.at(at + b) & bits)
				{
					noOverlap = false;
					firstProblem = firstProblem.empty() ? std::string(kind.name) + "." + field.name : firstProblem;
				}

				claimed.at(at + b) |= bits;
			}
		}
	}

	Check(numbersUnique, "no two kinds share a number, so a strategy's number names one kind");
	Check(placesInside, "every field lies inside its kind's smallest payload, after the 20 bytes every strategy has" +
	                    (firstProblem.empty() ? std::string() : " (first problem: " + firstProblem + ")"));
	Check(noOverlap, "no two fields claim the same bytes (fields sharing a word use separate bits)");
	Check(reasonsGiven, "every field with a limit says why, so a refused value comes with its reason");
	Check(creationsAllowed, "every creation value is one the editor allows, so a new strategy never starts out wrong");
	Check(choicesNamed, "every dropdown's creation value is one of its named choices");

	bool choicesAllowed = true;

	for (size_t i = 0; i < StrategyChoiceCount; i++)
	{
		ChoiceName const &choice = StrategyChoices[i];
		Field const *const field = FieldOf(FindStrategyKind(choice.kind), choice.field);

		if (!field || field->unit != Unit::Choice || !ValueIsAllowed(*field, choice.value, 0))
		{
			choicesAllowed = false;
		}
	}

	Check(choicesAllowed, "every named choice belongs to a dropdown field of its kind, and is a value that field allows");

	bool bitsBelong = true;

	for (size_t i = 0; i < StrategyBitCount; i++)
	{
		BitName const &bit = StrategyBits[i];
		Field const *const field = FieldOf(FindStrategyKind(bit.kind), bit.field);

		if (!field || field->unit != Unit::Flags || (field->mask && (bit.bit & ~field->mask)))
		{
			bitsBelong = false;
		}
	}

	Check(bitsBelong, "every named bit belongs to a flags field of its kind, inside that field's bits");

	bool maskedAreUnsigned = true;
	bool objectsStartEmpty = true;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		for (size_t f = 0; f < StrategyKinds[k].fieldCount; f++)
		{
			Field const &field = StrategyKinds[k].fields[f];

			if (field.mask && field.storage != Storage::UInt32)
			{
				maskedAreUnsigned = false;
			}

			if (field.storage == Storage::ObjectId && field.hasCreation && field.creation != 0)
			{
				objectsStartEmpty = false;
			}
		}
	}

	Check(maskedAreUnsigned, "every masked field is stored unsigned (UInt32), which the mask check's conversion relies on");
	Check(objectsStartEmpty, "every object field's creation value is 0, no object: one number can't name an object");
}

static void SizesAreTheGames()
{
	printf("Each kind's size is the game's own (GetStrategySize in strategy_chunk.cpp, plus the chunk's own 8 bytes)\n");

	struct Expected { int32_t kind; size_t size; };

	// The game's formulas, as written in its code: 8 for the chunk's index and spare2, 12 for AvpStrat, then the kind's own.
	Expected const expected[] =
	{
		{ 50, 8 + 12 + 8 },             // DoorStrategy: 8
		{ 51, 8 + 12 + 44 },            // LiftStrategy: 44, plus its lists
		{ 52, 8 + 12 + 28 },            // BinSwitchStrategy: 28
		{ 53, 8 + 12 },                 // a plain AvpStrat
		{ 2002, 8 + 12 },               // MummySimple: read as a plain AvpStrat, as 53 is
		{ 54, 8 + 12 + 24 },            // PlatLiftStrategy: 24
		{ 59, 8 + 12 + 16 },            // SwitchDoorStrategy: 16
		{ 61, 8 + 12 + 28 + 4 },        // LinkSwitchStrategy: a BinSwitch, a count, plus its links
		{ 62, 8 + 12 + 32 },            // SimpleStrategy: 32
		{ 63, 8 + 12 + 12 },            // ConsoleStrategy: 12
		{ 64, 8 + 12 + 36 },            // LightingStrategy: 36
		{ 66, 8 + 12 + 32 },            // MultiSwitchStrategy: 32, plus its lists
		{ 67, 8 + 12 + 20 },            // TeleportStrategy: 20
		{ 68, 8 + 12 + 32 + 8 + 24 },   // AreaSwitchStrategy: a MultiSwitch, 8, and two corners
		{ 69, 8 + 12 + 32 },            // EnemyStrategy: 32
		{ 70, 8 + 12 + 36 },            // MissionObjectiveStrategy: 36, plus its lists
		{ 72, 8 + 12 + 12 },            // TrackStrategy: 12, plus its point effects
		{ 73, 8 + 12 + 12 },            // TextMessageStrategy: 12
		{ 75, 8 + 12 + 16 },            // FanStrategy: 16
		{ 76, 8 + 12 + 12 },            // HierarchyStrategy: 12, plus its point effects
		{ 77, 8 + 12 + 12 + 24 },       // DeathVolumeStrategy: 12 and two corners
		{ 78, 8 + 12 + 20 },            // SelfDestructStrategy: 20
		{ 79, 8 + 12 + 32 },            // GeneratorStrategy: 32
		{ 80, 8 + 12 + 12 + 8 + 8 },    // TrackDestructStrategy: a Track, 8, and an object
	};

	bool allRight = true;
	bool longerRefused = true;
	bool shorterRefused = true;

	for (Expected const &each : expected)
	{
		std::vector<uint8_t> const right = Payload(each.kind, each.size);
		std::vector<uint8_t> const longer = Payload(each.kind, each.size + 4);
		std::vector<uint8_t> const shorter = Payload(each.kind, each.size - 4);

		if (SizeOf(right) != SizeCheck::Right)
		{
			allRight = false;
			printf("     kind %d of %u bytes was refused\n", (int)each.kind, (unsigned)each.size);
		}

		if (SizeOf(longer) != SizeCheck::WrongSize)
		{
			longerRefused = false;
			printf("     kind %d, 4 bytes longer, wasn't refused as the wrong size\n", (int)each.kind);
		}

		// 4 bytes fewer cuts a linked switch's link count off (it's its last number); every other kind still reads its counts.
		SizeCheck const shorterShould = each.kind == 61 ? SizeCheck::CountCutOff : SizeCheck::WrongSize;

		if (each.size - 4 >= 20 && SizeOf(shorter) != shorterShould)
		{
			shorterRefused = false;
			printf("     kind %d, 4 bytes shorter, wasn't refused for the right reason\n", (int)each.kind);
		}
	}

	Check(allRight, "a strategy of each kind, with its lists empty, is accepted at exactly the game's size");
	Check(longerRefused, "4 bytes more is refused as the wrong size, as the game refuses it");
	Check(shorterRefused, "4 bytes fewer is refused as the wrong size (a linked switch's, as its link count cut off)");

	std::vector<uint8_t> tooShort = Payload(9999, 19);
	Check(SizeOf(tooShort) == SizeCheck::TooShort, "a payload too short for the numbers every strategy has is refused as too short");

	std::vector<uint8_t> unknown = Payload(9999, 37);
	Check(SizeOf(unknown) == SizeCheck::Right, "a kind the editor doesn't know is kept at any size, as the game reads it as a plain block");

	std::vector<uint8_t> airlock = Payload(57, 101);
	Check(SizeOf(airlock) == SizeCheck::Right, "an Airlock (57) is kept at any size: the game's reader has no case for it");

	bool wordsGiven = true;
	SizeCheck const all[] = { SizeCheck::Right, SizeCheck::TooShort, SizeCheck::CountCutOff, SizeCheck::CountNegative,
	                          SizeCheck::CountTooBig, SizeCheck::WrongSize, SizeCheck::NameCutOff };

	for (SizeCheck const one : all)
	{
		for (SizeCheck const other : all)
		{
			if (SizeCheckWords(one)[0] == 0 || (one != other && strcmp(SizeCheckWords(one), SizeCheckWords(other)) == 0))
			{
				wordsGiven = false;
			}
		}
	}

	Check(wordsGiven, "every size reason has its own words, so a mapper is told which rule refused the file");

	// A number that's none of the reasons gets the words for a size that can't be checked. A reason left out of SizeCheckWords would
	// get those words too, and a mapper wouldn't be told which rule it was.
	char const *const noReasonsWords = SizeCheckWords((SizeCheck)99);
	char const *const cutOffWords = SizeCheckWords(SizeCheck::NameCutOff);
	Check(cutOffWords[0] != 0 && strcmp(cutOffWords, noReasonsWords) != 0,
	      "a name cut off has words of its own, not the words for a size that can't be checked");
}

static void ListsAreMeasured()
{
	printf("Lists: their counts are checked against the bytes really there, as the game's ReadListCount does\n");

	// A many-target switch with 2 targets and 1 linked switch: 52 + 2 * 12 + 8.
	std::vector<uint8_t> multi = Payload(66, 52 + 24 + 8);
	PutInt(multi, 36, 2);
	PutInt(multi, 40 + 24, 1);
	Check(SizeOf(multi) == SizeCheck::Right, "a many-target switch with 2 targets and a link is accepted at 52 + 24 + 8 bytes");

	std::vector<uint8_t> multiShort = Payload(66, 52 + 24);
	PutInt(multiShort, 36, 2);
	PutInt(multiShort, 40 + 24, 1);
	Check(SizeOf(multiShort) == SizeCheck::WrongSize, "the same counts in a payload 8 bytes short are refused as the wrong size");

	std::vector<uint8_t> negative = Payload(66, 52);
	PutInt(negative, 36, -1);
	Check(SizeOf(negative) == SizeCheck::CountNegative, "a negative count of targets is refused as negative, before it sizes anything");

	std::vector<uint8_t> huge = Payload(66, 52);
	PutInt(huge, 36, 0x7FFFFFFF);
	Check(SizeOf(huge) == SizeCheck::CountTooBig, "a count of targets too big for the payload is refused as too big");

	// The rule: a count is measured against the bytes left before it's multiplied by its items' size. This count's items would
	// take just past what 32 bits can hold, in a payload with room for one target, and it's refused as too big.
	std::vector<uint8_t> edge = Payload(66, 60);
	PutInt(edge, 36, 0x15555556);
	Check(SizeOf(edge) == SizeCheck::CountTooBig,
	      "a count at the 32-bit edge is refused as too big: counts are measured before they're multiplied");

	std::vector<uint8_t> justFits = Payload(66, 52 + 12 * 3);
	PutInt(justFits, 36, 3);
	Check(SizeOf(justFits) == SizeCheck::Right, "a count that exactly fits the payload is accepted");

	std::vector<uint8_t> cutBeforeCount = Payload(66, 38);
	Check(SizeOf(cutBeforeCount) == SizeCheck::CountCutOff, "a payload that ends inside a count is refused as the count cut off");

	// The count reader asked directly about a list whose items take no bytes. No table has one, but there's no number of such items
	// the bytes left could be measured against, so it's refused whatever its count, before anything is divided by the item's size.
	std::vector<uint8_t> noBytes = Payload(66, 52);
	size_t count = 0;
	PutInt(noBytes, 36, 3);
	SizeCheck const threeOfNothing = detail::ReadCount(noBytes.data(), noBytes.size(), 36, 0, &count);
	PutInt(noBytes, 36, 0);
	SizeCheck const noneOfNothing = detail::ReadCount(noBytes.data(), noBytes.size(), 36, 0, &count);
	Check(threeOfNothing == SizeCheck::CountTooBig && noneOfNothing == SizeCheck::CountTooBig && count == 0,
	      "a list whose items take no bytes is refused as too big, with a count of 3 or of 0, and gives no count");

	// A lift with 1 linked lift and 2 lifts elsewhere: 64 + 8 + 2 * 28.
	std::vector<uint8_t> lift = Payload(51, 64 + 8 + 56);
	PutInt(lift, 20, 1);
	PutInt(lift, 24 + 8 + 32, 2);
	Check(SizeIsRight(lift), "a lift with one linked lift and two elsewhere is accepted at 64 + 8 + 56 bytes");

	// A mission objective that alters 1 objective and has 2 request targets: 56 + 12 + 24.
	std::vector<uint8_t> mission = Payload(70, 56 + 12 + 24);
	PutInt(mission, 40, 1);
	PutInt(mission, 44 + 12, 2);
	Check(SizeIsRight(mission), "a mission objective with 1 objective and 2 request targets is accepted at 56 + 12 + 24 bytes");

	std::vector<uint8_t> link = Payload(61, 52 + 16);
	PutInt(link, 48, 2);
	Check(SizeIsRight(link), "a linked switch with 2 links is accepted at 52 + 16 bytes");

	// A track with 2 point effects, the second with 3 targets: 32 + 2 * 12 + 3 * 16. Each effect is its point, a pad, then the count.
	std::vector<uint8_t> track = Payload(72, 32 + 24 + 48);
	PutInt(track, 20, 2);
	PutInt(track, 24 + 8, 0);
	PutInt(track, 24 + 12 + 8, 3);
	Check(SizeIsRight(track), "a track with 2 point effects and 3 targets is accepted at 32 + 24 + 48 bytes");

	std::vector<uint8_t> trackLie = Payload(72, 32 + 24 + 48);
	PutInt(trackLie, 20, 2);
	PutInt(trackLie, 24 + 12 + 8, 1000);
	Check(SizeOf(trackLie) == SizeCheck::CountTooBig, "a point effect counting more targets than the payload holds is refused as too big");

	std::vector<uint8_t> trackPad = Payload(72, 32 + 24 + 48);
	PutInt(trackPad, 20, 2);
	PutInt(trackPad, 24 + 4, 3);
	Check(SizeOf(trackPad) == SizeCheck::WrongSize,
	      "the targets' count is read after the pad, not in its place (the game's order: point, pad, count)");

	std::vector<uint8_t> destruct = Payload(80, 48 + 12);
	PutInt(destruct, 20, 1);
	Check(SizeIsRight(destruct), "a destructible track with one empty point effect is accepted at 48 + 12 bytes");

	// A field after the first list moves with it: with 2 targets, a many-target switch's link count is at 40 + 24.
	StrategyKind const *const kind = FindStrategyKind(66);
	Field const *const links = FieldOf(kind, "NumLinks");
	size_t at = 0;
	Check(kind && links && FieldPlace(*kind, *links, multi.data(), multi.size(), &at) && at == 64,
	      "a field after a list is found after that list (NumLinks at 64 with 2 targets)");

	Field const *const flags = FieldOf(kind, "flags");
	Check(kind && flags && FieldPlace(*kind, *flags, multi.data(), multi.size(), &at) && at == multi.size() - 4,
	      "a field counted from the end is found there (the switch's flags are its last 4 bytes)");
}

static void ValuesAreChecked()
{
	printf("Values: each refusal names its reason\n");

	std::string why;
	Check(!Allowed(75, "speed_up_time", 0, &why) && why.find("1 to 999") != std::string::npos,
	      "a fan's speed-up time under one second is refused, and the reason says 1 to 999 (inteng takes whole seconds)");
	Check(Allowed(75, "speed_up_time", 1) && Allowed(75, "speed_up_time", 999) && !Allowed(75, "speed_up_time", 1000) && !Allowed(75, "speed_up_time", 1.5), "a fan's speed-up time is 1 to 999 whole seconds (shown in seconds, kept times 65536): 1 and 999 allowed, 1000 and 1.5 refused");
	Check(!Allowed(75, "slow_down_time", 0), "a fan's slow-down time of 0 is refused");

	Check(!Allowed(73, "message_string", 451, &why) && why.find("450") != std::string::npos,
	      "a message number over 450 is refused, and the reason says 450");
	Check(Allowed(73, "message_string", 450) && Allowed(73, "message_string", 0), "message numbers 0 and 450 are allowed");
	Check(!Allowed(73, "message_string", -1), "a negative message number is refused");
	Check(!Allowed(70, "mission_description_string", 451), "a mission description's number over 450 is refused");

	Check(!Allowed(75, "fan_wind_strength", 1001), "a fan's wind over 1000 percent is refused (the game would read it as 100)");
	Check(!Allowed(78, "timer", 32768), "a self-destruct countdown over 32767 seconds is refused (it wouldn't fit in 16.16)");

	Check(!Allowed(77, "volume_min", 16777217), "a box's corner farther than 2 to the 24 from the centre is refused");
	Check(Allowed(77, "volume_min", -16777216), "a box's corner exactly 2 to the 24 away is allowed");

	Check(!Allowed(62, "integrity", 256), "an object's integrity over 255 is refused: the game reads only its lowest 8 bits");
	Check(Allowed(62, "explosion", 0x10), "an object's small explosion (0x10) is allowed");
	Check(!Allowed(62, "explosion", 0x08), "a value outside a masked field's own bits is refused");

	why.clear();
	Check(!Allowed(50, "TimeToOpen", 256, &why) && why == DoesNotFitReason,
	      "a door's time over 255 tenths is refused because the file holds it in one byte");
	why.clear();
	Check(!Allowed(52, "Mode", 1.5, &why) && why == NotWholeReason, "a fraction is refused, as not whole, where the file holds a whole number");

	// NaN and the endless values are made from their bits, so nothing about how this test is compiled can change them.
	double const notANumber = std::numeric_limits<double>::quiet_NaN();
	double const endless = std::numeric_limits<double>::infinity();
	why.clear();
	Check(!Allowed(52, "Mode", notANumber, &why) && why == NotANumberReason,
	      "NaN is refused as not a number, even in the /fp:fast build (the check reads its bits)");
	why.clear();
	Check(!Allowed(50, "spare", -endless, &why) && why == NotANumberReason,
	      "an endless value is refused as not a number, even where the field has no limit of its own");

	Check(ChoiceIsNamed(52, "Mode", 4) && !ChoiceIsNamed(52, "Mode", 3),
	      "a switch's dropdown offers Time Delay (4) but not 3, the game's own state while it moves");
}

static void OldFilesAreUpgraded()
{
	printf("Old files: the game's own upgrades, done the same way\n");

	// A many-target switch of the old form: 3 targets, pad1 = 2, and the flag not set. The first 2 targets get "off" (0).
	std::vector<uint8_t> multi = Payload(66, 52 + 36);
	PutInt(multi, 36, 3);
	PutInt(multi, 48, 7);
	PutInt(multi, 60, 7);
	PutInt(multi, 72, 7);
	PutInt(multi, multi.size() - 8, 2);
	bool const changed = UpgradeOldStrategy(multi.data(), multi.size());
	Check(changed && GetInt(multi, 48) == 0 && GetInt(multi, 60) == 0 && GetInt(multi, 72) == 1,
	      "an old many-target switch's first pad1 targets get request 0 and the rest 1, as the game's reader does");
	Check(GetInt(multi, (size_t)multi.size() - 8) == 0 && ((uint32_t)GetInt(multi, multi.size() - 4) & 0x80000000u),
	      "and then pad1 is 0 and its flag says the requests are stored one by one");
	Check(!UpgradeOldStrategy(multi.data(), multi.size()), "an upgraded switch isn't changed again");

	std::vector<uint8_t> area = Payload(68, 84 + 12);
	PutInt(area, 36, 1);
	PutInt(area, area.size() - 40, 1);
	Check(UpgradeOldStrategy(area.data(), area.size()) && GetInt(area, 48) == 0 &&
	      ((uint32_t)GetInt(area, area.size() - 36) & 0x80000000u),
	      "an old area switch is upgraded the same way (pad1 = 1, so its one target gets 0), with its flags 36 bytes from the end");

	std::vector<uint8_t> negativePad = Payload(66, 52 + 12);
	PutInt(negativePad, 36, 1);
	PutInt(negativePad, negativePad.size() - 8, -5);
	UpgradeOldStrategy(negativePad.data(), negativePad.size());
	Check(GetInt(negativePad, 48) == 1, "a negative pad1 gives every target request 1, as the game's signed comparison does");

	// A placed object of the old form: integrity 5 without its flag, and junk after it.
	std::vector<uint8_t> object = Payload(62, 52);
	PutInt(object, 32, 5);
	PutInt(object, 36, 0x13);
	PutInt(object, 40, 9);
	PutInt(object, 44, 123);
	PutInt(object, 48, 456);
	Check(UpgradeOldStrategy(object.data(), object.size()) && GetInt(object, 36) == 0 && GetInt(object, 40) == 0 &&
	      GetInt(object, 44) == 0 && GetInt(object, 48) == 0,
	      "an old placed object's flags, request and target are cleared as junk, as the game's reader does");
	Check((uint32_t)GetInt(object, 32) == 0x80000005u, "and its integrity keeps its value, with the flag that says it's upgraded");

	std::vector<uint8_t> wrongSize = Payload(62, 56);
	PutInt(wrongSize, 36, 0x13);
	Check(!UpgradeOldStrategy(wrongSize.data(), wrongSize.size()) && GetInt(wrongSize, 36) == 0x13,
	      "a strategy whose size the game would refuse isn't upgraded or touched");
}

static void NewOnesStartAsTheOldEditorMadeThem()
{
	printf("New strategies start as the old editor made them\n");

	std::vector<uint8_t> const door = NewOne(50);
	Check(door.size() == 28 && door.at(24) == 10 && door.at(25) == 5, "a new door opens in 1.0 s and closes in 0.5 s");

	std::vector<uint8_t> const fan = NewOne(75);
	Check(fan.size() == 36 && GetInt(fan, 20) == 655360 && GetInt(fan, 24) == 655360 && GetInt(fan, 28) == 100,
	      "a new fan speeds up and slows down in 10 s, with a wind of 100");

	std::vector<uint8_t> const object = NewOne(62);
	Check(object.size() == 52 && GetInt(object, 28) == 50 && (uint32_t)GetInt(object, 32) == 0x80000002u &&
	      GetInt(object, 36) == 1,
	      "a new placed object has mass 50, integrity 2 with its always-set flag, and tells its target when destroyed");

	std::vector<uint8_t> const multi = NewOne(66);
	Check(multi.size() == 52 && GetInt(multi, 24) == 1 && (uint32_t)GetInt(multi, 48) == 0x80000000u,
	      "a new many-target switch waits, and has the always-set flag");

	std::vector<uint8_t> const airlock = NewOne(57);
	Check(airlock.size() == 64 && GetInt(airlock, 8) == 51 && (GetInt(airlock, 24 + 24) & 0x2) && GetInt(airlock, 24 + 28) == -1,
	      "a new Airlock is written as a Lift (51) with its Airlock flag, on floor -1, as the old editor wrote it");

	std::vector<uint8_t> const noTel = NewOne(60);
	Check(noTel.size() == 64 && GetInt(noTel, 8) == 51 && (GetInt(noTel, 24 + 24) & 0x4),
	      "a new LiftNoTel is written as a Lift with its no-teleport flag");

	std::vector<uint8_t> const selfDestruct = NewOne(78);
	Check(selfDestruct.size() == 40 && GetInt(selfDestruct, 20) == 60, "a new self-destruct counts down from 60 seconds");

	bool allAccepted = true;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		if (StrategyKinds[k].offered && !SizeIsRight(NewOne(StrategyKinds[k].number)))
		{
			allAccepted = false;
			printf("     a new kind %d isn't accepted\n", (int)StrategyKinds[k].number);
		}
	}

	Check(allAccepted, "every new strategy the editor makes is one the game's size rule accepts");
	Check(NewOne(53).empty() && NewOne(71).empty() && NewOne(9999).empty(),
	      "a kind the old editor didn't offer, or doesn't exist, isn't made");

	uint8_t small[16];
	size_t length = 0;
	Check(!MakeNewStrategy(50, small, sizeof(small), &length), "a buffer too small for a new strategy isn't written");
}

static void SettersWriteOnlyTheirPart()
{
	printf("Setting a field changes that field and nothing else\n");

	StrategyKind const *const volume = FindStrategyKind(77);
	Field const *const corner = FieldOf(volume, "volume_min");
	std::vector<uint8_t> box = NewOne(77);
	PutInt(box, 20, 1);
	PutInt(box, 24, 2);
	PutInt(box, 28, 3);
	char const *why = 0;
	Check(volume && corner && SetFieldAxis(*volume, *corner, box.data(), box.size(), 1, 500, &why) && GetInt(box, 20) == 1 &&
	      GetInt(box, 24) == 500 && GetInt(box, 28) == 3,
	      "setting a box corner's y changes y alone; x and z keep their values");
	Check(volume && corner && !SetField(*volume, *corner, box.data(), box.size(), 7, &why) && GetInt(box, 20) == 1 &&
	      GetInt(box, 24) == 500 && GetInt(box, 28) == 3,
	      "a three-number field can't be set from one number, and is left as it was");
	Check(volume && corner && !SetFieldAxis(*volume, *corner, box.data(), box.size(), 1, 16777217, &why) && GetInt(box, 24) == 500,
	      "an axis value outside its limit is refused and changes nothing");
	Check(volume && corner && !SetFieldAxis(*volume, *corner, box.data(), box.size(), 3, 5, &why), "there's no fourth axis");

	StrategyKind const *const object = FindStrategyKind(62);
	Field const *const target = FieldOf(object, "targetID");
	std::vector<uint8_t> placed = NewOne(62);
	Check(object && target && SetFieldObject(*object, *target, placed.data(), placed.size(), 7, 9) && GetInt(placed, 44) == 7 &&
	      GetInt(placed, 48) == 9,
	      "an object field takes its two halves as given: first, then second");
	Check(object && target && !SetField(*object, *target, placed.data(), placed.size(), 7, &why) && GetInt(placed, 44) == 7 &&
	      GetInt(placed, 48) == 9,
	      "an object field can't be set from one number, and is left as it was");

	StrategyKind const *const door = FindStrategyKind(50);
	Field const *const open = FieldOf(door, "TimeToOpen");
	std::vector<uint8_t> doorBytes = NewOne(50);
	Check(door && open && SetField(*door, *open, doorBytes.data(), doorBytes.size(), 15, &why) && doorBytes.at(24) == 15 &&
	      doorBytes.at(25) == 5,
	      "setting a door's opening time changes that byte alone; its closing time stays 5");

	StrategyKind const *const fan = FindStrategyKind(75);
	Field const *const speedUp = FieldOf(fan, "speed_up_time");
	std::vector<uint8_t> fanBytes = NewOne(75);
	why = 0;
	Check(fan && speedUp && !SetField(*fan, *speedUp, fanBytes.data(), fanBytes.size(), 0, &why) && GetInt(fanBytes, 20) == 655360 &&
	      why == speedUp->why,
	      "a refused value changes nothing, and the reason given is the field's own");

	// S-K75-1 (0x5356b0-0x53575a): the view shows a fan's time as the word / 65536, ROUNDED TOWARD 0 (a C division); a hostile file's negative word must read so too, not toward minus infinity
	std::vector<uint8_t> hostileFan = NewOne(75);
	double seconds = 0.0;

	PutInt(hostileFan, 20, -65537);
	Check(fan && speedUp && ReadField(*fan, *speedUp, hostileFan.data(), hostileFan.size(), &seconds, nullptr) && seconds == -1.0,
	      "a fan time word of -65537 reads -1 second, as inteng divides by 65536 (not -2, an arithmetic shift)");
	PutInt(hostileFan, 20, 65537);
	Check(fan && speedUp && ReadField(*fan, *speedUp, hostileFan.data(), hostileFan.size(), &seconds, nullptr) && seconds == 1.0, "and 65537 reads 1 second");
}

static void FieldsOutsideThePayloadAreRefused()
{
	printf("A field that doesn't fit where it would go is refused, and nothing is written:\n");

	char const *why = 0;

	// A door cut short: its own fields start at 20, so a 20-byte payload holds none of them.
	StrategyKind const *const door = FindStrategyKind(50);
	Field const *const open = FieldOf(door, "TimeToOpen");
	std::vector<uint8_t> shortDoor = Payload(50, 20);
	std::vector<uint8_t> const before = shortDoor;
	Check(door && open && !SetField(*door, *open, shortDoor.data(), shortDoor.size(), 15, &why) && shortDoor == before,
	      "setting a field the payload is too short for is refused, and the payload is unchanged");

	// The setters at a place, asked directly. The buffer has 24 bytes, but the payload is said to be 22, so a 4-byte field at 20
	// would run past it; a wrong check would write the buffer's last two bytes, which the test can see.
	Field const whole = { "test", "", Anchor::Start, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" };
	Field const triple = { "test", "", Anchor::Start, 20, Storage::IntTriple, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" };
	Field const object = { "test", "", Anchor::Start, 20, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" };
	std::vector<uint8_t> room(24, 0xAA);
	std::vector<uint8_t> const untouched = room;

	Check(!detail::SetFieldAt(whole, room.data(), 22, 20, 5, &why) && room == untouched,
	      "a number that would run past the payload is refused, and nothing is written");
	Check(!detail::SetAxisAt(triple, room.data(), 22, 20, 0, 5, &why) && room == untouched,
	      "a three-number field that would run past the payload is refused");
	Check(!detail::SetObjectAt(object, room.data(), 22, 20, 1, 2) && room == untouched,
	      "an object field that would run past the payload is refused");
	Check(!detail::WriteCreationAt(whole, room.data(), 22, 20) && room == untouched,
	      "a creation value that would run past the payload isn't written");
	Check(!detail::SetFieldAt(whole, room.data(), 22, (size_t)-1, 5, &why) && room == untouched,
	      "a place near the top of a size_t doesn't wrap round to one inside the payload");
	Check(detail::SetFieldAt(whole, room.data(), 24, 20, 5, &why) && room.at(20) == 5 && room.at(23) == 0,
	      "the same field fits once the payload is long enough");
}

// A field of that storage with no limit of its own, so only its storage decides.
static Field OfStorage(Storage storage)
{
	Field const field = { "test", "", Anchor::Start, 0, storage, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" };
	return field;
}

// Whether a value is allowed in a field of that storage, and the reason when it isn't ("" when it is).
static char const *StorageVerdict(Storage storage, double value)
{
	char const *why = 0;
	return ValueIsAllowed(OfStorage(storage), value, &why) ? "" : (why ? why : "refused with no reason");
}

static void StorageRangesAreTheStorages()
{
	printf("Each storage's range is what it can hold:\n");

	struct Edge
	{
		Storage storage;
		char const *name;
		double lowest;
		double highest;
		bool whole;
	};

	Edge const edges[] =
	{
		{ Storage::Byte, "a byte", 0, UCHAR_MAX, true },
		{ Storage::Int16, "a signed 16-bit number", SHRT_MIN, SHRT_MAX, true },
		{ Storage::UInt16, "an unsigned 16-bit number", 0, USHRT_MAX, true },
		{ Storage::Int32, "a signed 32-bit number", INT_MIN, INT_MAX, true },
		{ Storage::UInt32, "an unsigned 32-bit number", 0, UINT_MAX, true },
		{ Storage::IntTriple, "each number of a three-number field", INT_MIN, INT_MAX, true },
		{ Storage::ObjectId, "each half of an object", 0, UINT_MAX, true },
		{ Storage::Float32, "a 32-bit float", -FLT_MAX, FLT_MAX, false },
		{ Storage::Float64, "a 64-bit float", -DBL_MAX, DBL_MAX, false },
	};

	for (Edge const &edge : edges)
	{
		double lowest = 1, highest = -1;
		StorageRange(edge.storage, &lowest, &highest);
		std::string const name = edge.name;

		Check(lowest == edge.lowest && highest == edge.highest, name + ": the range is the storage's own");
		Check(strcmp(StorageVerdict(edge.storage, edge.lowest), "") == 0 && strcmp(StorageVerdict(edge.storage, edge.highest), "") == 0,
		      name + ": both ends are allowed");

		if (edge.whole)
		{
			Check(StorageVerdict(edge.storage, edge.lowest - 1) == DoesNotFitReason &&
			      StorageVerdict(edge.storage, edge.highest + 1) == DoesNotFitReason,
			      name + ": one past either end is refused, as not fitting");
		}
	}

	Check(StorageVerdict(Storage::Float32, (double)FLT_MAX * 2) == DoesNotFitReason &&
	      StorageVerdict(Storage::Float32, -(double)FLT_MAX * 2) == DoesNotFitReason,
	      "a 32-bit float refuses twice its largest value, as not fitting");
	Check(StorageVerdict(Storage::Float64, std::numeric_limits<double>::infinity()) == NotANumberReason,
	      "past a 64-bit float's largest value is endless, refused as not a real number");
}

// Whether a value fits a field's storage, and its bits for a masked field. Where it does, only the field's own limit can refuse it.
static bool FitsTheStorage(Field const &field, double value)
{
	double lowest = 0, highest = 0;
	StorageRange(field.storage, &lowest, &highest);

	if (value < lowest || value > highest)
	{
		return false;
	}

	// Every masked field is stored unsigned (TablesHoldTogether checks it), so the value isn't negative here.
	return !field.mask || ((uint32_t)value & ~field.mask) == 0;
}

static void LimitsAreTheirEnds()
{
	printf("Every field's limit: both its ends are allowed, and one past either end is refused with the field's own reason\n");

	// Every field of every kind with a limit of its own and a whole-number storage. Three units are left out, because a range from
	// one end to the other isn't what limits them:
	// - a list's count (ListCount) changes only when its items are added or taken away, never by a number;
	// - a set of flags (Flags) is limited by which of its bits a mapper may change (its mask), not by a lowest and a highest;
	// - a choice (Choice) takes only the values its choices name, which can leave a gap between its ends (ChoicesAreNamed walks
	//   every one of its values instead).
	// One past an end is tried only where the storage holds it: past that, the storage refuses it, before the limit is looked at.
	bool endsAllowed = true;
	bool pastRefused = true;
	size_t fields = 0;
	size_t pastTried = 0;
	std::string firstProblem;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		StrategyKind const &kind = StrategyKinds[k];

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			Field const &field = kind.fields[f];

			if (field.limit == Limit::None || field.storage == Storage::Float32 || field.storage == Storage::Float64 ||
			    field.unit == Unit::ListCount || field.unit == Unit::Flags || field.unit == Unit::Choice)
			{
				continue;
			}

			fields++;
			std::string const name = std::string(kind.name) + "." + field.name;

			if (!ValueIsAllowed(field, field.lowest, 0) || !ValueIsAllowed(field, field.highest, 0))
			{
				endsAllowed = false;
				firstProblem = firstProblem.empty() ? name : firstProblem;
			}

			double const pastEnds[] = { field.lowest - 1, field.highest + 1 };

			for (double const past : pastEnds)
			{
				if (!FitsTheStorage(field, past))
				{
					continue;
				}

				pastTried++;
				char const *why = 0;

				if (ValueIsAllowed(field, past, &why) || why != field.why)
				{
					pastRefused = false;
					firstProblem = firstProblem.empty() ? name : firstProblem;
				}
			}
		}
	}

	std::string const problem = firstProblem.empty() ? std::string() : " (first problem: " + firstProblem + ")";
	Check(fields > 0 && endsAllowed,
	      "for each of the " + std::to_string(fields) + " fields with a limit, its lowest and highest values are allowed" + problem);
	Check(pastTried > 0 && pastRefused,
	      "and each of the " + std::to_string(pastTried) + " values one past an end that its storage holds is refused, with the "
	      "field's own reason" + problem);
}

// A new strategy of that kind, as MakeNewStrategy makes it, with the field found by its name, for the setters' checks below.
struct NewStrategy
{
	StrategyKind const *kind;
	Field const *field;
	std::vector<uint8_t> bytes;
};

static NewStrategy NewWithField(int32_t number, char const *name)
{
	NewStrategy made;
	made.kind = FindStrategyKind(number);
	made.field = FieldOf(made.kind, name);
	made.bytes = NewOne(number);
	return made;
}

static void ListCountsAreNotNumbers()
{
	printf("A list's count changes only with its items: the setters refuse it as a number\n");

	// Every list's count in the tables: how many lifts, links, targets, mission and request targets or point effects follow it.
	// Set from a number, it would no longer match the items after it, and the game would refuse the strategy or read it wrongly.
	struct Count { int32_t kind; char const *field; };

	Count const counts[] =
	{
		{ 51, "NumAssocLifts" }, { 51, "NumExternalLifts" }, { 61, "NumLinks" }, { 66, "NumTargets" }, { 66, "NumLinks" },
		{ 68, "NumTargets" }, { 68, "NumLinks" }, { 70, "num_mission_targets" }, { 70, "num_request_targets" },
		{ 72, "num_point_effects" }, { 76, "num_point_effects" }, { 80, "num_point_effects" },
	};

	size_t const countCount = sizeof(counts) / sizeof(counts[0]);
	size_t inTables = 0;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		for (size_t f = 0; f < StrategyKinds[k].fieldCount; f++)
		{
			inTables += StrategyKinds[k].fields[f].unit == Unit::ListCount ? 1 : 0;
		}
	}

	bool allCounts = inTables == countCount;
	bool allRefused = true;

	for (Count const &each : counts)
	{
		// A new one of the kind, with its lists empty, so the count has its place; 1 is a value its storage could hold.
		NewStrategy made = NewWithField(each.kind, each.field);

		if (!made.kind || !made.field || made.field->unit != Unit::ListCount || made.bytes.empty())
		{
			allCounts = false;
			printf("     kind %d's %s isn't a list's count in the tables\n", (int)each.kind, each.field);
			continue;
		}

		std::vector<uint8_t> const before = made.bytes;
		char const *why = 0;

		if (SetField(*made.kind, *made.field, made.bytes.data(), made.bytes.size(), 1, &why) || why != ListCountReason ||
		    made.bytes != before)
		{
			allRefused = false;
			printf("     kind %d's %s was set from a number, or refused for another reason\n", (int)each.kind, each.field);
		}
	}

	Check(allCounts, "the " + std::to_string(countCount) + " lists' counts are each a list's count (ListCount) in the tables, and no "
	                 "other field is");
	Check(allRefused, "SetField refuses every one of them as a list's count, and leaves the strategy byte for byte as it was");

	// Two of them on their own: one at a place counted from the start, and one counted from the end of the first list.
	NewStrategy lift = NewWithField(51, "NumAssocLifts");
	std::vector<uint8_t> const liftBefore = lift.bytes;
	char const *why = 0;
	Check(lift.kind && lift.field && !SetField(*lift.kind, *lift.field, lift.bytes.data(), lift.bytes.size(), 2, &why) &&
	      why == ListCountReason && lift.bytes == liftBefore,
	      "a lift's count of linked lifts can't be typed in: it's refused as a list's count, and the lift is unchanged");

	NewStrategy mission = NewWithField(70, "num_request_targets");
	std::vector<uint8_t> const missionBefore = mission.bytes;
	why = 0;
	Check(mission.kind && mission.field &&
	      !SetField(*mission.kind, *mission.field, mission.bytes.data(), mission.bytes.size(), 2, &why) && why == ListCountReason &&
	      mission.bytes == missionBefore,
	      "a mission objective's count of request targets, after its first list, is refused the same way");

	// The setter at a known place, which SetChunkField uses too, so a chunk's list count would be refused the same way.
	Field const count = { "test", "", Anchor::Start, 20, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0, "", false, 0, "" };
	std::vector<uint8_t> room(24, 0xAA);
	std::vector<uint8_t> const untouched = room;
	why = 0;
	Check(!detail::SetFieldAt(count, room.data(), room.size(), 20, 3, &why) && why == ListCountReason && room == untouched,
	      "the setter every chunk's setter shares refuses a list's count too, and writes nothing");
}

// One of the fields every strategy has, by its name.
static Field const *BaseField(char const *name)
{
	for (size_t i = 0; i < StrategyBaseFieldCount; i++)
	{
		if (strcmp(StrategyBaseFields[i].name, name) == 0)
		{
			return &StrategyBaseFields[i];
		}
	}

	return 0;
}

static void FieldsBelongToTheirKind()
{
	printf("A strategy's setters take only its kind's own fields and the ones every strategy has, never its kind's number\n");

	// Each foreign field below has a place that the strategy it's tried on uses for something else, so it would be written over
	// that strategy's own numbers if it were taken.
	NewStrategy door = NewWithField(50, "TimeToOpen");
	NewStrategy area = NewWithField(68, "trigger_min");
	NewStrategy enemy = NewWithField(69, "DeathTarget");
	Field const *const fanSpeed = FieldOf(FindStrategyKind(75), "speed_up_time");       // +20, a door's flags
	Field const *const boxCorner = FieldOf(FindStrategyKind(77), "volume_min");        // +20, an area switch's rest state and mode
	Field const *const switchTarget = FieldOf(FindStrategyKind(52), "Target");         // +36, an enemy's path
	Field const *const kindNumber = BaseField("StrategyType");
	Field const *const extra = BaseField("ExtraData");

	if (!door.kind || !door.field || door.bytes.empty() || !area.kind || !area.field || area.bytes.empty() || !enemy.kind ||
	    !enemy.field || enemy.bytes.empty() || !fanSpeed || !boxCorner || !switchTarget || !kindNumber || !extra)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	char const *why = 0;
	std::vector<uint8_t> const doorBefore = door.bytes;
	Check(!SetField(*door.kind, *fanSpeed, door.bytes.data(), door.bytes.size(), 100, &why) && why == NotThisKindsReason &&
	      door.bytes == doorBefore,
	      "SetField refuses a fan's field in a door, as not one the door has, and the door is unchanged");
	why = 0;
	Check(!SetField(*door.kind, *kindNumber, door.bytes.data(), door.bytes.size(), 75, &why) && why == NotThisKindsReason &&
	      door.bytes == doorBefore,
	      "SetField refuses the kind's own number, which would make the door read as another kind, and the door is unchanged");

	std::vector<uint8_t> doorExpected = doorBefore;
	PutInt(doorExpected, 16, 7);
	Check(SetField(*door.kind, *extra, door.bytes.data(), door.bytes.size(), 7, &why) && door.bytes == doorExpected,
	      "SetField still takes a field every strategy has (ExtraData), and writes only that");
	doorExpected.at(24) = 15;
	Check(SetField(*door.kind, *door.field, door.bytes.data(), door.bytes.size(), 15, &why) && door.bytes == doorExpected,
	      "and it still takes the door's own fields (its opening time)");

	std::vector<uint8_t> const areaBefore = area.bytes;
	why = 0;
	Check(!SetFieldAxis(*area.kind, *boxCorner, area.bytes.data(), area.bytes.size(), 0, 500, &why) && why == NotThisKindsReason &&
	      area.bytes == areaBefore,
	      "SetFieldAxis refuses a volume of death's corner in an area switch, as not one the switch has, and the switch is unchanged");
	why = 0;
	Check(!SetFieldAxis(*area.kind, *kindNumber, area.bytes.data(), area.bytes.size(), 0, 75, &why) && why == NotThisKindsReason &&
	      area.bytes == areaBefore,
	      "SetFieldAxis refuses the kind's own number too, for the same reason");

	// The switch's own box corner is 32 bytes from its end; its third number (z) is 8 bytes into it.
	std::vector<uint8_t> areaExpected = areaBefore;
	PutInt(areaExpected, areaExpected.size() - 32 + 8, 500);
	Check(SetFieldAxis(*area.kind, *area.field, area.bytes.data(), area.bytes.size(), 2, 500, &why) && area.bytes == areaExpected,
	      "SetFieldAxis still takes the area switch's own box corner");

	// SetFieldObject gives no reason (it has no place for one), so only its refusal and the unchanged bytes can be checked.
	std::vector<uint8_t> const enemyBefore = enemy.bytes;
	Check(!SetFieldObject(*enemy.kind, *switchTarget, enemy.bytes.data(), enemy.bytes.size(), 7, 9) && enemy.bytes == enemyBefore,
	      "SetFieldObject refuses a switch's target in an enemy, as not one the enemy has, and the enemy is unchanged");
	Check(!SetFieldObject(*enemy.kind, *kindNumber, enemy.bytes.data(), enemy.bytes.size(), 75, 0) && enemy.bytes == enemyBefore,
	      "SetFieldObject refuses the kind's own number too");

	std::vector<uint8_t> enemyExpected = enemyBefore;
	PutInt(enemyExpected, 28, 7);
	PutInt(enemyExpected, 32, 9);
	Check(SetFieldObject(*enemy.kind, *enemy.field, enemy.bytes.data(), enemy.bytes.size(), 7, 9) && enemy.bytes == enemyExpected,
	      "SetFieldObject still takes the enemy's own death target");
}

static void ChoicesAreNamed()
{
	printf("A choice field takes only the values its choices name\n");

	// A switch's mode is limited to 0-5, but 3 is the game's own state while a switch is moving, so no choice names it.
	NewStrategy binSwitch = NewWithField(52, "Mode");

	if (!binSwitch.kind || !binSwitch.field || binSwitch.bytes.empty())
	{
		Check(false, "the kind and field these checks use are in the tables");
		return;
	}

	std::vector<uint8_t> const before = binSwitch.bytes;
	char const *why = 0;
	Check(!SetField(*binSwitch.kind, *binSwitch.field, binSwitch.bytes.data(), binSwitch.bytes.size(), 3, &why) &&
	      why == UnnamedChoiceReason && binSwitch.bytes == before,
	      "a switch's mode of 3, inside its limit but named by no choice, is refused as unnamed, and the switch is unchanged");
	why = 0;
	Check(!SetField(*binSwitch.kind, *binSwitch.field, binSwitch.bytes.data(), binSwitch.bytes.size(), 6, &why) &&
	      why == binSwitch.field->why && binSwitch.bytes == before,
	      "a mode of 6, past its limit, is refused with the limit's own reason first");
	Check(SetField(*binSwitch.kind, *binSwitch.field, binSwitch.bytes.data(), binSwitch.bytes.size(), 4, &why) &&
	      GetInt(binSwitch.bytes, 24) == 4,
	      "a named mode (4, Time Delay) is taken");

	// Every choice field of every kind, value by value: inside its limit, SetField takes a value exactly when one of its choices
	// names it, so a dropdown and the setter always agree. A choice field needs a limit, or its values couldn't be walked here.
	bool agree = true;
	size_t walked = 0;
	std::string firstProblem;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		StrategyKind const &kind = StrategyKinds[k];

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			Field const &field = kind.fields[f];

			if (field.unit != Unit::Choice)
			{
				continue;
			}

			walked++;
			std::string const name = std::string(kind.name) + "." + field.name;

			if (field.limit == Limit::None)
			{
				agree = false;
				firstProblem = firstProblem.empty() ? name + " has no limit" : firstProblem;
				continue;
			}

			for (double value = field.lowest; value <= field.highest; value++)
			{
				std::vector<uint8_t> bytes = NewOne(kind.number);
				bool const taken = !bytes.empty() && SetField(kind, field, bytes.data(), bytes.size(), value, 0);
				bool const named = ChoiceIsNamed(kind.number, field.name, (int32_t)value);

				if (taken != named)
				{
					agree = false;
					firstProblem = firstProblem.empty() ? name + " " + std::to_string((int)value) : firstProblem;
				}
			}
		}
	}

	Check(walked > 0 && agree,
	      "for each of the " + std::to_string(walked) + " choice fields, every value inside its limit that SetField takes has a name, "
	      "and every named one is taken" + (firstProblem.empty() ? std::string() : " (first problem: " + firstProblem + ")"));
}

// A kind of strategy of the test's own, with a field of each of the three storages no real kind has (UInt16, Float32 and Float64), so
// that every storage goes through the setter and the reader the same way. It's never offered, and no number finds it.
static Field const ReadBackFields[] =
{
	{ "u16", "U16", Anchor::Start, 20, Storage::UInt16, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "f32", "F32", Anchor::Start, 24, Storage::Float32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "f64", "F64", Anchor::Start, 28, Storage::Float64, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
};

static StrategyKind const ReadBackKind = { 9999, "ReadBack", "", SizeRule::AnySize, 36, ReadBackFields, 3, false, false, 9999 };

// What ReadField gives back for a refusal: a value the test sets first, so a refusal that changed the value would show.
static double const Sentinel = -12345.0;

// Sets a value with the setter, reads it back with the reader, and says whether the setter took it and the reader gave the same value.
static bool ReadsBackAs(StrategyKind const &kind, Field const &field, std::vector<uint8_t> &bytes, double value)
{
	char const *why = 0;
	double got = Sentinel;

	return SetField(kind, field, bytes.data(), bytes.size(), value, &why) &&
	       ReadField(kind, field, bytes.data(), bytes.size(), &got, &why) && got == value;
}

// What ReadField gives for a field, with *ok saying whether it read it, and the reason it gave in *why (when asked for). A refusal
// gives the sentinel back.
static double ReadOf(StrategyKind const &kind, Field const &field, std::vector<uint8_t> const &bytes, bool *ok, char const **why = 0)
{
	double value = Sentinel;
	char const *reason = 0;
	*ok = ReadField(kind, field, bytes.data(), bytes.size(), &value, &reason);

	if (why)
	{
		*why = reason;
	}

	return value;
}

static void FieldsAreReadBack()
{
	printf("A field is read back exactly as the file holds it (the setters' inverses):\n");

	StrategyKind const *const door = FindStrategyKind(50);
	StrategyKind const *const fan = FindStrategyKind(75);
	StrategyKind const *const bin = FindStrategyKind(52);
	StrategyKind const *const object = FindStrategyKind(62);
	StrategyKind const *const volume = FindStrategyKind(77);
	StrategyKind const *const lift = FindStrategyKind(51);
	bool ok = false;
	char const *why = 0;

	// Every storage: the setter's value comes back, at the ends of its range too, so a sign or a width read wrong would show.
	Field const *const open = FieldOf(door, "TimeToOpen");
	Field const *const spare = FieldOf(door, "spare");
	Field const *const doorFlags = FieldOf(door, "DoorFlags");
	std::vector<uint8_t> doorBytes = NewOne(50);
	Check(door && open && ReadsBackAs(*door, *open, doorBytes, 1) && ReadsBackAs(*door, *open, doorBytes, 15) &&
	      ReadsBackAs(*door, *open, doorBytes, 255),
	      "Byte: 1, 15 and 255 (0 is refused: the old editor takes 0.1 s at least) come back as they went in (a byte is never read as a negative number)");

	// A switch door's two times share one word (inteng FUN_0052d060: open in bits 20-25, close in bits 26-31, 0 to 63 tenths): each is read and set alone
	{
		StrategyKind const *const sd = FindStrategyKind(59);
		Field const *const open = FieldOf(sd, "TimeToOpen");
		Field const *const close = FieldOf(sd, "TimeToClose");
		Field const *const spare = FieldOf(sd, "spare1");
		std::vector<uint8_t> bytes = NewOne(59);
		bool ok = false;
		double closeNow = Sentinel;
		double spareNow = Sentinel;
		Check(sd && open && close && spare && ReadsBackAs(*sd, *open, bytes, 63) && ReadsBackAs(*sd, *close, bytes, 17) && ReadOf(*sd, *open, bytes, &ok) == 63 &&
		      (closeNow = ReadOf(*sd, *close, bytes, &ok)) == 17 && (spareNow = ReadOf(*sd, *spare, bytes, &ok)) == 0 &&
		      Allowed(59, "TimeToOpen", 0) && !Allowed(59, "TimeToOpen", 64) && !Allowed(59, "TimeToClose", 64),
		      "a switch door's open and close times (0 to 63 tenths) share one word, each set and read alone, the spare bits left alone");
	}

	Check(door && spare && ReadsBackAs(*door, *spare, doorBytes, -32768) && ReadsBackAs(*door, *spare, doorBytes, -1) &&
	      ReadsBackAs(*door, *spare, doorBytes, 32767),
	      "Int16: -32768, -1 and 32767 come back as they went in (a sign is kept)");
	Check(door && doorFlags && ReadsBackAs(*door, *doorFlags, doorBytes, 0) && ReadsBackAs(*door, *doorFlags, doorBytes, 31) &&
	      ReadsBackAs(*door, *doorFlags, doorBytes, 4294967295.0),
	      "UInt32: 0, 31 and 4,294,967,295 come back as they went in (a word is never read as a negative number)");

	Field const *const wind = FieldOf(fan, "fan_wind_strength");
	Field const *const pad = FieldOf(fan, "pad2");
	std::vector<uint8_t> fanBytes = NewOne(75);
	Check(fan && wind && ReadsBackAs(*fan, *wind, fanBytes, -1000) && ReadsBackAs(*fan, *wind, fanBytes, 1000),
	      "Int32: -1000 and 1000 come back as they went in");
	Check(fan && pad && ReadsBackAs(*fan, *pad, fanBytes, -2147483648.0) && ReadsBackAs(*fan, *pad, fanBytes, 2147483647),
	      "Int32: the two ends of the storage come back as they went in");

	std::vector<uint8_t> testBytes(36, 0);
	Check(ReadsBackAs(ReadBackKind, ReadBackFields[0], testBytes, 0) && ReadsBackAs(ReadBackKind, ReadBackFields[0], testBytes, 65535),
	      "UInt16: 0 and 65535 come back as they went in");
	// Bytes 34 12, then AB CD after them: a reader that took the high byte first, or read four bytes, wouldn't give 0x1234.
	testBytes[20] = 0x34;
	testBytes[21] = 0x12;
	testBytes[22] = 0xAB;
	testBytes[23] = 0xCD;
	double word16 = Sentinel;
	Check(ReadField(ReadBackKind, ReadBackFields[0], testBytes.data(), testBytes.size(), &word16, &why) && word16 == 0x1234,
	      "UInt16: the bytes 34 12 read as 0x1234, low byte first, and only the field's own two bytes are read");
	testBytes[20] = 0;
	testBytes[21] = 0;
	testBytes[22] = 0;
	testBytes[23] = 0;
	Check(ReadsBackAs(ReadBackKind, ReadBackFields[1], testBytes, 1.5) && ReadsBackAs(ReadBackKind, ReadBackFields[1], testBytes, -2.25) &&
	      ReadsBackAs(ReadBackKind, ReadBackFields[1], testBytes, FLT_MAX),
	      "Float32: 1.5, -2.25 and the largest come back as they went in");
	Check(ReadsBackAs(ReadBackKind, ReadBackFields[2], testBytes, 1234.5678) && ReadsBackAs(ReadBackKind, ReadBackFields[2], testBytes, -0.5) &&
	      ReadsBackAs(ReadBackKind, ReadBackFields[2], testBytes, 1e300),
	      "Float64: 1234.5678, -0.5 and 1e300 come back as they went in");

	// A read changes nothing.
	std::vector<uint8_t> const doorBefore = doorBytes;
	ReadOf(*door, *open, doorBytes, &ok);
	Check(ok && doorBytes == doorBefore, "reading a field changes no byte");

	// A masked field gives its own bits, after the mask and not shifted: a placed object's integrity is the low byte of a word, and its
	// explosion is two bits in the middle of another.
	Field const *const integrity = FieldOf(object, "integrity");
	Field const *const explosion = FieldOf(object, "explosion");
	Field const *const objectFlags = FieldOf(object, "flags");
	std::vector<uint8_t> placed = NewOne(62);
	PutInt(placed, 32, (int32_t)0xAABBCCDD);
	PutInt(placed, 36, (int32_t)0xFFFFFF1F);
	Check(object && integrity && ReadOf(*object, *integrity, placed, &ok) == 0xDD && ok,
	      "a masked field reads only its mask's bits: the low byte of 0xAABBCCDD is 0xDD");
	Check(object && explosion && ReadOf(*object, *explosion, placed, &ok) == 0x10 && ok,
	      "and not shifted: the explosion bits of 0xFFFFFF1F are 0x10, not 1");
	Check(object && objectFlags && ReadOf(*object, *objectFlags, placed, &ok) == 0x3 && ok,
	      "two fields sharing a word read each its own bits: the flags of 0xFFFFFF1F are 0x3");
	Check(object && integrity && ReadsBackAs(*object, *integrity, placed, 100) && GetInt(placed, 32) == (int32_t)0xAABBCC64,
	      "a masked value set and read back agrees, and the other bits of the word are as they were");
	Check(object && explosion && ReadsBackAs(*object, *explosion, placed, 0x20) && GetInt(placed, 36) == (int32_t)0xFFFFFF2F,
	      "and so does a masked choice");

	// A corner of a box: axis 0, 1 and 2 are x, y and z, and there's no other.
	Field const *const corner = FieldOf(volume, "volume_min");
	std::vector<uint8_t> box = NewOne(77);
	PutInt(box, 20, 1);
	PutInt(box, 24, -2);
	PutInt(box, 28, 3);
	int32_t axisValue = 77777;
	Check(volume && corner && ReadFieldAxis(*volume, *corner, box.data(), box.size(), 0, &axisValue, &why) && axisValue == 1,
	      "ReadFieldAxis reads axis 0, x");
	Check(volume && corner && ReadFieldAxis(*volume, *corner, box.data(), box.size(), 1, &axisValue, &why) && axisValue == -2,
	      "axis 1, y, with its sign");
	Check(volume && corner && ReadFieldAxis(*volume, *corner, box.data(), box.size(), 2, &axisValue, &why) && axisValue == 3,
	      "and axis 2, z");
	axisValue = 77777;
	Check(volume && corner && !ReadFieldAxis(*volume, *corner, box.data(), box.size(), 3, &axisValue, &why) && axisValue == 77777,
	      "there's no axis 3, and the value is left alone");
	Check(volume && corner && !ReadFieldAxis(*volume, *corner, box.data(), box.size(), -1, &axisValue, &why) && axisValue == 77777,
	      "nor axis -1");
	ReadOf(*volume, *corner, box, &ok);
	Check(volume && corner && !ok, "a corner isn't one number: ReadField refuses it");
	axisValue = 77777;
	Check(door && open && !ReadFieldAxis(*door, *open, doorBytes.data(), doorBytes.size(), 0, &axisValue, &why) && axisValue == 77777,
	      "and a number isn't a corner: ReadFieldAxis refuses it, and leaves the value alone");

	// An object field: its two halves, as the file stores them, and 0 and 0 for no object.
	Field const *const target = FieldOf(object, "targetID");
	uint32_t first = 11;
	uint32_t second = 22;
	std::vector<uint8_t> freshPlaced = NewOne(62);
	Check(object && target && ReadFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), &first, &second) && first == 0 &&
	      second == 0,
	      "a new placed object's target is 0 and 0: no object");
	Check(object && target && SetFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), 7, 9) &&
	      ReadFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), &first, &second) && first == 7 && second == 9,
	      "an object set with its two halves reads back with them: first, then second");
	Check(object && target && SetFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), 0xFFFFFFFFu, 0x80000000u) &&
	      ReadFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), &first, &second) && first == 0xFFFFFFFFu &&
	      second == 0x80000000u,
	      "the halves are whole words with no sign");
	first = 11;
	second = 22;
	Check(object && integrity && !ReadFieldObject(*object, *integrity, freshPlaced.data(), freshPlaced.size(), &first, &second) &&
	      first == 11 && second == 22,
	      "a number isn't an object: ReadFieldObject refuses it and leaves the halves alone");
	ReadOf(*object, *target, freshPlaced, &ok);
	Check(object && target && !ok, "and an object isn't one number: ReadField refuses it");

	// A field after a list is read from after the list, the list's count measured against the bytes first.
	Field const *const floorField = FieldOf(lift, "Floor");
	Field const *const facing = FieldOf(lift, "Facing");
	Field const *const assocDoor = FieldOf(lift, "AssocDoor");
	Field const *const linked = FieldOf(lift, "NumAssocLifts");
	std::vector<uint8_t> liftBytes = Payload(51, 72);
	PutInt(liftBytes, 20, 1);
	PutInt(liftBytes, 32, 5);
	PutInt(liftBytes, 36, 6);
	PutInt(liftBytes, 60, 123);
	PutInt(liftBytes, 68, 7);
	Check(lift && linked && ReadOf(*lift, *linked, liftBytes, &ok) == 1 && ok, "a list's count reads as the count the file holds");
	Check(lift && floorField && ReadOf(*lift, *floorField, liftBytes, &ok) == 123 && ok,
	      "a field after a list of one is read from 8 bytes further on: Floor, 28 after the list's end, is 123");
	Check(lift && facing && ReadOf(*lift, *facing, liftBytes, &ok) == 7 && ok, "a field counted from the end is read from the end: Facing is 7");
	Check(lift && assocDoor && ReadFieldObject(*lift, *assocDoor, liftBytes.data(), liftBytes.size(), &first, &second) && first == 5 &&
	      second == 6,
	      "an object after the list is read from there too");
	PutInt(liftBytes, 20, 1000);
	first = 11;
	second = 22;
	bool floorRead = true;
	ReadOf(*lift, *floorField, liftBytes, &floorRead);
	Check(lift && floorField && assocDoor && !floorRead &&
	      !ReadFieldObject(*lift, *assocDoor, liftBytes.data(), liftBytes.size(), &first, &second) && first == 11 && second == 22,
	      "a list count bigger than the bytes there are is refused, for a number and for an object, and nothing is read");

	// A choice is read as the file holds it, even when its list has no name for it.
	Field const *const mode = FieldOf(bin, "Mode");
	Field const *const objectType = FieldOf(object, "Type");
	std::vector<uint8_t> switchBytes = NewOne(52);
	PutInt(switchBytes, 24, 77);
	PutInt(freshPlaced, 20, 99);
	Check(bin && mode && ReadOf(*bin, *mode, switchBytes, &ok) == 77 && ok,
	      "a switch's mode that no choice names is read as 77: the panel shows it and flags it");
	Check(object && objectType && ReadOf(*object, *objectType, freshPlaced, &ok) == 99 && ok, "and so is an object's type of 99");

	// A value that isn't a real number is given back as it is: the panel shows it and flags it.
	std::vector<uint8_t> nan32(36, 0);
	PutInt(nan32, 24, (int32_t)0x7FC00000);
	double got = Sentinel;
	Check(ReadField(ReadBackKind, ReadBackFields[1], nan32.data(), nan32.size(), &got, &why) && IsNaN(got),
	      "Float32: a NaN in the file reads as a NaN");
	PutInt(nan32, 24, (int32_t)0x7F800001);
	got = Sentinel;
	Check(ReadField(ReadBackKind, ReadBackFields[1], nan32.data(), nan32.size(), &got, &why) && IsNaN(got),
	      "Float32: so does a signalling NaN");
	PutInt(nan32, 24, (int32_t)0x7F800000);
	got = Sentinel;
	Check(ReadField(ReadBackKind, ReadBackFields[1], nan32.data(), nan32.size(), &got, &why) && IsNaNOrEndless(got) && !IsNaN(got),
	      "Float32: an endless value reads as endless, not as a NaN");
	std::vector<uint8_t> nan64(36, 0);
	PutInt(nan64, 32, (int32_t)0x7FF80000);
	got = Sentinel;
	Check(ReadField(ReadBackKind, ReadBackFields[2], nan64.data(), nan64.size(), &got, &why) && IsNaN(got),
	      "Float64: a NaN in the file reads as a NaN");

	// Refusals: each leaves the value alone, and gives its reason where the setter would.
	Field const *const speedUp = FieldOf(fan, "speed_up_time");
	std::vector<uint8_t> doorForRefusals = NewOne(50);
	std::vector<uint8_t> const doorKept = doorForRefusals;
	why = 0;
	got = ReadOf(*door, *speedUp, doorForRefusals, &ok, &why);
	Check(speedUp && !ok && got == Sentinel && why == NotThisKindsReason && doorForRefusals == doorKept,
	      "another kind's field is refused with the same reason the setter gives, and the value is left alone");
	Field const *const kindNumber = BaseField("StrategyType");
	why = 0;
	got = ReadOf(*door, *kindNumber, doorForRefusals, &ok, &why);
	Check(kindNumber && !ok && got == Sentinel && why == NotThisKindsReason,
	      "the kind's own number is refused too, as it is by the setter");
	Field const *const type = BaseField("Type");
	PutInt(doorForRefusals, 12, 4);
	Check(type && ReadOf(*door, *type, doorForRefusals, &ok) == 4 && ok, "a field every strategy has reads in any kind: Type is 4");

	std::vector<uint8_t> shortFan = Payload(75, 23);
	std::vector<uint8_t> fitsFan = Payload(75, 24);
	why = 0;
	got = ReadOf(*fan, *speedUp, shortFan, &ok, &why);
	Check(!ok && got == Sentinel && why == 0, "a payload one byte too short for the field is refused, the value is left alone, and no reason is made up");
	Check(ReadOf(*fan, *speedUp, fitsFan, &ok) == 0 && ok, "and one byte longer holds it");

	double none = Sentinel;
	Check(!ReadField(*fan, *speedUp, 0, 0, &none, &why) && none == Sentinel, "no payload at all, of length 0, is refused");
	bool readRefused = false;
	bool readRan = Survives([&] { readRefused = !ReadField(*fan, *speedUp, 0, 24, &none, &why); });
	Check(readRan && readRefused && none == Sentinel, "and so is no payload with a length said");
	readRefused = false;
	readRan = Survives([&] { readRefused = !ReadField(*fan, *speedUp, fitsFan.data(), fitsFan.size(), 0, &why); });
	Check(readRan && readRefused, "nowhere to put the value is refused, and nothing is read");
	int32_t noAxis = 77777;
	Check(!ReadFieldAxis(*volume, *corner, 0, 0, 0, &noAxis, &why) && noAxis == 77777, "no payload is refused by the axis reader too");
	uint32_t noFirst = 11;
	uint32_t noSecond = 22;
	Check(!ReadFieldObject(*object, *target, 0, 0, &noFirst, &noSecond) && noFirst == 11 && noSecond == 22,
	      "and by the object reader");

	// The guards that keep these readers off a null pointer when a length IS said (FieldPlace alone would refuse a length of 0). Each
	// check runs under Survives, so a guard that's gone makes ITS check FAILED, by name, instead of stopping the whole program.
	bool refused = false;
	noAxis = 77777;
	bool ran = Survives([&] { refused = !ReadFieldAxis(*volume, *corner, 0, box.size(), 0, &noAxis, &why); });
	Check(ran && refused && noAxis == 77777, "the axis reader refuses no payload with a length said, and reads nothing");
	refused = false;
	ran = Survives([&] { refused = !ReadFieldAxis(*volume, *corner, box.data(), box.size(), 0, 0, &why); });
	Check(ran && refused, "the axis reader refuses nowhere to put the value, and reads nothing");
	noFirst = 11;
	noSecond = 22;
	refused = false;
	ran = Survives([&] { refused = !ReadFieldObject(*object, *target, 0, freshPlaced.size(), &noFirst, &noSecond); });
	Check(ran && refused && noFirst == 11 && noSecond == 22, "the object reader refuses no payload with a length said, and leaves the halves alone");
	refused = false;
	ran = Survives([&] { refused = !ReadFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), 0, &noSecond); });
	Check(ran && refused && noSecond == 22, "the object reader refuses nowhere to put the first half, and leaves the second alone");
	refused = false;
	ran = Survives([&] { refused = !ReadFieldObject(*object, *target, freshPlaced.data(), freshPlaced.size(), &noFirst, 0); });
	Check(ran && refused && noFirst == 11, "and nowhere to put the second half, and leaves the first alone");
	std::vector<uint8_t> shortPlaced = Payload(62, 51);
	Check(!ReadFieldObject(*object, *target, shortPlaced.data(), shortPlaced.size(), &noFirst, &noSecond) && noFirst == 11 && noSecond == 22,
	      "an object field that runs one byte past the payload is refused, and the halves are left alone");
	std::vector<uint8_t> shortBox = Payload(77, 31);
	Check(!ReadFieldAxis(*volume, *corner, shortBox.data(), shortBox.size(), 0, &noAxis, &why) && noAxis == 77777,
	      "and so is a box corner that runs one byte past it");
}

int main()
{
	TablesHoldTogether();
	StorageRangesAreTheStorages();
	SizesAreTheGames();
	ListsAreMeasured();
	ValuesAreChecked();
	OldFilesAreUpgraded();
	NewOnesStartAsTheOldEditorMadeThem();
	SettersWriteOnlyTheirPart();
	FieldsOutsideThePayloadAreRefused();
	LimitsAreTheirEnds();
	ListCountsAreNotNumbers();
	FieldsBelongToTheirKind();
	ChoicesAreNamed();
	FieldsAreReadBack();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
