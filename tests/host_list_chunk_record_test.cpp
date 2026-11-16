// The test for src/host/list_chunk_record: ONE ITEM of a chunk that holds a list (a track's section, an alternate location, a path's point, ...)
// turned into the field panel's read-only record. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds chunk payloads in memory as the game's own writer lays them out, some of them
// hostile, builds their records, and checks the records against the field model's tables and the panel's own check of a list
// (ui::CheckFieldList). The real tables have few labelled fields (the old editor drew most of these chunks by hand), so the parts a record
// is made of are also tried on a table of the test's own, with a field of each storage. A check that would crash if a guard were lost runs
// under Survives (crash_guard.h), so it FAILS by name instead of stopping the program. The program gives back a non-zero result if any
// check failed.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

#include "crash_guard.h"
#include "field_model.h"
#include "field_panel_host.h"
#include "list_chunk_record.h"
#include "real_numbers.h"
#include "tracks_fields.h"

using namespace model;
using host::BuildChunkItemRecord;
using host::BuildRecordOfParts;
using host::FieldRecord;
using host::PartBit;
using host::PartReader;
using host::PartShown;

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

static void PutFloat(std::vector<uint8_t> &bytes, size_t at, float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	PutInt(bytes, at, (int32_t)bits);
}

static bool PanelAccepts(FieldRecord const &record)
{
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };
	return ui::CheckFieldList(record.list.data(), record.list.size(), &problem);
}

static bool AllReadOnly(FieldRecord const &record)
{
	for (ui::FieldDescriptor const &descriptor : record.list)
	{
		if (!descriptor.readOnly)
		{
			return false;
		}
	}

	return true;
}

// The place of a descriptor of that name in a record, or -1
static int Place(FieldRecord const &record, char const *name)
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

// Whether a record has nothing in it to show. A refusal whose words were made on the spot keeps them in the record's text, so *why stays good
// as long as the record does: that one text is all a refused record may hold.
static bool IsEmpty(FieldRecord const &record)
{
	return record.title.empty() && record.list.empty() && record.values.empty() && record.text.size() <= 1 && record.choiceLists.empty() &&
	       record.valueText.empty();
}

// Whether a record is whole: one value for each descriptor, no more than the panel host shows, read-only, and accepted by the panel's check
static bool IsWhole(FieldRecord const &record)
{
	return record.list.size() == record.values.size() && record.list.size() <= host::kFieldPanelHostMostFields && AllReadOnly(record) && PanelAccepts(record);
}

// ---- The parts a record is made of, tried on a table of the test's own ----

// Reads whole numbers and fractions at a field's place in a few bytes, and remembers each field it was asked for. Some of what it does is for the
// checks to bend: a field it can't read, a flags word of its own choosing, and running out of memory.
class TestReader : public PartReader
{
public:
	explicit TestReader(std::vector<uint8_t> const &bytes) : bytes(bytes), unreadable(0), flagsWord(0), useFlagsWord(false), outOfMemory(false) {}

	bool Number(Field const &field, double *value) const override
	{
		asked.push_back(field.name);

		if (outOfMemory)
		{
			throw std::bad_alloc();
		}

		if (unreadable && strcmp(unreadable, field.name) == 0)
		{
			return false;
		}

		if (useFlagsWord && field.unit == Unit::Flags)
		{
			*value = flagsWord;
			return true;
		}

		if (field.offset + 4 > bytes.size())
		{
			return false;
		}

		uint32_t bits = 0;

		for (int i = 0; i < 4; i++)
		{
			bits |= (uint32_t)bytes[field.offset + i] << (8 * i);
		}

		if (field.storage == Storage::Float32)
		{
			float real;
			memcpy(&real, &bits, sizeof(real));
			*value = real;
		}
		else if (field.storage == Storage::UInt32)
		{
			*value = bits & (field.mask ? field.mask : 0xFFFFFFFFu);
		}
		else
		{
			*value = (int32_t)bits;
		}

		return true;
	}

	bool Axis(Field const &field, int axis, int32_t *value) const override
	{
		asked.push_back(field.name);

		if (unreadable && strcmp(unreadable, field.name) == 0)
		{
			return false;
		}

		size_t const at = field.offset + 4 * (size_t)axis;

		if (at + 4 > bytes.size())
		{
			return false;
		}

		uint32_t bits = 0;

		for (int i = 0; i < 4; i++)
		{
			bits |= (uint32_t)bytes[at + i] << (8 * i);
		}

		*value = (int32_t)bits;
		return true;
	}

	bool wasAskedFor(char const *name) const
	{
		for (std::string const &said : asked)
		{
			if (said == name)
			{
				return true;
			}
		}

		return false;
	}

	std::vector<uint8_t> bytes;
	char const *unreadable;       // the name of a field to refuse, or null
	double flagsWord;             // what a flags field reads as, when useFlagsWord
	bool useFlagsWord;
	bool outOfMemory;             // reading a number runs out of memory
	mutable std::vector<std::string> asked;
};

// A reader that never says how to read a field of three numbers: the default is to refuse it
class NumbersOnlyReader : public PartReader
{
public:
	bool Number(Field const &, double *value) const override
	{
		*value = 1;
		return true;
	}
};

static Field const OwnFields[] =
{
	{ "count", "A count", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "Why a count is what it is", false, 0, "this test" },
	{ "ratio", "Ratio", Anchor::Start, 4, Storage::Float32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
	{ "corner", "Corner", Anchor::Start, 8, Storage::IntTriple, Unit::GameUnits, 0, Limit::Editor, -1000, 1000, "inside the level", false, 0, "this test" },
	{ "hidden", "", Anchor::Start, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
	{ "flags", "", Anchor::Start, 24, Storage::UInt32, Unit::Flags, 0x7, Limit::None, 0, 0, "Why the flags are what they are", false, 0, "this test" },
};

static PartBit const OwnBits[] =
{
	{ "flags", 0x1, "Loud" },
	{ "flags", 0x2, "" },
	{ "flags", 0x4, "Fast" },
	{ "other", 0x1, "Never shown: another field's bit" },
};

// 28 bytes: count 42, ratio 0.5, corner -5, 6, 7, hidden 99, flags 5 (Loud and Fast)
static std::vector<uint8_t> OwnBytes()
{
	std::vector<uint8_t> bytes(28, 0);

	PutInt(bytes, 0, 42);
	PutFloat(bytes, 4, 0.5f);
	PutInt(bytes, 8, -5);
	PutInt(bytes, 12, 6);
	PutInt(bytes, 16, 7);
	PutInt(bytes, 20, 99);
	PutInt(bytes, 24, 5);
	return bytes;
}

static void PartsOfItsOwn()
{
	printf("The parts a record is made of, on a table of the test's own:\n");

	TestReader reader(OwnBytes());
	PartShown const head = { OwnFields, sizeof(OwnFields) / sizeof(OwnFields[0]), OwnBits, sizeof(OwnBits) / sizeof(OwnBits[0]), &reader };
	FieldRecord record;
	char const *why = 0;

	Check(BuildRecordOfParts("A title", &head, 0, &record, &why), "a record is built from a head alone");
	Check(record.title == "A title", "its title is the one given");
	Check(IsWhole(record), "the record is whole, read-only and accepted by the panel's check");

	// 1 count, 1 ratio, 3 of the corner, and the 2 flag bits that have words
	Check(record.list.size() == 7, "it shows 7 fields: a number, a fraction, the corner's three numbers and the two bits with words");

	int const count = Place(record, "A count");
	int const ratio = Place(record, "Ratio");
	int const x = Place(record, "Corner x");
	int const y = Place(record, "Corner y");
	int const z = Place(record, "Corner z");
	int const loud = Place(record, "Loud");
	int const fast = Place(record, "Fast");

	Check(count == 0 && ratio == 1 && x == 2 && y == 3 && z == 4 && loud == 5 && fast == 6, "the fields come in the table's order: the bits where their flags field is");
	Check(count >= 0 && record.list[(size_t)count].kind == ui::FieldKind::Number && record.values[(size_t)count].number == 42 &&
	      record.list[(size_t)count].decimals == 0, "a whole number is a Number holding what the file holds");
	Check(ratio >= 0 && record.list[(size_t)ratio].kind == ui::FieldKind::Number && record.values[(size_t)ratio].number == 0.5 &&
	      record.list[(size_t)ratio].decimals == 3, "a fraction is a Number of 3 decimals");
	Check(x >= 0 && y >= 0 && z >= 0 && record.values[(size_t)x].number == -5 && record.values[(size_t)y].number == 6 && record.values[(size_t)z].number == 7 &&
	      record.list[(size_t)x].unit && strcmp(record.list[(size_t)x].unit, "mm") == 0,
	      "a field of three numbers is three Numbers, x, y and z, with the unit's words");
	Check(loud >= 0 && fast >= 0 && record.list[(size_t)loud].kind == ui::FieldKind::Bit && record.list[(size_t)loud].bit == 0x1 &&
	      record.values[(size_t)loud].word == 5 && record.list[(size_t)fast].bit == 0x4 && record.values[(size_t)fast].word == 5,
	      "a flags field is one Bit for each named bit, each holding the whole word");
	Check(Place(record, "Never shown: another field's bit") < 0 && Place(record, "") < 0, "another field's bit isn't shown, nor a bit with no words");
	Check(!reader.wasAskedFor("hidden"), "a field the old editor showed no label for isn't shown, and its bytes are never read");
	Check(count >= 0 && record.list[(size_t)count].help && strcmp(record.list[(size_t)count].help, "Why a count is what it is") == 0 &&
	      ratio >= 0 && record.list[(size_t)ratio].help == 0 && loud >= 0 && record.list[(size_t)loud].help &&
	      strcmp(record.list[(size_t)loud].help, "Why the flags are what they are") == 0,
	      "a field's help is its reason, none when it has none, and each bit of a flags field has the flags field's");

	// A head and an item together: the head's fields come first
	TestReader itemReader(OwnBytes());
	PartShown const itemPart = { OwnFields + 2, 1, 0, 0, &itemReader };
	Check(BuildRecordOfParts("Both", &head, &itemPart, &record, &why) && record.list.size() == 10 && Place(record, "Corner x") == 2 &&
	      strcmp(record.list[7].name, "Corner x") == 0 && IsWhole(record),
	      "a head and an item make one record, the head's fields first and the item's after them");
	Check(BuildRecordOfParts("Neither", 0, 0, &record, &why) && record.title == "Neither" && record.list.empty() && IsWhole(record),
	      "no head and no item is a record of a title alone, and the panel accepts it (an empty list is a sound one)");

	// Words: cut to what a descriptor takes, and each letter the font doesn't have made a "?"
	static Field const wordy[] =
	{
		{ "long", "A label of sixty letters and no more, so it must be cut short: xyz", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
		  "", false, 0, "this test" },
		{ "odd", "Tab\there", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "helpful", "Helpful", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
		  "A reason of well over two hundred letters: it goes on and on, and says again and again, in more and more words than the panel's help "
		  "line has room for, so the end of it has to be cut off where the panel's own limit says, which is two hundred letters and no more",
		  false, 0, "this test" },
	};

	TestReader wordsReader(OwnBytes());
	PartShown const wordsPart = { wordy, 3, 0, 0, &wordsReader };
	Check(BuildRecordOfParts("Words", &wordsPart, 0, &record, &why) && record.list.size() == 3 && strlen(record.list[0].name) == ui::kFieldNameMost &&
	      strcmp(record.list[1].name, "Tab?here") == 0 && record.list[2].help && strlen(record.list[2].help) == ui::kFieldHelpMost && IsWhole(record),
	      "a label is cut to 48 letters, a letter the font doesn't have is a '?', and a help line is cut to 200: the panel accepts the list");
}

static void PartsAreRefusedWhole()
{
	printf("A part that can't be shown refuses the whole record, with its reason:\n");

	FieldRecord record;
	char const *why = 0;

	// Each of the two parts, with a field it can't read
	TestReader unreadable(OwnBytes());
	unreadable.unreadable = "ratio";
	PartShown const head = { OwnFields, 5, OwnBits, 4, &unreadable };

	Check(!BuildRecordOfParts("Head", &head, 0, &record, &why) && why && strstr(why, "own field ratio") && IsEmpty(record),
	      "a field of the head that can't be read refuses the record, names the field, and leaves nothing in it");
	Check(!BuildRecordOfParts("Item", 0, &head, &record, &why) && why && strstr(why, "item's field ratio") && IsEmpty(record),
	      "a field of the item that can't be read refuses it the same way, as the item's");

	TestReader flagsReader(OwnBytes());
	flagsReader.unreadable = "flags";
	PartShown const flagsPart = { OwnFields, 5, OwnBits, 4, &flagsReader };
	Check(!BuildRecordOfParts("Flags", &flagsPart, 0, &record, &why) && why && strstr(why, "flags") && IsEmpty(record),
	      "a flags field that can't be read refuses the record");

	TestReader tripleReader(OwnBytes());
	tripleReader.unreadable = "corner";
	PartShown const triplePart = { OwnFields, 5, OwnBits, 4, &tripleReader };
	Check(!BuildRecordOfParts("Corner", &triplePart, 0, &record, &why) && why && strstr(why, "corner") && IsEmpty(record),
	      "a field of three numbers whose numbers can't be read refuses the record");

	// What the record has no way to show
	static Field const objectField = { "target", "Target", Anchor::Start, 0, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" };
	static Field const matrixField = { "turn", "Turn", Anchor::Start, 0, Storage::IntMatrix, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" };
	static Field const choiceField = { "kind", "Kind", Anchor::Start, 0, Storage::Int32, Unit::Choice, 0, Limit::None, 0, 0, "", false, 0, "this test" };
	static Field const hiddenObject = { "target", "", Anchor::Start, 0, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" };
	TestReader reader(OwnBytes());
	PartShown const objectPart = { &objectField, 1, 0, 0, &reader };
	PartShown const matrixPart = { &matrixField, 1, 0, 0, &reader };
	PartShown const choicePart = { &choiceField, 1, 0, 0, &reader };
	PartShown const hiddenPart = { &hiddenObject, 1, 0, 0, &reader };

	Check(!BuildRecordOfParts("Object", &objectPart, 0, &record, &why) && IsEmpty(record), "a labelled object field can't be shown: the record is refused, never silently left out");
	Check(!BuildRecordOfParts("Matrix", &matrixPart, 0, &record, &why) && IsEmpty(record), "and a labelled orientation");
	Check(!BuildRecordOfParts("Choice", &choicePart, 0, &record, &why) && IsEmpty(record), "and a labelled choice");
	Check(BuildRecordOfParts("Hidden", &hiddenPart, 0, &record, &why) && record.list.empty(), "but the same object field with no label is skipped, as every unlabelled field is");

	// A field of three numbers, with a reader that can't read any
	NumbersOnlyReader numbersOnly;
	PartShown const triplesPart = { OwnFields + 2, 1, 0, 0, &numbersOnly };
	Check(!BuildRecordOfParts("Triple", &triplesPart, 0, &record, &why) && IsEmpty(record),
	      "a part whose reader never says how to read a field of three numbers has it refused, by the default");

	// A flags word that isn't a flags word
	TestReader wordReader(OwnBytes());
	wordReader.useFlagsWord = true;
	PartShown const wordPart = { OwnFields + 4, 1, OwnBits, 4, &wordReader };
	double const notWords[] = { 1.5, 4294967296.0, -2147483649.0 };   // a fraction, one too big, one too small
	bool allRefused = true;

	for (int i = 0; i < 3; i++)
	{
		wordReader.flagsWord = notWords[i];
		allRefused = allRefused && !BuildRecordOfParts("Word", &wordPart, 0, &record, &why) && IsEmpty(record);
	}

	uint64_t const nanBits = 0x7FF8000000000001ull;
	uint64_t const endlessBits = 0x7FF0000000000000ull;
	double nan, endless;
	memcpy(&nan, &nanBits, sizeof(nan));
	memcpy(&endless, &endlessBits, sizeof(endless));
	wordReader.flagsWord = nan;
	allRefused = allRefused && !BuildRecordOfParts("Word", &wordPart, 0, &record, &why);
	wordReader.flagsWord = endless;
	allRefused = allRefused && !BuildRecordOfParts("Word", &wordPart, 0, &record, &why);
	Check(allRefused, "a flags word that's a fraction, 'not a number', endless, past 32 bits one way or the other is refused: a flags field is a whole number");

	wordReader.flagsWord = -1;
	Check(BuildRecordOfParts("Word", &wordPart, 0, &record, &why) && record.values[0].word == 0xFFFFFFFFu, "a flags word of -1 is all 32 bits set, as the file holds it");
	wordReader.flagsWord = 4294967295.0;
	Check(BuildRecordOfParts("Word", &wordPart, 0, &record, &why) && record.values[0].word == 0xFFFFFFFFu, "and so is the largest, 4294967295");
	wordReader.flagsWord = -2147483648.0;
	Check(BuildRecordOfParts("Word", &wordPart, 0, &record, &why) && record.values[0].word == 0x80000000u, "and the least, -2147483648, is bit 31");

	// More fields than the panel host shows
	std::vector<Field> many;

	for (size_t i = 0; i < host::kFieldPanelHostMostFields + 1; i++)
	{
		many.push_back(OwnFields[0]);
	}

	TestReader manyReader(OwnBytes());
	PartShown const manyPart = { many.data(), host::kFieldPanelHostMostFields, 0, 0, &manyReader };
	PartShown const tooManyPart = { many.data(), host::kFieldPanelHostMostFields + 1, 0, 0, &manyReader };

	Check(BuildRecordOfParts("Most", &manyPart, 0, &record, &why) && record.list.size() == host::kFieldPanelHostMostFields && IsWhole(record),
	      "exactly " + std::to_string(host::kFieldPanelHostMostFields) + " fields, the most the panel host shows, are shown");
	Check(!BuildRecordOfParts("TooMany", &tooManyPart, 0, &record, &why) && why && strstr(why, "more fields than the panel shows") && IsEmpty(record),
	      "one more is refused, with nothing left in the record");

	// A record the panel's own check refuses (a bit that isn't one bit), here as the second field of the record
	static PartBit const twoBits[] = { { "flags", 0x3, "Two bits" } };
	static Field const countAndFlags[] = { OwnFields[0], OwnFields[4] };
	TestReader bitsReader(OwnBytes());
	PartShown const twoBitsPart = { countAndFlags, 2, twoBits, 1, &bitsReader };
	Check(!BuildRecordOfParts("Bits", &twoBitsPart, 0, &record, &why) && why && strstr(why, "didn't pass the panel's check (field 1)") && IsEmpty(record),
	      "a record the panel's own check refuses is refused whole, saying which field");

	// A bit with no field's name belongs to no field, and is passed over
	static PartBit const nobodysBit[] = { { 0, 0x1, "Nobody's" }, { "flags", 0x1, "Loud" } };
	TestReader nobodyReader(OwnBytes());
	PartShown const nobodyPart = { OwnFields + 4, 1, nobodysBit, 2, &nobodyReader };
	bool built = false;
	bool survived = Survives([&] { built = BuildRecordOfParts("Nobody", &nobodyPart, 0, &record, &why); });
	Check(survived && built && record.list.size() == 1 && strcmp(record.list[0].name, "Loud") == 0,
	      "a bit with no field's name is passed over, not read: the field's other bits are shown");

	// A field of three numbers keeps its axis in its label: the label is cut two letters short of the most, so "<label> x" always fits
	static Field const longCorner[] =
	{
		{ "corner", "A corner whose label takes all forty-eight letters, no less", Anchor::Start, 8, Storage::IntTriple, Unit::GameUnits, 0, Limit::None, 0, 0,
		  "", false, 0, "this test" },
	};
	TestReader longReader(OwnBytes());
	PartShown const longPart = { longCorner, 1, 0, 0, &longReader };
	Check(BuildRecordOfParts("Long", &longPart, 0, &record, &why) && record.list.size() == 3 && strlen(record.list[0].name) == ui::kFieldNameMost &&
	      record.list[0].name[ui::kFieldNameMost - 1] == 'x' && record.list[2].name[ui::kFieldNameMost - 1] == 'z' && IsWhole(record),
	      "a three-number field with a label of 48 or more letters is cut so that its axis fits in 48: the panel accepts the names");

	// Nothing to fill
	bool refused = false;
	bool ran = Survives([&] { refused = !BuildRecordOfParts("x", &head, 0, 0, &why) && why && strstr(why, "no record"); });
	Check(ran && refused, "no record to fill is refused");

	// Running out of memory
	TestReader memoryReader(OwnBytes());
	memoryReader.outOfMemory = true;
	PartShown const memoryPart = { OwnFields, 5, OwnBits, 4, &memoryReader };
	Check(!BuildRecordOfParts("Memory", &memoryPart, 0, &record, &why) && why && strstr(why, "memory") && IsEmpty(record),
	      "running out of memory refuses the record, with nothing shown, as any other mistake does");
}

// ---- A track ----

// The game's own layout of a track section (win95/chunk_object.hpp:374-390), as model_tracks_test.cpp has it
struct GameQuat
{
	float x, y, z, w;
};

struct GameVectorInt
{
	int x, y, z;
};

struct GameTrackSection
{
	GameQuat quat_start;
	GameQuat quat_end;
	GameVectorInt pivot_start;
	GameVectorInt pivot_end;
	GameVectorInt object_offset;
	int time_for_section;
	int spare;
};

static_assert(sizeof(GameTrackSection) == 76, "the game's ChunkTrackSection is 76 bytes (win95/chunk_object.hpp:374-390)");

// A track as the game writes it (win95/chunk_object.cpp:962-988): the count, the sections, the flags and the timer's start
static std::vector<uint8_t> GameTrack(std::vector<GameTrackSection> const &sections, int32_t flags, int32_t timerStart)
{
	size_t const flagsAt = 4 + sections.size() * sizeof(GameTrackSection);
	std::vector<uint8_t> bytes(flagsAt + 8, 0);

	PutInt(bytes, 0, (int32_t)sections.size());

	for (size_t i = 0; i < sections.size(); i++)
	{
		memcpy(bytes.data() + 4 + i * sizeof(GameTrackSection), &sections[i], sizeof(GameTrackSection));
	}

	PutInt(bytes, flagsAt, flags);
	PutInt(bytes, flagsAt + 4, timerStart);
	return bytes;
}

// Three sections, each taking a different time: 1065, 2065 and 3065
static std::vector<GameTrackSection> ThreeSections()
{
	std::vector<GameTrackSection> sections;

	for (int k = 0; k < 3; k++)
	{
		GameTrackSection s = {};

		s.time_for_section = 1000 * (k + 1) + 65;
		s.spare = 77 + k;
		sections.push_back(s);
	}

	return sections;
}

static int32_t const TrackFlags = (int32_t)0x80000005u;   // the game's own bit 31, and "Loop Track" and "Loop back and forth"
static int32_t const TrackTimer = 1234567;

static void ATrackShowsItsHeadAndOneSection()
{
	printf("A track shows its own numbers and one section:\n");

	std::vector<uint8_t> const bytes = GameTrack(ThreeSections(), TrackFlags, TrackTimer);
	FieldRecord record;
	char const *why = 0;

	Check(BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 0, &record, &why), "the first section's record is built");
	Check(record.title == "track: OBJTRAK2 item 1/3", "its title counts from 1: index 0 of 3 is \"item 1/3\"");
	Check(IsWhole(record), "the record is whole, read-only and accepted by the panel's check");

	// The old editor's words: the four check boxes, the timer's start, and the section's time. Nothing else of a track is labelled.
	Check(record.list.size() == 6, "it shows 6 fields: the 4 named flag bits, the timer's start and the section's time");
	Check(Place(record, "Loop Track") == 0 && Place(record, "Playing at start") == 1 && Place(record, "Loop back and forth") == 2 &&
	      Place(record, "Use Track Smoothing") == 3 && Place(record, "Timer start (seconds)") == 4 && Place(record, "Time for section") == 5,
	      "in the table's order: the track's own fields first, then the section's");

	bool bitsRight = true;

	for (int i = 0; i < 4; i++)
	{
		bitsRight = bitsRight && record.list[(size_t)i].kind == ui::FieldKind::Bit && record.list[(size_t)i].bit == (1u << i) && record.values[(size_t)i].word == 5;
	}

	Check(bitsRight, "each flag bit is a Bit holding the word 5: bits 1 and 4 set, and the game's own bit 31 not in it");

	int const timer = Place(record, "Timer start (seconds)");
	int const time = Place(record, "Time for section");

	Check(timer >= 0 && record.list[(size_t)timer].kind == ui::FieldKind::Number && record.values[(size_t)timer].number == TrackTimer &&
	      record.list[(size_t)timer].unit && strcmp(record.list[(size_t)timer].unit, "s/65536") == 0,
	      "the timer's start is a Number holding what the file holds, with its unit's words");
	Check(time >= 0 && record.values[(size_t)time].number == 1065, "section 1's time is the first section's: 1065");
	Check(Place(record, "") < 0 && Place(record, "quat_start_x") < 0 && Place(record, "pivot_start x") < 0,
	      "a section's turns and places, which the old editor drew in its view and never labelled, aren't shown");

	// The index chooses the section, and both ends of the list are reached
	Check(BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 1, &record, &why) && record.title == "track: OBJTRAK2 item 2/3" &&
	      record.values[(size_t)Place(record, "Time for section")].number == 2065, "index 1 is item 2/3, with the second section's time, 2065");
	Check(BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 2, &record, &why) && record.title == "track: OBJTRAK2 item 3/3" &&
	      record.values[(size_t)Place(record, "Time for section")].number == 3065, "index n-1, 2, is item 3/3 (the last), with the last section's time, 3065");

	// An item the list doesn't hold
	bool allRefused = true;
	size_t const past[] = { 3, 4, 1000, (size_t)0x80000000u, (size_t)-1 };

	for (size_t item : past)
	{
		allRefused = allRefused && !BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), item, &record, &why) && why &&
		             strcmp(why, "the chunk has no such item") == 0 && IsEmpty(record);
	}

	Check(allRefused, "an item from the count up (3, 4, 1000, 2^31, the most a size can say) is refused, with nothing left in the record");

	// Flags of every bit and a timer of nothing, as a level might hold them
	std::vector<uint8_t> const plain = GameTrack(ThreeSections(), 0, 0);
	Check(BuildChunkItemRecord("OBJTRAK2", plain.data(), plain.size(), 0, &record, &why) && record.values[0].word == 0 &&
	      record.values[(size_t)Place(record, "Timer start (seconds)")].number == 0, "a track with no flag set and a timer of 0 shows them as they are");
	std::vector<uint8_t> const hostile = GameTrack(ThreeSections(), (int32_t)0xFFFFFFFFu, -5);
	Check(BuildChunkItemRecord("OBJTRAK2", hostile.data(), hostile.size(), 0, &record, &why) && record.values[0].word == 15 &&
	      record.values[(size_t)Place(record, "Timer start (seconds)")].number == -5 && IsWhole(record),
	      "a timer below the loader's least is shown as the file holds it, and every flag set shows the four bits a mapper chooses");
}

static void ATrackWithNoSections()
{
	printf("A track with no section shows its own numbers alone:\n");

	std::vector<uint8_t> const bytes = GameTrack(std::vector<GameTrackSection>(), TrackFlags, TrackTimer);
	FieldRecord record;
	char const *why = 0;

	Check(BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 0, &record, &why) && record.title == "track: OBJTRAK2 (no items)" &&
	      record.list.size() == 5 && Place(record, "Time for section") < 0 && IsWhole(record),
	      "item 0 of a track with no section is its own 5 fields, and the title says it has no items");
	Check(!BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 1, &record, &why) && why && strcmp(why, "the chunk has no such item") == 0 && IsEmpty(record),
	      "any other item is refused");
	Check(!BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), (size_t)-1, &record, &why) && IsEmpty(record), "the most a size can say too");

	// A record that was in use is replaced whole, not added to
	std::vector<uint8_t> const three = GameTrack(ThreeSections(), TrackFlags, TrackTimer);
	Check(BuildChunkItemRecord("OBJTRAK2", three.data(), three.size(), 0, &record, &why) && record.list.size() == 6 &&
	      BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 0, &record, &why) && record.list.size() == 5 && IsWhole(record),
	      "building into a record that already holds one replaces it: nothing of the first is left");
	Check(BuildChunkItemRecord("OBJTRAK2", three.data(), three.size(), 0, &record, &why) &&
	      !BuildChunkItemRecord("OBJTRAK2", three.data(), three.size(), 7, &record, &why) && IsEmpty(record),
	      "and a refusal into a record that holds one empties it");
}

static void ATrackOfTheWrongSize()
{
	printf("A track whose size the game refuses:\n");

	std::vector<uint8_t> const bytes = GameTrack(ThreeSections(), TrackFlags, TrackTimer);
	FieldRecord record;
	char const *why = 0;

	std::vector<uint8_t> shortBy1(bytes.begin(), bytes.end() - 1);
	Check(!BuildChunkItemRecord("OBJTRAK2", shortBy1.data(), shortBy1.size(), 0, &record, &why) && why &&
	      strcmp(why, ChunkSizeCheckWords(SizeCheck::TooShort)) == 0 && IsEmpty(record),
	      "a track one byte short of its timer's start is refused, with the model's own reason");

	std::vector<uint8_t> cutShort(2, 0);
	Check(!BuildChunkItemRecord("OBJTRAK2", cutShort.data(), cutShort.size(), 0, &record, &why) && why &&
	      strcmp(why, ChunkSizeCheckWords(SizeCheck::CountCutOff)) == 0, "a payload that ends inside its count says so");

	std::vector<uint8_t> negative = bytes;
	PutInt(negative, 0, -1);
	Check(!BuildChunkItemRecord("OBJTRAK2", negative.data(), negative.size(), 0, &record, &why) && why &&
	      strcmp(why, ChunkSizeCheckWords(SizeCheck::CountNegative)) == 0, "a count below 0 says so");

	std::vector<uint8_t> tooMany = bytes;
	PutInt(tooMany, 0, 4);
	Check(!BuildChunkItemRecord("OBJTRAK2", tooMany.data(), tooMany.size(), 0, &record, &why) && why &&
	      strcmp(why, ChunkSizeCheckWords(SizeCheck::CountTooBig)) == 0, "a count of more sections than the bytes hold says so");
}

// ---- Hostile counts, in payloads of every size, for every item asked for ----

static void HostileTracks()
{
	printf("Hostile counts, in payloads of every size:\n");

	int32_t const counts[] = { 0, 1, 2, 3, -1, (int32_t)0x80000000u, 0x7FFFFFFF, 0x40000001, 56512728, 119304648 };
	size_t const items[] = { 0, 1, 2, 3, 1000, (size_t)-1 };
	bool survived = true;
	bool agreed = true;
	bool whole = true;
	bool refusalsEmpty = true;
	size_t built = 0;
	size_t refused = 0;

	for (int32_t const count : counts)
	{
		for (size_t length = 0; length <= 4 + 3 * sizeof(GameTrackSection) + 8 + 12; length++)
		{
			std::vector<uint8_t> bytes(length, 0x5A);

			if (length >= 4)
			{
				PutInt(bytes, 0, count);
			}

			size_t held = 0;
			bool const sizeRight = TrackSectionCount(bytes.data(), bytes.size(), &held);

			for (size_t item : items)
			{
				FieldRecord record;
				char const *why = 0;
				bool ok = false;

				survived = survived && Survives([&] { ok = BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), item, &record, &why); });

				bool const expected = sizeRight && (held == 0 ? item == 0 : item < held);

				agreed = agreed && ok == expected;

				if (ok)
				{
					whole = whole && IsWhole(record) && record.title.size() <= host::kFieldPanelHostTitleMost && !record.title.empty();
					built++;
				}
				else
				{
					refusalsEmpty = refusalsEmpty && IsEmpty(record) && why != 0;
					refused++;
				}
			}
		}
	}

	Check(survived, "nothing stopped the program, for " + std::to_string(sizeof(counts) / sizeof(counts[0])) + " counts, in payloads of every size, for 6 items each");
	Check(agreed && built > 100 && refused > 1000,
	      "a record is built exactly when the track is a size the game reads and holds the item (" + std::to_string(built) + " built, " + std::to_string(refused) + " refused)");
	Check(whole, "every record built is whole, read-only, accepted by the panel's check, no more than 64 fields, and its title fits what the host reads");
	Check(refusalsEmpty, "every refusal leaves the record empty and gives its reason");
}

// ---- What isn't a chunk with items ----

static void ChunksItDoesNotBuild()
{
	printf("What it doesn't build:\n");

	std::vector<uint8_t> const bytes = GameTrack(ThreeSections(), TrackFlags, TrackTimer);
	FieldRecord record;
	char const *why = 0;

	Check(!BuildChunkItemRecord("STDLIGHT", bytes.data(), bytes.size(), 0, &record, &why) && why &&
	      strcmp(why, "this kind of chunk has no items: it's shown whole, not item by item") == 0 && IsEmpty(record),
	      "a chunk of the base family (a light) has no items: refused, with words that say BuildChunkRecord shows it");
	Check(!BuildChunkItemRecord("MODACOUS", bytes.data(), bytes.size(), 0, &record, &why) && why &&
	      strcmp(why, "this kind of chunk has no items: it's shown whole, not item by item") == 0, "and one of the acoustics family");
	Check(!BuildChunkItemRecord("NOSUCHCH", bytes.data(), bytes.size(), 0, &record, &why) && why &&
	      strcmp(why, "the editor doesn't know this kind of chunk") == 0 && IsEmpty(record), "a chunk no family has is unknown");

	// Nothing handed over. Each check that would read through a null pointer if its guard were gone runs under Survives.
	bool refused = false;
	bool ran = Survives([&] { refused = !BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 0, 0, &why) && why && strstr(why, "no record"); });
	Check(ran && refused, "no record to fill is refused");
	refused = false;
	ran = Survives([&] { refused = !BuildChunkItemRecord("OBJTRAK2", 0, bytes.size(), 0, &record, &why) && why && strstr(why, "no chunk"); });
	Check(ran && refused && IsEmpty(record), "no payload, with a length said, is refused");
	refused = false;
	ran = Survives([&] { refused = !BuildChunkItemRecord(0, bytes.data(), bytes.size(), 0, &record, &why) && why && strstr(why, "doesn't know"); });
	Check(ran && refused && IsEmpty(record), "no chunk name is refused as unknown");
	refused = false;
	ran = Survives([&] { refused = BuildChunkItemRecord("OBJTRAK2", bytes.data(), bytes.size(), 0, &record, 0); });
	Check(ran && refused, "a build with no place for a reason works");
}

int main()
{
	PartsOfItsOwn();
	PartsAreRefusedWhole();
	ATrackShowsItsHeadAndOneSection();
	ATrackWithNoSections();
	ATrackOfTheWrongSize();
	HostileTracks();
	ChunksItDoesNotBuild();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
