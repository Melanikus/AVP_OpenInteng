// The test for src/host's field adapters: the record the field panel shows for what's selected. Written 2026-11-14 by Melanikus
// (Dietrich) for AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <type_traits>
#include <utility>

#include "field_adapters.h"
#include "field_edit.h"
#include "field_panel_host.h"
#include "field_model.h"
#include "strategy_record.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A check of a record's title: when it fails, it says what it wanted and what it got
static void CheckTitle(FieldRecord const &record, std::string const &wanted, std::string const &what)
{
	Check(record.title == wanted, what);

	if (record.title != wanted)
	{
		printf("      wanted: %s\n      got:    %s\n", wanted.c_str(), record.title.c_str());
	}
}

// A record points into itself, so a copy of one would point into the one it came from: it can only be moved
static_assert(!std::is_copy_constructible<FieldRecord>::value && !std::is_copy_assignable<FieldRecord>::value, "a record can't be copied");
static_assert(std::is_move_constructible<FieldRecord>::value && std::is_move_assignable<FieldRecord>::value, "a record can be moved");

static Selected AnObject(const uint8_t (&id)[kSelectionIdBytes])
{
	Selected selected = {};

	selected.kind = SelectedKind::Object;
	memcpy(selected.objectId, id, kSelectionIdBytes);
	return selected;
}

static Selected AModule(int32_t index)
{
	Selected selected = {};

	selected.kind = SelectedKind::Module;
	selected.moduleIndex = index;
	return selected;
}

static bool IsEmpty(FieldRecord const &record)
{
	return record.title.empty() && record.list.empty() && record.values.empty() && record.text.empty() && record.choiceLists.empty()
		&& record.valueText.empty();
}

// A record that holds something of everything, so a test can see it let go
static void FillTheRecord(FieldRecord *record)
{
	record->title = "left over";
	record->list.resize(2);
	record->values.resize(2);
	record->text.push_back("words");
	record->choiceLists.emplace_back();
	record->valueText.push_back("bytes");
}

static void TestNothingToShow()
{
	puts("Nothing to show:");

	FieldRecord record;
	Selected nothing = {};

	FillTheRecord(&record);
	Check(!FieldAdapters_Build(nothing, &record) && IsEmpty(record), "nothing selected gives back false, and the record it held is let go of");

	Check(!FieldAdapters_Build(nothing, 0), "no place for the record gives back false, and doesn't crash");

	Selected strange = {};

	strange.kind = (SelectedKind)99;
	FillTheRecord(&record);
	Check(!FieldAdapters_Build(strange, &record) && IsEmpty(record), "a kind of selection it doesn't know is refused, and the record is let go of");

	uint8_t const noName[kSelectionIdBytes] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	FillTheRecord(&record);
	Check(!FieldAdapters_Build(AnObject(noName), &record) && IsEmpty(record), "an object whose ID is all 0, the game's own \"no name\", is refused, like the selection refuses it");

	FillTheRecord(&record);
	Check(!FieldAdapters_Build(AModule(-1), &record) && IsEmpty(record) && !FieldAdapters_Build(AModule(INT32_MIN), &record) && IsEmpty(record),
		"a module numbered below 0 is refused");
}

static void TestTheTitles()
{
	puts("What's selected is the title:");

	FieldRecord record;
	uint8_t const id[kSelectionIdBytes] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };

	FillTheRecord(&record);
	Check(FieldAdapters_Build(AnObject(id), &record), "an object is shown");
	CheckTitle(record, "Object 0123456789ABCDEF", "its title is its ID as 16 hex digits, in capital letters, as the status line says it");
	Check(record.list.empty() && record.values.empty() && record.text.empty() && record.choiceLists.empty() && record.valueText.empty(),
		"and what the record held before is gone: the stand-in has no fields");

	uint8_t const edges[kSelectionIdBytes] = { 0xFF, 0xFE, 0x00, 0x01, 0xA0, 0x0A, 0x7F, 0x80 };

	Check(FieldAdapters_Build(AnObject(edges), &record), "an ID with bytes at the edges is shown");
	CheckTitle(record, "Object FFFE0001A00A7F80", "each byte is two digits, with its leading 0");

	uint8_t const oneBit[kSelectionIdBytes] = { 0, 0, 0, 0, 0, 0, 0, 1 };

	Check(FieldAdapters_Build(AnObject(oneBit), &record), "an ID that is all 0 but one bit isn't \"no name\"");
	CheckTitle(record, "Object 0000000000000001", "and its title says so");

	Check(FieldAdapters_Build(AModule(0), &record), "module 0 is shown");
	CheckTitle(record, "Module 0", "its title is its number");
	Check(FieldAdapters_Build(AModule(12345), &record), "a module is shown");
	CheckTitle(record, "Module 12345", "with its number in plain digits");
	Check(FieldAdapters_Build(AModule(INT32_MAX), &record), "the largest number is shown");
	CheckTitle(record, "Module 2147483647", "in full");

	// Only the field for its kind counts
	Selected both = AModule(7);

	memset(both.objectId, 0xAA, sizeof(both.objectId));
	Check(FieldAdapters_Build(both, &record), "a module whose ID bytes hold something");
	CheckTitle(record, "Module 7", "is a module and nothing else");
}

static void TestTheHostShowsIt()
{
	puts("The host shows it:");

	FieldRecord record;
	uint8_t const id[kSelectionIdBytes] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };

	FieldAdapters_Build(AnObject(id), &record);

	FieldPanelHost shown;
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	Check(shown.Show(record.title.c_str(), record.list.data(), record.list.size(), record.values.data(), &problem), "the host takes the stand-in record");
	Check(shown.Showing() && shown.Title() == "Object 0123456789ABCDEF" && shown.Panel().Count() == 0, "it shows the title, and no field");
	Check(shown.HeightFor(15) == 15 + 2 * kPaintMarginDown, "and asks for one line's height, and the margins");

	// The record moves, as one is kept for the host in the editor: what the host shows stays what it was
	FieldRecord kept = std::move(record);

	CheckTitle(kept, "Object 0123456789ABCDEF", "a moved record keeps its title");

	shown.Clear();
	Check(!shown.Showing(), "and the host lets go of it before the record goes");
}

static void TestTheHeaderIsShown()
{
	puts("An object the level holds shows its header:");

	// The stand-in level's two objects (tests/host_field_adapters_level_stand_in.cpp)
	uint8_t const door[kSelectionIdBytes] = { 0x44, 0x4F, 0x4F, 0x52, 0x30, 0x30, 0x30, 0x31 };
	uint8_t const nameless[kSelectionIdBytes] = { 0x4E, 0x4F, 0x4E, 0x41, 0x4D, 0x45, 0x30, 0x31 };
	FieldRecord record;

	FillTheRecord(&record);
	Check(FieldAdapters_Build(AnObject(door), &record), "an object the level holds is shown");
	CheckTitle(record, "door_main", "its title is its name");
	Check(record.list.size() == 6 && record.values.size() == 6 && strcmp(record.list[0].name, "Game mode: Marine") == 0,
		"with the six boxes of its header, Game mode first, and nothing left over from before");
	Check(record.list.size() == 6 && (record.values[0].word & record.list[0].bit) != 0 && (record.values[3].word & record.list[3].bit) == 0,
		"the door is in the Marine game and not at Easy, as its flags say");

	Check(FieldAdapters_Build(AnObject(nameless), &record), "an object with no name is shown");
	CheckTitle(record, "Object 4E4F4E414D453031", "its title is its ID, as the stand-in's is");
	Check(record.list.size() == 6, "and it has its six boxes");

	FieldPanelHost shown;
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	Check(FieldAdapters_Build(AnObject(door), &record) &&
		shown.Show(record.title.c_str(), record.list.data(), record.list.size(), record.values.data(), &problem) && shown.Panel().Count() == 6,
		"the host takes the door's record, with its six fields");
	shown.Clear();
}

static void TestTheStrategyIsShown()
{
	puts("An object with a strategy shows its strategy's fields, then its header's boxes:");

	// The stand-in level's two objects with strategies (tests/host_field_adapters_level_stand_in.cpp)
	uint8_t const withStrategy[kSelectionIdBytes] = { 0x53, 0x54, 0x52, 0x41, 0x54, 0x30, 0x30, 0x31 };
	uint8_t const broken[kSelectionIdBytes] = { 0x42, 0x41, 0x44, 0x53, 0x54, 0x52, 0x41, 0x54 };

	// The strategy's own record, built straight from the bytes the stand-in gives (a new Door, 50)
	uint8_t bytes[1024];
	size_t length = 0;
	FieldRecord alone;
	char const *why = 0;

	Check(model::MakeNewStrategy(50, bytes, sizeof(bytes), &length) && BuildStrategyRecord(bytes, length, &alone, &why) && !alone.list.empty(),
		"(a new Door's strategy record builds on its own)");

	size_t const n = alone.list.size();
	FieldRecord record;

	FillTheRecord(&record);
	Check(FieldAdapters_Build(AnObject(withStrategy), &record), "an object with a strategy is shown");
	CheckTitle(record, "lever_a: " + alone.title, "its title is its name, then its strategy's kind and number");
	Check(record.list.size() == n + 6 && record.values.size() == n + 6, "it holds the strategy's fields and the header's six boxes, and nothing from before");

	bool same = record.list.size() == n + 6;

	for (size_t i = 0; same && i < n; i++)
	{
		same = strcmp(record.list[i].name, alone.list[i].name) == 0;
	}

	Check(same, "the strategy's fields come first, in the strategy record's own order");
	Check(record.list.size() == n + 6 && strcmp(record.list[n].name, "Game mode: Marine") == 0 && strcmp(record.list[n + 5].name, "Difficulty: Hard") == 0,
		"then the six boxes, from Game mode: Marine to Difficulty: Hard");

	FieldPanelHost shown;
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };

	Check(shown.Show(record.title.c_str(), record.list.data(), record.list.size(), record.values.data(), &problem) && shown.Panel().Count() == n + 6,
		"the host takes the whole record");
	shown.Clear();

	Check(FieldAdapters_Build(AnObject(broken), &record), "an object whose strategy can't be read is shown");
	CheckTitle(record, "broken_one", "with its name as the title");
	Check(record.list.size() == 6, "and its header's six boxes alone");
}

static void TestThePointersHold()
{
	puts("A record's pointers into itself hold:");

	FieldRecord record;
	ui::FieldDescriptor field = ui::EmptyFieldDescriptor();

	record.title = "Door";
	record.text.push_back("volume");
	field.name = record.text.back().c_str();
	field.kind = ui::FieldKind::Number;
	record.list.push_back(field);
	record.values.push_back(ui::EmptyFieldValue());

	// More words after the first was pointed to, as an adapter adds them field by field
	for (int i = 0; i < 100; i++)
	{
		record.text.push_back("w" + std::to_string(i));
	}

	FieldRecord kept = std::move(record);

	Check(kept.list[0].name == kept.text[0].c_str() && strcmp(kept.list[0].name, "volume") == 0,
		"after 100 more words and a move, a descriptor still points at its own words");
}

// The level's settings (the scale here) are shown and NEVER edited: inteng has no screen for them (they were the old CENCON tool's dialogs), so every field is read-only
namespace host { extern Chunk *const kScaleChunk; }

class ScaleStore : public host::PayloadStore
{
public:
	bool PayloadOf(Chunk *chunk, std::vector<unsigned char> *payload) const override
	{
		if (chunk != host::kScaleChunk || !payload)
		{
			return false;
		}

		double const scale = 0.8;
		payload->assign(sizeof(scale), 0);
		memcpy(payload->data(), &scale, sizeof(scale));
		return true;
	}

	bool SetPayload(Chunk *, std::vector<unsigned char> const &) override
	{
		return false;
	}
};

static void TestTheLevelSettingsAreReadOnly()
{
	puts("the level's settings are read-only");

	ScaleStore const store;
	host::FieldRecord record;
	host::ShownEdit edit;

	Check(host::FieldAdapters_BuildLevelSettings(&store, &record, &edit), "the settings are built from the chunk the level has");
	Check(!record.list.empty() && record.title == "Level settings", "they have fields and the title");

	bool allReadOnly = !record.list.empty();

	for (size_t i = 0; i < record.list.size(); i++)
	{
		allReadOnly = allReadOnly && record.list[i].readOnly;
	}

	Check(allReadOnly, "and every one is read-only: inteng has no edit for the level's settings");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheLevelSettingsAreReadOnly();

	TestNothingToShow();
	TestTheTitles();
	TestTheHostShowsIt();
	TestTheHeaderIsShown();
	TestTheStrategyIsShown();
	TestThePointersHold();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
