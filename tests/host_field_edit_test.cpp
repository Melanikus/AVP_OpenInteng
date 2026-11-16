// The test for src/host/field_edit: a value changed in the field panel is checked by the field model and put into the bytes of its chunk.
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file, and it needs no chunk library: a chunk is only a name the store knows it by, so a test's
// "chunks" are made-up addresses that nothing reads, and its store is a small one of its own that counts what it's asked to keep. It
// builds strategy and chunk payloads in memory with the field model, edits them, and checks the bytes: exactly one field's bytes change,
// and an edit that's refused changes nothing and never reaches the store. The program gives back a non-zero result if any check failed.
// A check that would crash if a guard were lost runs under Survives (crash_guard.h), so it FAILS by name.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "crash_guard.h"
#include "field_edit.h"
#include "field_model.h"

using namespace model;
using host::EditAnswer;
using host::EditWhat;
using host::FieldEdit;

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

// A store of the test's own: what it holds, how often it was asked to keep something, and whether it will
class FakeStore : public host::PayloadStore
{
public:
	std::map<Chunk *, Bytes> held;
	int keeps;
	bool willKeep;

	FakeStore() : keeps(0), willKeep(true) {}

	bool PayloadOf(Chunk *chunk, Bytes *payload) const override
	{
		std::map<Chunk *, Bytes>::const_iterator const found = held.find(chunk);

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

		if (!willKeep)
		{
			return false;
		}

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

static int32_t GetIntAt(Bytes const &bytes, size_t at)
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

// An edit under Survives: if it crashed, the answer is Crashed, so the check that asked fails BY NAME instead of the program stopping
static EditAnswer const Crashed = (EditAnswer)99;

static EditAnswer StrategyEdit(host::PayloadStore *store, Chunk *chunk, int32_t kind, FieldEdit const &edit, char const **why)
{
	EditAnswer answer = Crashed;
	Survives([&] { answer = host::ApplyStrategyPayloadEdit(store, chunk, kind, edit, why); });
	return answer;
}

static EditAnswer ChunkEdit(host::PayloadStore *store, Chunk *chunk, ChunkKind const &kind, FieldEdit const &edit, char const **why)
{
	EditAnswer answer = Crashed;
	Survives([&] { answer = host::ApplyChunkEdit(store, chunk, kind, edit, why); });
	return answer;
}

static FieldEdit Number(char const *field, double value)
{
	FieldEdit edit;
	edit.field = field;
	edit.what = EditWhat::Number;
	edit.number = value;
	return edit;
}

static FieldEdit AxisOf(char const *field, int axis, int32_t value)
{
	FieldEdit edit;
	edit.field = field;
	edit.what = EditWhat::Axis;
	edit.axis = axis;
	edit.axisValue = value;
	return edit;
}

static FieldEdit ObjectOf(char const *field, uint32_t first, uint32_t second)
{
	FieldEdit edit;
	edit.field = field;
	edit.what = EditWhat::Object;
	edit.first = first;
	edit.second = second;
	return edit;
}

static void StrategyEdits()
{
	printf("Editing a strategy:\n");

	FakeStore store;
	Chunk *const door = Fake(1);
	Bytes const doorBefore = NewStrategy(50);
	store.held[door] = doorBefore;
	char const *why = 0;

	Check(!doorBefore.empty(), "a new door is made by the field model");
	Check(StrategyEdit(&store, door, 50, Number("TimeToOpen", 33), &why) == EditAnswer::Applied && store.keeps == 1,
	      "a door's time to open is set to 33, and the store is asked to keep the bytes once");

	Bytes expected = doorBefore;
	expected[24] = 33;
	Check(store.held[door] == expected, "exactly one byte changed: byte 24, the time to open. The index, the spare number, the kind and every other byte are as they were");

	Check(StrategyEdit(&store, door, 50, Number("TimeToClose", 9), &why) == EditAnswer::Applied && store.held[door][24] == 33 && store.held[door][25] == 9,
	      "a second edit of the same chunk builds on the first: the time to close is 9 and the time to open is still 33");

	Check(StrategyEdit(&store, door, 50, Number("DoorFlags", 5), &why) == EditAnswer::Applied && store.held[door][20] == 5 && store.held[door][21] == 0,
	      "a flags word is set as a whole number: 5 in its four bytes");

	Check(StrategyEdit(&store, door, 50, Number("index", 7), &why) == EditAnswer::Applied && store.held[door][0] == 7 && store.held[door][1] == 0 &&
	      store.held[door][4] == 0x88,
	      "a field every strategy has (its index, at byte 0) can be edited, and the spare number beside it is not touched");

	// Another kinds: a three-number field and an object field
	Chunk *const volume = Fake(2);
	Chunk *const switchDoor = Fake(3);
	Bytes const volumeBefore = NewStrategy(77);
	Bytes const switchBefore = NewStrategy(59);
	store.held[volume] = volumeBefore;
	store.held[switchDoor] = switchBefore;

	Check(StrategyEdit(&store, volume, 77, AxisOf("volume_min", 1, 500), &why) == EditAnswer::Applied && GetIntAt(store.held[volume], 24) == 500 &&
	      GetIntAt(store.held[volume], 20) == GetIntAt(volumeBefore, 20) && GetIntAt(store.held[volume], 28) == GetIntAt(volumeBefore, 28),
	      "one number of a three-number field is set, 500 for y, and x and z are as they were");

	Check(StrategyEdit(&store, switchDoor, 59, ObjectOf("AssocDoor", 7, 9), &why) == EditAnswer::Applied && GetIntAt(store.held[switchDoor], 20) == 7 &&
	      GetIntAt(store.held[switchDoor], 24) == 9, "an object field is set to its two halves, 7 and 9");
}

// Everything below changes nothing: the bytes are as they were and the store is never asked to keep any
static void Refusals()
{
	printf("What an edit refuses, and what it leaves alone:\n");

	FakeStore store;
	Chunk *const door = Fake(1);
	Bytes const before = NewStrategy(50);
	store.held[door] = before;
	char const *why = 0;

	auto untouched = [&]() { return store.keeps == 0 && store.held[door] == before; };

	double nan = 0;
	uint64_t const nanBits = 0x7FF8000000000000ull;
	memcpy(&nan, &nanBits, sizeof(nan));

	Check(StrategyEdit(&store, door, 50, Number("TimeToOpen", 300), &why) == EditAnswer::ValueRefused && why && strlen(why) > 0 && untouched(),
	      "a time of 300 doesn't fit a byte: refused, with the model's own words, and nothing is kept");
	why = 0;
	Check(StrategyEdit(&store, door, 50, Number("TimeToOpen", nan), &why) == EditAnswer::ValueRefused && why && strcmp(why, NotANumberReason) == 0 && untouched(),
	      "a number that isn't one is refused for that reason");
	why = 0;
	Check(StrategyEdit(&store, door, 50, Number("TimeToOpen", 2.5), &why) == EditAnswer::ValueRefused && why && strcmp(why, NotWholeReason) == 0 && untouched(),
	      "a fraction in a whole-number field is refused for that reason");

	// A light effect's type is 0 to 3
	Chunk *const light = Fake(2);
	Bytes const lightBefore = NewStrategy(64);
	store.held[light] = lightBefore;
	StrategyKind const *const lighting = FindStrategyKind(64);
	Field const *typeField = 0;

	for (size_t i = 0; lighting && i < lighting->fieldCount; i++)
	{
		typeField = strcmp(lighting->fields[i].name, "type") == 0 ? &lighting->fields[i] : typeField;
	}

	why = 0;
	Check(typeField && StrategyEdit(&store, light, 64, Number("type", 4), &why) == EditAnswer::ValueRefused && why && strcmp(why, typeField->why) == 0 &&
	      store.held[light] == lightBefore && store.keeps == 0,
	      "a light effect's type of 4 is refused with the field's own reason, the very words of its table");
	Check(StrategyEdit(&store, light, 64, Number("type", 2), &why) == EditAnswer::Applied && GetIntAt(store.held[light], 20) == 2 && store.keeps == 1,
	      "and a named type, 2, is taken");
	store.keeps = 0;

	// A list's count can't be set from a number
	Chunk *const lift = Fake(3);
	Bytes const liftBefore = NewStrategy(51);
	store.held[lift] = liftBefore;
	why = 0;
	Check(!liftBefore.empty() && StrategyEdit(&store, lift, 51, Number("NumAssocLifts", 2), &why) == EditAnswer::ValueRefused && why && strcmp(why, ListCountReason) == 0 &&
	      store.held[lift] == liftBefore && store.keeps == 0, "a list's count is refused: it changes only with its items");

	// What the edit names
	Check(StrategyEdit(&store, door, 50, Number("NoSuchThing", 1), &why) == EditAnswer::NoSuchField && untouched(), "a name that is no field is refused");
	Check(StrategyEdit(&store, door, 50, Number("fan_wind_strength", 1), &why) == EditAnswer::NoSuchField && untouched(), "another kind's field is refused");
	Check(StrategyEdit(&store, door, 50, Number("StrategyType", 51), &why) == EditAnswer::NoSuchField && untouched(),
	      "the kind's own number isn't a field to edit: refused, so a strategy can't be made another kind");
	Check(StrategyEdit(&store, door, 50, Number("", 1), &why) == EditAnswer::BadCall && untouched(), "an empty name is a bad call");
	Check(StrategyEdit(&store, door, 50, Number(0, 1), &why) == EditAnswer::BadCall && untouched(), "and so is no name");
	std::string const tooLong(host::kFieldEditNameMost + 1, 'x');
	Check(StrategyEdit(&store, door, 50, Number(tooLong.c_str(), 1), &why) == EditAnswer::BadCall && untouched(), "a name with no end within 63 bytes is a bad call");

	// What it names it for
	Check(StrategyEdit(&store, door, 51, Number("TimeToOpen", 1), &why) == EditAnswer::KindChanged && untouched(),
	      "an edit made for another kind (a lift, 51) than the strategy now is refused");

	Chunk *const odd = Fake(4);
	Bytes unknown(28, 0);
	PutInt(unknown, 8, 9999);
	store.held[odd] = unknown;
	Check(StrategyEdit(&store, odd, 9999, Number("TimeToOpen", 1), &why) == EditAnswer::UnknownKind && store.held[odd] == unknown && store.keeps == 0,
	      "a kind the editor has no table for, 9999, is refused");

	// What the store holds
	Chunk *const nothing = Fake(5);
	Check(StrategyEdit(&store, nothing, 50, Number("TimeToOpen", 1), &why) == EditAnswer::NoPayload && untouched(), "a chunk the store has no bytes for is refused");

	Chunk *const shortDoor = Fake(6);
	Bytes shorter = before;
	shorter.pop_back();
	store.held[shortDoor] = shorter;
	Check(StrategyEdit(&store, shortDoor, 50, Number("TimeToOpen", 1), &why) == EditAnswer::PayloadRefused && store.held[shortDoor] == shorter && store.keeps == 0,
	      "bytes one short of a door's size are refused for their size: the game would refuse them");
	Chunk *const tiny = Fake(7);
	store.held[tiny] = Bytes(12, 0);
	Check(StrategyEdit(&store, tiny, 50, Number("TimeToOpen", 1), &why) == EditAnswer::PayloadRefused && store.keeps == 0, "and bytes too few to hold a kind at all");
	Chunk *const empty = Fake(8);
	store.held[empty] = Bytes();
	Check(StrategyEdit(&store, empty, 50, Number("TimeToOpen", 1), &why) == EditAnswer::PayloadRefused && store.keeps == 0, "and none");

	// Nothing handed over
	bool answered = false;
	EditAnswer answer = EditAnswer::Applied;
	answered = Survives([&] { answer = StrategyEdit(0, door, 50, Number("TimeToOpen", 1), &why); });
	Check(answered && answer == EditAnswer::BadCall, "no store is a bad call");
	answered = Survives([&] { answer = StrategyEdit(&store, 0, 50, Number("TimeToOpen", 1), &why); });
	Check(answered && answer == EditAnswer::BadCall && untouched(), "no chunk is a bad call");
	answered = Survives([&] { answer = StrategyEdit(&store, door, 50, Number("TimeToOpen", 1), 0); });
	Check(answered && answer == EditAnswer::Applied && store.keeps == 1, "and no place for the reason is fine");
	store.held[door] = before;
	store.keeps = 0;

	// The store may say no
	store.willKeep = false;
	Check(StrategyEdit(&store, door, 50, Number("TimeToOpen", 33), &why) == EditAnswer::NotStored && store.keeps == 1 && store.held[door] == before,
	      "a store that won't keep the bytes gives NotStored, and holds what it held");

	// The words
	bool allHaveWords = true;

	for (int i = 0; i <= (int)EditAnswer::NoMemory; i++)
	{
		char const *const words = host::EditAnswerWords((EditAnswer)i);
		allHaveWords = allHaveWords && words && words[0] != 0;
	}

	Check(allHaveWords, "every answer has plain words");
}

static void ChunkEdits()
{
	printf("Editing a chunk:\n");

	ChunkKind const *const light = FindChunkKind("PLOBJLIT");
	ChunkKind const *const particle = FindChunkKind("PARGENDA");
	ChunkKind const *const sound = FindChunkKind("SOUNDOB2");
	Check(light && particle && sound, "the kinds this part edits are in the table");

	if (!light || !particle || !sound)
	{
		return;
	}

	FakeStore store;
	Chunk *const lamp = Fake(1);
	Bytes const lampBefore = ValidChunk(*light);
	store.held[lamp] = lampBefore;
	Field const *const fadeUp = FindChunkField(*light, "fade_up_time");
	char const *why = 0;

	Check(fadeUp && ChunkEdit(&store, lamp, *light, Number("fade_up_time", 1234), &why) == EditAnswer::Applied && store.keeps == 1 && GetIntAt(store.held[lamp], fadeUp->offset) == 1234,
	      "a light's fade-up time is set to 1234, and the store keeps the bytes");
	Bytes expected = lampBefore;
	PutInt(expected, fadeUp ? fadeUp->offset : 0, 1234);
	Check(store.held[lamp] == expected, "exactly that field's four bytes changed");

	Check(ChunkEdit(&store, lamp, *light, Number("brightness", 65536), &why) == EditAnswer::Applied, "a field the old editor showed no label for can still be edited by its name");

	why = 0;
	Check(ChunkEdit(&store, lamp, *light, Number("fade_up_time", -5), &why) == EditAnswer::ValueRefused && why && fadeUp && GetIntAt(store.held[lamp], fadeUp->offset) == 1234,
	      "a negative time is refused, and the 1234 is still there");

	// A three-number field and an object field of a particle generator
	Chunk *const smoke = Fake(2);
	store.held[smoke] = ValidChunk(*particle);
	Field const *const location = FindChunkField(*particle, "location");
	Field const *const parentField = FindChunkField(*particle, "parent_id");
	Check(location && ChunkEdit(&store, smoke, *particle, AxisOf("location", 2, -300), &why) == EditAnswer::Applied && GetIntAt(store.held[smoke], location->offset + 8) == -300 &&
	      GetIntAt(store.held[smoke], location->offset) == 0, "one number of a chunk's three-number field is set (z of the location, -300), and x is as it was");
	Check(parentField && ChunkEdit(&store, smoke, *particle, ObjectOf("parent_id", 0xAABBCCDDu, 0x11223344u), &why) == EditAnswer::Applied &&
	      GetIntAt(store.held[smoke], parentField->offset) == (int32_t)0xAABBCCDDu && GetIntAt(store.held[smoke], parentField->offset + 4) == 0x11223344,
	      "an object field of a chunk is set to its two halves");

	// Refusals
	int const keepsBefore = store.keeps;
	Bytes const smokeBefore = store.held[smoke];
	Check(ChunkEdit(&store, smoke, *particle, Number("fade_up_time", 1), &why) == EditAnswer::NoSuchField, "another kind's field is refused");
	Check(ChunkEdit(&store, smoke, *particle, ObjectOf("type", 1, 2), &why) == EditAnswer::ValueRefused, "an object put into a field that isn't an object field is refused");
	Check(ChunkEdit(&store, Fake(9), *particle, Number("type", 1), &why) == EditAnswer::NoPayload, "a chunk the store has no bytes for is refused");
	Chunk *const cut = Fake(3);
	Bytes shorter = ValidChunk(*particle);
	shorter.pop_back();
	store.held[cut] = shorter;
	Check(ChunkEdit(&store, cut, *particle, Number("type", 1), &why) == EditAnswer::PayloadRefused, "bytes shorter than the game reads are refused");
	Check(ChunkEdit(&store, smoke, *particle, Number(0, 1), &why) == EditAnswer::BadCall && ChunkEdit(0, smoke, *particle, Number("type", 1), &why) == EditAnswer::BadCall &&
	      ChunkEdit(&store, 0, *particle, Number("type", 1), &why) == EditAnswer::BadCall, "no name, no store and no chunk are bad calls");
	Check(store.keeps == keepsBefore && store.held[smoke] == smokeBefore, "and none of those reached the store or changed a byte");

	// The store may say no
	store.willKeep = false;
	Bytes const heldBefore = store.held[lamp];
	Check(ChunkEdit(&store, lamp, *light, Number("fade_up_time", 5), &why) == EditAnswer::NotStored && store.held[lamp] == heldBefore,
	      "a store that won't keep a chunk's bytes gives NotStored, and holds what it held");
	store.willKeep = true;

	// A chunk with a name after its numbers: the names aren't touched
	Chunk *const hiss = Fake(4);
	Bytes soundBytes = ValidChunk(*sound);
	soundBytes.back() = 0;
	store.held[hiss] = soundBytes;
	Check(ChunkEdit(&store, hiss, *sound, Number("max_volume", 100), &why) == EditAnswer::Applied && store.held[hiss].size() == soundBytes.size(),
	      "a sound's volume is set, and the bytes are as many as before (its name after the numbers is carried as it was)");
}

int main()
{
	StrategyEdits();
	Refusals();
	ChunkEdits();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
