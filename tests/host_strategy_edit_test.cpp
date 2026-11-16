// The test for src/host/strategy_edit: an edit of one field of an object's strategy, found by its ID in a level made in memory, and the
// strategy's bytes read back from the store. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It reads no file. It builds levels with the game's own chunk library, as host_strategy_lookup_test.cpp does (a level with its environment, and
// objects made by the game's real constructors from bytes, some holding a strategy), and a small store of its own that holds each strategy
// chunk's bytes as the lookup gave them. The field edits themselves are tested in host_field_edit_test.cpp, without a chunk library: this
// test is about the join: the object's ID finds the right chunk, the edit goes to the store, and the strategy's bytes are read back as the
// STORE holds them, which after an edit is not what the game's own class for the chunk says. The program gives back a non-zero result if any
// check failed. Each check's words say what it proves.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_type.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "module.h"
#include "strategy_def.h"
#include "bh_lift.h"
#include "environment_chunk.hpp"
#include "avp_chunk.hpp"
#include "strategy_chunk.hpp"

#include "field_model.h"
#include "crash_guard.h"
#include "field_edit.h"
#include "strategy_edit.h"
#include "strategy_lookup.h"

using host::ApplyStrategyEdit;
using host::EditAnswer;
using host::EditWhat;
using host::FieldEdit;
using host::FindObjectStrategy;
using host::LookUpStrategyBytes;
using host::ObjectStrategy;
using host::StrategyFound;

// ---- Stand-ins for the parts of the game the chunk code calls (the same as avpx/tests/rebeccas_test.cpp)

HANDLE avp_CreateFile(const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
{
	return INVALID_HANDLE_VALUE;
}

DWORD avp_GetFileAttributes(const char *)
{
	return INVALID_FILE_ATTRIBUTES;
}

void avp_MessageBox(const char *message, int)
{
	puts(message);
}

void avp_exit(int code)
{
	exit(code);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// ---- The checks

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

// ---- Making chunks as the level file holds them: an 8-byte name, a 4-byte size that counts these 12 bytes too, then what it holds

typedef std::vector<char> Bytes;

static void PutNumber(Bytes &out, int value)
{
	char four[4];
	memcpy(four, &value, sizeof(four));
	out.insert(out.end(), four, four + 4);
}

static Bytes MakeChunk(const char *name, Bytes const &inside)
{
	Bytes out(name, name + 8);
	PutNumber(out, (int)(12 + inside.size()));
	out.insert(out.end(), inside.begin(), inside.end());
	return out;
}

static Bytes Join(Bytes first, Bytes const &second)
{
	first.insert(first.end(), second.begin(), second.end());
	return first;
}

// A name, its zero, and zeros after it up to a multiple of 4, as the game writes names
static Bytes NameBytes(const char *name)
{
	Bytes out(name, name + strlen(name) + 1);

	while (out.size() % 4 != 0)
	{
		out.push_back(0);
	}

	return out;
}

// An OBJHEAD1 chunk: 60 bytes of numbers (all 0), then the object's name (chunk_object.cpp:392-446)
static Bytes ObjectHeader(const char *name)
{
	Bytes inside(60, 0);
	Bytes const words = NameBytes(name);
	inside.insert(inside.end(), words.begin(), words.end());
	return MakeChunk("OBJHEAD1", inside);
}

static Bytes StrategyChunk(std::vector<uint8_t> const &payload)
{
	return MakeChunk("AVPSTRAT", Bytes(payload.begin(), payload.end()));
}

// A level made in memory. The game's chunk library keeps the level (File_Chunk) and its chunks; each chunk lifts itself out of its parent
// when it's deleted, and the objects and the environment are deleted before the level is.
struct TestLevel
{
	File_Chunk file;
	std::unique_ptr<Environment_Data_Chunk> environment;
	std::vector<std::unique_ptr<Object_Chunk> > objects;

	TestLevel(const char *levelName)
	{
		Parent_File = &file;

		Bytes const inside = MakeChunk("RIFFNAME", NameBytes(levelName));
		environment.reset(new Environment_Data_Chunk(&file, inside.data(), inside.size()));
	}

	~TestLevel()
	{
		objects.clear();
		environment.reset();
		Parent_File = 0;
	}

	// An object made from the bytes of its children, and given its ID, as the game does after a level is read (Object_Chunk::
	// post_input_processing, chunk_object.cpp:232)
	Object_Chunk *Add(Bytes const &children)
	{
		objects.push_back(std::unique_ptr<Object_Chunk>(new Object_Chunk(&file, children.data(), children.size())));
		objects.back()->CalculateID();
		return objects.back().get();
	}

	// An object with a name and a strategy
	Object_Chunk *AddWithStrategy(const char *name, std::vector<uint8_t> const &payload)
	{
		return Add(Join(ObjectHeader(name), MakeChunk("OBJPRJDT", StrategyChunk(payload))));
	}

	// An object with a name and nothing else
	Object_Chunk *AddPlain(const char *name)
	{
		return Add(ObjectHeader(name));
	}
};

static void IdOf(Object_Chunk const *object, uint8_t *id)
{
	memcpy(id, &object->object_data.ID, 8);
}

static bool IdIsAllZero(uint8_t const *id)
{
	for (int i = 0; i < 8; i++)
	{
		if (id[i] != 0)
		{
			return false;
		}
	}

	return true;
}

// ---- Strategies' bytes, made by the field model

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

// A new strategy as the old editor made one, with the two numbers before it (the index and the spare, payload bytes 0 to 7) set to
// numbers that aren't 0, so a lookup that dropped them would show
static std::vector<uint8_t> NewStrategy(int32_t kind)
{
	std::vector<uint8_t> bytes(512, 0xEE);
	size_t length = 0;

	if (!model::MakeNewStrategy(kind, bytes.data(), bytes.size(), &length))
	{
		bytes.clear();
		return bytes;
	}

	bytes.resize(length);
	PutInt(bytes, 0, 0x11223344);
	PutInt(bytes, 4, 0x55667788);
	return bytes;
}

static model::Field const *FieldNamed(model::StrategyKind const &kind, const char *name)
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

// A door whose time to open is that many tenths of a second, so each door in a test can be told from another
static std::vector<uint8_t> DoorOpeningIn(int tenths)
{
	std::vector<uint8_t> bytes = NewStrategy(50);
	model::StrategyKind const *const door = model::FindStrategyKind(50);
	model::Field const *const open = door ? FieldNamed(*door, "TimeToOpen") : 0;
	char const *why = 0;

	if (!door || !open || bytes.empty() || !model::SetField(*door, *open, bytes.data(), bytes.size(), tenths, &why))
	{
		bytes.clear();
	}

	return bytes;
}

// where a door's time to open is in its payload (field_model's table: Anchor::Start, offset 24)
static size_t const TimeToOpenAt = 24;

// ---- The store: what the editor holds for each strategy chunk (OpenLevelFile's PayloadOf and SetPayload, as a small one of the test's own)

class TestStore : public host::PayloadStore
{
public:
	std::map<Chunk *, std::vector<unsigned char> > held;
	int keeps = 0;

	bool PayloadOf(Chunk *chunk, std::vector<unsigned char> *payload) const override
	{
		auto const found = held.find(chunk);

		if (found == held.end() || !payload)
		{
			return false;
		}

		*payload = found->second;
		return true;
	}

	bool SetPayload(Chunk *chunk, std::vector<unsigned char> const &payload) override
	{
		keeps++;
		held[chunk] = payload;
		return true;
	}

	// The store holds what the lookup gives for this object now: the bytes the level was read from
	bool Take(File_Chunk *level, uint8_t const *id)
	{
		ObjectStrategy found;

		if (FindObjectStrategy(level, id, &found) != StrategyFound::Found)
		{
			return false;
		}

		held[found.chunk] = found.payload;
		return true;
	}
};

static FieldEdit TimeToOpenSetTo(double tenths)
{
	FieldEdit edit;
	edit.field = "TimeToOpen";
	edit.what = EditWhat::Number;
	edit.number = tenths;
	return edit;
}

// ---- The tests

static void TestEditByID()
{
	puts("1. an object's strategy is edited by the object's ID, and read back from the store");

	TestLevel level("lvl");
	std::vector<uint8_t> const door = DoorOpeningIn(77);
	std::vector<uint8_t> const switchDoor = NewStrategy(59);
	Object_Chunk *const first = level.AddWithStrategy("door one", door);
	Object_Chunk *const second = level.AddWithStrategy("switch door two", switchDoor);
	uint8_t firstId[8], secondId[8];
	IdOf(first, firstId);
	IdOf(second, secondId);

	TestStore store;
	Check(store.Take(&level.file, firstId) && store.Take(&level.file, secondId), "the store holds both strategies as the level gave them");

	StrategyFound lookup = StrategyFound::BadCall;
	char const *why = 0;
	EditAnswer const answer = ApplyStrategyEdit(&level.file, firstId, 50, TimeToOpenSetTo(33), &store, &why, &lookup);

	Check(answer == EditAnswer::Applied && lookup == StrategyFound::Found && store.keeps == 1, "the first door's time to open is set to 33: applied, found, and the store kept the bytes once");

	ObjectStrategy now;
	Check(LookUpStrategyBytes(&level.file, store, firstId, &now) == StrategyFound::Found && now.payload.size() == door.size() && now.payload[TimeToOpenAt] == 33,
	      "the strategy read back for the panel has the new time, 33");

	std::vector<uint8_t> expected = door;
	expected[TimeToOpenAt] = 33;
	Check(now.payload == expected, "and every other byte is as it was: the index, the spare number, the kind, the flags");

	ObjectStrategy asTheClassHasIt;
	Check(FindObjectStrategy(&level.file, firstId, &asTheClassHasIt) == StrategyFound::Found && asTheClassHasIt.payload == door && asTheClassHasIt.payload[TimeToOpenAt] == 77,
	      "while the game's own class for the chunk still gives 77: an edit never changes the class, so a record built from FindObjectStrategy would show the old time");

	ObjectStrategy other;
	Check(LookUpStrategyBytes(&level.file, store, secondId, &other) == StrategyFound::Found && other.payload == switchDoor, "the other object's strategy is untouched");
	Check(now.chunk && now.chunk == asTheClassHasIt.chunk, "both lookups name the same AVPSTRAT chunk");
}

static void TestWhatItRefuses()
{
	puts("2. what an edit by ID refuses, and leaves alone");

	TestLevel level("lvl");
	Object_Chunk *const door = level.AddWithStrategy("a door", DoorOpeningIn(10));
	Object_Chunk *const plain = level.AddPlain("a plain wall");
	uint8_t doorId[8], plainId[8];
	IdOf(door, doorId);
	IdOf(plain, plainId);

	TestStore store;
	store.Take(&level.file, doorId);
	auto const before = store.held;
	StrategyFound lookup = StrategyFound::Found;
	char const *why = 0;

	Check(ApplyStrategyEdit(&level.file, plainId, 50, TimeToOpenSetTo(5), &store, &why, &lookup) == EditAnswer::NoPayload && lookup == StrategyFound::NoStrategy && store.keeps == 0,
	      "an object with no strategy gives NoPayload, with the lookup's own answer, NoStrategy, and nothing kept");

	uint8_t const nobody[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Check(ApplyStrategyEdit(&level.file, nobody, 50, TimeToOpenSetTo(5), &store, &why, &lookup) == EditAnswer::NoPayload && lookup == StrategyFound::NoSuchObject && store.keeps == 0,
	      "an ID no object has gives NoPayload, and the lookup says NoSuchObject");

	Check(ApplyStrategyEdit(0, doorId, 50, TimeToOpenSetTo(5), &store, &why, &lookup) == EditAnswer::NoPayload && lookup == StrategyFound::NoLevel && store.keeps == 0,
	      "no level gives NoPayload, and the lookup says NoLevel");

	ObjectStrategy plainLook;
	Check(LookUpStrategyBytes(&level.file, store, plainId, &plainLook) == StrategyFound::NoStrategy && plainLook.chunk == 0 && plainLook.payload.empty(),
	      "the strategy's bytes of an object with no strategy: NoStrategy, with nothing in the answer");

	Check(ApplyStrategyEdit(&level.file, doorId, 51, TimeToOpenSetTo(5), &store, &why, &lookup) == EditAnswer::KindChanged && store.keeps == 0 && store.held == before,
	      "an edit made for another kind than the strategy is refused, and nothing is kept");
	Check(ApplyStrategyEdit(&level.file, doorId, 50, TimeToOpenSetTo(300), &store, &why, &lookup) == EditAnswer::ValueRefused && why && store.keeps == 0 && store.held == before,
	      "a value the field can't hold is refused, with the model's words, and nothing is kept");

	// A chunk the store doesn't hold gives NoPayload from the store, not a guess from the class
	TestStore empty;
	Check(ApplyStrategyEdit(&level.file, doorId, 50, TimeToOpenSetTo(5), &empty, &why, &lookup) == EditAnswer::NoPayload && lookup == StrategyFound::Found && empty.keeps == 0,
	      "a store that holds nothing for the chunk gives NoPayload: the edit is never made from the game's class");
	ObjectStrategy none;
	Check(LookUpStrategyBytes(&level.file, empty, doorId, &none) == StrategyFound::NoStrategyBytes && none.chunk == 0 && none.payload.empty(),
	      "and the strategy's bytes aren't given either: NoStrategyBytes, with nothing in the answer");

	// Nothing handed over
	bool ran = Survives([&] { Check(ApplyStrategyEdit(&level.file, 0, 50, TimeToOpenSetTo(5), &store, &why, &lookup) == EditAnswer::BadCall, "no ID is a bad call"); });
	Check(ran, "(and it doesn't crash)");
	ran = Survives([&] { Check(ApplyStrategyEdit(&level.file, doorId, 50, TimeToOpenSetTo(5), 0, &why, &lookup) == EditAnswer::BadCall, "no store is a bad call"); });
	Check(ran, "(and it doesn't crash)");
	ran = Survives([&] { Check(ApplyStrategyEdit(&level.file, doorId, 50, TimeToOpenSetTo(5), &store, 0, 0) == EditAnswer::Applied, "no place for the reason or the lookup's answer is fine"); });
	Check(ran, "(and it doesn't crash)");
}

static void TestTwoObjectsOneName()
{
	puts("3. two objects with one name: the first is edited, as it is the one the lookup finds");

	TestLevel level("lvl");
	Object_Chunk *const first = level.AddWithStrategy("same name", DoorOpeningIn(11));
	Object_Chunk *const second = level.AddWithStrategy("same name", DoorOpeningIn(22));
	uint8_t id[8];
	IdOf(first, id);

	uint8_t secondId[8];
	IdOf(second, secondId);
	TestStore store;
	store.Take(&level.file, id);
	Check(memcmp(id, secondId, 8) == 0, "the two objects have one ID");

	char const *why = 0;
	Check(ApplyStrategyEdit(&level.file, id, 50, TimeToOpenSetTo(44), &store, &why, 0) == EditAnswer::Applied, "an edit by that ID is applied");

	ObjectStrategy now;
	Check(LookUpStrategyBytes(&level.file, store, id, &now) == StrategyFound::Found && now.payload[TimeToOpenAt] == 44, "to the first object, the one the lookup finds");
	ObjectStrategy secondAsRead;
	Check(FindObjectStrategy(&level.file, secondId, &secondAsRead) == StrategyFound::Found && secondAsRead.chunk == now.chunk, "(the lookup by the same ID gives the same chunk, so the second object is never the one edited)");
}

int main()
{
	TestEditByID();
	TestWhatItRefuses();
	TestTwoObjectsOneName();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
