// The test for src/host/record_edit: a record made EDITABLE, and a value the mapper kept in the field panel turned into an edit of the bytes it
// came from. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds strategy and chunk payloads in memory, builds their records, loads the records into
// the REAL field panel (ui::FieldPanel), types into it and presses keys as the window would (ui_event.h), and then asks record_edit to apply what
// was kept to a small store of the test's own (a chunk is only a made-up address that nothing reads). Each check says what it proves: the panel
// itself refuses what the field spec refuses, a kept value changes exactly its own field's bytes, two bits of one flags word changed one after
// the other both stay, and what can't be edited (a descriptor with no source, a list's count, a field of another kind) never is. A check that
// would crash if a guard were lost runs under Survives (crash_guard.h), so it FAILS by name. The program gives back a non-zero result if any
// check failed.

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "crash_guard.h"
#include "chunk_record.h"
#include "field_descriptor_from_model.h"
#include "field_edit.h"
#include "field_model.h"
#include "field_panel.h"
#include "real_numbers.h"
#include "record_edit.h"
#include "strategy_record.h"
#include "ui_event.h"

using namespace model;
using host::ApplyKeptChunkField;
using host::ApplyKeptStrategyField;
using host::EditAnswer;
using host::FieldEdit;
using host::FieldRecord;
using host::KeptAnswer;
using host::PanelEditAnswer;
using ui::Key;
using ui::PanelAnswer;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	fflush(stdout);   // so a crash later in the run never loses the lines before it

	if (!good)
	{
		checksFailed++;
	}
}

typedef std::vector<unsigned char> Bytes;

// A store of the test's own: what it holds, and how often it was asked to keep something
class FakeStore : public host::PayloadStore
{
public:
	std::map<Chunk *, Bytes> held;
	int keeps;

	FakeStore() : keeps(0) {}

	bool PayloadOf(Chunk *chunk, Bytes *payload) const override
	{
		const std::map<Chunk *, Bytes>::const_iterator found = held.find(chunk);

		if (found == held.end() || !payload)
		{
			return false;
		}

		*payload = found->second;
		return true;
	}

	bool SetPayload(Chunk *chunk, Bytes const &payload) override
	{
		keeps++;
		held[chunk] = payload;
		return true;
	}
};

// A made-up chunk: an address nothing reads
static Chunk *Fake(int n)
{
	return reinterpret_cast<Chunk *>((uintptr_t)(0x1000 + 16 * n));
}

static void PutInt(Bytes &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (unsigned char)((bits >> (8 * i)) & 0xFF);
	}
}

// A new strategy as the old editor made one, with its index and spare number (payload bytes 0 to 7) not 0, so an edit that touched them would show
static Bytes NewStrategy(int32_t kind)
{
	Bytes bytes(512, 0xEE);
	size_t length = 0;

	if (!MakeNewStrategy(kind, bytes.data(), bytes.size(), &length))
	{
		bytes.clear();
		return bytes;
	}

	bytes.resize(length);
	PutInt(bytes, 0, 0x11223344);
	PutInt(bytes, 4, 0x55667788);
	return bytes;
}

// A chunk payload the game's reader accepts, every number 0
static Bytes ValidChunk(ChunkKind const &kind)
{
	size_t size = kind.fixedBytes > kind.leastBytes ? kind.fixedBytes : kind.leastBytes;
	size += kind.namesRequired;
	Bytes bytes(size, 0);

	if (kind.sizeFieldAt >= 0)
	{
		PutInt(bytes, (size_t)kind.sizeFieldAt, (int32_t)(bytes.size() - (size_t)kind.sizeFieldAt));
	}

	return bytes;
}

static Field const *FieldNamed(StrategyKind const &kind, char const *name)
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

static Field const *ChunkFieldNamed(ChunkKind const &kind, char const *name)
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

// A strategy field's number in the bytes, as stored; -12345.5 when it can't be read
static double NumberIn(StrategyKind const &kind, char const *name, Bytes const &bytes)
{
	Field const *const field = FieldNamed(kind, name);
	double value = -12345.5;

	if (field)
	{
		ReadField(kind, *field, bytes.data(), bytes.size(), &value, 0);
	}

	return value;
}

static double ChunkNumberIn(ChunkKind const &kind, char const *name, Bytes const &bytes)
{
	Field const *const field = ChunkFieldNamed(kind, name);
	double value = -12345.5;

	if (field)
	{
		ReadChunkField(kind, *field, bytes.data(), bytes.size(), &value, 0);
	}

	return value;
}

// A record and the panel loaded with it
struct Shown
{
	FieldRecord record;
	ui::FieldPanel panel;

	bool Load()
	{
		ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };
		return panel.Load(record.list.data(), record.list.size(), record.values.data(), &problem);
	}
};

// The place in the record of the descriptor with that label, or -1
static size_t At(FieldRecord const &record, char const *label)
{
	for (size_t i = 0; i < record.list.size(); i++)
	{
		if (strcmp(record.list[i].name, label) == 0)
		{
			return i;
		}
	}

	return (size_t)-1;
}

// ---- Driving the panel, as the window does (the same as ui_field_panel_test.cpp's)

static PanelAnswer Type(ui::FieldPanel &panel, char const *letters)
{
	PanelAnswer answer = PanelAnswer::Nothing;

	for (char const *at = letters; *at; at++)
	{
		answer = panel.Handle(ui::LetterEvent((unsigned char)*at));
	}

	return answer;
}

// Selects everything, types over it, and presses Enter: the answer is Enter's
static PanelAnswer TypeOver(ui::FieldPanel &panel, char const *letters)
{
	panel.Handle(ui::LetterEvent('a', true));
	Type(panel, letters);
	return panel.Handle(ui::KeyEvent(Key::Enter));
}

// ---- The tests

static void TestRecordsAndSources()
{
	puts("1. a record is read-only until it is made editable, and remembers where each field comes from");

	StrategyKind const *const door = FindStrategyKind(50);
	Bytes const payload = NewStrategy(50);
	FieldRecord plain;
	char const *why = 0;

	Check(host::BuildStrategyRecord(payload.data(), payload.size(), &plain, &why) && !plain.list.empty(), "a door's record is built");

	bool allReadOnly = true;
	bool allSourced = plain.sources.size() == plain.list.size();

	for (size_t i = 0; i < plain.list.size(); i++)
	{
		allReadOnly = allReadOnly && plain.list[i].readOnly;
		allSourced = allSourced && i < plain.sources.size() && plain.sources[i].field != 0 && plain.sources[i].what != host::SourceKind::None;
	}

	Check(allReadOnly, "as built, every descriptor is read-only");
	Check(allSourced, "and every descriptor remembers its model field: one source for each, none empty");

	size_t const time = At(plain, "Time take to open (0.1 - 25 sec)");
	size_t const locked = At(plain, "Door is locked");
	Check(time != (size_t)-1 && plain.sources[time].what == host::SourceKind::Number && strcmp(plain.sources[time].field->name, "TimeToOpen") == 0,
	      "the time to open is a Number sourced from the field TimeToOpen");
	Check(locked != (size_t)-1 && plain.sources[locked].what == host::SourceKind::Bit && plain.sources[locked].bit == 0x1 &&
	          strcmp(plain.sources[locked].field->name, "DoorFlags") == 0,
	      "a door's bit is a Bit sourced from DoorFlags, with its own bit");

	FieldRecord editable;
	Check(host::BuildEditableStrategyRecord(payload.data(), payload.size(), &editable, &why) && editable.list.size() == plain.list.size(),
	      "the same strategy's editable record has the same descriptors");

	bool allOpen = true;
	bool checksOnNumbers = true;

	for (size_t i = 0; i < editable.list.size(); i++)
	{
		allOpen = allOpen && !editable.list[i].readOnly;
		checksOnNumbers = checksOnNumbers && ((editable.list[i].kind == ui::FieldKind::Number) == (editable.list[i].numberCheck != 0));
	}

	Check(allOpen, "made editable, every descriptor of the door is open: each field of it is one the model can set");
	Check(checksOnNumbers, "and exactly the numbers carry the field spec's own check");

	// A record with a descriptor nobody gave a source (an object's header bit) and then one that has
	FieldRecord mixed;
	host::PushBit(&mixed, "Header bit", 0, 0x1, 0);
	host::PushNumber(&mixed, "Time", 0, *FieldNamed(*door, "TimeToOpen"), 5);
	Check(mixed.sources.size() == 2 && mixed.sources[0].what == host::SourceKind::None && mixed.sources[1].what == host::SourceKind::Number,
	      "a descriptor with no source gets an empty one, so each source is at its own descriptor's place");
	host::MakeStrategyRecordEditable(&mixed, *door);
	Check(mixed.list[0].readOnly && !mixed.list[1].readOnly, "and it stays read-only when the record is made editable, while the one beside it opens");

	// A header bit pushed with no source, and nothing else, has no sources at all
	FieldRecord headerOnly;
	host::PushBit(&headerOnly, "Header bit", 0, 0x1, 0);
	Check(headerOnly.sources.empty(), "a record of header bits alone has no sources");

	host::EmptyRecord(&editable);
	Check(editable.sources.empty() && editable.list.empty(), "EmptyRecord empties the sources too");

	// A chunk's record
	ChunkKind const *const particle = FindChunkKind("PARGENDA");
	Bytes const chunk = ValidChunk(*particle);
	FieldRecord chunkRecord;
	Check(host::BuildEditableChunkRecord("PARGENDA", chunk.data(), chunk.size(), &chunkRecord, &why) && !chunkRecord.list.empty(),
	      "a particle generator's editable record is built");
	size_t const type = At(chunkRecord, "Type");
	size_t const parent = At(chunkRecord, "Parent");
	Check(type != (size_t)-1 && !chunkRecord.list[type].readOnly && chunkRecord.sources[type].what == host::SourceKind::Choice &&
	          chunkRecord.sources[type].ofChunk && memcmp(chunkRecord.sources[type].chunk, "PARGENDA", 9) == 0,
	      "its Type is an open Choice, a chunk's, sourced from PARGENDA");
	Check(parent != (size_t)-1 && !chunkRecord.list[parent].readOnly && chunkRecord.list[parent].textCheck != 0 &&
	          chunkRecord.list[parent].numberCheck == 0,
	      "its Parent is an open object text with the object check, and no number check");

	// A field that is a list's count is never opened, whatever it shows
	static Field const OwnFields[] =
	{
		{ "count", "Count", Anchor::Start, 20, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0, "", false, 0, "" },
		{ "word", "Word", Anchor::Start, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "" },
	};
	static StrategyKind const OwnKind = { 9990, "Own", "", SizeRule::AnySize, 28, OwnFields, 2, false, false, 9990 };
	Bytes own(28, 0);
	PutInt(own, 8, 9990);
	FieldRecord ownRecord;
	Check(host::BuildStrategyRecordOfKind(OwnKind, own.data(), own.size(), &ownRecord, &why), "a kind of its own, with a list's count, is built");
	host::MakeStrategyRecordEditable(&ownRecord, OwnKind);
	size_t const count = At(ownRecord, "Count");
	size_t const word = At(ownRecord, "Word");
	Check(count != (size_t)-1 && ownRecord.list[count].readOnly && word != (size_t)-1 && !ownRecord.list[word].readOnly,
	      "a list's count stays read-only, and the plain number beside it opens");
}

static void TestNumberEdit()
{
	puts("2. a number typed in the panel is refused or kept by the panel itself, and a kept one changes exactly its own bytes");

	StrategyKind const &door = *FindStrategyKind(50);
	Bytes const original = NewStrategy(50);
	FakeStore store;
	store.held[Fake(1)] = original;

	Shown shown;
	char const *why = 0;
	host::BuildEditableStrategyRecord(original.data(), original.size(), &shown.record, &why);
	Check(shown.Load(), "the panel loads the editable door record");

	size_t const time = At(shown.record, "Time take to open (0.1 - 25 sec)");
	shown.panel.ClickField(time);

	Check(TypeOver(shown.panel, "300") != PanelAnswer::Kept && store.keeps == 0, "300 tenths doesn't fit a byte: the panel doesn't keep it, and nothing reaches the store");
	Check(TypeOver(shown.panel, "33") == PanelAnswer::Kept, "33 is kept by the panel");

	EditAnswer editAnswer = EditAnswer::BadCall;
	KeptAnswer const answer = ApplyKeptStrategyField(&store, Fake(1), door, shown.record, shown.panel, time, &editAnswer, &why);
	Check(answer == KeptAnswer::Applied && editAnswer == EditAnswer::Applied && store.keeps == 1, "applied, once");

	Bytes const &now = store.held[Fake(1)];
	Check(NumberIn(door, "TimeToOpen", now) == 33, "the strategy now holds 33 tenths");

	Bytes expected = original;
	size_t at = 0;
	FieldPlace(door, *FieldNamed(door, "TimeToOpen"), original.data(), original.size(), &at);
	expected[at] = 33;
	Check(now == expected, "and every other byte is as it was: the index, the spare number, the kind, the flags");

	// A record that was never made editable is refused by the apply, whatever the panel holds
	Shown readOnly;
	host::BuildStrategyRecord(original.data(), original.size(), &readOnly.record, &why);
	readOnly.Load();
	Check(ApplyKeptStrategyField(&store, Fake(1), door, readOnly.record, readOnly.panel, time, 0, &why) == KeptAnswer::NotEditable && store.keeps == 1,
	      "a record that wasn't made editable is NotEditable, and nothing is kept");

	FieldEdit readOnlyEdit;
	Check(host::EditFromPanel(readOnly.record, readOnly.panel, time, 0, &readOnlyEdit, &why) == PanelEditAnswer::NotEditable,
	      "and an edit made straight from a read-only record is NotEditable too");

	// A field past the record, and the bad calls
	Check(ApplyKeptStrategyField(&store, Fake(1), door, shown.record, shown.panel, shown.record.list.size(), 0, &why) == KeptAnswer::BadCall,
	      "a field past the record is a bad call");
	bool ran = Survives([&] { Check(ApplyKeptStrategyField(0, Fake(1), door, shown.record, shown.panel, time, 0, 0) == KeptAnswer::BadCall, "no store is a bad call"); });
	Check(ran, "(and it doesn't crash)");
	ran = Survives([&] { Check(ApplyKeptStrategyField(&store, 0, door, shown.record, shown.panel, time, 0, 0) == KeptAnswer::BadCall, "no chunk is a bad call"); });
	Check(ran, "(and it doesn't crash)");
	Check(ApplyKeptStrategyField(&store, Fake(9), door, shown.record, shown.panel, time, 0, &why) == KeptAnswer::NoPayload && store.keeps == 1,
	      "a chunk the store holds nothing for gives NoPayload");

	// A record for another kind than the one it's applied with
	StrategyKind const &binSwitch = *FindStrategyKind(52);
	Check(ApplyKeptStrategyField(&store, Fake(1), binSwitch, shown.record, shown.panel, time, 0, &why) == KeptAnswer::NotEditable && store.keeps == 1,
	      "a door's record applied as a binary switch's is NotEditable: the field isn't that kind's");
}

static void TestBits()
{
	puts("3. two bits of one flags word, changed one after the other, both stay");

	StrategyKind const &door = *FindStrategyKind(50);
	Bytes const original = NewStrategy(50);
	FakeStore store;
	store.held[Fake(1)] = original;

	Shown shown;
	char const *why = 0;
	host::BuildEditableStrategyRecord(original.data(), original.size(), &shown.record, &why);
	shown.Load();

	size_t const locked = At(shown.record, "Door is locked");
	size_t const startsOpen = At(shown.record, "Starts open");
	double const before = NumberIn(door, "DoorFlags", original);

	shown.panel.ClickField(locked);
	Check(shown.panel.Handle(ui::KeyEvent(Key::Space)) == PanelAnswer::Kept && shown.panel.BitChecked(locked), "Space turns 'Door is locked' on in the panel");
	Check(ApplyKeptStrategyField(&store, Fake(1), door, shown.record, shown.panel, locked, 0, &why) == KeptAnswer::Applied &&
	          NumberIn(door, "DoorFlags", store.held[Fake(1)]) == (double)((uint32_t)before | 0x1),
	      "applied: the word has the bit, and every other bit as it was");

	// The panel isn't reloaded: 'Starts open' still holds the word the level had, without the first bit
	shown.panel.ClickField(startsOpen);
	Check(shown.panel.BitWord(startsOpen) == (uint32_t)before, "the second bit's own copy of the word is still the old one");
	Check(shown.panel.Handle(ui::KeyEvent(Key::Space)) == PanelAnswer::Kept && shown.panel.BitChecked(startsOpen), "Space turns 'Starts open' on");
	Check(ApplyKeptStrategyField(&store, Fake(1), door, shown.record, shown.panel, startsOpen, 0, &why) == KeptAnswer::Applied &&
	          NumberIn(door, "DoorFlags", store.held[Fake(1)]) == (double)((uint32_t)before | 0x1 | 0x4),
	      "applied: BOTH bits are in the word: the first change wasn't lost, though the panel's own copy of the word never had it");

	// And a bit turned off
	shown.panel.ClickField(locked);
	Check(shown.panel.Handle(ui::KeyEvent(Key::Space)) == PanelAnswer::Kept && !shown.panel.BitChecked(locked), "Space turns 'Door is locked' off again");
	Check(ApplyKeptStrategyField(&store, Fake(1), door, shown.record, shown.panel, locked, 0, &why) == KeptAnswer::Applied &&
	          NumberIn(door, "DoorFlags", store.held[Fake(1)]) == (double)(((uint32_t)before | 0x4) & ~0x1u),
	      "applied: the first bit is off, the second stays on");

	// EditFromPanel by hand: the word it is given is the one the bit is set or cleared in
	FieldEdit edit;
	shown.panel.ClickField(locked);
	shown.panel.Handle(ui::KeyEvent(Key::Space));   // on again
	Check(host::EditFromPanel(shown.record, shown.panel, locked, 0xF0, &edit, &why) == PanelEditAnswer::Made && edit.what == host::EditWhat::Number &&
	          edit.number == (double)(0xF0u | 0x1) && strcmp(edit.field, "DoorFlags") == 0,
	      "EditFromPanel sets the bit in the word it's given, and names the field by its model name");
}

static void TestChoiceAxisObject()
{
	puts("4. a choice, one axis of three, and an object are kept and applied");

	StrategyKind const &binSwitch = *FindStrategyKind(52);
	Bytes const original = NewStrategy(52);
	FakeStore store;
	store.held[Fake(1)] = original;

	Shown shown;
	char const *why = 0;
	host::BuildEditableStrategyRecord(original.data(), original.size(), &shown.record, &why);
	shown.Load();

	size_t const mode = At(shown.record, "Mode");
	shown.panel.ClickField(mode);
	double const modeBefore = NumberIn(binSwitch, "Mode", original);
	Check(shown.panel.Handle(ui::KeyEvent(Key::Enter)) == PanelAnswer::Moved && shown.panel.Handle(ui::KeyEvent(Key::Down)) == PanelAnswer::Moved &&
	          shown.panel.Handle(ui::KeyEvent(Key::Enter)) == PanelAnswer::Kept && (double)shown.panel.ChoiceValue(mode) != modeBefore,
	      "the mode's list is opened, the next choice is lit and picked");
	Check(ApplyKeptStrategyField(&store, Fake(1), binSwitch, shown.record, shown.panel, mode, 0, &why) == KeptAnswer::Applied &&
	          NumberIn(binSwitch, "Mode", store.held[Fake(1)]) == (double)shown.panel.ChoiceValue(mode),
	      "applied: the strategy holds the choice's value");

	// An object: typed as text
	size_t const target = At(shown.record, "Switch's target");
	shown.panel.ClickField(target);
	Check(TypeOver(shown.panel, "hello") != PanelAnswer::Kept, "text that isn't an object is not kept by the panel");
	Check(shown.panel.StatusWords() && strcmp(shown.panel.StatusWords(), host::NotAnObjectReason) == 0 && shown.panel.StatusIsTheSpecs(),
	      "and the status line gives the object check's own words");
	Check(TypeOver(shown.panel, "7334ED09:ef81a000") == PanelAnswer::Kept, "an object's two halves, in either case of hex digits, are kept");

	Field const &targetField = *FieldNamed(binSwitch, "Target");
	Check(ApplyKeptStrategyField(&store, Fake(1), binSwitch, shown.record, shown.panel, target, 0, &why) == KeptAnswer::Applied, "applied");
	uint32_t first = 0, second = 0;
	Bytes const &held = store.held[Fake(1)];
	Check(ReadFieldObject(binSwitch, targetField, held.data(), held.size(), &first, &second) && first == 0x7334ED09u && second == 0xEF81A000u,
	      "the object's halves are in the bytes, first then second");

	Check(TypeOver(shown.panel, "none") == PanelAnswer::Kept, "'none' is kept");
	Check(ApplyKeptStrategyField(&store, Fake(1), binSwitch, shown.record, shown.panel, target, 0, &why) == KeptAnswer::Applied &&
	          ReadFieldObject(binSwitch, targetField, store.held[Fake(1)].data(), store.held[Fake(1)].size(), &first, &second) && first == 0 && second == 0,
	      "applied: the object is none again");

	// An axis: a volume of death's corner
	StrategyKind const &volume = *FindStrategyKind(77);
	Bytes const volumeBytes = NewStrategy(77);
	store.held[Fake(2)] = volumeBytes;
	Shown corner;
	host::BuildEditableStrategyRecord(volumeBytes.data(), volumeBytes.size(), &corner.record, &why);
	corner.Load();
	size_t const y = At(corner.record, "V : Edit Volume y");
	Check(y != (size_t)-1, "the corner's y is its own box");
	corner.panel.ClickField(y);
	Check(TypeOver(corner.panel, "-500") == PanelAnswer::Kept, "-500 is kept");
	Check(ApplyKeptStrategyField(&store, Fake(2), volume, corner.record, corner.panel, y, 0, &why) == KeptAnswer::Applied, "applied");

	Field const &cornerField = *FieldNamed(volume, "volume_min");
	int32_t axes[3] = { 7, 7, 7 };
	int32_t was[3] = { 7, 7, 7 };
	bool read = true;

	for (int axis = 0; axis < 3; axis++)
	{
		read = read && ReadFieldAxis(volume, cornerField, store.held[Fake(2)].data(), store.held[Fake(2)].size(), axis, &axes[axis], 0) &&
		       ReadFieldAxis(volume, cornerField, volumeBytes.data(), volumeBytes.size(), axis, &was[axis], 0);
	}

	Check(read && axes[1] == -500 && axes[0] == was[0] && axes[2] == was[2], "the corner's y is -500, and its x and z are as they were");
}

static void TestHostileValues()
{
	puts("5. what the panel hands over is hostile: an axis that no int32 holds, a fraction, and a field nobody can edit");

	StrategyKind const &volume = *FindStrategyKind(77);
	Bytes const volumeBytes = NewStrategy(77);
	char const *why = 0;

	// A level's own value can be outside the field's range: the panel keeps it as it is. The panel's own boxes never let a typed one through,
	// so this loads one straight in, as a level could hold it
	double const handed[] = { 5e9, -5e9, 1.5, 0.0, 2147483648.0 };

	for (size_t n = 0; n < sizeof(handed) / sizeof(handed[0]); n++)
	{
		Shown corner;
		host::BuildEditableStrategyRecord(volumeBytes.data(), volumeBytes.size(), &corner.record, &why);
		size_t const x = At(corner.record, "V : Edit Volume x");
		corner.record.values[x].number = handed[n];
		corner.Load();

		FieldEdit edit;
		PanelEditAnswer answer = PanelEditAnswer::BadCall;
		why = 0;
		bool const ran = Survives([&] { answer = host::EditFromPanel(corner.record, corner.panel, x, 0, &edit, &why); });
		bool const wanted = handed[n] == 0.0;
		char words[96];
		snprintf(words, sizeof(words), "an axis value of %g is %s", handed[n], wanted ? "made into an edit" : "refused, with a reason");
		Check(ran && (wanted ? answer == PanelEditAnswer::Made && edit.what == host::EditWhat::Axis && edit.axisValue == 0
		                     : answer == PanelEditAnswer::ValueRefused && why != 0),
		      words);

		// The whole apply says the same, and a refused one never reaches the store
		FakeStore applyStore;
		applyStore.held[Fake(1)] = volumeBytes;
		KeptAnswer kept = KeptAnswer::Applied;
		bool const ranApply = Survives([&] { kept = ApplyKeptStrategyField(&applyStore, Fake(1), volume, corner.record, corner.panel, x, 0, &why); });
		snprintf(words, sizeof(words), "the apply of an axis value of %g is %s", handed[n], wanted ? "made" : "refused, and the store is never asked to keep it");
		Check(ranApply && kept == (wanted ? KeptAnswer::Applied : KeptAnswer::ValueRefused) && applyStore.keeps == (wanted ? 1 : 0), words);
	}

	// A number that isn't a number
	{
		Shown corner;
		host::BuildEditableStrategyRecord(volumeBytes.data(), volumeBytes.size(), &corner.record, &why);
		size_t const x = At(corner.record, "V : Edit Volume x");
		uint64_t const bits = 0x7FF8000000000000ull;   // a quiet NaN, by its bits: the editor is built with /fp:fast
		memcpy(&corner.record.values[x].number, &bits, sizeof(bits));
		corner.Load();

		FieldEdit edit;
		PanelEditAnswer answer = PanelEditAnswer::Made;
		bool const ran = Survives([&] { answer = host::EditFromPanel(corner.record, corner.panel, x, 0, &edit, &why); });
		Check(ran && answer == PanelEditAnswer::ValueRefused, "a NaN is refused as an axis value");
	}

	// Bad calls
	Shown corner;
	host::BuildEditableStrategyRecord(volumeBytes.data(), volumeBytes.size(), &corner.record, &why);
	corner.Load();
	FieldEdit edit;
	bool ran = Survives([&] { Check(host::EditFromPanel(corner.record, corner.panel, 0, 0, 0, 0) == PanelEditAnswer::BadCall, "no place for the edit is a bad call"); });
	Check(ran, "(and it doesn't crash)");
	Check(host::EditFromPanel(corner.record, corner.panel, corner.record.list.size(), 0, &edit, 0) == PanelEditAnswer::BadCall, "a field past the record is a bad call");

	ui::FieldPanel unloaded;
	Check(host::EditFromPanel(corner.record, unloaded, 0, 0, &edit, 0) == PanelEditAnswer::BadCall, "a panel with nothing loaded is a bad call");

	// A record with no source for a descriptor: not editable, whatever the descriptor says
	Shown headerOnly;
	host::PushBit(&headerOnly.record, "Header bit", 0, 0x1, 0);
	headerOnly.record.list[0].readOnly = false;   // even a descriptor that says it's open is refused when nothing says where it comes from
	headerOnly.Load();
	Check(host::EditFromPanel(headerOnly.record, headerOnly.panel, 0, 0, &edit, 0) == PanelEditAnswer::NotEditable, "a descriptor with no source is NotEditable");
	(void)volume;
}

static void TestTheSpecsCheckInsideThePanel()
{
	puts("6. the field spec's own check is inside the panel: a number the box takes but the field can't hold is refused, with the spec's words");

	// A kind of its own, numbered like a placed object (62), whose field "Type" is a choice (the model names 0 to 16 for it): a stored value no
	// int32 holds is shown as a Number, so it can be typed over, and only the named values may be kept
	static Field const OwnFields[] =
	{
		{ "Mask", "Masked", Anchor::Start, 20, Storage::UInt32, Unit::Plain, 0x30, Limit::Editor, 0, 0x30, "the mask's reason", false, 0, "" },
		{ "Type", "Type", Anchor::Start, 24, Storage::UInt32, Unit::Choice, 0, Limit::None, 0, 0, "", false, 0, "" },
	};
	static StrategyKind const OwnKind = { 62, "Own", "", SizeRule::AnySize, 28, OwnFields, 2, false, false, 62 };
	Bytes own(28, 0);
	PutInt(own, 8, 62);
	PutInt(own, 24, -1);   // 0xFFFFFFFF
	Shown shown;
	char const *why = 0;
	Check(host::BuildStrategyRecordOfKind(OwnKind, own.data(), own.size(), &shown.record, &why), "the kind of its own is built");
	host::MakeStrategyRecordEditable(&shown.record, OwnKind);
	Check(shown.Load(), "and loaded");

	size_t const mask = At(shown.record, "Masked");
	size_t const type = At(shown.record, "Type");
	Check(type != (size_t)-1 && shown.record.list[type].kind == ui::FieldKind::Number && shown.record.sources[type].what == host::SourceKind::Number && shown.record.sources[type].field->unit == Unit::Choice,
	      "a choice no int32 holds is a Number, sourced from a field whose unit is Choice");

	shown.panel.ClickField(mask);
	Check(TypeOver(shown.panel, "5") != PanelAnswer::Kept && shown.panel.StatusIsTheSpecs() && shown.panel.StatusWords() &&
	          strcmp(shown.panel.StatusWords(), DoesNotFitReason) == 0,
	      "5 is inside the box's range but not inside the field's mask: refused, with the model's own words");
	Check(TypeOver(shown.panel, "32") == PanelAnswer::Kept && shown.panel.NumberValue(mask) == 32.0, "32 fits the mask: kept");

	shown.panel.ClickField(type);
	Check(TypeOver(shown.panel, "20") != PanelAnswer::Kept && shown.panel.StatusIsTheSpecs() && shown.panel.StatusWords() &&
	          strcmp(shown.panel.StatusWords(), UnnamedChoiceReason) == 0,
	      "20 is a value the field takes but its choices don't name: refused, with the model's own words");
	Check(TypeOver(shown.panel, "5") == PanelAnswer::Kept && shown.panel.NumberValue(type) == 5.0, "5 is named: kept");
	Check(TypeOver(shown.panel, "4294967295") != PanelAnswer::Kept, "and a value no int32 holds can't be named: not kept");
}

static void TestChunkEdit()
{
	puts("7. a chunk's fields are kept and applied the same way");

	ChunkKind const &particle = *FindChunkKind("PARGENDA");
	Bytes const original = ValidChunk(particle);
	FakeStore store;
	store.held[Fake(1)] = original;

	Shown shown;
	char const *why = 0;
	Check(host::BuildEditableChunkRecord("PARGENDA", original.data(), original.size(), &shown.record, &why) && shown.Load(), "the particle generator's editable record is loaded");

	size_t const frequency = At(shown.record, "Frequency");
	size_t const probability = At(shown.record, "Probability");
	size_t const inactive = At(shown.record, "Inactive");
	size_t const type = At(shown.record, "Type");
	size_t const parent = At(shown.record, "Parent");

	shown.panel.ClickField(frequency);
	Check(TypeOver(shown.panel, "77") == PanelAnswer::Kept, "77 is kept for the frequency");
	Check(ApplyKeptChunkField(&store, Fake(1), particle, shown.record, shown.panel, frequency, 0, &why) == KeptAnswer::Applied &&
	          ChunkNumberIn(particle, "time", store.held[Fake(1)]) == 77,
	      "applied: the chunk holds 77");

	shown.panel.ClickField(probability);
	Check(TypeOver(shown.panel, "150") != PanelAnswer::Kept, "150 percent isn't kept: the field's range is 0 to 100");

	shown.panel.ClickField(inactive);
	Check(shown.panel.Handle(ui::KeyEvent(Key::Space)) == PanelAnswer::Kept, "the inactive bit is turned on");
	Check(ApplyKeptChunkField(&store, Fake(1), particle, shown.record, shown.panel, inactive, 0, &why) == KeptAnswer::Applied &&
	          ChunkNumberIn(particle, "flags", store.held[Fake(1)]) == 1,
	      "applied: the chunk's flags word has the bit");

	shown.panel.ClickField(type);
	Check(shown.panel.Handle(ui::KeyEvent(Key::Enter)) == PanelAnswer::Moved && shown.panel.Handle(ui::KeyEvent(Key::Down)) == PanelAnswer::Moved &&
	          shown.panel.Handle(ui::KeyEvent(Key::Enter)) == PanelAnswer::Kept && shown.panel.ChoiceValue(type) == 1,
	      "the next particle type is picked");
	Check(ApplyKeptChunkField(&store, Fake(1), particle, shown.record, shown.panel, type, 0, &why) == KeptAnswer::Applied &&
	          ChunkNumberIn(particle, "type", store.held[Fake(1)]) == 1,
	      "applied: the chunk holds the type");

	shown.panel.ClickField(parent);
	Check(TypeOver(shown.panel, "00000001:00000002") == PanelAnswer::Kept, "an object is kept for the parent");
	Check(ApplyKeptChunkField(&store, Fake(1), particle, shown.record, shown.panel, parent, 0, &why) == KeptAnswer::Applied, "applied");
	uint32_t first = 0, second = 0;
	Check(ReadChunkFieldObject(particle, *ChunkFieldNamed(particle, "parent_id"), store.held[Fake(1)].data(), store.held[Fake(1)].size(), &first, &second) &&
	          first == 1 && second == 2,
	      "the parent's two halves are in the chunk");

	// All the changes are in, and nothing else is
	Bytes expected = original;
	PutInt(expected, 4, 1);                         // flags
	expected[8] = 77;                               // time, 16 bits, little-endian
	PutInt(expected, 0, 1);                         // type
	PutInt(expected, 32, 1);
	PutInt(expected, 36, 2);                        // parent_id
	Check(store.held[Fake(1)] == expected && store.keeps == 4, "the chunk's bytes are the original with exactly those fields changed, and the store kept four times");

	// The other kind's chunk
	ChunkKind const &light = *FindChunkKind("STDLIGHT");
	Check(ApplyKeptChunkField(&store, Fake(1), light, shown.record, shown.panel, frequency, 0, &why) == KeptAnswer::NotEditable && store.keeps == 4,
	      "a particle generator's record applied as a light's is NotEditable: the field isn't that chunk's");

	// A strategy's record applied as a chunk's, and the other way round
	Shown door;
	Bytes const doorBytes = NewStrategy(50);
	host::BuildEditableStrategyRecord(doorBytes.data(), doorBytes.size(), &door.record, &why);
	door.Load();
	Check(ApplyKeptChunkField(&store, Fake(1), particle, door.record, door.panel, 0, 0, &why) == KeptAnswer::NotEditable &&
	          ApplyKeptStrategyField(&store, Fake(1), *FindStrategyKind(50), shown.record, shown.panel, frequency, 0, &why) == KeptAnswer::NotEditable,
	      "a strategy's record is never applied as a chunk's, nor a chunk's as a strategy's");
}

// inteng's light mode: turning No PreLight on also leaves the Omni bit on in what it saves (F-LIGHT-COUPLING, FUN_004acf30 / FUN_004ad630); turning it off changes
// nothing saved; and No shadows (engine 0x10), which inteng never changes, is not offered
static void TestLightCoupling()
{
	puts("7b. a light's No PreLight carries the Omni bit with it, as inteng's does");

	ChunkKind const &light = *FindChunkKind("STDLIGHT");
	Bytes original = ValidChunk(light);
	original.resize(84, 0);
	FakeStore store;
	store.held[Fake(1)] = original;

	Shown shown;
	char const *why = 0;
	Check(host::BuildEditableChunkRecord("STDLIGHT", original.data(), original.size(), &shown.record, &why) && shown.Load(), "a light's editable record is loaded");

	size_t const noPreLight = At(shown.record, "No prelight");
	Check(noPreLight != (size_t)-1, "it has the No PreLight bit");
	Check(At(shown.record, "No shadows") == (size_t)-1 || shown.record.list[At(shown.record, "No shadows")].readOnly,
	      "and No shadows, which inteng's light mode never changes, is not offered as a change");

	if (noPreLight == (size_t)-1)
	{
		return;
	}

	shown.panel.ClickField(noPreLight);
	Check(shown.panel.Handle(ui::KeyEvent(Key::Space)) == PanelAnswer::Kept, "No PreLight is turned on");
	Check(ApplyKeptChunkField(&store, Fake(1), light, shown.record, shown.panel, noPreLight, 0, &why) == KeptAnswer::Applied &&
	          ChunkNumberIn(light, "local_light_flags", store.held[Fake(1)]) == 0x40 && ChunkNumberIn(light, "engine_light_flags", store.held[Fake(1)]) == 0,
	      "applied: the local flags word has 0x40, and the engine word is still 0 until the coupling runs");
	Check(host::ApplyLightCoupling(&store, Fake(1), light, shown.record, noPreLight) == host::ApplyLightCouplingAnswer::Applied &&
	          ChunkNumberIn(light, "engine_light_flags", store.held[Fake(1)]) == 4 && ChunkNumberIn(light, "local_light_flags", store.held[Fake(1)]) == 0x40,
	      "the coupling puts the Omni bit (4) in the engine word and changes nothing else");
	Check(host::ApplyLightCoupling(&store, Fake(1), light, shown.record, noPreLight) == host::ApplyLightCouplingAnswer::NotCoupled,
	      "and run again it has nothing more to do");

	// the panel is made again from the bytes the store holds (as the editor does after a change), and the bit is turned off
	Shown again;
	Check(host::BuildEditableChunkRecord("STDLIGHT", store.held[Fake(1)].data(), store.held[Fake(1)].size(), &again.record, &why) && again.Load(), "the record is made again from the held bytes");
	size_t const noPreLightAgain = At(again.record, "No prelight");
	again.panel.ClickField(noPreLightAgain);
	Check(again.panel.Handle(ui::KeyEvent(Key::Space)) == PanelAnswer::Kept &&
	          ApplyKeptChunkField(&store, Fake(1), light, again.record, again.panel, noPreLightAgain, 0, &why) == KeptAnswer::Applied &&
	          host::ApplyLightCoupling(&store, Fake(1), light, again.record, noPreLightAgain) == host::ApplyLightCouplingAnswer::NotCoupled &&
	          ChunkNumberIn(light, "local_light_flags", store.held[Fake(1)]) == 0 && ChunkNumberIn(light, "engine_light_flags", store.held[Fake(1)]) == 4,
	      "turned off again, the local bit goes and the engine word is left as it is (inteng clears 0x300 only, which its save never writes)");

	// A bit that is not No PreLight has no coupling
	size_t const runtime = At(shown.record, "Runtime");
	Check(runtime == (size_t)-1 || host::ApplyLightCoupling(&store, Fake(1), light, shown.record, runtime) == host::ApplyLightCouplingAnswer::NotCoupled,
	      "another bit has no coupling");
}

// A light's brightness, spread and range are shown and taken in the units inteng's light shows them in (FUN_004ac2f0: percent, degrees, metres, two decimals), and kept as the whole numbers the chunk holds
static void TestLightUnits()
{
	puts("7c. a light's numbers are in inteng's units");

	host::SetDisplayLevelScale(0.8);

	ChunkKind const &light = *FindChunkKind("STDLIGHT");
	Bytes original = ValidChunk(light);
	original.resize(84, 0);
	PutInt(original, 52, 19586);   // brightness 29.89%
	PutInt(original, 56, 1000);    // spread 87.89 degrees
	PutInt(original, 60, 9200);    // range 7.36 m at scale 0.8
	FakeStore store;
	store.held[Fake(1)] = original;

	Shown shown;
	char const *why = 0;
	Check(host::BuildEditableChunkRecord("STDLIGHT", original.data(), original.size(), &shown.record, &why) && shown.Load(), "a light's editable record is loaded");

	size_t const brightness = At(shown.record, "Brightness");
	size_t const spread = At(shown.record, "Spread");
	size_t const range = At(shown.record, "Range");

	Check(brightness != (size_t)-1 && spread != (size_t)-1 && range != (size_t)-1, "it has brightness, spread and range");

	if (brightness == (size_t)-1 || spread == (size_t)-1 || range == (size_t)-1)
	{
		return;
	}

	Check(fabs(shown.record.values[brightness].number - 29.8859) < 0.001 && strcmp(shown.record.list[brightness].unit, "%") == 0 && shown.record.list[brightness].decimals == 2, "the brightness shows as 29.89 %");
	Check(fabs(shown.record.values[spread].number - 87.890625) < 0.0001 && strcmp(shown.record.list[spread].unit, "deg") == 0, "the spread shows as 87.89 deg");
	Check(fabs(shown.record.values[range].number - 7.36) < 0.0001 && strcmp(shown.record.list[range].unit, "m") == 0, "the range shows as 7.36 m (9200 mm x the level's scale 0.8)");
	Check(fabs(shown.record.list[brightness].highest - 200.0) < 0.0001 && fabs(shown.record.list[spread].highest - 180.0) < 0.0001, "and the limits are in the same units: brightness up to 200 %, spread up to 180 deg");

	shown.panel.ClickField(brightness);
	Check(TypeOver(shown.panel, "50") == PanelAnswer::Kept, "50 % is kept for the brightness");
	Check(ApplyKeptChunkField(&store, Fake(1), light, shown.record, shown.panel, brightness, 0, &why) == KeptAnswer::Applied && ChunkNumberIn(light, "brightness", store.held[Fake(1)]) == 32768, "applied: the chunk holds 32768");

	shown.panel.ClickField(spread);
	Check(TypeOver(shown.panel, "90") == PanelAnswer::Kept, "90 deg is kept for the spread");
	Check(ApplyKeptChunkField(&store, Fake(1), light, shown.record, shown.panel, spread, 0, &why) == KeptAnswer::Applied && ChunkNumberIn(light, "spread", store.held[Fake(1)]) == 1024, "applied: the chunk holds 1024");

	shown.panel.ClickField(range);
	Check(TypeOver(shown.panel, "4") == PanelAnswer::Kept, "4 m is kept for the range");
	Check(ApplyKeptChunkField(&store, Fake(1), light, shown.record, shown.panel, range, 0, &why) == KeptAnswer::Applied && ChunkNumberIn(light, "range", store.held[Fake(1)]) == 5000, "applied: the chunk holds 5000 mm (4 m at scale 0.8)");

	shown.panel.ClickField(brightness);
	Check(TypeOver(shown.panel, "250") != PanelAnswer::Kept, "250 % is refused: inteng keeps a light's brightness to 200 %");
	host::SetDisplayLevelScale(1.0);
}

static void TestObjectText()
{
	puts("8. the text of an object");

	uint32_t first = 7, second = 7;
	Check(host::ParseObjectText((unsigned char const *)"none", 4, &first, &second) && first == 0 && second == 0, "'none' is no object");
	first = second = 7;
	Check(host::ParseObjectText((unsigned char const *)"None", 4, &first, &second) && first == 0 && second == 0 &&
	          host::ParseObjectText((unsigned char const *)"NONE", 4, &first, &second),
	      "in any case");
	Check(host::ParseObjectText((unsigned char const *)"00000000:00000000", 17, &first, &second) && first == 0 && second == 0, "eight zeros twice is no object either");
	Check(host::ParseObjectText((unsigned char const *)"7334ED09:EF81A000", 17, &first, &second) && first == 0x7334ED09u && second == 0xEF81A000u,
	      "two groups of 8 hex digits are the two halves");
	Check(host::ParseObjectText((unsigned char const *)"abcdef01:23456789", 17, &first, &second) && first == 0xABCDEF01u && second == 0x23456789u, "in lower case too");

	first = second = 7;
	unsigned char const *const bad[] = {
		(unsigned char const *)"7334ED09-EF81A000", (unsigned char const *)"7334ED09:EF81A00", (unsigned char const *)"7334ED09:EF81A0000",
		(unsigned char const *)"7334ED0G:EF81A000", (unsigned char const *)"7334ED09:EF81A00 ", (unsigned char const *)"none ", (unsigned char const *)"nonex",
		(unsigned char const *)"\xFF" "334ED09:EF81A000", (unsigned char const *)"", (unsigned char const *)"7334ED09EF81A0000",
	};
	size_t const lengths[] = { 17, 16, 18, 17, 17, 5, 5, 17, 0, 17 };
	bool refused = true;

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		refused = refused && !host::ParseObjectText(bad[i], lengths[i], &first, &second);
	}

	Check(refused && first == 7 && second == 7, "dashes, a short or long text, a letter that isn't a digit, a space, a high byte and an empty text are refused, and nothing is set");

	bool ran = Survives([&] { Check(!host::ParseObjectText(0, 17, &first, &second), "no text is refused"); });
	Check(ran, "(and it doesn't crash)");
	ran = Survives([&] { Check(!host::ParseObjectText((unsigned char const *)"none", 4, 0, &second) && !host::ParseObjectText((unsigned char const *)"none", 4, &first, 0), "no place for the answer is refused"); });
	Check(ran, "(and it doesn't crash)");
}

int main()
{
	TestRecordsAndSources();
	TestNumberEdit();
	TestBits();
	TestChoiceAxisObject();
	TestHostileValues();
	TestTheSpecsCheckInsideThePanel();
	TestChunkEdit();
	TestLightCoupling();
	TestLightUnits();
	TestObjectText();

	// inteng's Door: Proximity door (0x2) and Switch activated door (0x8) are one choice (FUN_0052cbb0 0x52cdff)
	Check(host::InteingFlagRules(50, "DoorFlags", 0x2, 0x2) == 0x2 && host::InteingFlagRules(50, "DoorFlags", 0x2, 0x2 | 0x8) == 0x2, "inteng rule: Proximity door on turns Switch activated off");
	Check(host::InteingFlagRules(50, "DoorFlags", 0x8, 0x8 | 0x2) == 0x8 && host::InteingFlagRules(50, "DoorFlags", 0x8, 0x1 | 0x8) == (0x1 | 0x8), "inteng rule: Switch activated door on turns Proximity off, other bits kept");
	Check(host::InteingFlagRules(50, "DoorFlags", 0x2, 0x1) == (0x1 | 0x8) && host::InteingFlagRules(50, "DoorFlags", 0x8, 0x4) == (0x4 | 0x2), "inteng rule: turning one of the pair off turns the other on");
	Check(host::InteingFlagRules(50, "DoorFlags", 0x1, 0x1) == 0x1 && host::InteingFlagRules(75, "DoorFlags", 0x2, 0x2 | 0x8) == (0x2 | 0x8), "inteng rule: Locked and other kinds are left as the panel made them");

	// a target picked by name (inteng's "Select target for switch" list): with a resolver an object field takes a name, without one it doesn't
	{
		uint32_t a = 7, b = 7;
		unsigned char const door[] = { 'd', 'o', 'o', 'r', '7' };

		Check(!host::ParseObjectText(door, sizeof(door), &a, &b), "an object's name is refused when the editor gives no lookup");

		host::SetObjectNameResolver([](char const *name, size_t length, uint32_t *first, uint32_t *second) -> bool
		{
			if (length == 5 && strcmp(name, "door7") == 0)
			{
				*first = 0x11223344;
				*second = 0x55667788;
				return true;
			}

			return false;
		});

		unsigned char const other[] = { 'n', 'o', 'p', 'e' };
		unsigned char const hex[] = "7334ED09:EF81A000";

		Check(host::ParseObjectText(door, sizeof(door), &a, &b) && a == 0x11223344 && b == 0x55667788 && !host::ParseObjectText(other, sizeof(other), &a, &b) &&
		      host::ParseObjectText(hex, 17, &a, &b) && a == 0x7334ED09 && b == 0xEF81A000, "an object's name is taken through the lookup; an unknown name is refused; the hex form still works");

		host::SetObjectNameResolver(0);
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
