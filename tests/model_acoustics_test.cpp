// The test for src/model's acoustics tables (acoustics_fields.h, chunk_fields_acoustics.cpp): each chunk's layout is the game's reader's,
// every limit refuses the value just outside it and any number that isn't one, the 26 rooms are the game's own list in its order, and
// the rules for a room's echo are the game's. Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// One of the game's headers is read here as it is in the pinned AvPx commit, so a change there fails this test instead of leaving the
// tables behind: reverb_choice.h, which names the game's 26 rooms and says how much echo a room's number comes to.
// It runs none of the editor and reads no file. It builds payloads in memory, some of them hostile. The program gives back a non-zero
// result if any check failed. Each check's words say what it proves.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "acoustics_fields.h"
#include "field_model.h"
#include "name_model.h"

#include "../avpx/3dc/reverb_choice.h"

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

static void PutBits(std::vector<uint8_t> &bytes, size_t at, uint32_t bits)
{
	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

static uint32_t GetBits(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t bits = 0;

	for (int i = 0; i < 4; i++)
	{
		bits |= (uint32_t)bytes.at(at + i) << (8 * i);
	}

	return bits;
}

static int32_t GetInt(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t const bits = GetBits(bytes, at);
	int32_t value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static float GetFloat(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t const bits = GetBits(bytes, at);
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

// What the game reads from one of these chunks, exactly as its reader does (win95/chunk_object.cpp:911-915 for a module,
// win95/environment_chunk.cpp:200-204 for the level): a whole number, a fraction and a whole number, one straight after the other.
struct GameReading
{
	int32_t kind;
	float reverb;
	int32_t spare;
};

static GameReading ReadLikeTheGame(std::vector<uint8_t> const &bytes)
{
	GameReading reading;
	reading.kind = GetInt(bytes, 0);
	reading.reverb = GetFloat(bytes, 4);
	reading.spare = GetInt(bytes, 8);
	return reading;
}

static bool NamesAFileAndLine(char const *where)
{
	char const *const colon = where ? strstr(where, ":") : 0;

	return colon && colon > where && colon[1] >= '0' && colon[1] <= '9';
}

// Whether a value is allowed in a field, and the reason when it isn't ("" when it is).
static char const *Verdict(Field const &field, double value)
{
	char const *why = 0;
	return ValueIsAllowed(field, value, &why) ? "" : (why ? why : "refused with no reason");
}

static bool Allowed(Field const &field, double value)
{
	return ValueIsAllowed(field, value, 0);
}

// Whether a text is plain letters the panel's font has (bytes 32 to 126: src/ui/text_field.cpp, IsFontLetter) and at most most long.
// An empty text is allowed when mayBeEmpty (a label of "" means the panel skips the field).
static bool WordsFit(char const *text, size_t most, bool mayBeEmpty)
{
	if (!text)
	{
		return false;
	}

	size_t length = 0;

	while (length <= most && text[length] != 0)
	{
		unsigned char const letter = (unsigned char)text[length];

		if (letter < 32 || letter > 126)
		{
			return false;
		}

		length++;
	}

	return length <= most && (length > 0 || mayBeEmpty);
}

// How many bytes at the start of a kind's payload hold a name instead of numbers (the name tables, name_model.h, keep those, so no field
// covers them). A light set header starts with the set's name, 8 bytes; none of the others starts with a name.
static unsigned LeadingNameBytes(ChunkKind const &kind)
{
	return strncmp(kind.id, "LTSETHDR", 8) == 0 ? 8 : 0;
}

// Whether the kind's fields cover every byte of its numbers exactly once
static bool FieldsCoverTheNumbers(ChunkKind const &kind)
{
	unsigned const nameBytes = LeadingNameBytes(kind);
	std::vector<int> covered(kind.fixedBytes, 0);

	for (size_t f = 0; f < kind.fieldCount; f++)
	{
		unsigned const first = kind.fields[f].offset;
		unsigned const last = first + StorageBytes(kind.fields[f].storage);

		for (unsigned b = first; b < last && b < kind.fixedBytes; b++)
		{
			covered[b]++;
		}
	}

	for (unsigned b = 0; b < kind.fixedBytes; b++)
	{
		if (covered[b] != (b < nameBytes ? 0 : 1))
		{
			return false;
		}
	}

	return true;
}

static bool FieldIs(ChunkKind const &kind, char const *name, unsigned offset, Storage storage)
{
	Field const *const field = FindChunkField(kind, name);

	return field && field->anchor == Anchor::Start && field->offset == offset && field->storage == storage;
}

// The words for one named value of a choice field, or "" when it has none
static char const *LabelOf(char const *chunk, char const *field, int32_t value)
{
	for (size_t i = 0; i < AcousticsChoiceCount; i++)
	{
		if (strncmp(AcousticsChoices[i].chunk, chunk, 8) == 0 && strcmp(AcousticsChoices[i].field, field) == 0 &&
		    AcousticsChoices[i].value == value)
		{
			return AcousticsChoices[i].label;
		}
	}

	return "";
}

static bool SameLettersIgnoringCase(char const *a, char const *b)
{
	while (*a && *b)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
		{
			return false;
		}

		a++;
		b++;
	}

	return *a == 0 && *b == 0;
}

static SizeCheck SizeOf(ChunkKind const &kind, size_t length)
{
	std::vector<uint8_t> bytes(length + 1, 0);   // never empty, so data() is never null
	return CheckChunkSize(kind, bytes.data(), length);
}

static bool EchoIs(RoomEcho const &echo, int32_t kind, float amount)
{
	return echo.kind == kind && echo.amount == amount;
}

static void TablesHoldTogether()
{
	printf("The tables hold together:\n");

	std::set<std::string> ids;
	bool unique = true, readers = true, parents = true, sizes = true, covered = true, fieldNames = true, starts = true, inside = true;
	bool masks = true, limits = true, whys = true, choices = true, creations = true, words = true;

	for (size_t k = 0; k < AcousticsKindCount; k++)
	{
		ChunkKind const &kind = AcousticsKinds[k];

		unique = ids.insert(kind.id).second && unique;
		readers = strlen(kind.id) == 8 && NamesAFileAndLine(kind.reader) && readers;
		parents = kind.parent[0] == 0 && parents;
		// None of these chunks has a list, names after its numbers, or a block that holds its own size: the game reads exactly the fixed part
		sizes = kind.listCountAt < 0 && kind.listItemBytes == 0 && kind.namesRequired + kind.namesOptional == 0 && kind.sizeFieldAt < 0 &&
		        kind.fixedBytes == kind.leastBytes && sizes;
		covered = FieldsCoverTheNumbers(kind) && covered;

		std::set<std::string> names;

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			Field const &field = kind.fields[f];
			double lowest = 0, highest = 0;
			StorageRange(field.storage, &lowest, &highest);

			fieldNames = names.insert(field.name).second && fieldNames;
			starts = field.anchor == Anchor::Start && starts;
			inside = field.offset + StorageBytes(field.storage) <= kind.fixedBytes && inside;
			masks = field.mask == 0 && field.unit != Unit::Flags && masks;
			whys = (field.limit == Limit::None) == (field.why[0] == 0) && field.source[0] != 0 && whys;
			creations = (!field.hasCreation || ValueIsAllowed(field, field.creation, 0)) && creations;
			words = WordsFit(field.label, 48, true) && WordsFit(field.why, 200, true) && words;

			if (field.limit != Limit::None)
			{
				limits = field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest && limits;
			}

			if (field.unit == Unit::Choice)
			{
				// Every value the limit allows has a name
				bool named = field.limit != Limit::None;

				for (double v = field.lowest; named && v <= field.highest; v++)
				{
					named = AcousticsChoiceIsNamed(kind.id, field.name, (int32_t)v);
				}

				choices = named && choices;
			}
		}
	}

	bool choicesBelong = true, choicesOnce = true;
	std::set<std::string> choiceKeys;

	for (size_t i = 0; i < AcousticsChoiceCount; i++)
	{
		ChunkKind const *const kind = FindAcousticsKind(AcousticsChoices[i].chunk);
		Field const *const field = kind ? FindChunkField(*kind, AcousticsChoices[i].field) : 0;

		// A named choice belongs to a choice field, is a value its limit allows, and is in words the panel's font has
		choicesBelong = field && field->unit == Unit::Choice && AcousticsChoices[i].value >= field->lowest &&
		                AcousticsChoices[i].value <= field->highest && WordsFit(AcousticsChoices[i].label, 48, false) && choicesBelong;
		choicesOnce = choiceKeys.insert(std::string(AcousticsChoices[i].chunk) + "/" + AcousticsChoices[i].field + "/" +
		                                std::to_string(AcousticsChoices[i].value)).second && choicesOnce;
	}

	Check(AcousticsKindCount > 0 && unique, "each acoustics chunk kind is in the table once");
	Check(readers, "every kind is an 8-letter chunk name, and names the game's reader as a file and line");
	Check(parents, "every kind is read by the game inside any chunk (registered with no parent, win95/chunk.hpp:689), so it names none");
	Check(sizes, "every kind is a plain fixed part: no list, no names, no block that holds its own size, and its registered size is all of it");
	Check(covered, "a kind's fields cover every byte of its numbers once, and none is shared");
	Check(fieldNames, "no kind has two fields of one name");
	Check(starts, "every field is counted from the start of its payload");
	Check(inside, "every field lies inside its kind's fixed part");
	Check(masks, "no field here is a set of flags with a mask");
	Check(limits, "every limit is a range its storage can hold");
	Check(whys, "a field with a limit says why, and only those do; every field says where its facts come from");
	Check(choices, "every value a choice field allows has a name");
	Check(choicesBelong, "every named choice belongs to a choice field, is a value its limit allows, and is in words the panel's font has");
	Check(choicesOnce, "no value of a choice field is named twice");
	Check(creations, "every starting value a table claims is one its field allows");
	Check(words, "every label is at most 48 letters and every reason at most 200, all plain letters the panel's font has");
	Check(FindAcousticsKind(0) == 0 && FindAcousticsKind("STDLIGHT") == 0 && FindAcousticsKind("MODACOUS") != 0 &&
	      !AcousticsChoiceIsNamed(0, "env_index", 0) && !AcousticsChoiceIsNamed("MODACOUS", 0, 0),
	      "the lookups refuse no name, and a kind these tables don't hold");

	// A chunk's name as a level file holds it: its 8 letters and straight after them the chunk's size, with no zero between
	char const header[12] = { 'M', 'O', 'D', 'A', 'C', 'O', 'U', 'S', 24, 0, 0, 0 };
	Check(FindAcousticsKind(header) != 0 && FindAcousticsKind(header) == FindAcousticsKind("MODACOUS") &&
	      AcousticsChoiceIsNamed(header, "env_index", 25) && FindAcousticsKind("MODACOUX") == 0 &&
	      !AcousticsChoiceIsNamed("MODACOUX", "env_index", 25),
	      "a kind is found from the 8 letters of a chunk's name with no zero after them, and all 8 letters are compared");
}

static void LayoutsAreTheGames()
{
	printf("Layouts are the game's reader's:\n");

	ChunkKind const *const module = FindAcousticsKind("MODACOUS");
	ChunkKind const *const level = FindAcousticsKind("ENVACOUS");
	Field const *const moduleType = module ? FindChunkField(*module, "env_index") : 0;
	Field const *const moduleReverb = module ? FindChunkField(*module, "reverb") : 0;
	Field const *const moduleSpare = module ? FindChunkField(*module, "spare") : 0;
	Field const *const levelType = level ? FindChunkField(*level, "env_index") : 0;
	Field const *const levelReverb = level ? FindChunkField(*level, "reverb") : 0;
	Field const *const levelSpare = level ? FindChunkField(*level, "spare") : 0;

	if (!moduleType || !moduleReverb || !moduleSpare || !levelType || !levelReverb || !levelSpare)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	// The reader takes a whole number at 0, a fraction at 4 and a whole number at 8 (win95/chunk_object.cpp:911-915 for a module,
	// win95/environment_chunk.cpp:200-204 for the level), and the chunk is registered to hold at least 12 bytes
	// (win95/chunk_object.cpp:898, win95/environment_chunk.cpp:187)
	Check(module->leastBytes == 12 && module->fixedBytes == 12 && module->fieldCount == 3 && module->parent[0] == 0 &&
	      FieldIs(*module, "env_index", 0, Storage::Int32) && FieldIs(*module, "reverb", 4, Storage::Float32) &&
	      FieldIs(*module, "spare", 8, Storage::Int32),
	      "a module's acoustics is 12 bytes, read in any chunk and used inside the module's data (MODULEDT): a whole number at 0, a fraction at 4, a whole number at 8");
	Check(level->leastBytes == 12 && level->fixedBytes == 12 && level->fieldCount == 3 && level->parent[0] == 0 &&
	      FieldIs(*level, "env_index", 0, Storage::Int32) && FieldIs(*level, "reverb", 4, Storage::Float32) &&
	      FieldIs(*level, "spare", 8, Storage::Int32),
	      "the level's acoustics is 12 bytes, read in any chunk and used inside the level's environment data (REBENVDT), laid out the same way");

	char const *why = 0;
	std::vector<uint8_t> moduleBytes(12, 0);
	bool const moduleSet = SetChunkField(*module, *moduleType, moduleBytes.data(), moduleBytes.size(), 21, &why) &&
	                       SetChunkField(*module, *moduleReverb, moduleBytes.data(), moduleBytes.size(), 0.25, &why) &&
	                       SetChunkField(*module, *moduleSpare, moduleBytes.data(), moduleBytes.size(), 7, &why);
	GameReading const moduleRead = ReadLikeTheGame(moduleBytes);

	Check(moduleSet && moduleRead.kind == 21 && moduleRead.reverb == 0.25f && moduleRead.spare == 7,
	      "what the model writes into a module's acoustics is what the game's reader reads back, field for field");

	std::vector<uint8_t> levelBytes(12, 0);
	bool const levelSet = SetChunkField(*level, *levelType, levelBytes.data(), levelBytes.size(), 12, &why) &&
	                      SetChunkField(*level, *levelReverb, levelBytes.data(), levelBytes.size(), 2, &why) &&
	                      SetChunkField(*level, *levelSpare, levelBytes.data(), levelBytes.size(), 3, &why);
	GameReading const levelRead = ReadLikeTheGame(levelBytes);

	Check(levelSet && levelRead.kind == 12 && levelRead.reverb == 2.0f && levelRead.spare == 3,
	      "what the model writes into the level's acoustics is what the game's reader reads back, field for field");
}

static void SizesAreTheGames()
{
	printf("Sizes are checked as the game's reader checks them:\n");

	ChunkKind const *const module = FindAcousticsKind("MODACOUS");
	ChunkKind const *const level = FindAcousticsKind("ENVACOUS");

	if (!module || !level)
	{
		Check(false, "the kinds these checks use are in the tables");
		return;
	}

	Check(SizeOf(*module, 0) == SizeCheck::TooShort && SizeOf(*module, 11) == SizeCheck::TooShort,
	      "a module's acoustics of 0 or 11 bytes is refused as too short: the game keeps it as plain bytes and refuses the level");
	Check(SizeOf(*module, 12) == SizeCheck::Right, "a module's acoustics of 12 bytes is the right size");
	Check(SizeOf(*module, 13) == SizeCheck::Right && SizeOf(*module, 100) == SizeCheck::Right,
	      "a longer module's acoustics is read, and the rest ignored, as the game does");
	Check(SizeOf(*level, 0) == SizeCheck::TooShort && SizeOf(*level, 11) == SizeCheck::TooShort,
	      "the level's acoustics of 0 or 11 bytes is refused as too short: the game keeps it as plain bytes and refuses the level");
	Check(SizeOf(*level, 12) == SizeCheck::Right, "the level's acoustics of 12 bytes is the right size");
	Check(SizeOf(*level, 13) == SizeCheck::Right && SizeOf(*level, 100) == SizeCheck::Right,
	      "a longer level's acoustics is read, and the rest ignored, as the game does");
}

static void ValuesAreChecked()
{
	printf("Values are checked:\n");

	ChunkKind const *const module = FindAcousticsKind("MODACOUS");
	ChunkKind const *const level = FindAcousticsKind("ENVACOUS");
	Field const *const moduleType = module ? FindChunkField(*module, "env_index") : 0;
	Field const *const moduleReverb = module ? FindChunkField(*module, "reverb") : 0;
	Field const *const moduleSpare = module ? FindChunkField(*module, "spare") : 0;
	Field const *const levelType = level ? FindChunkField(*level, "env_index") : 0;
	Field const *const levelReverb = level ? FindChunkField(*level, "reverb") : 0;
	Field const *const levelSpare = level ? FindChunkField(*level, "spare") : 0;

	if (!moduleType || !moduleReverb || !moduleSpare || !levelType || !levelReverb || !levelSpare)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	double const notANumber = std::numeric_limits<double>::quiet_NaN();
	double const endless = std::numeric_limits<double>::infinity();

	Check(strcmp(Verdict(*moduleType, -1), "") == 0 && strcmp(Verdict(*moduleType, 0), "") == 0 && strcmp(Verdict(*moduleType, 25), "") == 0,
	      "a module's acoustic type of -1 (the level's), 0 or 25 is allowed");
	Check(Verdict(*moduleType, -2) == moduleType->why && Verdict(*moduleType, 26) == moduleType->why,
	      "a module's acoustic type of -2 or 26, just outside, is refused for the field's own reason");
	Check(Verdict(*moduleType, notANumber) == NotANumberReason && Verdict(*moduleType, endless) == NotANumberReason &&
	      Verdict(*moduleType, -endless) == NotANumberReason,
	      "a module's acoustic type that is NaN or endless is refused as not a number");
	Check(Verdict(*moduleType, 1.5) == NotWholeReason && Verdict(*moduleType, 2147483648.0) == DoesNotFitReason &&
	      Verdict(*moduleType, -2147483649.0) == DoesNotFitReason,
	      "a module's acoustic type with a fraction, or too big for the 4 bytes the file keeps it in, is refused for those reasons");

	Check(strcmp(Verdict(*moduleReverb, -1), "") == 0 && strcmp(Verdict(*moduleReverb, 0), "") == 0 &&
	      strcmp(Verdict(*moduleReverb, 0.5), "") == 0 && strcmp(Verdict(*moduleReverb, 1), "") == 0 &&
	      strcmp(Verdict(*moduleReverb, 2), "") == 0,
	      "a module's reverb of -1 (the level's), 0, 0.5, 1 or 2 (the default) is allowed");
	Check(Verdict(*moduleReverb, -1.0001) == moduleReverb->why && Verdict(*moduleReverb, 2.0001) == moduleReverb->why,
	      "a module's reverb of -1.0001 or 2.0001, just outside, is refused for the field's own reason");
	Check(Verdict(*moduleReverb, notANumber) == NotANumberReason && Verdict(*moduleReverb, endless) == NotANumberReason &&
	      Verdict(*moduleReverb, -endless) == NotANumberReason,
	      "a module's reverb that is NaN or endless is refused as not a number");
	Check(Verdict(*moduleReverb, 1e39) == DoesNotFitReason && Verdict(*moduleReverb, -1e39) == DoesNotFitReason,
	      "a module's reverb too big for the 4-byte fraction the file keeps it in is refused for that reason");

	Check(strcmp(Verdict(*levelType, 0), "") == 0 && strcmp(Verdict(*levelType, 25), "") == 0,
	      "the level's acoustic type of 0 or 25 is allowed");
	Check(Verdict(*levelType, -1) == levelType->why && Verdict(*levelType, 26) == levelType->why,
	      "the level's acoustic type of -1 or 26, just outside, is refused for the field's own reason");
	Check(Verdict(*levelType, notANumber) == NotANumberReason && Verdict(*levelType, endless) == NotANumberReason &&
	      Verdict(*levelType, 1.5) == NotWholeReason,
	      "the level's acoustic type that is NaN or endless is refused as not a number, and a fraction as not whole");

	Check(strcmp(Verdict(*levelReverb, 0), "") == 0 && strcmp(Verdict(*levelReverb, 0.5), "") == 0 &&
	      strcmp(Verdict(*levelReverb, 1), "") == 0 && strcmp(Verdict(*levelReverb, 2), "") == 0,
	      "the level's reverb of 0, 0.5, 1 or 2 (the default) is allowed");
	Check(Verdict(*levelReverb, -0.0001) == levelReverb->why && Verdict(*levelReverb, 2.0001) == levelReverb->why,
	      "the level's reverb of -0.0001 or 2.0001, just outside, is refused for the field's own reason");
	Check(Verdict(*levelReverb, notANumber) == NotANumberReason && Verdict(*levelReverb, endless) == NotANumberReason &&
	      Verdict(*levelReverb, -endless) == NotANumberReason,
	      "the level's reverb that is NaN or endless is refused as not a number");

	Check(strcmp(Verdict(*moduleSpare, 2147483647.0), "") == 0 && strcmp(Verdict(*levelSpare, -2147483648.0), "") == 0 &&
	      Verdict(*moduleSpare, 0.5) == NotWholeReason && Verdict(*levelSpare, notANumber) == NotANumberReason,
	      "a spare number takes any whole number the file can hold, and nothing that isn't one");
}

static void ChoicesAreTheGames()
{
	printf("The 26 rooms are the game's, in its order:\n");

	ChunkKind const *const module = FindAcousticsKind("MODACOUS");
	ChunkKind const *const level = FindAcousticsKind("ENVACOUS");
	Field const *const moduleType = module ? FindChunkField(*module, "env_index") : 0;
	Field const *const levelType = level ? FindChunkField(*level, "env_index") : 0;

	if (!moduleType || !levelType)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	bool moduleNames = true, levelNames = true;

	for (int room = 0; room < kReverbRoomKinds; room++)
	{
		moduleNames = SameLettersIgnoringCase(LabelOf("MODACOUS", "env_index", room), Reverb_RoomName((unsigned int)room)) && moduleNames;
		levelNames = SameLettersIgnoringCase(LabelOf("ENVACOUS", "env_index", room), Reverb_RoomName((unsigned int)room)) && levelNames;
	}

	Check(kReverbRoomKinds == 26 && RoomKindCount == kReverbRoomKinds, "the game still has 26 kinds of room (reverb_choice.h), and so do the tables");
	Check(moduleNames && levelNames,
	      "each of the 26 numbers is named for the room the game's own list names (reverb_choice.h), in the game's order, for a module and for the level");
	Check(moduleType->lowest == -1 && moduleType->highest == kReverbRoomKinds - 1 && levelType->lowest == 0 &&
	      levelType->highest == kReverbRoomKinds - 1,
	      "the types allowed run from the game's first room to its last, and a module may also say -1");
	Check(strcmp(LabelOf("MODACOUS", "env_index", -1), "Default") == 0 && !AcousticsChoiceIsNamed("ENVACOUS", "env_index", -1) &&
	      !AcousticsChoiceIsNamed("MODACOUS", "env_index", 26) && !AcousticsChoiceIsNamed("ENVACOUS", "env_index", 26),
	      "-1 is the module's \"Default\" alone, and 26 names no room");
}

static void EchoRulesAreTheGames()
{
	printf("The rules for a room's echo are the game's:\n");

	float const notANumber = std::numeric_limits<float>::quiet_NaN();
	float const endless = std::numeric_limits<float>::infinity();

	// The level's echo (avp/win95/projload.cpp:1962-1969)
	Check(EchoIs(LevelEcho(false, 12, 0.5f), 0, -1.0f), "a level with no acoustics chunk has room kind 0 and an amount of -1, left to distance");
	Check(EchoIs(LevelEcho(true, 12, 0.5f), 12, 0.5f), "a level's chunk gives its kind, and its amount when it is 1 or less, as they are");
	Check(EchoIs(LevelEcho(true, 5, 1.0f), 5, 1.0f) && EchoIs(LevelEcho(true, 5, 1.0001f), 5, -1.0f) && EchoIs(LevelEcho(true, 0, 2.0f), 0, -1.0f),
	      "a level's amount of 1 is used, and one above 1, such as 1.0001 or the 2 all 6 stock levels with one hold, means the default, -1");
	Check(EchoIs(LevelEcho(true, -3, -0.5f), -3, -0.5f) && RoomAmountKept(-0.5f) == -1.0f,
	      "a level's negative kind and negative amount are kept as they are, and the sound side reads the amount as -1");
	Check(EchoIs(LevelEcho(true, 7, notANumber), 7, -1.0f) && EchoIs(LevelEcho(true, 7, endless), 7, -1.0f) &&
	      RoomAmountKept(LevelEcho(true, 7, -endless).amount) == -1.0f,
	      "by the IEEE rules, a level's amount that is NaN or endless comes to -1 (the game's /fp:fast build doesn't promise it; the loader refuses such a level)");

	// A module's echo (avp/win95/projload.cpp:2027-2028 and :2384-2397): a level whose chunk says kind 12, amount 0.5
	RoomEcho const level = { 12, 0.5f };

	Check(EchoIs(ModuleEcho(level, false, 3, 0.2f), 12, 0.5f), "a module with no acoustics chunk has the level's kind and amount");
	Check(EchoIs(ModuleEcho(level, true, -1, -1.0f), 12, 0.5f), "a new module's chunk, with kind -1 and reverb -1, leaves both to the level's");
	Check(EchoIs(ModuleEcho(level, true, 21, -1.0f), 21, 0.5f) && EchoIs(ModuleEcho(level, true, -1, 0.2f), 12, 0.2f),
	      "a module's own kind replaces the level's alone, and its own reverb replaces the level's alone");
	Check(EchoIs(ModuleEcho(level, true, 0, 0.0f), 0, 0.0f), "a module's kind 0 and reverb 0 are values of their own, not the level's");
	Check(EchoIs(ModuleEcho(level, true, 3, 1.0f), 3, 1.0f) && EchoIs(ModuleEcho(level, true, 3, 1.0001f), 3, -1.0f) &&
	      EchoIs(ModuleEcho(level, true, 3, 2.0f), 3, -1.0f),
	      "a module's reverb of 1 is used, and one above 1, such as 1.0001 or 2, means the default, -1");
	Check(EchoIs(ModuleEcho(level, true, -2, -0.0001f), 12, 0.5f), "any negative kind or reverb, not only -1, leaves the level's");
	Check(EchoIs(ModuleEcho(level, true, 3, notANumber), 3, 0.5f) && EchoIs(ModuleEcho(level, true, 3, endless), 3, -1.0f) &&
	      EchoIs(ModuleEcho(level, true, 3, -endless), 3, 0.5f),
	      "by the IEEE rules, a module's reverb that is NaN or minus endless leaves the level's, and plus endless is above 1 (the game's /fp:fast build doesn't promise it; the loader refuses such a level)");

	// The sound side (avp/win95/openal_audio.cpp:1670 and :1693, reverb_choice.h:27-30)
	Check(RoomKindIsKnown(0) && RoomKindIsKnown(25) && !RoomKindIsKnown(26) && !RoomKindIsKnown(-1) &&
	      !RoomKindIsKnown(std::numeric_limits<int32_t>::min()),
	      "the sound side has room kinds 0 to 25, and takes 26, a negative number and the smallest number as ones it doesn't have");
	Check(RoomAmountKept(0.0f) == 0.0f && RoomAmountKept(0.5f) == 0.5f && RoomAmountKept(1.0f) == 1.0f && RoomAmountKept(1.0001f) == -1.0f &&
	      RoomAmountKept(-0.0001f) == -1.0f && RoomAmountKept(2.0f) == -1.0f && RoomAmountKept(notANumber) == -1.0f &&
	      RoomAmountKept(endless) == -1.0f && RoomAmountKept(-endless) == -1.0f,
	      "the sound side keeps an amount of 0 to 1 as it is, and anything else as -1 (NaN and endless too, by the IEEE rules)");

	float const amounts[] = { -5.0f, -1.0f, -0.0001f, 0.0f, 0.25f, 0.5f, 1.0f, 1.0001f, 2.0f, 100.0f };
	bool sameAsTheGame = true;

	for (float amount : amounts)
	{
		sameAsTheGame = RoomAmountKept(amount) == Reverb_RoomAmount(amount) && sameAsTheGame;
	}

	Check(sameAsTheGame, "the amount kept is what the game's own Reverb_RoomAmount (reverb_choice.h) keeps, for every real number tried");
}

static void SettersWriteOnlyTheirPart()
{
	printf("Setters write only their own part:\n");

	ChunkKind const *const module = FindAcousticsKind("MODACOUS");
	ChunkKind const *const level = FindAcousticsKind("ENVACOUS");
	Field const *const moduleType = module ? FindChunkField(*module, "env_index") : 0;
	Field const *const moduleReverb = module ? FindChunkField(*module, "reverb") : 0;
	Field const *const moduleSpare = module ? FindChunkField(*module, "spare") : 0;
	Field const *const levelType = level ? FindChunkField(*level, "env_index") : 0;

	if (!moduleType || !moduleReverb || !moduleSpare || !levelType)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	char const *why = 0;
	std::vector<uint8_t> bytes(12, 0x11);

	Check(SetChunkField(*module, *moduleType, bytes.data(), bytes.size(), 21, &why) && GetBits(bytes, 0) == 21 &&
	      GetBits(bytes, 4) == 0x11111111 && GetBits(bytes, 8) == 0x11111111,
	      "setting a module's acoustic type changes its 4 bytes alone");
	Check(SetChunkField(*module, *moduleReverb, bytes.data(), bytes.size(), 0.25, &why) && GetBits(bytes, 4) == 0x3E800000 &&
	      GetBits(bytes, 0) == 21 && GetBits(bytes, 8) == 0x11111111,
	      "setting a module's reverb writes the fraction's own bits, 0.25 as 0x3E800000, and nothing else");
	Check(SetChunkField(*module, *moduleSpare, bytes.data(), bytes.size(), -2, &why) && GetBits(bytes, 8) == 0xFFFFFFFE &&
	      GetBits(bytes, 0) == 21 && GetBits(bytes, 4) == 0x3E800000,
	      "setting a module's spare number writes its 4 bytes alone, a negative one in two's complement");

	// A level file can hold a reverb that is NaN (0x7FC00000): the size is still right, and a real one the mapper sets replaces it
	std::vector<uint8_t> hostile(12, 0);
	PutBits(hostile, 0, 0xFFFFFFFF);
	PutBits(hostile, 4, 0x7FC00000);
	Check(CheckChunkSize(*module, hostile.data(), hostile.size()) == SizeCheck::Right &&
	      SetChunkField(*module, *moduleReverb, hostile.data(), hostile.size(), -1, &why) && GetBits(hostile, 4) == 0xBF800000 &&
	      GetBits(hostile, 0) == 0xFFFFFFFF,
	      "a reverb that is NaN doesn't change the chunk's size, and a real one the mapper sets replaces it and leaves the rest");

	std::vector<uint8_t> const before = bytes;
	why = 0;
	Check(!SetChunkField(*module, *moduleReverb, bytes.data(), bytes.size(), 2.5, &why) && why == moduleReverb->why && bytes == before,
	      "a refused reverb changes nothing, and the reason given is the field's own");
	why = 0;
	Check(!SetChunkField(*module, *moduleReverb, bytes.data(), bytes.size(), std::numeric_limits<double>::quiet_NaN(), &why) &&
	      why == NotANumberReason && bytes == before,
	      "a reverb that is NaN is refused as not a number, and nothing changes");
	Check(!SetChunkField(*module, *levelType, bytes.data(), bytes.size(), 3, &why) && bytes == before,
	      "a field of the level's acoustics is refused for a module's, rather than written at its own offset");
	Check(!SetChunkField(*module, *moduleReverb, bytes.data(), 7, 0.5, &why) && bytes == before,
	      "a field past the end of a short payload is refused, and nothing is written");
}

static void NewChunksAreTheLibrarys()
{
	printf("A new chunk is the game's chunk library's:\n");

	// The library's constructors (win95/chunk_object.cpp:900-906, win95/environment_chunk.cpp:189-195): a module's is type -1 and reverb
	// -1.0, the level's is type 0 and reverb 2, each with a spare of 0
	uint8_t const newModule[12] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00 };
	uint8_t const newLevel[12] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 };
	std::vector<uint8_t> made(16, 0x55);
	size_t length = 0;

	Check(MakeNewAcousticsChunk("MODACOUS", made.data(), made.size(), &length) && length == 12 && memcmp(made.data(), newModule, 12) == 0 &&
	      made.at(12) == 0x55 && made.at(15) == 0x55,
	      "a new module's acoustics is 12 bytes: type -1 and reverb -1, both the level's, and a spare of 0, with nothing past them touched");

	std::vector<uint8_t> madeLevel(16, 0x55);
	length = 0;

	Check(MakeNewAcousticsChunk("ENVACOUS", madeLevel.data(), madeLevel.size(), &length) && length == 12 &&
	      memcmp(madeLevel.data(), newLevel, 12) == 0 && madeLevel.at(12) == 0x55,
	      "a new level's acoustics is 12 bytes: type 0 (Generic), reverb 2 (the default) and a spare of 0");

	std::vector<uint8_t> tooSmall(11, 0x55);
	std::vector<uint8_t> const tooSmallBefore = tooSmall;
	length = 99;

	Check(!MakeNewAcousticsChunk("MODACOUS", tooSmall.data(), tooSmall.size(), &length) && tooSmall == tooSmallBefore && length == 99,
	      "a new chunk that doesn't fit the room given is refused: nothing is written and the length is left alone");
	Check(!MakeNewAcousticsChunk("STDLIGHT", made.data(), made.size(), &length) && !MakeNewAcousticsChunk(0, made.data(), made.size(), &length),
	      "a kind these tables don't hold, or no kind at all, makes nothing");
}

static void StockLevelsAreAllowed()
{
	printf("The values the game's own levels hold are allowed (for module types, the twelve commonest):\n");

	ChunkKind const *const module = FindAcousticsKind("MODACOUS");
	ChunkKind const *const level = FindAcousticsKind("ENVACOUS");
	Field const *const moduleType = module ? FindChunkField(*module, "env_index") : 0;
	Field const *const moduleReverb = module ? FindChunkField(*module, "reverb") : 0;
	Field const *const levelType = level ? FindChunkField(*level, "env_index") : 0;
	Field const *const levelReverb = level ? FindChunkField(*level, "reverb") : 0;

	if (!moduleType || !moduleReverb || !levelType || !levelReverb)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	// The game's own levels, from AvP-docs: engine\inteng\raw\stock-field-values.txt lines 13-14 and 23-24 (the twelve commonest module
	// types), and tools\drake\rif-survey\floats-85704f7a.txt lines 3-4
	double const moduleTypes[] = { 21, 3, 13, 10, 1, 14, 5, 9, 8, 22, 12, 6 };
	double const moduleReverbs[] = { -1, 0.1, 0.2, 0.3, 0.4, 2 };
	double const levelTypes[] = { 0, 12 };
	double const levelReverbs[] = { 2 };
	bool moduleTypesAllowed = true, moduleReverbsAllowed = true, levelTypesAllowed = true, levelReverbsAllowed = true;

	for (double value : moduleTypes)
	{
		moduleTypesAllowed = Allowed(*moduleType, value) && moduleTypesAllowed;
	}

	for (double value : moduleReverbs)
	{
		moduleReverbsAllowed = Allowed(*moduleReverb, value) && moduleReverbsAllowed;
	}

	for (double value : levelTypes)
	{
		levelTypesAllowed = Allowed(*levelType, value) && levelTypesAllowed;
	}

	for (double value : levelReverbs)
	{
		levelReverbsAllowed = Allowed(*levelReverb, value) && levelReverbsAllowed;
	}

	Check(moduleTypesAllowed, "the twelve commonest module acoustic types of the game's own levels are allowed (the survey shows 12 of its 21)");
	Check(moduleReverbsAllowed, "the module reverbs of the game's own levels (-1, 0.1 to 0.4, 2) are allowed");
	Check(levelTypesAllowed, "the level acoustic types of the game's own levels (0 and 12) are allowed");
	Check(levelReverbsAllowed, "the level reverb of the game's own levels (2) is allowed");
}

// What the game reads from a light scale, exactly as its reader does (win95/chunk_light.cpp:297-331): a whole number, then six
// fractions, four of which it turns into whole numbers as it reads them (each "above" and each "add"), then three more whole numbers.
struct LightScaleReading
{
	int32_t lightMode;
	float prelightMultiply;
	int32_t prelightMultiplyAbove;
	int32_t prelightAdd;
	float runtimeMultiply;
	int32_t runtimeMultiplyAbove;
	int32_t runtimeAdd;
	int32_t spare1;
	int32_t spare2;
	int32_t spare3;
};

static LightScaleReading ReadLightScaleLikeTheGame(std::vector<uint8_t> const &bytes)
{
	LightScaleReading reading;
	reading.lightMode = GetInt(bytes, 0);
	reading.prelightMultiply = GetFloat(bytes, 4);
	reading.prelightMultiplyAbove = (int32_t)GetFloat(bytes, 8);
	reading.prelightAdd = (int32_t)GetFloat(bytes, 12);
	reading.runtimeMultiply = GetFloat(bytes, 16);
	reading.runtimeMultiplyAbove = (int32_t)GetFloat(bytes, 20);
	reading.runtimeAdd = (int32_t)GetFloat(bytes, 24);
	reading.spare1 = GetInt(bytes, 28);
	reading.spare2 = GetInt(bytes, 32);
	reading.spare3 = GetInt(bytes, 36);
	return reading;
}

static void LightSetChunksAreTheGames()
{
	printf("The light set's chunks are the game's reader's:\n");

	ChunkKind const *const ambience = FindAcousticsKind("AMBIENCE");
	ChunkKind const *const header = FindAcousticsKind("LTSETHDR");
	ChunkKind const *const scale = FindAcousticsKind("LITSCALE");
	Field const *const ambienceField = ambience ? FindChunkField(*ambience, "ambience") : 0;
	Field const *const padField = header ? FindChunkField(*header, "pad") : 0;
	Field const *const lightMode = scale ? FindChunkField(*scale, "light_mode") : 0;
	Field const *const prelightMultiply = scale ? FindChunkField(*scale, "prelight_multiply") : 0;
	Field const *const prelightAbove = scale ? FindChunkField(*scale, "prelight_multiply_above") : 0;
	Field const *const prelightAdd = scale ? FindChunkField(*scale, "prelight_add") : 0;
	Field const *const runtimeMultiply = scale ? FindChunkField(*scale, "runtime_multiply") : 0;
	Field const *const runtimeAbove = scale ? FindChunkField(*scale, "runtime_multiply_above") : 0;
	Field const *const runtimeAdd = scale ? FindChunkField(*scale, "runtime_add") : 0;
	Field const *const spare1 = scale ? FindChunkField(*scale, "spare1") : 0;
	Field const *const spare2 = scale ? FindChunkField(*scale, "spare2") : 0;
	Field const *const spare3 = scale ? FindChunkField(*scale, "spare3") : 0;

	if (!ambience || !header || !scale || !ambienceField || !padField || !lightMode || !prelightMultiply || !prelightAbove ||
	    !prelightAdd || !runtimeMultiply || !runtimeAbove || !runtimeAdd || !spare1 || !spare2 || !spare3)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	double const notANumber = std::numeric_limits<double>::quiet_NaN();
	double const endless = std::numeric_limits<double>::infinity();
	char const *why = 0;

	// AMBIENCE (win95/chunk_light.cpp:262 and :265-277, win95/chunk_light.hpp:218-243): one whole number, at least 4 bytes
	Check(ambience->leastBytes == 4 && ambience->fixedBytes == 4 && ambience->fieldCount == 1 && ambience->parent[0] == 0 &&
	      FieldIs(*ambience, "ambience", 0, Storage::Int32) && ambienceField->unit == Unit::Fixed16 &&
	      strcmp(ambienceField->label, "Ambience") == 0 && !ambienceField->hasCreation,
	      "an ambience is 4 bytes, read in any chunk and used inside the light set (LIGHTSET): one whole number in 16.16 fixed point, labelled as the old editor shows it, with no starting value");
	Check(SizeOf(*ambience, 0) == SizeCheck::TooShort && SizeOf(*ambience, 3) == SizeCheck::TooShort &&
	      SizeOf(*ambience, 4) == SizeCheck::Right && SizeOf(*ambience, 5) == SizeCheck::Right && SizeOf(*ambience, 100) == SizeCheck::Right,
	      "an ambience of 0 or 3 bytes is refused as too short, 4 is the right size, and a longer one is read with the rest ignored");

	std::vector<uint8_t> ambienceBytes(4, 0);
	bool const full = SetChunkField(*ambience, *ambienceField, ambienceBytes.data(), ambienceBytes.size(), 65536, &why) &&
	                  GetInt(ambienceBytes, 0) == 65536 && ambienceBytes.at(0) == 0x00 && ambienceBytes.at(1) == 0x00 &&
	                  ambienceBytes.at(2) == 0x01 && ambienceBytes.at(3) == 0x00;
	bool const onePercent = SetChunkField(*ambience, *ambienceField, ambienceBytes.data(), ambienceBytes.size(), 655, &why) &&
	                        GetInt(ambienceBytes, 0) == 655 && ambienceBytes.at(0) == 0x8F && ambienceBytes.at(1) == 0x02 &&
	                        ambienceBytes.at(2) == 0x00 && ambienceBytes.at(3) == 0x00;

	Check(full && onePercent,
	      "what the model writes into an ambience is what the game's reader reads: 65536 is the bytes 00 00 01 00, and 655 is 8F 02 00 00");
	Check(strcmp(Verdict(*ambienceField, 0), "") == 0 && strcmp(Verdict(*ambienceField, 65536), "") == 0 &&
	      Verdict(*ambienceField, -1) == ambienceField->why && Verdict(*ambienceField, 65537) == ambienceField->why,
	      "an ambience of 0 or 65536 (0 to 100 percent) is allowed, and one just outside, -1 or 65537, is refused for the field's own reason");
	Check(Verdict(*ambienceField, notANumber) == NotANumberReason && Verdict(*ambienceField, endless) == NotANumberReason &&
	      Verdict(*ambienceField, -endless) == NotANumberReason && Verdict(*ambienceField, 0.5) == NotWholeReason,
	      "an ambience that is NaN or endless is refused as not a number, and a fraction as not whole");
	// The game's own levels (AvP-docs: game\bake-study\result-formula-all-lights.txt, the "amb" column): 0 in 55 levels, 655 in two, 2620 in one, 83 in one
	Check(Allowed(*ambienceField, 0) && Allowed(*ambienceField, 83) && Allowed(*ambienceField, 655) && Allowed(*ambienceField, 2620),
	      "the ambiences of the game's own levels (0, 83, 655 and 2620) are allowed");

	// LTSETHDR (win95/chunk_light.cpp:40 and :59-67, win95/chunk_light.hpp:101-123): a name of 8 bytes, then one whole number
	NameKind const *const setName = FindNameKind("LTSETHDR", "light set name");

	Check(header->leastBytes == 12 && header->fixedBytes == 12 && header->fieldCount == 1 && header->parent[0] == 0 &&
	      FieldIs(*header, "pad", 8, Storage::Int32) && padField->label[0] == 0 && !padField->hasCreation,
	      "a light set header is 12 bytes, read in any chunk and used inside the light set (LIGHTSET): one whole number at 8, with no label, so the panel skips it, and no starting value");
	Check(setName && setName->storage == NameStorage::Fixed && setName->gameMost == 8 && padField->offset == setName->gameMost,
	      "the 8 bytes before that number are the light set's name, a fixed name of 8 bytes in the name tables, so no number is kept there");
	Check(SizeOf(*header, 11) == SizeCheck::TooShort && SizeOf(*header, 12) == SizeCheck::Right && SizeOf(*header, 13) == SizeCheck::Right,
	      "a light set header of 11 bytes is refused as too short, and 12 or more is read");

	std::vector<uint8_t> headerBytes = { 0x4E, 0x4F, 0x52, 0x4D, 0x41, 0x4C, 0x4C, 0x54, 0x00, 0x00, 0x00, 0x00 };   // "NORMALLT", then 0
	std::vector<uint8_t> const headerBefore = headerBytes;

	Check(SetChunkField(*header, *padField, headerBytes.data(), headerBytes.size(), 9, &why) && GetInt(headerBytes, 8) == 9 &&
	      memcmp(headerBytes.data(), headerBefore.data(), 8) == 0,
	      "setting a light set header's number writes its 4 bytes alone and leaves the name, \"NORMALLT\", as it was");

	std::vector<uint8_t> room(64, 0x55);
	std::vector<uint8_t> const roomBefore = room;
	size_t length = 99;

	Check(!MakeNewAcousticsChunk("AMBIENCE", room.data(), room.size(), &length) && !MakeNewAcousticsChunk("LTSETHDR", room.data(), room.size(), &length) &&
	      !MakeNewAcousticsChunk("LITSCALE", room.data(), room.size(), &length) && room == roomBefore && length == 99,
	      "no new light set chunk is made: the chunk library gives an ambience, a header and a light scale's mode no starting value, so nothing is written");

	// LITSCALE (win95/chunk_light.cpp:282 and :294-333, win95/chunk_light.hpp:245-269): a whole number, six fractions, three whole numbers
	Check(scale->leastBytes == 40 && scale->fixedBytes == 40 && scale->fieldCount == 10 && scale->parent[0] == 0 &&
	      FieldIs(*scale, "light_mode", 0, Storage::Int32) && FieldIs(*scale, "prelight_multiply", 4, Storage::Float32) &&
	      FieldIs(*scale, "prelight_multiply_above", 8, Storage::Float32) && FieldIs(*scale, "prelight_add", 12, Storage::Float32) &&
	      FieldIs(*scale, "runtime_multiply", 16, Storage::Float32) && FieldIs(*scale, "runtime_multiply_above", 20, Storage::Float32) &&
	      FieldIs(*scale, "runtime_add", 24, Storage::Float32) && FieldIs(*scale, "spare1", 28, Storage::Int32) &&
	      FieldIs(*scale, "spare2", 32, Storage::Int32) && FieldIs(*scale, "spare3", 36, Storage::Int32),
	      "a light scale is 40 bytes, read in any chunk and used inside the light set (LIGHTSET): a whole number at 0, six fractions from 4 to 24, three whole numbers from 28");
	Check(SizeOf(*scale, 39) == SizeCheck::TooShort && SizeOf(*scale, 40) == SizeCheck::Right && SizeOf(*scale, 44) == SizeCheck::Right &&
	      SizeOf(*scale, 100) == SizeCheck::Right,
	      "a light scale of 39 bytes is refused as too short, 40 is the right size, and 44, which the chunk library writes, or more is read with the rest ignored");

	uint8_t const scaleExpected[40] =
	{
		0x01, 0x00, 0x00, 0x00,   // light_mode 1
		0x00, 0x00, 0xC0, 0x3F,   // prelight_multiply 1.5
		0x00, 0x00, 0x2C, 0x41,   // prelight_multiply_above 10.75, which the game reads as 10
		0x00, 0x00, 0x60, 0xC0,   // prelight_add -3.5, which the game reads as -3
		0x00, 0x00, 0x00, 0x3F,   // runtime_multiply 0.5
		0x00, 0x00, 0x48, 0x43,   // runtime_multiply_above 200
		0x00, 0x00, 0xE0, 0x40,   // runtime_add 7
		0x04, 0x00, 0x00, 0x00,   // spare1 4
		0x05, 0x00, 0x00, 0x00,   // spare2 5
		0x06, 0x00, 0x00, 0x00    // spare3 6
	};
	std::vector<uint8_t> scaleBytes(40, 0);
	bool const scaleSet =
		SetChunkField(*scale, *lightMode, scaleBytes.data(), scaleBytes.size(), 1, &why) &&
		SetChunkField(*scale, *prelightMultiply, scaleBytes.data(), scaleBytes.size(), 1.5, &why) &&
		SetChunkField(*scale, *prelightAbove, scaleBytes.data(), scaleBytes.size(), 10.75, &why) &&
		SetChunkField(*scale, *prelightAdd, scaleBytes.data(), scaleBytes.size(), -3.5, &why) &&
		SetChunkField(*scale, *runtimeMultiply, scaleBytes.data(), scaleBytes.size(), 0.5, &why) &&
		SetChunkField(*scale, *runtimeAbove, scaleBytes.data(), scaleBytes.size(), 200, &why) &&
		SetChunkField(*scale, *runtimeAdd, scaleBytes.data(), scaleBytes.size(), 7, &why) &&
		SetChunkField(*scale, *spare1, scaleBytes.data(), scaleBytes.size(), 4, &why) &&
		SetChunkField(*scale, *spare2, scaleBytes.data(), scaleBytes.size(), 5, &why) &&
		SetChunkField(*scale, *spare3, scaleBytes.data(), scaleBytes.size(), 6, &why);
	LightScaleReading const scaleRead = ReadLightScaleLikeTheGame(scaleBytes);

	Check(scaleSet && memcmp(scaleBytes.data(), scaleExpected, 40) == 0 && scaleRead.lightMode == 1 && scaleRead.prelightMultiply == 1.5f &&
	      scaleRead.prelightMultiplyAbove == 10 && scaleRead.prelightAdd == -3 && scaleRead.runtimeMultiply == 0.5f &&
	      scaleRead.runtimeMultiplyAbove == 200 && scaleRead.runtimeAdd == 7 && scaleRead.spare1 == 4 && scaleRead.spare2 == 5 &&
	      scaleRead.spare3 == 6,
	      "what the model writes into a light scale is these 40 bytes, and the game's reader reads them back, turning 10.75 into 10 and -3.5 into -3");
	Check(strcmp(Verdict(*prelightMultiply, 16777216), "") == 0 && strcmp(Verdict(*runtimeAdd, -16777216), "") == 0 &&
	      Verdict(*prelightMultiply, 16777218) == prelightMultiply->why && Verdict(*runtimeAdd, -16777218) == runtimeAdd->why &&
	      Verdict(*prelightAbove, 16777218) == prelightAbove->why && Verdict(*runtimeAbove, -16777218) == runtimeAbove->why,
	      "a light scale's numbers of 2^24 either way are allowed, and one just outside, 16777218 or -16777218 (the next a float holds), is refused for the field's own reason");
	Check(Verdict(*prelightMultiply, notANumber) == NotANumberReason && Verdict(*runtimeMultiply, endless) == NotANumberReason &&
	      Verdict(*prelightAdd, -endless) == NotANumberReason && Verdict(*runtimeAbove, 1e39) == DoesNotFitReason,
	      "a light scale's number that is NaN or endless is refused as not a number, and one too big for a 4-byte fraction for that reason");
	Check(prelightMultiply->hasCreation && prelightMultiply->creation == 1 && runtimeMultiply->hasCreation && runtimeMultiply->creation == 1 &&
	      prelightAbove->hasCreation && prelightAbove->creation == 0 && prelightAdd->hasCreation && prelightAdd->creation == 0 &&
	      runtimeAbove->hasCreation && runtimeAbove->creation == 0 && runtimeAdd->hasCreation && runtimeAdd->creation == 0 &&
	      spare1->hasCreation && spare1->creation == 0 && spare2->hasCreation && spare2->creation == 0 &&
	      spare3->hasCreation && spare3->creation == 0 && !lightMode->hasCreation,
	      "a new light scale is the chunk library's: each multiply 1, each above and add 0, the spares 0, and the mode left to whoever makes it");

	// The six fractions have the words of cencon's dialog 146, "Scaling For PSX Lights", in the order of its boxes (inferred: Apone's decision)
	Check(strcmp(prelightMultiply->label, "Prelight: Scale") == 0 && strcmp(prelightAbove->label, "Prelight: Scale if above") == 0 &&
	      strcmp(prelightAdd->label, "Prelight: Add") == 0 && strcmp(runtimeMultiply->label, "Runtime: Scale") == 0 &&
	      strcmp(runtimeAbove->label, "Runtime: Scale if above") == 0 && strcmp(runtimeAdd->label, "Runtime: Add") == 0,
	      "a light scale's six fractions have cencon's words (dialog 146), with no trailing \" :\"");
	Check(lightMode->label[0] == 0 && spare1->label[0] == 0 && spare2->label[0] == 0 && spare3->label[0] == 0,
	      "the mode and the spares have no label, so the panel skips them and keeps their bytes as they are");
}

int main()
{
	TablesHoldTogether();
	LayoutsAreTheGames();
	SizesAreTheGames();
	ValuesAreChecked();
	ChoicesAreTheGames();
	EchoRulesAreTheGames();
	SettersWriteOnlyTheirPart();
	NewChunksAreTheLibrarys();
	StockLevelsAreAllowed();
	LightSetChunksAreTheGames();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
