// The test for src/host/strategy_lookup: from an object's 8-byte ID to its strategy, in a level made in memory. Written 2026-11-14 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It reads no file. It builds levels with the game's own chunk library, the way avpx/tests/rebeccas_test.cpp does: a level (File_Chunk)
// holding its environment (REBENVDT, with the level's name: an object's ID is made from that name and the object's own, chunk_object.cpp:
// 368-382) and objects (RBOBJECT), each object made by the game's real constructor from bytes: a header (OBJHEAD1) and, for some, a
// project-data chunk (OBJPRJDT) holding a strategy (AVPSTRAT). Then it asks for strategies by ID. The program gives back a non-zero
// result if any check failed. Each check's words say what it proves.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "strategy_lookup.h"

using host::FindObjectStrategy;
using host::FindSelectedStrategy;
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

// ---- The tests

static void TestFindsStrategies()
{
	puts("1. an object's strategy is found by its ID, and is exactly the bytes the level held");

	TestLevel level("lvl");
	std::vector<uint8_t> const door = DoorOpeningIn(77);
	std::vector<uint8_t> const switchDoor = NewStrategy(59);

	Check(!door.empty() && !switchDoor.empty(), "a door and a switch door are made by the field model");

	Object_Chunk *const first = level.AddWithStrategy("door one", door);
	Object_Chunk *const second = level.AddWithStrategy("switch door two", switchDoor);
	Object_Chunk *const scenery = level.AddPlain("a plain wall");

	uint8_t firstId[8], secondId[8], sceneryId[8];
	IdOf(first, firstId);
	IdOf(second, secondId);
	IdOf(scenery, sceneryId);

	Check(!IdIsAllZero(firstId) && !IdIsAllZero(secondId) && memcmp(firstId, secondId, 8) != 0,
	      "the game gives each object an ID of its own, which isn't 0 (so the checks below mean something)");

	ObjectStrategy found;
	StrategyFound const answer = FindObjectStrategy(&level.file, firstId, &found);

	Check(answer == StrategyFound::Found, "the first object's strategy is found");
	Check(found.payload == door, "its payload is exactly the bytes the level held, from the index and spare number on");
	Check(found.payload.size() == 28 && GetInt(found.payload, 8) == 50, "a door's payload is 28 bytes, and its kind, at +8, is 50");
	Check(found.payload.size() > TimeToOpenAt && found.payload[TimeToOpenAt] == 77, "the door's time to open, 77, is in it");
	Check(model::StrategySizeIsRight(found.payload.data(), found.payload.size()), "the field model accepts its size");

	Chunk *const project = first->lookup_single_child("OBJPRJDT");
	Chunk *const strategyChunk = project ? ((Object_Project_Data_Chunk *)project)->lookup_single_child("AVPSTRAT") : 0;

	Check(strategyChunk && found.chunk == strategyChunk, "the chunk given back is the object's own AVPSTRAT chunk");

	StrategyFound const secondAnswer = FindObjectStrategy(&level.file, secondId, &found);

	Check(secondAnswer == StrategyFound::Found && found.payload == switchDoor && GetInt(found.payload, 8) == 59,
	      "the second object's strategy is its own: a switch door, exactly its bytes");

	StrategyFound const sceneryAnswer = FindObjectStrategy(&level.file, sceneryId, &found);

	Check(sceneryAnswer == StrategyFound::NoStrategy && found.chunk == 0 && found.payload.empty(),
	      "an object with no strategy gives NoStrategy, and nothing in the answer");
}

static void TestKindTheEditorDoesntKnow()
{
	puts("2. a strategy of a kind the editor doesn't know is found too, with every byte");

	TestLevel level("lvl");
	std::vector<uint8_t> odd(36, 0);

	for (size_t i = 0; i < odd.size(); i++)
	{
		odd[i] = (uint8_t)(i + 1);
	}

	PutInt(odd, 8, 9999);   // the kind: one the game reads as a plain block of any size

	Object_Chunk *const object = level.AddWithStrategy("odd one", odd);
	uint8_t id[8];
	IdOf(object, id);

	ObjectStrategy found;

	Check(FindObjectStrategy(&level.file, id, &found) == StrategyFound::Found && found.payload == odd,
	      "an unknown kind's 36 bytes come back exactly as they went in");
}

static void TestNothingToFind()
{
	puts("3. what isn't there: no object, no name, no level, no strategy, an empty strategy");

	TestLevel level("lvl");
	Object_Chunk *const door = level.AddWithStrategy("a door", DoorOpeningIn(10));
	Object_Chunk *const noData = level.AddPlain("no project data");
	Object_Chunk *const dataOnly = level.Add(Join(ObjectHeader("data only"), MakeChunk("OBJPRJDT", Bytes())));

	// an object whose strategy chunk was made by the editor and not filled in: the chunk is there and holds no strategy at all
	Object_Chunk *const empty = level.Add(ObjectHeader("empty strategy"));
	Object_Project_Data_Chunk *const emptyData = new Object_Project_Data_Chunk(empty);
	AVP_Strategy_Chunk *const emptyStrategy = new AVP_Strategy_Chunk(emptyData);

	Check(emptyStrategy->Strategy == 0, "the editor-made strategy chunk holds no strategy");

	uint8_t doorId[8], noDataId[8], dataOnlyId[8], emptyId[8];
	IdOf(door, doorId);
	IdOf(noData, noDataId);
	IdOf(dataOnly, dataOnlyId);
	IdOf(empty, emptyId);

	ObjectStrategy found;
	uint8_t const nobody[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint8_t const zeros[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	found.chunk = (Chunk *)&level;                  // something left over, which every refusal must clear
	found.payload.assign(5, 0xAB);
	Check(FindObjectStrategy(&level.file, nobody, &found) == StrategyFound::NoSuchObject && found.chunk == 0 && found.payload.empty(),
	      "an ID no object has gives NoSuchObject, and clears what was in the answer");

	found.payload.assign(5, 0xAB);
	Check(FindObjectStrategy(&level.file, zeros, &found) == StrategyFound::NoName && found.payload.empty(), "an ID of all 0 gives NoName");

	found.payload.assign(5, 0xAB);
	Check(FindObjectStrategy(0, doorId, &found) == StrategyFound::NoLevel && found.payload.empty(), "no level gives NoLevel");

	Check(FindObjectStrategy(&level.file, doorId, 0) == StrategyFound::BadCall, "no place for the answer gives BadCall");

	found.payload.assign(5, 0xAB);
	Check(FindObjectStrategy(&level.file, 0, &found) == StrategyFound::BadCall && found.payload.empty(), "no ID gives BadCall, and clears the answer");

	Check(FindObjectStrategy(&level.file, noDataId, &found) == StrategyFound::NoStrategy, "an object with no OBJPRJDT gives NoStrategy");
	Check(FindObjectStrategy(&level.file, dataOnlyId, &found) == StrategyFound::NoStrategy, "an OBJPRJDT with no AVPSTRAT in it gives NoStrategy");
	Check(FindObjectStrategy(&level.file, emptyId, &found) == StrategyFound::NoStrategyBytes && found.chunk == 0,
	      "a strategy chunk holding no strategy gives NoStrategyBytes, and the lookup doesn't fall over");
	Check(FindObjectStrategy(&level.file, doorId, &found) == StrategyFound::Found, "and the door is still found after all of those");
}

static void TestTwoObjectsOneName()
{
	puts("4. two objects with one name have one ID: the first is the one");

	TestLevel level("lvl");
	Object_Chunk *const first = level.AddWithStrategy("same name", DoorOpeningIn(11));
	Object_Chunk *const second = level.AddWithStrategy("same name", DoorOpeningIn(22));
	uint8_t firstId[8], secondId[8];
	IdOf(first, firstId);
	IdOf(second, secondId);

	Check(memcmp(firstId, secondId, 8) == 0 && !IdIsAllZero(firstId), "the two objects have the same, non-zero ID");

	ObjectStrategy found;

	Check(FindObjectStrategy(&level.file, firstId, &found) == StrategyFound::Found && found.payload.size() > TimeToOpenAt &&
	      found.payload[TimeToOpenAt] == 11, "the first in the level's own order is the one given (its time to open is 11, not 22)");
}

static void TestTooBig()
{
	puts("5. a strategy larger than any real one is refused");

	TestLevel level("lvl");
	std::vector<uint8_t> big(host::kStrategyLookupMostBytes + 8, 0);
	PutInt(big, 8, 9999);

	Object_Chunk *const object = level.AddWithStrategy("a big one", big);
	uint8_t id[8];
	IdOf(object, id);

	ObjectStrategy found;
	Check(FindObjectStrategy(&level.file, id, &found) == StrategyFound::TooBig && found.chunk == 0 && found.payload.empty(),
	      "a payload one byte over the most is TooBig, with nothing in the answer");

	std::vector<uint8_t> most(host::kStrategyLookupMostBytes, 0);
	PutInt(most, 8, 9999);

	Object_Chunk *const exact = level.AddWithStrategy("the most", most);
	IdOf(exact, id);

	Check(FindObjectStrategy(&level.file, id, &found) == StrategyFound::Found && found.payload.size() == host::kStrategyLookupMostBytes,
	      "a payload of exactly the most is given");
}

static void TestSelection()
{
	puts("6. what's selected: only an object is looked for");

	TestLevel level("lvl");
	Object_Chunk *const door = level.AddWithStrategy("a door", DoorOpeningIn(33));
	host::Selected selected = {};

	ObjectStrategy found;
	found.payload.assign(3, 1);

	selected.kind = host::SelectedKind::Nothing;
	Check(FindSelectedStrategy(&level.file, selected, &found) == StrategyFound::NotAnObject && found.payload.empty(), "nothing selected gives NotAnObject");

	selected.kind = host::SelectedKind::Module;
	selected.moduleIndex = 4;
	Check(FindSelectedStrategy(&level.file, selected, &found) == StrategyFound::NotAnObject, "a module selected gives NotAnObject");

	selected.kind = host::SelectedKind::Object;
	IdOf(door, selected.objectId);
	Check(FindSelectedStrategy(&level.file, selected, &found) == StrategyFound::Found && found.payload.size() > TimeToOpenAt &&
	      found.payload[TimeToOpenAt] == 33, "an object selected gives its strategy");

	Check(FindSelectedStrategy(&level.file, selected, 0) == StrategyFound::BadCall, "no place for the answer gives BadCall");

	// every answer has words
	bool allHaveWords = true;

	for (int i = 0; i <= (int)StrategyFound::NoMemory; i++)
	{
		char const *const words = host::StrategyFoundWords((StrategyFound)i);
		allHaveWords = allHaveWords && words && words[0] != 0;
	}

	Check(allHaveWords, "every answer has plain words for the status line");
}

// What the take-out rows (tests/takeouts/host_strategy_lookup_test.json) showed the checks above didn't reach
static void TestWhatTheTakeOutRowsShowedMissing()
{
	puts("7. an ID that differs by one byte, the least strategy and one byte over the most, what a refusal clears, the words of an unknown answer");

	ObjectStrategy found;

	// An ID that is 0 in every byte but one is a name, not "no name", whichever byte it is
	{
		TestLevel level("lvl");
		bool eachIsAName = true;

		for (int at = 0; at < 8; at++)
		{
			uint8_t id[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			id[at] = 1;
			eachIsAName = eachIsAName && FindObjectStrategy(&level.file, id, &found) == StrategyFound::NoSuchObject;
		}

		Check(eachIsAName, "an ID that is 0 in every byte but one is a name (no such object), not NoName, whichever byte it is");
	}

	// An ID that differs from an object's own in one byte only is not that object's, whichever byte it is
	{
		TestLevel level("lvl");
		Object_Chunk *const object = level.AddWithStrategy("an object", DoorOpeningIn(1));
		uint8_t id[8];
		IdOf(object, id);

		Check(FindObjectStrategy(&level.file, id, &found) == StrategyFound::Found && found.payload.size() > TimeToOpenAt &&
		      found.payload[TimeToOpenAt] == 1, "the object is found by its own ID (so the check below means something)");

		bool eachIsAnother = true;

		for (int at = 0; at < 8; at++)
		{
			uint8_t other[8];
			memcpy(other, id, 8);
			other[at] = (uint8_t)(other[at] + 1);
			eachIsAnother = eachIsAnother && FindObjectStrategy(&level.file, other, &found) == StrategyFound::NoSuchObject;
		}

		Check(eachIsAnother, "an ID that differs from the object's own in one byte only is no object's (no such object), whichever byte it is");
	}

	// The least a strategy can be is 20 bytes (the index, the spare number, then a kind and two more numbers), and a payload one byte over the most is too big
	{
		TestLevel level("lvl");
		std::vector<uint8_t> least(20, 0);
		PutInt(least, 8, 9999);
		std::vector<uint8_t> oneOver(host::kStrategyLookupMostBytes + 1, 0);
		PutInt(oneOver, 8, 9999);

		uint8_t leastId[8], oneOverId[8];
		IdOf(level.AddWithStrategy("the least", least), leastId);
		IdOf(level.AddWithStrategy("one over", oneOver), oneOverId);

		Check(FindObjectStrategy(&level.file, leastId, &found) == StrategyFound::Found && found.payload == least,
		      "a strategy of 20 bytes, the least there can be, is given");
		Check(FindObjectStrategy(&level.file, oneOverId, &found) == StrategyFound::TooBig && found.chunk == 0 && found.payload.empty(),
		      "a payload of the most and one byte over is TooBig");
	}

	// Every refusal clears the answer's chunk as well as its bytes
	{
		TestLevel level("lvl");
		host::Selected selected = {};

		found.chunk = (Chunk *)&level;                  // something left over, which every refusal must clear
		found.payload.assign(5, 0xAB);
		Check(FindObjectStrategy(&level.file, 0, &found) == StrategyFound::BadCall && found.chunk == 0 && found.payload.empty(),
		      "no ID gives BadCall, and clears the chunk and the bytes of the answer");

		found.chunk = (Chunk *)&level;
		found.payload.assign(5, 0xAB);
		selected.kind = host::SelectedKind::Nothing;
		Check(FindSelectedStrategy(&level.file, selected, &found) == StrategyFound::NotAnObject && found.chunk == 0 && found.payload.empty(),
		      "nothing selected gives NotAnObject, and clears the chunk and the bytes of the answer");

		Check(FindSelectedStrategy(&level.file, selected, 0) == StrategyFound::BadCall, "nothing selected, with no place for the answer, gives BadCall");
	}

	// An answer the code doesn't know still has words
	{
		char const *const words = host::StrategyFoundWords((StrategyFound)99);

		Check(words && words[0] != 0, "an answer the code doesn't know still has plain words");
	}
}

int main()
{
	TestFindsStrategies();
	TestKindTheEditorDoesntKnow();
	TestNothingToFind();
	TestTwoObjectsOneName();
	TestTooBig();
	TestSelection();
	TestWhatTheTakeOutRowsShowedMissing();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
