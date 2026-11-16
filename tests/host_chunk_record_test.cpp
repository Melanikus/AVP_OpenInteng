// The test for src/host/chunk_record: one chunk of a level turned into the field panel's read-only record. Written 2026-11-14 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds chunk payloads in memory, some of them hostile, builds their records, and
// checks the records against the field model's tables and against the panel's own check of a list (ui::CheckFieldList). The program
// gives back a non-zero result if any check failed. Each check's words say what it proves. A check that would crash if a guard were lost
// runs under Survives (crash_guard.h), so it FAILS by name instead of stopping the program.

#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "chunk_payloads.h"
#include "crash_guard.h"
#include "chunk_record.h"
#include "field_model.h"
#include "field_panel_host.h"
#include "real_numbers.h"

using namespace model;
using host::BuildChunkRecord;
using host::BuildChunkRecordOfKind;
using host::FieldRecord;

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

// A payload the game's reader accepts, with every number 0 (chunk_payloads.h)
static std::vector<uint8_t> Valid(ChunkKind const &kind)
{
	return ValidChunkPayload(kind);
}

static bool PanelAccepts(FieldRecord const &record)
{
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };
	return ui::CheckFieldList(record.list.data(), record.list.size(), &problem);
}

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

static size_t CountKind(FieldRecord const &record, ui::FieldKind kind)
{
	size_t count = 0;

	for (ui::FieldDescriptor const &descriptor : record.list)
	{
		count += descriptor.kind == kind ? 1 : 0;
	}

	return count;
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

static void PlacedLight()
{
	printf("A placed light's record:\n");

	ChunkKind const *const light = FindChunkKind("PLOBJLIT");
	Check(light != 0, "the placed light is in the table");

	if (!light)
	{
		return;
	}

	std::vector<uint8_t> bytes = Valid(*light);
	Field const *const fadeUp = FindChunkField(*light, "fade_up_time");
	Field const *const lightType = FindChunkField(*light, "light_type");
	char const *why = 0;

	Check(fadeUp && lightType && SetChunkField(*light, *fadeUp, bytes.data(), bytes.size(), 1234, &why) &&
	      SetChunkField(*light, *lightType, bytes.data(), bytes.size(), 1, &why),
	      "a fade-up time of 1234 and a light type of 1 are put in by the model's setters");

	FieldRecord record;
	why = 0;
	Check(BuildChunkRecord("PLOBJLIT", bytes.data(), bytes.size(), &record, &why), "the record is built");
	Check(record.title == "placed light (PLOBJLIT)", "its title is the kind's words and its chunk's name");
	Check(PanelAccepts(record), "the panel accepts the list");
	Check(AllReadOnly(record), "every descriptor is read-only");

	int const fade = Place(record, "Fade up time");
	Check(fade >= 0 && record.values[(size_t)fade].number == 1234 && record.list[(size_t)fade].kind == ui::FieldKind::Number &&
	      record.list[(size_t)fade].unit && strcmp(record.list[(size_t)fade].unit, "ms") == 0,
	      "the fade-up time is a Number holding 1234, in ms");
	Check(Place(record, "brightness") < 0 && Place(record, "") < 0, "a field the old editor showed no label for isn't shown");

	int const type = Place(record, "Light type");
	size_t named = 0;

	for (size_t i = 0; i < ChunkChoiceCount; i++)
	{
		named += (strcmp(ChunkChoices[i].chunk, "PLOBJLIT") == 0 && strcmp(ChunkChoices[i].field, "light_type") == 0) ? 1 : 0;
	}

	Check(type >= 0 && record.list[(size_t)type].kind == ui::FieldKind::Choice && record.list[(size_t)type].choiceCount == named && named > 0 &&
	      record.values[(size_t)type].choice == 1,
	      "the light type is a Choice with each of its " + std::to_string(named) + " named values, holding 1");

	// A flags field is one Bit for each named bit that has words, and the whole word is each one's value
	size_t namedBits = 0;

	for (size_t i = 0; i < ChunkBitCount; i++)
	{
		namedBits += (strcmp(ChunkBits[i].chunk, "PLOBJLIT") == 0 && ChunkBits[i].label && ChunkBits[i].label[0]) ? 1 : 0;
	}

	Check(namedBits > 0 && CountKind(record, ui::FieldKind::Bit) == namedBits, "there is one Bit for each of the " + std::to_string(namedBits) +
	      " named bits of its two flags fields");

	// A choice holding a value its list doesn't name is kept as it is
	PutInt(bytes, lightType->offset, 99);
	FieldRecord unnamed;
	int const unnamedType = BuildChunkRecord("PLOBJLIT", bytes.data(), bytes.size(), &unnamed, &why) ? Place(unnamed, "Light type") : -1;
	Check(unnamedType >= 0 && unnamed.values[(size_t)unnamedType].choice == 99 && PanelAccepts(unnamed),
	      "a light type the list doesn't name, 99, is kept as the file holds it, and the panel still accepts the list");
}

static void EveryKind()
{
	printf("Every kind of chunk the model has:\n");

	size_t made = 0;
	size_t mostFields = 0;
	std::string firstProblem;

	for (size_t k = 0; k < ChunkKindCount; k++)
	{
		ChunkKind const &kind = ChunkKinds[k];
		std::vector<uint8_t> bytes = Valid(kind);
		FieldRecord record;
		char const *why = 0;

		if (CheckChunkSize(kind, bytes.data(), bytes.size()) != SizeCheck::Right)
		{
			firstProblem = firstProblem.empty() ? std::string(kind.id) + ": the test's own payload isn't a size the model accepts" : firstProblem;
		}
		else if (!BuildChunkRecord(kind.id, bytes.data(), bytes.size(), &record, &why))
		{
			firstProblem = firstProblem.empty() ? std::string(kind.id) + ": " + (why ? why : "refused") : firstProblem;
		}
		else if (!PanelAccepts(record) || !AllReadOnly(record) || record.list.size() != record.values.size() ||
		         record.list.size() > host::kFieldPanelHostMostFields)
		{
			firstProblem = firstProblem.empty() ? std::string(kind.id) + ": the record is not whole, read-only and acceptable" : firstProblem;
		}
		else
		{
			made++;
			mostFields = record.list.size() > mostFields ? record.list.size() : mostFields;
		}
	}

	Check(ChunkKindCount > 0 && made == ChunkKindCount && firstProblem.empty(),
	      "all " + std::to_string(ChunkKindCount) + " kinds build a record from a payload of zeros, every one read-only and accepted by the panel (most fields: " +
	      std::to_string(mostFields) + (firstProblem.empty() ? ")" : "); first problem: " + firstProblem));
}

// A kind of the test's own, with a field of each storage, so each rule is tried on a field the real tables have no example of
static void AKindOfItsOwn()
{
	printf("A kind of its own, with a field of each storage:\n");

	static char const longLabel[] = "A label of sixty letters and no more, so it must be cut short: x";   // 61 letters: cut to 48
	static Field const fields[] =
	{
		{ "count", "A count", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "Why a count is what it is", false, 0, "this test" },
		{ "pos", "Corner", Anchor::Start, 4, Storage::IntTriple, Unit::GameUnits, 0, Limit::Editor, -1000, 1000, "inside the level", false, 0, "this test" },
		{ "target", "Target", Anchor::Start, 16, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "ratio", "Ratio", Anchor::Start, 24, Storage::Float32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "mode", "Mode", Anchor::Start, 28, Storage::Int32, Unit::Choice, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "flags", "", Anchor::Start, 32, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "turn", "Turn", Anchor::Start, 36, Storage::IntMatrix, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "hidden", "", Anchor::Start, 72, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "long", longLabel, Anchor::Start, 76, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "odd", "Caf\xE9 \x01 label", Anchor::Start, 80, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" },
		{ "lonely", "Lonely mode", Anchor::Start, 84, Storage::Int32, Unit::Choice, 0, Limit::None, 0, 0, "", false, 0, "this test" },
	};
	static ChunkKind const kind = { "TESTKIND", "a test kind", "", 88, 88, -1, 0, fields, sizeof(fields) / sizeof(fields[0]), "this test", 0, 0, -1 };
	static ChunkBitName const bits[] = { { "TESTKIND", "flags", 0x1, "First bit" }, { "TESTKIND", "flags", 0x4, "Third bit" },
	                                     { "TESTKIND", "flags", 0x8, "" }, { "OTHERKND", "flags", 0x2, "Another kind's bit" } };
	static ChunkChoiceName const choices[] = { { "TESTKIND", "mode", 0, "Off" }, { "OTHERKND", "mode", 9, "Elsewhere" }, { "TESTKIND", "mode", 5, "On" } };
	ChunkNames const names = { bits, 4, choices, 3 };

	std::vector<uint8_t> bytes(88, 0);
	PutInt(bytes, 0, -42);
	PutInt(bytes, 4, 10);
	PutInt(bytes, 8, -20);
	PutInt(bytes, 12, 30);
	PutInt(bytes, 16, 0x0A0B0C0D);
	PutInt(bytes, 20, 0x01020304);
	PutInt(bytes, 24, 0x7FC00001);   // a ratio that isn't a number
	PutInt(bytes, 28, 77);           // a mode its choices don't name
	PutInt(bytes, 32, 0x5);

	FieldRecord record;
	char const *why = 0;
	Check(BuildChunkRecordOfKind(kind, names, bytes.data(), bytes.size(), &record, &why), "the record is built");
	Check(record.title == "a test kind (TESTKIND)" && PanelAccepts(record) && AllReadOnly(record), "its title is right, the panel accepts it, and it's read-only");

	int const count = Place(record, "A count");
	Check(count >= 0 && record.values[(size_t)count].number == -42 && record.list[(size_t)count].help &&
	      strcmp(record.list[(size_t)count].help, "Why a count is what it is") == 0,
	      "a number holds its value and carries the field's reason as its help");

	int const x = Place(record, "Corner x");
	Check(x >= 0 && Place(record, "Corner y") == x + 1 && Place(record, "Corner z") == x + 2 && record.values[(size_t)x].number == 10 &&
	      record.values[(size_t)x + 1].number == -20 && record.values[(size_t)x + 2].number == 30 && record.list[(size_t)x].highest == 1000,
	      "a field of three numbers is three Numbers, x, y and z, with the field's limits");

	int const target = Place(record, "Target");
	Check(target >= 0 && record.list[(size_t)target].kind == ui::FieldKind::Text && std::string((char const *)record.values[(size_t)target].bytes,
	      record.values[(size_t)target].length) == "0A0B0C0D:01020304", "an object field is a Text, its two halves in hex, first half first");

	int const ratio = Place(record, "Ratio");
	Check(ratio >= 0 && !IsRealNumber(record.values[(size_t)ratio].number), "a fraction that isn't a number is kept as it is, not made a number");

	int const mode = Place(record, "Mode");
	Check(mode >= 0 && record.list[(size_t)mode].choiceCount == 2 && record.values[(size_t)mode].choice == 77,
	      "a choice has its two named values and keeps the value the file holds, 77, though no choice names it");

	Check(CountKind(record, ui::FieldKind::Bit) == 2 && Place(record, "First bit") >= 0 && Place(record, "Third bit") >= 0 &&
	      record.values[(size_t)Place(record, "First bit")].word == 5 && Place(record, "Another kind's bit") < 0 && Place(record, "") < 0,
	      "a flags field is a Bit for each named bit of ITS kind that has words, each holding the whole word, 5");

	Check(Place(record, "Turn") < 0, "an orientation isn't shown");
	Check(Place(record, "hidden") < 0, "a field with no label isn't shown (nor is its name used as a label)");

	std::string const cut(longLabel, 48);
	Check(Place(record, cut.c_str()) >= 0, "a label of more than 48 letters is cut to its first 48");
	Check(Place(record, "Caf? ? label") >= 0, "letters the font doesn't have become '?'");

	// The two ends of a list's size
	int const lonely = Place(record, "Lonely mode");
	Check(lonely >= 0 && record.list[(size_t)lonely].kind == ui::FieldKind::Number,
	      "a choice field with no named value at all is shown as a number, not as a box with nothing to pick");
	Check(record.list.size() == record.values.size() && record.list.size() == 12, "there is one value for each of the 12 descriptors");
}

static void Hostile()
{
	printf("What's refused:\n");

	ChunkKind const *const light = FindChunkKind("PLOBJLIT");
	std::vector<uint8_t> bytes = Valid(*light);
	char const *why = 0;
	FieldRecord record;

	bool built = true;
	bool ran = Survives([&] { built = BuildChunkRecord("PLOBJLIT", 0, bytes.size(), &record, &why); });
	Check(ran && !built && why && strlen(why) > 0 && record.list.empty(), "no payload, with a length said, is refused, with words, and the record is empty");

	built = true;
	why = 0;
	ran = Survives([&] { built = BuildChunkRecord("PLOBJLIT", bytes.data(), bytes.size(), 0, &why); });
	Check(ran && !built && why && strlen(why) > 0, "no record to fill is refused, with words");

	built = true;
	why = 0;
	ran = Survives([&] { built = BuildChunkRecord(0, bytes.data(), bytes.size(), &record, &why); });
	Check(ran && !built && why && strlen(why) > 0, "no chunk name is refused, with words");

	built = true;
	why = 0;
	ran = Survives([&] { built = BuildChunkRecord("NOSUCHCH", bytes.data(), bytes.size(), &record, &why); });
	Check(ran && !built && why && strstr(why, "doesn't know") != 0 && record.list.empty(), "a chunk no family has is refused: the editor doesn't know it");

	built = true;
	why = 0;
	ran = Survives([&] { built = BuildChunkRecord("PLOBJLIT", bytes.data(), light->leastBytes - 1, &record, &why); });
	Check(ran && !built && why && strcmp(why, ChunkSizeCheckWords(SizeCheck::TooShort)) == 0 && record.list.empty(),
	      "a payload one byte shorter than the game reads is refused, with the model's own words");

	built = true;
	why = 0;
	ran = Survives([&] { built = BuildChunkRecord("PLOBJLIT", bytes.data(), 0, &record, &why); });
	Check(ran && !built && why && strcmp(why, ChunkSizeCheckWords(SizeCheck::TooShort)) == 0, "a payload of no bytes is refused the same way");

	// A list whose count claims more items than the payload holds
	std::vector<uint8_t> bigCount = bytes;
	PutInt(bigCount, (size_t)light->listCountAt, 1000);
	built = true;
	why = 0;
	ran = Survives([&] { built = BuildChunkRecord("PLOBJLIT", bigCount.data(), bigCount.size(), &record, &why); });
	Check(ran && !built && why && strcmp(why, ChunkSizeCheckWords(SizeCheck::CountTooBig)) == 0, "a list that counts more items than the payload holds is refused");

	// A refused record is left empty even if it held an earlier one
	FieldRecord held;
	why = 0;
	Check(BuildChunkRecord("PLOBJLIT", bytes.data(), bytes.size(), &held, &why) && !held.list.empty(), "a record is built");
	why = 0;
	built = true;
	ran = Survives([&] { built = BuildChunkRecord("NOSUCHCH", bytes.data(), bytes.size(), &held, &why); });
	Check(ran && !built && held.list.empty() && held.values.empty() && held.title.empty(),
	      "then a refusal into the same record leaves it empty, never half of the old one");
}

// More fields than the panel shows: 64 are accepted and 65 refused as a whole, with words
static void TooManyFields()
{
	printf("More fields than the panel shows:\n");

	for (size_t count : { host::kFieldPanelHostMostFields, host::kFieldPanelHostMostFields + 1 })
	{
		std::vector<std::string> words;
		std::vector<Field> fields;

		for (size_t i = 0; i < count; i++)
		{
			words.push_back("Field number " + std::to_string(i));
		}

		for (size_t i = 0; i < count; i++)
		{
			Field const field = { words[i].c_str(), words[i].c_str(), Anchor::Start, (unsigned)(4 * i), Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "this test" };
			fields.push_back(field);
		}

		ChunkKind kind = { "TESTMANY", "many", "", (unsigned)(4 * count), (unsigned)(4 * count), -1, 0, fields.data(), count, "this test", 0, 0, -1 };
		ChunkNames const names = { 0, 0, 0, 0 };
		std::vector<uint8_t> bytes(4 * count, 0);
		FieldRecord record;
		char const *why = 0;
		bool const built = BuildChunkRecordOfKind(kind, names, bytes.data(), bytes.size(), &record, &why);

		if (count == host::kFieldPanelHostMostFields)
		{
			Check(built && record.list.size() == count, "a kind with exactly " + std::to_string(count) + " fields, the most the panel shows, is built");
		}
		else
		{
			Check(!built && why && strstr(why, "more fields") != 0 && record.list.empty(), "a kind with " + std::to_string(count) + " fields is refused as a whole, with words");
		}
	}
}

// A record the panel's own check refuses: a field whose range is the wrong way round
static void ThePanelsOwnCheck()
{
	printf("A record the panel's own check refuses:\n");

	static Field const backwards[] = { { "bad", "Backwards", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::Editor, 10, 0, "a reason", false, 0, "this test" } };
	static ChunkKind const kind = { "TESTBACK", "backwards", "", 4, 4, -1, 0, backwards, 1, "this test", 0, 0, -1 };
	ChunkNames const names = { 0, 0, 0, 0 };
	std::vector<uint8_t> bytes(4, 0);
	FieldRecord record;
	char const *why = 0;
	bool const built = BuildChunkRecordOfKind(kind, names, bytes.data(), bytes.size(), &record, &why);

	Check(!built && why && strstr(why, "panel's check (field 0)") != 0 && record.list.empty() && record.title.empty(),
	      "a field whose range is the wrong way round, which the panel's check refuses, refuses the whole record, and names the field");
}

// A chunk's choice field can be unsigned too: a value no int32 holds is shown as a Number, exactly as the file holds it
static void AnUnsignedChoice()
{
	printf("An unsigned choice:\n");

	static Field const fields[] = { { "type", "Kind", Anchor::Start, 0, Storage::UInt32, Unit::Choice, 0, Limit::None, 0, 0, "", false, 0, "this test" } };
	static ChunkKind const kind = { "TESTUNSG", "unsigned", "", 4, 4, -1, 0, fields, 1, "this test", 0, 0, -1 };
	static ChunkChoiceName const choices[] = { { "TESTUNSG", "type", 0, "Zero" }, { "TESTUNSG", "type", 1, "One" } };
	ChunkNames const names = { 0, 0, choices, 2 };

	struct One
	{
		uint32_t stored;
		bool choice;
		double value;
	};
	One const cases[] = { { 1u, true, 1 }, { 0x7FFFFFFFu, true, 2147483647.0 }, { 0x80000000u, false, 2147483648.0 }, { 0xFFFFFFFFu, false, 4294967295.0 } };

	for (One const &one : cases)
	{
		std::vector<uint8_t> bytes(4, 0);
		PutInt(bytes, 0, (int32_t)one.stored);

		FieldRecord record;
		char const *why = 0;
		bool const built = BuildChunkRecordOfKind(kind, names, bytes.data(), bytes.size(), &record, &why);
		int const place = built ? Place(record, "Kind") : -1;
		char hex[16];
		snprintf(hex, sizeof(hex), "%08X", (unsigned)one.stored);
		bool const right = place >= 0 && PanelAccepts(record) &&
		                   (one.choice ? (record.list[(size_t)place].kind == ui::FieldKind::Choice && (double)record.values[(size_t)place].choice == one.value)
		                               : (record.list[(size_t)place].kind == ui::FieldKind::Number && record.values[(size_t)place].number == one.value));

		Check(right, std::string("a chunk field holding 0x") + hex + " is built as " + (one.choice ? "a Choice" : "a Number") + " holding " +
		      std::to_string((long long)one.value) + (built ? "" : std::string(" (refused: ") + (why ? why : "no reason") + ")"));
	}
}

int main()
{
	PlacedLight();
	EveryKind();
	AKindOfItsOwn();
	Hostile();
	TooManyFields();
	ThePanelsOwnCheck();
	AnUnsignedChoice();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
