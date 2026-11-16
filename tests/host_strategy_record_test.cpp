// The test for src/host/strategy_record: one strategy from a level turned into the field panel's read-only record. Written 2026-11-14 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds strategy payloads in memory, some of them hostile, builds their records, and
// checks the records against the field model's tables and against the panel's own check of a list (ui::CheckFieldList). The program gives
// back a non-zero result if any check failed. Each check's words say what it proves.

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "crash_guard.h"
#include "field_descriptor_from_model.h"
#include "field_model.h"
#include "real_numbers.h"
#include "strategy_record.h"

using namespace model;
using host::BuildStrategyRecord;
using host::BuildStrategyRecordOfKind;
using host::StrategyRecord;

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

// A strategy payload of that kind: every byte 0 but its kind, so every list is empty.
static std::vector<uint8_t> Zeroed(StrategyKind const &kind)
{
	std::vector<uint8_t> bytes(kind.fixedBytes < 20 ? 20 : kind.fixedBytes, 0);
	PutInt(bytes, 8, kind.number);
	return bytes;
}

// A new strategy as the old editor made one: every field at its creation value.
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

static Field const *FieldOf(StrategyKind const *kind, char const *name)
{
	for (size_t i = 0; kind && i < kind->fieldCount; i++)
	{
		if (strcmp(kind->fields[i].name, name) == 0)
		{
			return &kind->fields[i];
		}
	}

	return 0;
}

static bool PanelAccepts(StrategyRecord const &record)
{
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };
	return ui::CheckFieldList(record.list.data(), record.list.size(), &problem);
}

static std::vector<std::string> NamesOf(StrategyRecord const &record)
{
	std::vector<std::string> names;

	for (size_t i = 0; i < record.list.size(); i++)
	{
		names.push_back(record.list[i].name);
	}

	return names;
}

static int Place(StrategyRecord const &record, char const *name)
{
	for (size_t i = 0; i < record.list.size(); i++)
	{
		if (strcmp(record.list[i].name, name) == 0)
		{
			return (int)i;
		}
	}

	return -1;
}

static std::string TextOf(StrategyRecord const &record, int place)
{
	ui::FieldValue const &value = record.values.at((size_t)place);
	return std::string((char const *)value.bytes, value.length);
}

static void DoorRecord()
{
	printf("A door's record:\n");

	StrategyKind const *const door = FindStrategyKind(50);
	std::vector<uint8_t> bytes = NewOne(50);
	std::vector<uint8_t> const before = bytes;
	StrategyRecord record;
	char const *why = 0;

	Check(BuildStrategyRecord(bytes.data(), bytes.size(), &record, &why), "a new door's record is built");
	Check(bytes == before, "building it changes no byte of the payload");
	Check(record.title == "Door (50)", "its title is the old editor's name for the kind and its number: Door (50)");

	std::vector<std::string> const expected = {
		"Door is locked", "Proximity door", "Starts open", "Switch activated door ('lift door')",
		"Door is horizontal (ie. in floor/ceiling)", "Time take to open (0.1 - 25 sec)", "Time take to close (0.1 - 25 sec)" };
	Check(NamesOf(record) == expected,
	      "it shows the five flags by their own words, then the two times: the field with no label (spare), and the fields every strategy has, are left out");

	bool allReadOnly = !record.list.empty();

	for (size_t i = 0; i < record.list.size(); i++)
	{
		allReadOnly = allReadOnly && record.list[i].readOnly;
	}

	Check(allReadOnly, "every descriptor is read-only");
	Check(PanelAccepts(record) && record.values.size() == record.list.size(), "the panel's own check accepts the list, with a value for each");

	bool bitsAreRight = record.list.size() == 7;
	uint32_t const bitsOf[5] = { 0x1, 0x2, 0x4, 0x8, 0x10 };

	for (size_t i = 0; i < 5 && bitsAreRight; i++)
	{
		bitsAreRight = record.list[i].kind == ui::FieldKind::Bit && record.list[i].bit == bitsOf[i];
	}

	Check(bitsAreRight, "the five are Bit boxes, each with its own bit");
	Check(record.list.size() == 7 && record.list[5].kind == ui::FieldKind::Number && record.list[5].unit && strcmp(record.list[5].unit, "tenths s") == 0 &&
	      record.list[5].lowest == 1 && record.list[5].highest == 255 && record.list[5].decimals == 0,
	      "an opening time is a whole Number, 1 to 255 (a byte, inteng refuses 0), in tenths of a second");

	// A value set in the payload is the value shown
	Field const *const open = FieldOf(door, "TimeToOpen");
	Field const *const flags = FieldOf(door, "DoorFlags");
	char const *reason = 0;
	Check(open && flags && SetField(*door, *open, bytes.data(), bytes.size(), 15, &reason) &&
	      SetField(*door, *flags, bytes.data(), bytes.size(), 0x3, &reason) && BuildStrategyRecord(bytes.data(), bytes.size(), &record, &why),
	      "a door with its opening time 15 and its flags 3 is built again into the same record");
	Check(record.list.size() == 7, "and the record holds the new one alone, not both");
	Check(record.list.size() == 7 && record.values[5].number == 15 && record.values[6].number == 5,
	      "the times shown are the ones in the payload: 15 and the new door's 5");
	bool wordsAreRight = record.list.size() == 7;

	for (size_t i = 0; i < 5 && wordsAreRight; i++)
	{
		wordsAreRight = record.values[i].word == 3;
	}

	Check(wordsAreRight, "and each flag's value is the whole flags word, 3, which its box takes its own bit from");

	// A record can be moved, and what it points to goes with it
	StrategyRecord moved(std::move(record));
	Check(moved.list.size() == 7 && strcmp(moved.list[0].name, "Door is locked") == 0 && moved.title == "Door (50)",
	      "a record can be moved, and its descriptors still point to their words");
	static_assert(!std::is_copy_constructible<StrategyRecord>::value, "a copy of a record would point into the one it came from");
	Check(!std::is_copy_constructible<StrategyRecord>::value && !std::is_copy_assignable<StrategyRecord>::value,
	      "a record can't be copied, as a copy would point into the one it came from");
}

static void BinarySwitchRecord()
{
	printf("A binary switch's record:\n");

	StrategyKind const *const bin = FindStrategyKind(52);
	std::vector<uint8_t> bytes = NewOne(52);
	StrategyRecord record;
	char const *why = 0;
	Check(BuildStrategyRecord(bytes.data(), bytes.size(), &record, &why) && PanelAccepts(record), "a new switch's record is built, and the panel accepts it");
	Check(record.title == "Binary Switch (52)", "its title is Binary Switch (52)");

	int const mode = Place(record, "Mode");
	Check(mode >= 0 && record.list[(size_t)mode].kind == ui::FieldKind::Choice, "its mode is a Choice box");

	// Its choices are the table's, in order
	std::vector<ChoiceName> named;

	for (size_t i = 0; i < StrategyChoiceCount; i++)
	{
		if (StrategyChoices[i].kind == 52 && strcmp(StrategyChoices[i].field, "Mode") == 0)
		{
			named.push_back(StrategyChoices[i]);
		}
	}

	bool sameChoices = mode >= 0 && !named.empty() && record.list[(size_t)mode].choiceCount == named.size();

	for (size_t i = 0; sameChoices && i < named.size(); i++)
	{
		sameChoices = record.list[(size_t)mode].choices[i].value == named[i].value &&
		              strcmp(record.list[(size_t)mode].choices[i].words, named[i].label) == 0;
	}

	Check(sameChoices, "and they are the table's named values for it, each with its value and its words, in the table's order");
	Check(mode >= 0 && record.values[(size_t)mode].choice == 1, "a new switch's mode is 1, which is Wait");

	// Its flags: one Bit for each named bit that isn't always set
	std::vector<BitName> bits;

	for (size_t i = 0; i < StrategyBitCount; i++)
	{
		if (StrategyBits[i].kind == 52 && strcmp(StrategyBits[i].field, "flags") == 0 && !StrategyBits[i].alwaysSet)
		{
			bits.push_back(StrategyBits[i]);
		}
	}

	bool sameBits = !bits.empty();

	for (size_t i = 0; sameBits && i < bits.size(); i++)
	{
		int const at = Place(record, bits[i].label);
		sameBits = at >= 0 && record.list[(size_t)at].kind == ui::FieldKind::Bit && record.list[(size_t)at].bit == bits[i].bit;
	}

	Check(sameBits, "its flags are one Bit box for each of the table's named bits that the editor doesn't always set, each with its own bit");
	int const rest = Place(record, "Rest state on");
	Check(rest >= 0 && record.list[(size_t)rest].bit == 0x1, "the first is Rest state on, bit 1");

	// Its target is an object: none to begin with, then its two halves in hex
	int const target = Place(record, "Switch's target");
	Check(target >= 0 && record.list[(size_t)target].kind == ui::FieldKind::Text && TextOf(record, target) == "none" &&
	      record.list[(size_t)target].capacity >= 17,
	      "its target is a Text that says none for an object of 0 and 0");
	Field const *const targetField = FieldOf(bin, "Target");
	Check(targetField && SetFieldObject(*bin, *targetField, bytes.data(), bytes.size(), 7, 0xABCDEF01u) &&
	      BuildStrategyRecord(bytes.data(), bytes.size(), &record, &why) && Place(record, "Switch's target") >= 0 &&
	      TextOf(record, Place(record, "Switch's target")) == "00000007:ABCDEF01",
	      "and its two halves in hex, first, then second, once it has an object");
	Check(PanelAccepts(record) && record.list[(size_t)Place(record, "Switch's target")].capacity >= TextOf(record, Place(record, "Switch's target")).size(),
	      "which fits the Text's capacity, so the panel accepts it");

	// Units
	int const time = Place(record, "seconds");
	int const security = Place(record, "Security :");
	Check(time >= 0 && record.list[(size_t)time].unit && strcmp(record.list[(size_t)time].unit, "s/65536") == 0,
	      "a time in sixty-five-thousandths of a second says so: s/65536");
	Check(security >= 0 && record.list[(size_t)security].unit == 0 && record.list[(size_t)security].highest == (double)INT_MAX,
	      "a plain number has no unit, and its range is its limit's: 0 to the largest whole number");

	// A choice the table doesn't name is kept as the file holds it
	PutInt(bytes, 24, 77);
	Check(BuildStrategyRecord(bytes.data(), bytes.size(), &record, &why) && Place(record, "Mode") >= 0 &&
	      record.values[(size_t)Place(record, "Mode")].choice == 77 && PanelAccepts(record),
	      "a mode no choice names, 77, is built with its value kept, and the panel accepts it (it flags it)");
}

static void EnemyAndFanRecords()
{
	printf("An enemy's and a fan's records:\n");

	StrategyRecord record;
	char const *why = 0;
	std::vector<uint8_t> enemy = NewOne(69);
	Check(BuildStrategyRecord(enemy.data(), enemy.size(), &record, &why), "a new enemy's record is built");
	std::vector<std::string> const enemyNames = { "Mission", "Turn Death Target On/Off", "Death target", "Path - %d" };
	Check(record.title == "Enemy (69)" && NamesOf(record) == enemyNames,
	      "it shows its mission, its two targets' words and its path, and leaves out its three spares");
	Check(record.list.size() == 4 && record.list[0].kind == ui::FieldKind::Choice && record.list[0].choiceCount == 6 &&
	      record.list[1].kind == ui::FieldKind::Number && record.list[2].kind == ui::FieldKind::Text && record.list[3].kind == ui::FieldKind::Number,
	      "a choice of six missions, two numbers and an object, in that order");
	Check(PanelAccepts(record), "the panel accepts it");

	std::vector<uint8_t> fan = NewOne(75);
	Check(BuildStrategyRecord(fan.data(), fan.size(), &record, &why), "a new fan's record is built");
	std::vector<std::string> const fanNames = { "Speed up time (seconds)", "Slow down time (seconds)", "Wind strength (-1000 to 1000)" };
	Check(record.title == "Fan (75)" && NamesOf(record) == fanNames, "it shows its two times and its wind, and leaves out its spare");
	Check(record.list.size() == 3 && strcmp(record.list[0].unit, "s") == 0 && strcmp(record.list[2].unit, "%") == 0 &&
	      record.list[0].lowest == 1 && record.list[0].highest == 999 && record.list[2].lowest == -1000 && record.list[2].highest == 1000,
	      "its times are in whole seconds, 1 to 999, and its wind in % from -1000 to 1000, the limits the table gives");
	Check(record.values.size() == 3 && record.values[0].number == 10 && record.values[2].number == 100, "their values are the payload's: 10 seconds (655360 in the file) and 100");
	Check(record.list.size() == 3 && record.list[2].help && strstr(record.list[2].help, "the game reads anything else as 100"),
	      "a field's help is the table's reason for its limit");
	Check(PanelAccepts(record), "the panel accepts it");

	// A corner of a box is three numbers
	StrategyKind const *const volume = FindStrategyKind(77);
	std::vector<uint8_t> box = NewOne(77);
	PutInt(box, 20, 1);
	PutInt(box, 24, -2);
	PutInt(box, 28, 3);
	Check(BuildStrategyRecord(box.data(), box.size(), &record, &why) && volume, "a volume of death's record is built");
	int const x = Place(record, "V : Edit Volume x");
	Check(Place(record, "V : Edit Volume y") == x + 1 && Place(record, "V : Edit Volume z") == x + 2 && x >= 0,
	      "its corner, which the old editor labelled V : Edit Volume, is three Numbers, one for each axis, in x, y, z order");
	Check(x >= 0 && record.values[(size_t)x].number == 1 && record.values[(size_t)x + 1].number == -2 && record.values[(size_t)x + 2].number == 3,
	      "with the corner's three numbers");
	Check(x >= 0 && record.list[(size_t)x].lowest == -16777216 && record.list[(size_t)x].highest == 16777216 &&
	      record.list[(size_t)x].unit && strcmp(record.list[(size_t)x].unit, "mm") == 0,
	      "each within 16,777,216 game units of the centre, in mm: the corner's own limit");
	std::vector<std::string> const volumeNames = { "V : Edit Volume x", "V : Edit Volume y", "V : Edit Volume z", "Starts on", "Collision not required",
	                                               "Type in the damage per second. Type 0 for infini" };
	Check(NamesOf(record) == volumeNames, "the other corner, which the old editor showed no label for, isn't shown, and neither are its spares");

	// The one real label longer than a descriptor takes is cut to what it takes
	Field const *const damage = FieldOf(volume, "damage");
	std::string const cut = damage ? std::string(damage->label).substr(0, 48) : std::string();
	Check(damage && strlen(damage->label) == 57 && Place(record, cut.c_str()) >= 0 && cut.size() == 48,
	      "the damage's 57-letter label is cut to its first 48 letters, which the panel accepts");
	Check(PanelAccepts(record), "and the panel accepts the volume's list");
}

// What each kind's tables say, to the letters: nothing in the real tables is changed to fit the panel but the words that are too long or
// have a letter the font lacks, and this lists them, so a table that gets one is noticed. At the moment there's one: the volume of death's
// damage label, 57 letters, which is cut to 48.
static void RealWordsFitThePanel()
{
	printf("The real tables' words fit the panel as they are, but one:\n");

	size_t checked = 0;
	std::vector<std::string> problems;

	auto fits = [&](char const *words, size_t most, char const *what) {
		checked++;

		if (!words)
		{
			return;
		}

		size_t length = 0;
		bool letters = true;

		for (; words[length] != 0 && length <= most; length++)
		{
			letters = letters && (unsigned char)words[length] >= 32 && (unsigned char)words[length] <= 126;
		}

		if (length > most || !letters)
		{
			problems.push_back(std::string(what) + ": " + words);
		}
	};

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		StrategyKind const &kind = StrategyKinds[k];

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			// A field of three numbers has two letters added to its words
			fits(kind.fields[f].label, kind.fields[f].storage == Storage::IntTriple ? 46 : 48, "a label");
			fits(kind.fields[f].why, 200, "a reason");
		}
	}

	for (size_t f = 0; f < StrategyBaseFieldCount; f++)
	{
		fits(StrategyBaseFields[f].label, 48, "a label");
		fits(StrategyBaseFields[f].why, 200, "a reason");
	}

	for (size_t i = 0; i < StrategyBitCount; i++)
	{
		fits(StrategyBits[i].label, 48, "a bit's words");
	}

	for (size_t i = 0; i < StrategyChoiceCount; i++)
	{
		fits(StrategyChoices[i].label, 48, "a choice's words");
	}

	std::vector<std::string> const known = { "a label: Type in the damage per second. Type 0 for infinite damage" };
	std::string listed;

	for (std::string const &problem : problems)
	{
		listed += " [" + problem + "]";
	}

	Check(checked > 100 && problems == known,
	      "of the " + std::to_string(checked) + " labels, reasons, bit names and choice names, every one is at most what a descriptor takes, in letters the font has, "
	      "but the volume's damage label; problems found:" + listed);
}

static void EveryKind()
{
	printf("Every kind the model has:\n");

	size_t zeroed = 0;
	size_t made = 0;
	size_t fieldsShown = 0;
	size_t bitsShown = 0;
	size_t mostFields = 0;
	std::string firstProblem;

	auto note = [&](std::string const &problem) {
		if (firstProblem.empty())
		{
			firstProblem = problem;
		}
	};

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		StrategyKind const &kind = StrategyKinds[k];
		std::string const name = std::string(kind.name) + " (" + std::to_string(kind.number) + ")";
		std::vector<uint8_t> const blank = Zeroed(kind);
		StrategyRecord record;
		char const *why = 0;

		if (!StrategySizeIsRight(blank.data(), blank.size()))
		{
			note(name + ": its empty payload isn't the right size");
		}
		else if (!BuildStrategyRecord(blank.data(), blank.size(), &record, &why))
		{
			note(name + " from an empty payload: " + (why ? why : "no reason"));
		}
		else if (!PanelAccepts(record) || record.values.size() != record.list.size() || record.list.size() > 64)
		{
			note(name + " from an empty payload: the panel's check refuses its list");
		}
		else
		{
			// The Bit boxes are the kind's flags' named bits, labelled, and not the bits the editor always sets
			size_t shownBits = 0;

			for (size_t i = 0; i < record.list.size(); i++)
			{
				shownBits += record.list[i].kind == ui::FieldKind::Bit ? 1 : 0;
			}

			size_t namedBits = 0;

			for (size_t b = 0; b < StrategyBitCount; b++)
			{
				if (StrategyBits[b].kind == kind.number && !StrategyBits[b].alwaysSet && StrategyBits[b].label[0] && FieldOf(&kind, StrategyBits[b].field))
				{
					namedBits++;
				}
			}

			if (shownBits != namedBits)
			{
				note(name + ": " + std::to_string(shownBits) + " Bit boxes, but " + std::to_string(namedBits) + " named bits that aren't always set");
			}
			else
			{
				zeroed++;
				bitsShown += shownBits;
				fieldsShown += record.list.size();
				mostFields = record.list.size() > mostFields ? record.list.size() : mostFields;
			}
		}

		if (kind.offered)
		{
			std::vector<uint8_t> const fresh = NewOne(kind.number);
			StrategyRecord freshRecord;

			if (fresh.empty() || !BuildStrategyRecord(fresh.data(), fresh.size(), &freshRecord, &why))
			{
				note(name + " as a new one: " + (why ? why : "no reason"));
			}
			else if (!PanelAccepts(freshRecord) || freshRecord.title.empty())
			{
				note(name + " as a new one: the panel's check refuses its list");
			}
			else
			{
				made++;
			}
		}
	}

	Check(zeroed == StrategyKindCount && firstProblem.empty(),
	      "each of the model's " + std::to_string(StrategyKindCount) + " kinds builds from an empty payload, and the panel accepts every list (" +
	      std::to_string(fieldsShown) + " fields in all, the most in one kind " + std::to_string(mostFields) + "), with a Bit box for each named bit that isn't always set, " +
	      std::to_string(bitsShown) + " in all" + (firstProblem.empty() ? std::string() : " (first problem: " + firstProblem + ")"));

	size_t offered = 0;

	for (size_t k = 0; k < StrategyKindCount; k++)
	{
		offered += StrategyKinds[k].offered ? 1 : 0;
	}

	Check(offered > 0 && made == offered && firstProblem.empty(),
	      "each of the " + std::to_string(offered) + " kinds the old editor offered also builds as a new strategy, with its creation values" +
	      (firstProblem.empty() ? std::string() : " (first problem: " + firstProblem + ")"));
}

// A kind of the test's own, for what no real kind has: a field of each fraction storage, words too long or with letters the font lacks, a
// choice with no names, and an orientation. It's never offered, and no number finds it.
static char const LongLabel[] = "LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL";   // 60 letters
static char const LongWhy[] = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"
                             "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"
                             "WWWWWWWWWWWWWWWWWWWWWWWWWWWW";   // 250 letters
static char const AccentedLabel[] = "Caf\xE9 \x01 tab\t";
static Field const ProbeFields[] =
{
	{ "single", "Fraction", Anchor::Start, 20, Storage::Float32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "double", "Double fraction", Anchor::Start, 24, Storage::Float64, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "word", "Word", Anchor::Start, 32, Storage::UInt16, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "long", LongLabel, Anchor::Start, 36, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, LongWhy, false, 0, "" },
	{ "accented", AccentedLabel, Anchor::Start, 40, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "nameless", "Nameless choice", Anchor::Start, 44, Storage::Int32, Unit::Choice, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "turn", "Orientation", Anchor::Start, 48, Storage::IntMatrix, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	{ "hidden", "", Anchor::Start, 84, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
};

static StrategyKind const ProbeKind = { 9999, "Probe", "", SizeRule::AnySize, 88, ProbeFields, 8, false, false, 9999 };

// A field whose range is the wrong way round, which the panel's own check refuses (ui::NumberRuleFor: lowest above highest)
static Field const BackwardsFields[] =
{
	{ "reversed", "Reversed range", Anchor::Start, 20, Storage::Int32, Unit::Plain, 0, Limit::Editor, 10, 5, "a range the wrong way round", false, 0, "" },
};

static StrategyKind const BackwardsKind = { 9998, "Backwards", "", SizeRule::AnySize, 24, BackwardsFields, 1, false, false, 9998 };

static void HostileAndOddRecords()
{
	printf("Hostile and odd payloads:\n");

	char const *why = 0;
	StrategyRecord record;

	// Refusals: each gives its reason and leaves an EMPTY record, even when it was given a full one: the record is first filled with a
	// door's seven fields, then the refusal must empty it. Each runs under Survives (crash_guard.h), so a guard that's gone FAILS its own
	// check by name instead of stopping the program.
	std::vector<uint8_t> door = NewOne(50);
	std::vector<uint8_t> shortDoor(door.begin(), door.end() - 1);
	auto refill = [&]() {
		char const *ignored = 0;
		return BuildStrategyRecord(door.data(), door.size(), &record, &ignored) && record.list.size() == 7;
	};
	auto emptied = [&]() { return record.list.empty() && record.values.empty() && record.title.empty(); };
	bool refused = false;

	why = 0;
	bool ran = refill() && Survives([&] { refused = !BuildStrategyRecord(shortDoor.data(), shortDoor.size(), &record, &why); });
	Check(ran && refused && why && strcmp(why, SizeCheckWords(CheckStrategySize(shortDoor.data(), shortDoor.size(), 0))) == 0 && emptied(),
	      "a door one byte short is refused, for the model's own reason, and the full record it was given is emptied");
	std::vector<uint8_t> tiny(12, 0);
	why = 0;
	ran = refill() && Survives([&] { refused = !BuildStrategyRecord(tiny.data(), tiny.size(), &record, &why); });
	Check(ran && refused && why && strcmp(why, SizeCheckWords(SizeCheck::TooShort)) == 0 && emptied(),
	      "a payload shorter than the 20 bytes every strategy has is refused, for that reason, and the record is emptied");
	std::vector<uint8_t> unknown(28, 0);
	PutInt(unknown, 8, 9999);
	why = 0;
	ran = refill() && StrategySizeIsRight(unknown.data(), unknown.size()) &&
	      Survives([&] { refused = !BuildStrategyRecord(unknown.data(), unknown.size(), &record, &why); });
	Check(ran && refused && why && strstr(why, "doesn't know") && emptied(),
	      "a kind the editor doesn't know, 9999, is refused with its own reason, though the game would take its size, and the record is emptied");
	why = 0;
	ran = refill() && Survives([&] { refused = !BuildStrategyRecord(0, 0, &record, &why); });
	Check(ran && refused && why && emptied(), "no payload at all is refused, and the record is emptied");
	why = 0;
	ran = refill() && Survives([&] { refused = !BuildStrategyRecord(0, 28, &record, &why); });
	Check(ran && refused && why && emptied(), "and no payload with a length said");
	why = 0;
	ran = Survives([&] { refused = !BuildStrategyRecord(door.data(), door.size(), 0, &why); });
	Check(ran && refused && why, "no record to fill is refused");
	ran = Survives([&] { refused = !BuildStrategyRecord(door.data(), door.size(), 0, 0); });
	Check(ran && refused, "and it can be asked with no place for the reason");

	// A kind of the test's own: fractions, odd words, a choice with no names
	std::vector<uint8_t> probe(88, 0);
	PutInt(probe, 20, 0x7FC00000);
	PutInt(probe, 28, 0x7FF80000);
	why = 0;
	bool const built = BuildStrategyRecordOfKind(ProbeKind, probe.data(), probe.size(), &record, &why);
	Check(built, std::string("a kind of the test's own, with fractions and odd words, is built") + (built ? "" : std::string(" (") + (why ? why : "no reason") + ")"));
	Check(record.list.size() == 6 && record.title == "Probe (9999)" && PanelAccepts(record),
	      "it shows its six labelled fields, and not its orientation or its field with no label, and the panel accepts the list");
	Check(record.list.size() == 6 && record.list[0].kind == ui::FieldKind::Number && record.list[0].decimals == 3 &&
	      record.list[0].lowest == -1e9 && record.list[0].highest == 1e9,
	      "a fraction is a Number of three decimals, held to a billion either way, which the panel's number rule takes");
	Check(record.values.size() == 6 && IsNaN(record.values[0].number) && IsNaN(record.values[1].number),
	      "a fraction that isn't a number, as a float and as a double, is built and its value is a NaN: the panel shows it and flags it");
	Check(record.list.size() == 6 && record.list[2].highest == 65535 && record.list[2].lowest == 0, "a UInt16 is a Number from 0 to 65535");

	// Words that are too long, or have letters the font lacks
	Check(record.list.size() == 6 && strlen(record.list[3].name) == 48 && record.list[3].help && strlen(record.list[3].help) == 200,
	      "a label of 60 letters is cut to 48, and a reason of 250 to 200, and the panel still accepts the list");
	Check(record.list.size() == 6 && strcmp(record.list[4].name, "Caf? ? tab?") == 0,
	      "a letter the font lacks is shown as ?, so the label passes the panel's check");
	Check(record.list.size() == 6 && record.list[5].kind == ui::FieldKind::Number && strcmp(record.list[5].name, "Nameless choice") == 0,
	      "a choice with no named value can't be a dropdown, so it's shown as a Number, as it is stored");

	// A record built again replaces the last, and a failed one leaves nothing
	Check(BuildStrategyRecordOfKind(ProbeKind, probe.data(), probe.size(), &record, &why) && record.list.size() == 6, "building again into the same record gives six, not twelve");
	why = 0;
	Check(!BuildStrategyRecordOfKind(ProbeKind, probe.data(), 40, &record, &why) && why && record.list.empty() && record.values.empty() && record.title.empty(),
	      "a payload too short for the kind's fields is refused, with a reason, and the record is empty");
	Check(why && strstr(why, "couldn't be read") != 0, "and the reason says a field couldn't be read");
	Check(!BuildStrategyRecordOfKind(ProbeKind, 0, 0, &record, &why), "no payload is refused for a kind of the test's own too");

	// A record the panel's own check refuses is refused whole, naming the field: that's a mistake in a table
	std::vector<uint8_t> backwards(24, 0);
	why = 0;
	Check(!BuildStrategyRecordOfKind(BackwardsKind, backwards.data(), backwards.size(), &record, &why) && why &&
	      strstr(why, "panel's check (field 0)") != 0 && record.list.empty() && record.title.empty(),
	      "a field whose range is the wrong way round, which the panel's check refuses, refuses the whole record, and names the field");

	// The most fields a record has, 64: that many are accepted, and one more is refused for being too many
	std::vector<Field> many;

	for (int i = 0; i < 65; i++)
	{
		Field const one = { "many", "Many", Anchor::Start, (unsigned)(20 + 4 * i), Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" };
		many.push_back(one);
	}

	StrategyKind manyKind = { 9997, "Many", "", SizeRule::AnySize, 20 + 4 * 65, many.data(), 64, false, false, 9997 };
	std::vector<uint8_t> manyBytes(20 + 4 * 65, 0);
	why = 0;
	Check(BuildStrategyRecordOfKind(manyKind, manyBytes.data(), manyBytes.size(), &record, &why) && record.list.size() == 64 && PanelAccepts(record),
	      std::string("64 fields, the most a record has, are accepted") + (why ? std::string(" (") + why + ")" : std::string()));
	manyKind.fieldCount = 65;
	why = 0;
	Check(!BuildStrategyRecordOfKind(manyKind, manyBytes.data(), manyBytes.size(), &record, &why) && why && strstr(why, "more fields") != 0 &&
	      record.list.empty(),
	      "and 65 are refused, for being more than the panel shows");
}

// A Light Object's type is an UNSIGNED field, so a level can store any of 2^32 numbers in it, and the game keeps whatever it finds. A Choice's
// value is an int32: a number it can't hold is shown as a Number, exactly as the file holds it, and one it can is a Choice, named or not.
static void AnUnsignedChoice()
{
	printf("An unsigned choice:\n");

	struct One
	{
		uint32_t stored;
		bool choice;      // a Choice, or a Number
		double value;
	};
	One const cases[] = { { 3u, true, 3 }, { 0x7FFFFFFFu, true, 2147483647.0 }, { 0x80000000u, false, 2147483648.0 }, { 0xFFFFFFFFu, false, 4294967295.0 } };

	for (One const &one : cases)
	{
		std::vector<uint8_t> bytes = NewOne(64);
		PutInt(bytes, 20, (int32_t)one.stored);

		StrategyRecord record;
		char const *why = 0;
		bool const built = BuildStrategyRecord(bytes.data(), bytes.size(), &record, &why);
		int const place = built ? Place(record, "Light Types") : -1;
		char hex[16];
		snprintf(hex, sizeof(hex), "%08X", (unsigned)one.stored);
		bool const right = place >= 0 && PanelAccepts(record) &&
		                   (one.choice ? (record.list[(size_t)place].kind == ui::FieldKind::Choice && (double)record.values[(size_t)place].choice == one.value)
		                               : (record.list[(size_t)place].kind == ui::FieldKind::Number && record.values[(size_t)place].number == one.value));

		Check(right, std::string("a light effect's type of 0x") + hex + " is built as " + (one.choice ? "a Choice" : "a Number") + " holding " +
		      std::to_string((long long)one.value) + (built ? "" : std::string(" (refused: ") + (why ? why : "no reason") + ")"));
	}

	Check(host::FitsAChoice(2147483647.0) && host::FitsAChoice(-2147483648.0) && host::FitsAChoice(0) && !host::FitsAChoice(2147483648.0) &&
	      !host::FitsAChoice(-2147483649.0) && !host::FitsAChoice(4294967295.0),
	      "FitsAChoice takes the whole of the int32 range and nothing outside it");
	double nan = 0;
	uint64_t const nanBits = 0x7FF8000000000000ull;
	memcpy(&nan, &nanBits, sizeof(nan));
	Check(!host::FitsAChoice(nan), "and a number that isn't one, a NaN, is not taken");
}

// A field of three numbers has two letters added to its words, so its label is cut two letters short first: three names that fit
static Field const TripleFields[] =
{
	{ "corner", "A corner whose label is sixty letters long, which is cut to fit..", Anchor::Start, 20, Storage::IntTriple, Unit::GameUnits, 0,
	  Limit::Editor, -1000, 1000, "", false, 0, "" },
};
static StrategyKind const TripleKind = { 9996, "Triple", "", SizeRule::AnySize, 32, TripleFields, 1, false, false, 9996 };

static void ATripleLabelFits()
{
	printf("A three-number field's label:\n");

	std::vector<uint8_t> bytes(32, 0);
	StrategyRecord record;
	char const *why = 0;
	bool const built = BuildStrategyRecordOfKind(TripleKind, bytes.data(), bytes.size(), &record, &why);
	std::string const label = std::string(TripleFields[0].label);

	Check(label.size() == 65 && built && record.list.size() == 3 && PanelAccepts(record),
	      std::string("a corner whose label is 65 letters is built as three numbers, and the panel accepts the list") +
	      (built ? "" : std::string(" (") + (why ? why : "no reason") + ")"));

	bool fits = built && record.list.size() == 3;

	for (size_t axis = 0; fits && axis < 3; axis++)
	{
		std::string const name = record.list[axis].name;
		fits = name.size() == 48 && name.compare(0, 46, label, 0, 46) == 0 && name.compare(46, 2, std::string(" ") + "xyz"[axis]) == 0;
	}

	Check(fits, "each name is the first 46 letters of the label, then a space and its axis: 48 letters, as many as a descriptor takes");
}

int main()
{
	DoorRecord();
	BinarySwitchRecord();
	EnemyAndFanRecords();
	RealWordsFitThePanel();
	EveryKind();
	HostileAndOddRecords();
	AnUnsignedChoice();
	ATripleLabelFits();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
