// The test for src/model's AI tables and rules (the AI chunks: the patrol paths so far). Written 2026-11-14 by Melanikus for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds payloads in memory, some of them hostile, and checks what the model says
// about them. The program gives back a non-zero result if any check failed. Each check's words say what it proves.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "ai_fields.h"
#include "field_model.h"
#include "name_model.h"

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

// Appends a whole number to a payload, least significant byte first, as the game writes it on the PC.
static void Add(std::vector<uint8_t> &bytes, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.push_back((uint8_t)((bits >> (8 * i)) & 0xFF));
	}
}

// Puts a whole number at a place in a payload that's already there.
static void PutInt(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

// The whole number at a place in a payload.
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

// Whether two payloads are the same except in the bytes from "from" up to (not including) "to".
static bool ChangedOnly(std::vector<uint8_t> const &before, std::vector<uint8_t> const &after, size_t from, size_t to)
{
	if (before.size() != after.size())
	{
		return false;
	}

	for (size_t i = 0; i < before.size(); i++)
	{
		if ((i < from || i >= to) && before[i] != after[i])
		{
			return false;
		}
	}

	return true;
}

// The bytes of some text, with no zero after it.
static std::vector<uint8_t> Text(char const *text)
{
	std::vector<uint8_t> bytes;

	for (char const *letter = text; *letter; letter++)
	{
		bytes.push_back((uint8_t)*letter);
	}

	return bytes;
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

// The words of a named bit, or "" when the bit has no name.
static char const *BitLabel(char const *chunk, char const *part, char const *field, uint32_t bit)
{
	for (size_t i = 0; i < AiBitCount; i++)
	{
		if (strcmp(AiBits[i].chunk, chunk) == 0 && strcmp(AiBits[i].part, part) == 0 && strcmp(AiBits[i].field, field) == 0 &&
		    AiBits[i].bit == bit)
		{
			return AiBits[i].label;
		}
	}

	return "";
}

// ---- The patrol path ----

// One point of a path, as the game writes it (ChunkPathPoint, avp/win95/path_chunk.hpp:14-19).
struct TestPoint
{
	int32_t module;
	int32_t x, y, z;
	int32_t flags;
	int32_t spare2;
};

// A path's payload as the game's writer lays it out (avp/win95/path_chunk.cpp:64-88): the name and its zero, then zeros up to a multiple
// of 4 bytes, then PathID, flags, spare2, the count of points and the points.
static std::vector<uint8_t> PathBytes(char const *name, int32_t number, int32_t flags, int32_t spare2, std::vector<TestPoint> const &points)
{
	std::vector<uint8_t> bytes = Text(name);
	bytes.push_back(0);

	while (bytes.size() % 4 != 0)
	{
		bytes.push_back(0);
	}

	Add(bytes, number);
	Add(bytes, flags);
	Add(bytes, spare2);
	Add(bytes, (int32_t)points.size());

	for (size_t i = 0; i < points.size(); i++)
	{
		Add(bytes, points[i].module);
		Add(bytes, points[i].x);
		Add(bytes, points[i].y);
		Add(bytes, points[i].z);
		Add(bytes, points[i].flags);
		Add(bytes, points[i].spare2);
	}

	return bytes;
}

// Two points: module 7 at 1, 2, 3 with flags 3 and spare2 9; module 8 at 10, 20, 30 with no flags and spare2 0.
static std::vector<TestPoint> TwoPoints()
{
	TestPoint const first = { 7, 1, 2, 3, 3, 9 };
	TestPoint const second = { 8, 10, 20, 30, 0, 0 };
	std::vector<TestPoint> points;

	points.push_back(first);
	points.push_back(second);
	return points;
}

// A path's payload written out by hand from the reader's layout (avp/win95/path_chunk.cpp:26-55): the name "ab", its zero and one byte
// of padding; PathID 5; flags 2; spare2 0; a count of 1; and one point.
static uint8_t const SamplePath[] =
{
	0x61, 0x62, 0x00, 0x00,                                                  // "ab", its zero, and one byte of padding: (2 + 4) & ~3 = 4
	0x05, 0x00, 0x00, 0x00,                                                  // PathID 5
	0x02, 0x00, 0x00, 0x00,                                                  // flags 2: back and forth
	0x00, 0x00, 0x00, 0x00,                                                  // spare2 0
	0x01, 0x00, 0x00, 0x00,                                                  // the count: 1 point
	0x07, 0x00, 0x00, 0x00,                                                  // the point's module_index 7
	0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  // its location 1, 2, 3
	0x03, 0x00, 0x00, 0x00,                                                  // its flags 3
	0x09, 0x00, 0x00, 0x00,                                                  // its spare2 9
};

static std::vector<uint8_t> Sample()
{
	return std::vector<uint8_t>(SamplePath, SamplePath + sizeof(SamplePath));
}

static SizeCheck PathCheck(std::vector<uint8_t> const &bytes, PathParts *parts = 0)
{
	return CheckPathSize(bytes.data(), bytes.size(), parts);
}

struct ExpectedField
{
	char const *name;
	unsigned offset;
	Storage storage;
};

static void TablesHoldTogether()
{
	printf("The tables hold together:\n");

	std::set<std::string> ids;
	std::set<std::string> partKeys;
	bool unique = true, readers = true, parts = true, names = true, starts = true, inside = true, apart = true, masks = true;
	bool limits = true, whys = true, creations = true, plain = true, bitsBelong = true, bitsCover = true;

	for (size_t c = 0; c < AiChunkCount; c++)
	{
		AiChunk const &chunk = AiChunks[c];

		unique = ids.insert(chunk.id).second && unique;
		// An id is 8 letters, except MODZONE, which the game names with 7 and the zero after them.
		readers = (strlen(chunk.id) == 8 || strcmp(chunk.id, "MODZONE") == 0) && chunk.label[0] != 0 && NamesAFileAndLine(chunk.reader) &&
		          readers;
	}

	for (size_t p = 0; p < AiPartCount; p++)
	{
		AiPart const &part = AiParts[p];

		parts = FindAiChunk(part.chunk) != 0 && partKeys.insert(std::string(part.chunk) + "/" + part.label).second && part.label[0] != 0 &&
		        NamesAFileAndLine(part.reader) && part.bytes > 0 && part.fieldCount > 0 && parts;

		std::set<std::string> fieldNames;

		for (size_t f = 0; f < part.fieldCount; f++)
		{
			Field const &field = part.fields[f];
			double lowest = 0, highest = 0;
			StorageRange(field.storage, &lowest, &highest);

			names = fieldNames.insert(field.name).second && names;
			starts = field.anchor == Anchor::Start && starts;
			inside = field.offset + StorageBytes(field.storage) <= part.bytes && inside;
			masks = (field.mask == 0 || field.storage == Storage::UInt32) && (field.unit != Unit::Flags || field.mask != 0) && masks;
			whys = (field.limit == Limit::None) == (field.why[0] == 0) && field.source[0] != 0 && whys;
			creations = (!field.hasCreation || ValueIsAllowed(field, field.creation, 0)) && creations;
			plain = field.unit != Unit::Choice && field.unit != Unit::ListCount && plain;

			if (field.limit != Limit::None)
			{
				limits = field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest && limits;
			}

			// No two fields of a part share a byte.
			for (size_t g = f + 1; g < part.fieldCount; g++)
			{
				unsigned const aFrom = field.offset;
				unsigned const aTo = field.offset + StorageBytes(field.storage);
				unsigned const bFrom = part.fields[g].offset;
				unsigned const bTo = part.fields[g].offset + StorageBytes(part.fields[g].storage);

				apart = (aTo <= bFrom || bTo <= aFrom) && apart;
			}
		}
	}

	for (size_t i = 0; i < AiBitCount; i++)
	{
		AiPart const *const part = FindAiPart(AiBits[i].chunk, AiBits[i].part);
		Field const *const field = part ? FindAiField(*part, AiBits[i].field) : 0;
		uint32_t const bit = AiBits[i].bit;

		bitsBelong = field && field->unit == Unit::Flags && bit != 0 && (bit & (bit - 1)) == 0 && (bit & field->mask) == bit &&
		             AiBits[i].label[0] != 0 && bitsBelong;
	}

	// Every bit a mapper may change in a flags field has a name, so a panel can offer all of them.
	for (size_t p = 0; p < AiPartCount; p++)
	{
		for (size_t f = 0; f < AiParts[p].fieldCount; f++)
		{
			Field const &field = AiParts[p].fields[f];

			if (field.unit == Unit::Flags)
			{
				uint32_t named = 0;

				for (size_t i = 0; i < AiBitCount; i++)
				{
					if (strcmp(AiBits[i].chunk, AiParts[p].chunk) == 0 && strcmp(AiBits[i].part, AiParts[p].label) == 0 &&
					    strcmp(AiBits[i].field, field.name) == 0)
					{
						named |= AiBits[i].bit;
					}
				}

				bitsCover = named == field.mask && bitsCover;
			}
		}
	}

	Check(AiChunkCount > 0 && unique, "each AI chunk is in the table once");
	Check(readers, "every chunk is an 8-letter name (MODZONE has 7), has words, and names the game's reader as a file and line");
	Check(parts, "every part belongs to a chunk in the table, is in it once, has words, fields, a size, and names the game's reader as a file and line");
	Check(names, "no part has two fields of one name");
	Check(starts, "every field's anchor is Anchor::Start: its offset is counted from its part's origin");
	Check(inside, "every field lies inside its part's bytes");
	Check(apart, "no two fields of a part share a byte");
	Check(masks, "a masked field is stored unsigned, and every flags field says which bits a mapper may change");
	Check(limits, "every limit is a range its storage can hold");
	Check(whys, "a field with a limit says why, and only those do; every field says where its facts come from");
	Check(creations, "every starting value a table claims is one its field allows");
	Check(plain, "no field is a choice or a list's count: the AI parts have none (a chunk keeps its lists' counts itself)");
	Check(bitsBelong, "every named bit is one bit of a flags field, among the bits a mapper may change");
	Check(bitsCover, "every bit a mapper may change in a flags field has a name");
}

static void PathIsLaidOutAsTheGameReadsIt()
{
	printf("A patrol path is laid out as the game's reader reads it (avp/win95/path_chunk.cpp:26-55):\n");

	AiChunk const *const chunk = FindAiChunk("AVPPATH2");
	AiPart const *const head = FindAiPart("AVPPATH2", "path");
	AiPart const *const point = FindAiPart("AVPPATH2", "path point");

	if (!chunk || !head || !point)
	{
		Check(false, "the path chunk and its two parts are in the tables");
		return;
	}

	Check(chunk->parent[0] == 0 && chunk->leastBytes == 0, "the game registers a path to be read anywhere, with no smallest size (chunk.hpp:633)");
	Check(!head->item && head->origin == AiOrigin::AfterName && head->bytes == 16 && head->fieldCount == 3,
	      "a path's three numbers are counted from the end of its name, and with the count of its points take 16 bytes");
	Check(point->item && point->origin == AiOrigin::RecordStart && point->bytes == 24 && point->fieldCount == 4,
	      "a point is 24 bytes, its four fields counted from its own start");

	// Where the reader finds each number. From the end of the name: PathID, flags, spare2 (path_chunk.cpp:34, :42, :43). In a point:
	// module_index, location, flags, spare2 (ChunkPathPoint, path_chunk.hpp:14-19).
	static ExpectedField const headPlaces[] =
	{
		{ "PathID", 0, Storage::Int32 }, { "flags", 4, Storage::UInt32 }, { "spare2", 8, Storage::Int32 },
	};
	static ExpectedField const pointPlaces[] =
	{
		{ "module_index", 0, Storage::Int32 }, { "location", 4, Storage::IntTriple }, { "flags", 16, Storage::UInt32 },
		{ "spare2", 20, Storage::Int32 },
	};
	bool headRight = true, pointRight = true;

	for (size_t i = 0; i < sizeof(headPlaces) / sizeof(headPlaces[0]); i++)
	{
		Field const *const field = FindAiField(*head, headPlaces[i].name);
		headRight = field && field->offset == headPlaces[i].offset && field->storage == headPlaces[i].storage && headRight;
	}

	for (size_t i = 0; i < sizeof(pointPlaces) / sizeof(pointPlaces[0]); i++)
	{
		Field const *const field = FindAiField(*point, pointPlaces[i].name);
		pointRight = field && field->offset == pointPlaces[i].offset && field->storage == pointPlaces[i].storage && pointRight;
	}

	Check(headRight, "a path's PathID is at +0, flags at +4 and spare2 at +8, as the reader reads them");
	Check(pointRight, "a point's module_index is at +0, location at +4, flags at +16 and spare2 at +20");

	if (!headRight || !pointRight)
	{
		return;
	}

	// The hand-written bytes, read through the tables.
	std::vector<uint8_t> const bytes = Sample();
	PathParts parts = PathParts();

	Check(PathCheck(bytes, &parts) == SizeCheck::Right, "the hand-written path is the right size");
	Check(parts.nameLength == 2 && parts.headAt == 4 && parts.countAt == 16 && parts.pointCount == 1 && parts.pointsAt == 20,
	      "its parts are where the reader finds them: a name of 2 letters, the numbers at 4, the count at 16, one point at 20");

	Field const *const number = FindAiField(*head, "PathID");
	Field const *const flags = FindAiField(*head, "flags");
	Field const *const spare = FindAiField(*head, "spare2");
	Field const *const module = FindAiField(*point, "module_index");
	Field const *const location = FindAiField(*point, "location");
	Field const *const pointFlags = FindAiField(*point, "flags");
	Field const *const pointSpare = FindAiField(*point, "spare2");
	double value = -1;
	char const *why = 0;
	size_t origin = 0;
	int32_t x = 0, y = 0, z = 0;

	Check(ReadAiField(*head, *number, bytes.data(), bytes.size(), parts.headAt, &value, &why) && value == 5, "the path's PathID reads as 5");
	Check(ReadAiField(*head, *flags, bytes.data(), bytes.size(), parts.headAt, &value, &why) && value == 2, "its flags read as 2");
	Check(ReadAiField(*head, *spare, bytes.data(), bytes.size(), parts.headAt, &value, &why) && value == 0, "its spare2 reads as 0");
	Check(PathPointOrigin(parts, 0, &origin) && origin == 20, "point 0 starts at 20");
	Check(ReadAiField(*point, *module, bytes.data(), bytes.size(), origin, &value, &why) && value == 7, "the point's module_index reads as 7");
	Check(ReadAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin, 0, &x, &why) &&
	      ReadAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin, 1, &y, &why) &&
	      ReadAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin, 2, &z, &why) && x == 1 && y == 2 && z == 3,
	      "its location reads as 1, 2, 3");
	Check(!ReadAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin, 3, &x, &why) &&
	      !ReadAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin, -1, &x, &why) && x == 1,
	      "a position has three axes, 0, 1 and 2: axes 3 and -1 aren't read");
	Check(ReadAiField(*point, *pointFlags, bytes.data(), bytes.size(), origin, &value, &why) && value == 3, "its flags read as 3");
	Check(ReadAiField(*point, *pointSpare, bytes.data(), bytes.size(), origin, &value, &why) && value == 9, "its spare2 reads as 9");

	std::vector<TestPoint> onePoint;
	TestPoint const only = { 7, 1, 2, 3, 3, 9 };
	onePoint.push_back(only);
	Check(PathBytes("ab", 5, 2, 0, onePoint) == bytes, "the test's own writer lays the same path out in the same 44 bytes");

	// A masked field reads after its mask: the bits the editor has no name for are not part of the value.
	std::vector<uint8_t> wide = bytes;
	PutInt(wide, 8, 7);
	Check(ReadAiField(*head, *flags, wide.data(), wide.size(), parts.headAt, &value, &why) && value == 3,
	      "a path's flags read after their mask: 7 in the file reads as 3");

	// A whole number below zero reads as below zero: the file keeps it in two's complement.
	std::vector<uint8_t> negative = bytes;
	PutInt(negative, origin, -5);
	Check(ReadAiField(*point, *module, negative.data(), negative.size(), origin, &value, &why) && value == -5,
	      "a negative module_index reads as negative: -5 in the file is -5, not a number past 4 billion");

	value = 123;
	why = 0;
	Check(!ReadAiField(*point, *number, bytes.data(), bytes.size(), origin, &value, &why) && why == NotThisPartsReason && value == 123,
	      "a field of another part isn't read at its own offset: it's refused with its reason, and the value is left alone");
	why = 0;
	Check(!ReadAiField(*point, *location, bytes.data(), bytes.size(), origin, &value, &why) && value == 123,
	      "a field of three numbers isn't read as one number");
	Check(!ReadAiField(*head, *number, bytes.data(), 6, parts.headAt, &value, &why) && value == 123,
	      "a field that runs past the end of a short payload isn't read");
	Check(!ReadAiField(*head, *flags, bytes.data(), bytes.size(), (size_t)-1, &value, &why) && value == 123,
	      "a part that starts at the largest place there is isn't read: the field's offset can't wrap the sum round to a small place");
	Check(!ReadAiField(*head, *number, bytes.data(), bytes.size(), bytes.size() + 1, &value, &why) && value == 123,
	      "a part that starts past the end isn't read, and the value is left alone");
}

static void PathSizesAreTheGames()
{
	printf("A path's size is checked as the game's reader checks it:\n");

	PathParts parts = PathParts();
	std::vector<TestPoint> none;
	std::vector<uint8_t> const zeros4(4, 0);
	std::vector<uint8_t> const zeros16(16, 0);
	std::vector<uint8_t> const zeros19(19, 0);
	std::vector<uint8_t> const zeros20(20, 0);
	std::vector<uint8_t> abc = Text("abc");
	std::vector<uint8_t> abcd = Text("abcd");

	abc.push_back(0);
	abcd.push_back(0);

	Check(PathCheck(std::vector<uint8_t>()) == SizeCheck::NameCutOff, "an empty payload has no name, so the game refuses it");
	Check(PathCheck(Text("ab")) == SizeCheck::NameCutOff, "a name with no ending zero inside the payload is refused");
	Check(PathCheck(zeros4) == SizeCheck::TooShort, "a name and nothing after it can't hold the three numbers: refused as too short");
	Check(PathCheck(abc) == SizeCheck::TooShort, "a name that fills the payload to its zero is refused as too short");
	Check(PathCheck(abcd) == SizeCheck::TooShort, "a name whose padding would run past the end is refused as too short");
	Check(PathCheck(std::vector<uint8_t>(12, 0)) == SizeCheck::TooShort && PathCheck(std::vector<uint8_t>(15, 0)) == SizeCheck::TooShort,
	      "a name and only 8 to 11 bytes of numbers are fewer than the three the game reads one after another: refused as too short");
	Check(PathCheck(zeros16) == SizeCheck::CountCutOff, "the three numbers with no room for the count are refused");
	Check(PathCheck(zeros19) == SizeCheck::CountCutOff, "3 bytes of a count is no count: refused");
	Check(PathCheck(zeros20, &parts) == SizeCheck::Right && parts.nameLength == 0 && parts.headAt == 4 && parts.countAt == 16 &&
	      parts.pointCount == 0 && parts.pointsAt == 20,
	      "a path with no name and no points is 20 bytes, and is read");

	std::vector<uint8_t> const named = PathBytes("abcd", 5, 2, 0, none);
	Check(PathCheck(named, &parts) == SizeCheck::Right && parts.nameLength == 4 && parts.headAt == 8 && parts.countAt == 20 &&
	      parts.pointsAt == 24 && named.size() == 24,
	      "a name of 4 letters takes 8 bytes with its zero and padding: the numbers start at 8");
	Check(PathCheck(PathBytes("", 5, 2, 0, none), &parts) == SizeCheck::Right && parts.nameLength == 0 && parts.headAt == 4,
	      "a path may have no name at all: its zero and padding take 4 bytes");

	std::string const longName(300, 'x');
	Check(PathCheck(PathBytes(longName.c_str(), 5, 2, 0, none), &parts) == SizeCheck::Right && parts.nameLength == 300 && parts.headAt == 304,
	      "the game puts no limit on a name's length: 300 letters are read, and the numbers start at 304");

	std::vector<uint8_t> padded = Sample();
	padded.at(3) = 0x7F;
	Check(PathCheck(padded, &parts) == SizeCheck::Right && parts.nameLength == 2 && parts.headAt == 4,
	      "the padding after a name's zero may hold anything: the reader never looks at it");

	std::vector<uint8_t> hostile = Sample();
	PutInt(hostile, 16, -1);
	Check(PathCheck(hostile) == SizeCheck::CountNegative, "a count below zero is refused");
	PutInt(hostile, 16, INT32_MIN);
	Check(PathCheck(hostile) == SizeCheck::CountNegative, "the most negative count is refused too");
	PutInt(hostile, 16, 2);
	Check(PathCheck(hostile) == SizeCheck::CountTooBig, "a count of 2 with room for 1 point is refused as too big");
	PutInt(hostile, 16, INT32_MAX);
	Check(PathCheck(hostile) == SizeCheck::CountTooBig, "the biggest count there is is refused as too big");
	PutInt(hostile, 16, 178956971);
	Check(PathCheck(hostile) == SizeCheck::CountTooBig,
	      "a count of 178,956,971 (24 times it is 4,294,967,304, which wraps round to 8 in 32 bits) is refused: the sum doesn't wrap");

	std::vector<uint8_t> cut = Sample();
	cut.pop_back();
	Check(PathCheck(cut) == SizeCheck::CountTooBig, "a count of 1 with one byte too few for the point is refused");

	std::vector<uint8_t> twoBytes = PathBytes("ab", 5, 2, 0, TwoPoints());
	PathParts twoParts = PathParts();
	size_t origin = 0;

	Check(PathCheck(twoBytes, &twoParts) == SizeCheck::Right && twoParts.pointCount == 2 && twoBytes.size() == 68,
	      "a count of 2 with room for exactly 2 points is the right size");
	twoBytes.pop_back();
	Check(PathCheck(twoBytes) == SizeCheck::CountTooBig, "the same path one byte short is refused as too big");

	std::vector<uint8_t> longer = Sample();

	for (int i = 0; i < 5; i++)
	{
		longer.push_back(0xAA);
	}

	Check(PathCheck(longer, &parts) == SizeCheck::Right && parts.pointCount == 1, "bytes after the last point are ignored, as the game ignores them");

	Check(PathPointOrigin(twoParts, 0, &origin) && origin == 20 && PathPointOrigin(twoParts, 1, &origin) && origin == 44 &&
	      !PathPointOrigin(twoParts, 2, &origin),
	      "point 0 starts at 20, point 1 at 44, and there is no point 2");

	bool words = true;
	const SizeCheck all[] = { SizeCheck::Right, SizeCheck::TooShort, SizeCheck::CountCutOff, SizeCheck::CountNegative,
	                          SizeCheck::CountTooBig, SizeCheck::WrongSize, SizeCheck::NameCutOff };

	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
	{
		words = AiSizeCheckWords(all[i])[0] != 0 && words;

		for (size_t j = i + 1; j < sizeof(all) / sizeof(all[0]); j++)
		{
			words = strcmp(AiSizeCheckWords(all[i]), AiSizeCheckWords(all[j])) != 0 && words;
		}
	}

	Check(words, "every way a chunk can be refused has words of its own");
}

static void EveryLimitRefusesJustOutside()
{
	printf("Every limit refuses the value just outside it, and every field refuses what isn't a number:\n");

	bool outside = true, edges = true, notNumbers = true;
	double const notANumber = std::numeric_limits<double>::quiet_NaN();
	double const endless = std::numeric_limits<double>::infinity();

	for (size_t p = 0; p < AiPartCount; p++)
	{
		for (size_t f = 0; f < AiParts[p].fieldCount; f++)
		{
			Field const &field = AiParts[p].fields[f];
			double storageLowest = 0, storageHighest = 0;
			StorageRange(field.storage, &storageLowest, &storageHighest);

			notNumbers = Verdict(field, notANumber) == NotANumberReason && Verdict(field, endless) == NotANumberReason &&
			             Verdict(field, -endless) == NotANumberReason && notNumbers;

			if (field.limit != Limit::None)
			{
				// Just outside the limit the field's own reason is given, unless the file's own storage ends first: then it's that.
				char const *belowReason = field.why;
				char const *aboveReason = field.why;

				if (field.lowest - 1 < storageLowest)
				{
					belowReason = DoesNotFitReason;
				}

				if (field.highest + 1 > storageHighest)
				{
					aboveReason = DoesNotFitReason;
				}

				outside = Verdict(field, field.lowest - 1) == belowReason && Verdict(field, field.highest + 1) == aboveReason && outside;
				edges = (field.mask != 0 || (strcmp(Verdict(field, field.lowest), "") == 0 && strcmp(Verdict(field, field.highest), "") == 0)) &&
				        edges;
			}
		}
	}

	Check(notNumbers, "every field refuses NaN and infinity, either way");
	Check(outside, "every limit refuses the whole number just below it and just above it, with the field's own reason");
	Check(edges, "every limit allows its own two ends");
}

static void PathValuesAreChecked()
{
	printf("A path's values are checked:\n");

	AiPart const *const head = FindAiPart("AVPPATH2", "path");
	AiPart const *const point = FindAiPart("AVPPATH2", "path point");
	Field const *const number = head ? FindAiField(*head, "PathID") : 0;
	Field const *const flags = head ? FindAiField(*head, "flags") : 0;
	Field const *const module = point ? FindAiField(*point, "module_index") : 0;
	Field const *const location = point ? FindAiField(*point, "location") : 0;
	Field const *const pointFlags = point ? FindAiField(*point, "flags") : 0;

	if (!number || !flags || !module || !location || !pointFlags)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(Verdict(*number, -1) == number->why && strcmp(Verdict(*number, 0), "") == 0 && strcmp(Verdict(*number, 255), "") == 0 &&
	      Verdict(*number, 256) == number->why,
	      "a path's number is 0-255: the game refuses a level with a path outside that (path_chunk.cpp:37)");
	Check(Verdict(*number, 1.5) == NotWholeReason, "a path's number isn't a fraction");
	Check(Verdict(*number, 4294967296.0) == DoesNotFitReason, "a number too big for the 32 bits the file keeps is refused before its range");
	Check(strcmp(Verdict(*flags, 0), "") == 0 && strcmp(Verdict(*flags, 3), "") == 0 && Verdict(*flags, 4) == DoesNotFitReason &&
	      Verdict(*flags, -1) == DoesNotFitReason,
	      "a path's flags take only its two bits: loop and back and forth");
	Check(strcmp(Verdict(*module, -2147483648.0), "") == 0 && strcmp(Verdict(*module, 2147483647.0), "") == 0 &&
	      Verdict(*module, 2147483648.0) == DoesNotFitReason,
	      "a point's module index takes any whole number: the game finds the object by it, and passes over one that isn't there");
	Check(strcmp(Verdict(*location, 16777216), "") == 0 && strcmp(Verdict(*location, -16777216), "") == 0 &&
	      Verdict(*location, 16777217) == location->why && Verdict(*location, -16777217) == location->why,
	      "a point's position is kept within 2^24 of the level's middle on each axis");
	Check(number->limit == Limit::Loader && location->limit == Limit::Loader,
	      "a path's number and a point's place are limits the game's loader has itself (path_chunk.cpp:37 and :52), not only the editor's");
	Check(strcmp(Verdict(*pointFlags, 3), "") == 0 && Verdict(*pointFlags, 4) == DoesNotFitReason, "a point's flags take only its two bits");
}

static void PathSettersWriteOnlyTheirPart()
{
	printf("Setters write only their own part:\n");

	AiPart const *const head = FindAiPart("AVPPATH2", "path");
	AiPart const *const point = FindAiPart("AVPPATH2", "path point");
	Field const *const number = head ? FindAiField(*head, "PathID") : 0;
	Field const *const flags = head ? FindAiField(*head, "flags") : 0;
	Field const *const location = point ? FindAiField(*point, "location") : 0;
	Field const *const pointSpare = point ? FindAiField(*point, "spare2") : 0;

	if (!number || !flags || !location || !pointSpare)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	// A path with a name of 4 letters, so the numbers start at 8, and two points, at 24 and 48.
	std::vector<uint8_t> bytes = PathBytes("abcd", 5, 2, 0, TwoPoints());
	PathParts parts = PathParts();
	size_t origin0 = 0, origin1 = 0;

	if (CheckPathSize(bytes.data(), bytes.size(), &parts) != SizeCheck::Right || !PathPointOrigin(parts, 0, &origin0) ||
	    !PathPointOrigin(parts, 1, &origin1))
	{
		Check(false, "the path these checks use is the right size");
		return;
	}

	Check(parts.headAt == 8 && origin0 == 24 && origin1 == 48 && bytes.size() == 72,
	      "the path these checks use has its numbers at 8 and its points at 24 and 48, in 72 bytes");

	char const *why = 0;
	std::vector<uint8_t> before = bytes;

	Check(SetAiField(*head, *number, bytes.data(), bytes.size(), parts.headAt, 200, &why) && GetInt(bytes, 8) == 200 &&
	      ChangedOnly(before, bytes, 8, 12),
	      "setting a path's number changes that number alone, where the reader finds it after a name of 4 letters");

	PutInt(bytes, 12, INT32_MIN + 2);   // a bit the editor has no name for, as a file may have
	before = bytes;
	Check(SetAiField(*head, *flags, bytes.data(), bytes.size(), parts.headAt, 1, &why) && GetInt(bytes, 12) == INT32_MIN + 1 &&
	      ChangedOnly(before, bytes, 12, 16),
	      "setting a path's flags changes only the two bits they own, and keeps the bit the editor has no name for");

	before = bytes;
	Check(SetAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin1, 1, 5000, &why) && GetInt(bytes, 56) == 5000 &&
	      ChangedOnly(before, bytes, 56, 60),
	      "setting one axis of the second point's position changes that axis alone");

	before = bytes;
	Check(SetAiField(*point, *pointSpare, bytes.data(), bytes.size(), origin0, 77, &why) && GetInt(bytes, 44) == 77 &&
	      ChangedOnly(before, bytes, 44, 48),
	      "setting the first point's spare2 changes that number alone");

	before = bytes;
	why = 0;
	Check(!SetAiField(*head, *number, bytes.data(), bytes.size(), parts.headAt, 256, &why) && why == number->why && bytes == before,
	      "a path number the game refuses is refused, with the field's own reason, and nothing changes");
	why = 0;
	Check(!SetAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin0, 0, 16777217, &why) && why == location->why &&
	      bytes == before,
	      "a position past 2^24 is refused, with the field's own reason, and nothing changes");
	why = 0;
	Check(!SetAiField(*point, *number, bytes.data(), bytes.size(), origin0, 7, &why) && why == NotThisPartsReason && bytes == before,
	      "a field of another part is refused, rather than written at its own offset in this one");
	Check(!SetAiField(*point, *location, bytes.data(), bytes.size(), origin0, 1, &why) && bytes == before,
	      "a position can't be set from one number");
	Check(!SetAiFieldAxis(*point, *location, bytes.data(), bytes.size(), origin0, 3, 1, &why) && bytes == before,
	      "a position has three axes, 0, 1 and 2: axis 3 is refused");
	Check(!SetAiField(*head, *number, bytes.data(), 10, parts.headAt, 7, &why) && bytes == before,
	      "a field that runs past the end of a short payload is refused, and nothing is written");
	Check(!SetAiField(*head, *number, bytes.data(), bytes.size(), bytes.size() + 1, 7, &why) && bytes == before,
	      "a part that starts past the end is refused, and nothing is written");
	Check(!SetAiField(*head, *flags, bytes.data(), bytes.size(), (size_t)-1, 1, &why) && bytes == before,
	      "a part that starts at the largest place there is is refused: the sum can't wrap round to a small place");
}

static void PathNumbersAreUnique()
{
	printf("Each path has a number of its own:\n");

	AiPart const *const head = FindAiPart("AVPPATH2", "path");
	Field const *const number = head ? FindAiField(*head, "PathID") : 0;

	if (!number)
	{
		Check(false, "the field these checks use is in the tables");
		return;
	}

	char const *why = 0;
	int32_t const others[] = { 0, 1, 3 };

	Check(PathNumberIsAllowed(0, 0, 0, &why) && PathNumberIsAllowed(255, 0, 0, &why),
	      "with no other path, every number from 0 to 255 is free");
	why = 0;
	Check(!PathNumberIsAllowed(256, 0, 0, &why) && why == number->why && !PathNumberIsAllowed(-1, 0, 0, &why),
	      "a number outside 0-255 is refused, with the field's own reason");
	Check(PathNumberIsAllowed(2, others, 3, &why), "a number no other path has is free");
	why = 0;
	Check(!PathNumberIsAllowed(3, others, 3, &why) && why == PathNumberTakenReason && !PathNumberIsAllowed(0, others, 3, &why),
	      "a number another path has is refused: the game refuses a level where two paths share a number (environment_chunk.cpp:337)");

	std::vector<int32_t> all;

	for (int32_t n = 0; n < 256; n++)
	{
		all.push_back(n);
	}

	bool noneFree = true;

	for (int32_t n = 0; n < 256; n++)
	{
		noneFree = !PathNumberIsAllowed(n, all.data(), all.size(), 0) && noneFree;
	}

	Check(noneFree && !PathNumberIsAllowed(256, all.data(), all.size(), 0), "with all 256 numbers taken, none is free");
}

static void PathNamesAndBitsAreKnown()
{
	printf("A path's name and bits have the old editor's words:\n");

	NameKind const *const name = FindNameKind("AVPPATH2", "path name");

	Check(name && !name->required && name->most == NameMostWithNoGameLimit,
	      "the name table has a path's name, which the game can do without (the old editor's prompt: \"Name of path (optional)\")");
	Check(strcmp(BitLabel("AVPPATH2", "path", "flags", 0x1), "Loop path") == 0 &&
	      strcmp(BitLabel("AVPPATH2", "path", "flags", 0x2), "Go back and forth") == 0,
	      "the path's two flag bits are named as the old editor names them: \"Loop path\" and \"Go back and forth\"");
	Check(strcmp(BitLabel("AVPPATH2", "path point", "flags", 0x1), "Interpolated point") == 0 &&
	      strcmp(BitLabel("AVPPATH2", "path point", "flags", 0x2), "Location valid") == 0,
	      "a point's two flag bits are named as the game names them (the old editor's words for them aren't known)");
}

// ---- The waypoints ----

// A link from a waypoint to a waypoint of its own list, and a link from a waypoint to a module, as the game writes them
// (win95/chunk_waypoint.hpp:15-29).
struct TestLink
{
	int32_t index;
	int32_t flags;
};

struct TestModuleLink
{
	char const *name;
	int32_t flags;
};

// Appends one waypoint as the game's writer lays it out (win95/chunk_waypoint.cpp:273-318): lowest, highest and centre (x, y and z each),
// flags, spare2, the count of links and the links, the count of module links and each module link (its name and zero, padded with zeros
// up to a multiple of 4 bytes, then its flags).
static void AddWaypoint(std::vector<uint8_t> &bytes, int32_t const *corners, int32_t flags, std::vector<TestLink> const &links,
                        std::vector<TestModuleLink> const &moduleLinks)
{
	for (int i = 0; i < 9; i++)
	{
		Add(bytes, corners[i]);
	}

	Add(bytes, flags);
	Add(bytes, 0);
	Add(bytes, (int32_t)links.size());

	for (size_t i = 0; i < links.size(); i++)
	{
		Add(bytes, links[i].index);
		Add(bytes, links[i].flags);
	}

	Add(bytes, (int32_t)moduleLinks.size());

	for (size_t i = 0; i < moduleLinks.size(); i++)
	{
		std::vector<uint8_t> name = Text(moduleLinks[i].name);
		name.push_back(0);

		while (name.size() % 4 != 0)
		{
			name.push_back(0);
		}

		bytes.insert(bytes.end(), name.begin(), name.end());
		Add(bytes, moduleLinks[i].flags);
	}
}

// A chunk of two waypoints written out by hand from the reader's layout (win95/chunk_waypoint.cpp:45-161). Waypoint 0 starts at 4: min -10, -20,
// -30; max 10, 20, 30; centre 100, 200, 300; flags 1 (can crawl); spare2 0; one link, to waypoint 1, with flags 4; no module links.
// Waypoint 1 starts at 64: min -1, -2, -3; max 1, 2, 3; centre 7, 8, 9; flags 0; spare2 0; no links; one module link, named "ab", with flags 5.
static uint8_t const WaypointSampleBytes[] =
{
	0x02, 0x00, 0x00, 0x00,                                                  // the count: 2 waypoints
	0xF6, 0xFF, 0xFF, 0xFF, 0xEC, 0xFF, 0xFF, 0xFF, 0xE2, 0xFF, 0xFF, 0xFF,  // waypoint 0 (at 4): min -10, -20, -30
	0x0A, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,  // max 10, 20, 30
	0x64, 0x00, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00,  // centre 100, 200, 300
	0x01, 0x00, 0x00, 0x00,                                                  // flags 1: can crawl
	0x00, 0x00, 0x00, 0x00,                                                  // spare2 0
	0x01, 0x00, 0x00, 0x00,                                                  // the count of links: 1
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,                          // the link (at 52): to waypoint 1, flags 4
	0x00, 0x00, 0x00, 0x00,                                                  // the count of module links: 0
	0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF,  // waypoint 1 (at 64): min -1, -2, -3
	0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  // max 1, 2, 3
	0x07, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,  // centre 7, 8, 9
	0x00, 0x00, 0x00, 0x00,                                                  // flags 0
	0x00, 0x00, 0x00, 0x00,                                                  // spare2 0
	0x00, 0x00, 0x00, 0x00,                                                  // the count of links: 0
	0x01, 0x00, 0x00, 0x00,                                                  // the count of module links: 1
	0x61, 0x62, 0x00, 0x00,                                                  // the module link's name (at 116): "ab", its zero, 1 byte of padding
	0x05, 0x00, 0x00, 0x00,                                                  // its flags (at 120): 5
};

static std::vector<uint8_t> WaypointSample()
{
	return std::vector<uint8_t>(WaypointSampleBytes, WaypointSampleBytes + sizeof(WaypointSampleBytes));
}

// The same two waypoints, made by the test's own writer.
static std::vector<uint8_t> BuiltWaypointSample()
{
	int32_t const first[9] = { -10, -20, -30, 10, 20, 30, 100, 200, 300 };
	int32_t const second[9] = { -1, -2, -3, 1, 2, 3, 7, 8, 9 };
	TestLink const link = { 1, 4 };
	TestModuleLink const moduleLink = { "ab", 5 };
	std::vector<TestLink> links;
	std::vector<TestModuleLink> moduleLinks;
	std::vector<uint8_t> bytes;

	links.push_back(link);
	moduleLinks.push_back(moduleLink);
	Add(bytes, 2);
	AddWaypoint(bytes, first, 1, links, std::vector<TestModuleLink>());
	AddWaypoint(bytes, second, 0, std::vector<TestLink>(), moduleLinks);
	return bytes;
}

// A chunk of three waypoints with no links, 52 bytes each, with these flags words.
static std::vector<uint8_t> PlainWaypoints(int32_t flags0, int32_t flags1, int32_t flags2)
{
	int32_t const corners[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int32_t const flags[3] = { flags0, flags1, flags2 };
	std::vector<uint8_t> bytes;

	Add(bytes, 3);

	for (int i = 0; i < 3; i++)
	{
		AddWaypoint(bytes, corners, flags[i], std::vector<TestLink>(), std::vector<TestModuleLink>());
	}

	return bytes;
}

static SizeCheck WaypointCheck(std::vector<uint8_t> const &bytes, WaypointParts *parts = 0)
{
	return CheckWaypointSize(bytes.data(), bytes.size(), parts);
}

// Whether a part has exactly these fields, each at its place and stored as the reader reads it.
static bool PlacesAre(AiPart const &part, ExpectedField const *expected, size_t count)
{
	bool right = part.fieldCount == count;

	for (size_t i = 0; i < count; i++)
	{
		Field const *const field = FindAiField(part, expected[i].name);
		right = field && field->offset == expected[i].offset && field->storage == expected[i].storage && right;
	}

	return right;
}

// Reads a field of three numbers as x, y and z.
static bool ReadTriple(AiPart const &part, Field const &field, std::vector<uint8_t> const &bytes, size_t origin, int32_t *x, int32_t *y,
                       int32_t *z)
{
	char const *why = 0;

	return ReadAiFieldAxis(part, field, bytes.data(), bytes.size(), origin, 0, x, &why) &&
	       ReadAiFieldAxis(part, field, bytes.data(), bytes.size(), origin, 1, y, &why) &&
	       ReadAiFieldAxis(part, field, bytes.data(), bytes.size(), origin, 2, z, &why);
}

static void WaypointIsLaidOutAsTheGameReadsIt()
{
	printf("A waypoint chunk is laid out as the game's reader reads it (win95/chunk_waypoint.cpp:45-161):\n");

	AiChunk const *const chunk = FindAiChunk("WAYPOINT");
	AiPart const *const waypoint = FindAiPart("WAYPOINT", "waypoint");
	AiPart const *const link = FindAiPart("WAYPOINT", "waypoint link");
	AiPart const *const moduleLink = FindAiPart("WAYPOINT", "module link");

	if (!chunk || !waypoint || !link || !moduleLink)
	{
		Check(false, "the waypoint chunk and its three parts are in the tables");
		return;
	}

	// Where the reader finds each number. In a waypoint, from its start: min, max, centre, flags, spare2 (chunk_waypoint.cpp:79-91). In a
	// link, from its start: index, flags (WaypointLink, chunk_waypoint.hpp:15-19). In a module link, from the end of its name: flags (:124).
	static ExpectedField const waypointPlaces[] =
	{
		{ "min", 0, Storage::IntTriple }, { "max", 12, Storage::IntTriple }, { "centre", 24, Storage::IntTriple },
		{ "flags", 36, Storage::UInt32 }, { "spare2", 40, Storage::Int32 },
	};
	static ExpectedField const linkPlaces[] = { { "index", 0, Storage::Int32 }, { "flags", 4, Storage::UInt32 } };
	static ExpectedField const moduleLinkPlaces[] = { { "flags", 0, Storage::Int32 } };

	bool const waypointRight = waypoint->item && waypoint->origin == AiOrigin::RecordStart && waypoint->bytes == 48 &&
	                           PlacesAre(*waypoint, waypointPlaces, 5);
	bool const linkRight = link->item && link->origin == AiOrigin::RecordStart && link->bytes == 8 && PlacesAre(*link, linkPlaces, 2);
	bool const moduleLinkRight = moduleLink->item && moduleLink->origin == AiOrigin::AfterName && moduleLink->bytes == 4 &&
	                             PlacesAre(*moduleLink, moduleLinkPlaces, 1);

	Check(chunk->parent[0] == 0 && chunk->leastBytes == 0, "the game registers waypoints to be read anywhere, with no smallest size (chunk.hpp:633)");
	Check(waypointRight, "a waypoint's numbers are counted from its start, and with the count of its links take 48 bytes: min +0, max +12, centre +24, flags +36, spare2 +40");
	Check(linkRight, "a link is 8 bytes: index +0, flags +4");
	Check(moduleLinkRight, "a module link's flags come right after its name's zero and padding");

	if (!waypointRight || !linkRight || !moduleLinkRight)
	{
		return;
	}

	// The hand-written bytes, read through the tables.
	std::vector<uint8_t> const bytes = WaypointSample();
	WaypointParts parts = WaypointParts();
	WaypointPlace first = WaypointPlace();
	WaypointPlace second = WaypointPlace();
	WaypointPlace none = WaypointPlace();

	Check(WaypointCheck(bytes, &parts) == SizeCheck::Right && parts.count == 2 && parts.alienCount == 2 && parts.groundCount == 0,
	      "the hand-written chunk of two waypoints is the right size, and both are alien waypoints");
	Check(WaypointOrigin(bytes.data(), bytes.size(), 0, &first) && first.start == 4 && first.linkCount == 1 && first.linksAt == 52 &&
	      first.moduleLinkCount == 0 && first.moduleLinksAt == 64 && first.end == 64,
	      "waypoint 0 starts at 4, has 1 link, at 52, and no module links, and ends at 64");
	Check(WaypointOrigin(bytes.data(), bytes.size(), 1, &second) && second.start == 64 && second.linkCount == 0 && second.linksAt == 112 &&
	      second.moduleLinkCount == 1 && second.moduleLinksAt == 116 && second.end == 124,
	      "waypoint 1 starts at 64, has no links and 1 module link, at 116, and ends at 124");
	Check(!WaypointOrigin(bytes.data(), bytes.size(), 2, &none), "there is no waypoint 2");

	Field const *const lowest = FindAiField(*waypoint, "min");
	Field const *const highest = FindAiField(*waypoint, "max");
	Field const *const middle = FindAiField(*waypoint, "centre");
	Field const *const flags = FindAiField(*waypoint, "flags");
	Field const *const spare = FindAiField(*waypoint, "spare2");
	Field const *const linkIndex = FindAiField(*link, "index");
	Field const *const linkFlags = FindAiField(*link, "flags");
	Field const *const moduleFlags = FindAiField(*moduleLink, "flags");
	int32_t x = 0, y = 0, z = 0;
	double value = -1;
	char const *why = 0;
	size_t origin = 0;
	size_t nameAt = 0;

	Check(ReadTriple(*waypoint, *lowest, bytes, first.start, &x, &y, &z) && x == -10 && y == -20 && z == -30,
	      "waypoint 0's min reads as -10, -20, -30");
	Check(ReadTriple(*waypoint, *highest, bytes, first.start, &x, &y, &z) && x == 10 && y == 20 && z == 30, "its max reads as 10, 20, 30");
	Check(ReadTriple(*waypoint, *middle, bytes, first.start, &x, &y, &z) && x == 100 && y == 200 && z == 300,
	      "its centre reads as 100, 200, 300");
	Check(ReadAiField(*waypoint, *flags, bytes.data(), bytes.size(), first.start, &value, &why) && value == 1 &&
	      ReadAiField(*waypoint, *spare, bytes.data(), bytes.size(), first.start, &value, &why) && value == 0,
	      "its flags read as 1, can crawl, and its spare2 as 0");
	Check(ReadTriple(*waypoint, *lowest, bytes, second.start, &x, &y, &z) && x == -1 && y == -2 && z == -3,
	      "waypoint 1's min reads as -1, -2, -3");
	Check(ReadTriple(*waypoint, *highest, bytes, second.start, &x, &y, &z) && x == 1 && y == 2 && z == 3, "its max reads as 1, 2, 3");
	Check(ReadTriple(*waypoint, *middle, bytes, second.start, &x, &y, &z) && x == 7 && y == 8 && z == 9, "its centre reads as 7, 8, 9");
	Check(WaypointLinkOrigin(first, 0, &origin) && origin == 52 &&
	      ReadAiField(*link, *linkIndex, bytes.data(), bytes.size(), origin, &value, &why) && value == 1 &&
	      ReadAiField(*link, *linkFlags, bytes.data(), bytes.size(), origin, &value, &why) && value == 4,
	      "waypoint 0's link starts at 52 and leads to waypoint 1, with flags 4");
	Check(!WaypointLinkOrigin(first, 1, &origin), "waypoint 0 has no second link");
	Check(ModuleLinkOrigin(bytes.data(), bytes.size(), second, 0, &nameAt, &origin) && nameAt == 116 && origin == 120 &&
	      ReadAiField(*moduleLink, *moduleFlags, bytes.data(), bytes.size(), origin, &value, &why) && value == 5,
	      "waypoint 1's module link has its name at 116 and its flags at 120, and the flags read as 5");
	Check(!ModuleLinkOrigin(bytes.data(), bytes.size(), second, 1, &nameAt, &origin), "waypoint 1 has no second module link");
	Check(BuiltWaypointSample() == bytes, "the test's own writer lays the same two waypoints out in the same 124 bytes");
}

static void WaypointSizesAreTheGames()
{
	printf("A waypoint chunk's size is checked as the game's reader checks it:\n");

	WaypointParts parts = WaypointParts();
	std::vector<uint8_t> const sample = WaypointSample();

	Check(WaypointCheck(std::vector<uint8_t>()) == SizeCheck::CountCutOff, "an empty payload has no count of waypoints, so the game refuses it");
	Check(WaypointCheck(std::vector<uint8_t>(3, 0)) == SizeCheck::CountCutOff, "3 bytes of a count is no count: refused");
	Check(WaypointCheck(std::vector<uint8_t>(4, 0), &parts) == SizeCheck::Right && parts.count == 0 && parts.alienCount == 0 &&
	      parts.groundCount == 0,
	      "a count of 0 and nothing after it is the right size");

	std::vector<uint8_t> hostile = sample;
	PutInt(hostile, 0, -1);
	Check(WaypointCheck(hostile) == SizeCheck::CountNegative, "a count of waypoints below zero is refused");
	PutInt(hostile, 0, INT32_MIN);
	Check(WaypointCheck(hostile) == SizeCheck::CountNegative, "the most negative count is refused too");
	PutInt(hostile, 0, 3);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig, "a count of 3 waypoints with room for 2 is refused as too big");
	PutInt(hostile, 0, INT32_MAX);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig, "the biggest count there is is refused as too big");
	PutInt(hostile, 0, 82595525);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig,
	      "a count of 82,595,525 (52 times it is 4,294,967,300, which wraps round to 4 in 32 bits) is refused: the sum doesn't wrap");

	hostile = sample;
	PutInt(hostile, 48, -1);
	Check(WaypointCheck(hostile) == SizeCheck::CountNegative, "a waypoint's count of links below zero is refused");
	PutInt(hostile, 48, 1000);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig, "a count of 1000 links with room for 9 is refused as too big");
	PutInt(hostile, 48, 536870913);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig,
	      "a count of 536,870,913 links (8 times it is 4,294,967,304, which wraps round to 8 in 32 bits) is refused");
	PutInt(hostile, 48, 9);
	Check(WaypointCheck(hostile) == SizeCheck::CountCutOff,
	      "9 links fit the bytes there are but use them all up, leaving no count of module links: refused");

	hostile = sample;
	PutInt(hostile, 112, -1);
	Check(WaypointCheck(hostile) == SizeCheck::CountNegative, "a waypoint's count of module links below zero is refused");
	PutInt(hostile, 112, 100);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig, "a count of 100 module links with room for 1 is refused as too big");
	PutInt(hostile, 112, 2);
	Check(WaypointCheck(hostile) == SizeCheck::CountTooBig,
	      "a count of 2 module links with room for 8 bytes is refused as too big: each takes at least 8 (a name of 4 and its flags), not 4");

	std::vector<uint8_t> const cut(sample.begin(), sample.begin() + 115);
	Check(WaypointCheck(cut) == SizeCheck::TooShort,
	      "a waypoint with fewer than the 52 bytes every waypoint takes is refused, though the count of waypoints fits");

	std::vector<uint8_t> noZero = sample;

	for (size_t i = 116; i < 124; i++)
	{
		noZero.at(i) = (uint8_t)('a' + (i - 116));
	}

	Check(WaypointCheck(noZero) == SizeCheck::NameCutOff, "a module link's name with no ending zero inside the payload is refused");

	std::vector<uint8_t> noFlags = sample;
	noFlags.resize(116);
	Add(noFlags, 0x64636261);   // "abcd"
	Add(noFlags, 0);            // its zero, and the padding up to 8 bytes
	Check(WaypointCheck(noFlags) == SizeCheck::TooShort,
	      "a module link whose name ends the payload, with no room for its flags, is refused as too short");

	std::vector<uint8_t> longer = sample;

	for (int i = 0; i < 5; i++)
	{
		longer.push_back(0xAA);
	}

	Check(WaypointCheck(longer, &parts) == SizeCheck::Right && parts.count == 2, "bytes after the last waypoint are ignored, as the game ignores them");

	// Bytes after the last waypoint are not a waypoint, however much room they leave: nothing past the last one has an origin.
	std::vector<uint8_t> padded = sample;
	padded.resize(sample.size() + 60, 0);
	WaypointPlace place = WaypointPlace();
	WaypointPlace second = WaypointPlace();
	size_t nameAt = 0;
	size_t flagsAt = 0;

	Check(WaypointOrigin(padded.data(), padded.size(), 1, &place) && !WaypointOrigin(padded.data(), padded.size(), 2, &place) &&
	      !WaypointOrigin(padded.data(), padded.size(), (size_t)-1, &place),
	      "the waypoint after the last has no origin, nor has any number past it, though bytes with room for one follow");
	Check(WaypointOrigin(padded.data(), padded.size(), 1, &second) && ModuleLinkOrigin(padded.data(), padded.size(), second, 0, &nameAt, &flagsAt) &&
	      !ModuleLinkOrigin(padded.data(), padded.size(), second, 1, &nameAt, &flagsAt),
	      "the module link after the last has no origin, though bytes with room for one follow");
}

static void WaypointGroupsAreTheGames()
{
	printf("Waypoints are grouped, and their links and counts checked, as the game does (win95/chunk_waypoint.cpp:127-159):\n");

	WaypointParts parts = WaypointParts();
	char const *why = 0;

	Check(WaypointCheck(PlainWaypoints(0, 0, 0), &parts) == SizeCheck::Right && parts.count == 3 && parts.alienCount == 3 &&
	      parts.groundCount == 0,
	      "with no waypoint marked as the first ground one, all three are alien waypoints");
	Check(WaypointCheck(PlainWaypoints(0, 0x40000000, 0), &parts) == SizeCheck::Right && parts.alienCount == 1 && parts.groundCount == 2,
	      "the waypoint the file marks as the first ground one starts the ground list: 1 alien waypoint, 2 ground ones");
	Check(WaypointCheck(PlainWaypoints(0x40000000, 0, 0), &parts) == SizeCheck::Right && parts.alienCount == 0 && parts.groundCount == 3,
	      "marked first, all three are ground waypoints");
	Check(WaypointCheck(PlainWaypoints(0, 0x40000000, 0x40000001), &parts) == SizeCheck::Right && parts.alienCount == 2 &&
	      parts.groundCount == 1,
	      "when two waypoints are marked, the last mark wins, as in the reader (chunk_waypoint.cpp:127-131)");

	Check(WaypointLinkTargetIsAllowed(0, 2, &why) && WaypointLinkTargetIsAllowed(1, 2, &why),
	      "a link may lead to any place in its own list: 0 and 1 in a list of 2");
	why = 0;
	Check(!WaypointLinkTargetIsAllowed(2, 2, &why) && why != 0 && why[0] != 0, "a link can't lead past the end of its list, and says why");
	Check(!WaypointLinkTargetIsAllowed(-1, 2, &why), "a link can't lead to a place below 0");
	Check(!WaypointLinkTargetIsAllowed(0, 0, &why), "a link can't lead into an empty list");

	Check(WaypointGroupsFit(32767, 32767, &why), "two groups of 32,767 waypoints fit the game's 16-bit counts");
	why = 0;
	Check(!WaypointGroupsFit(32768, 0, &why) && why == WaypointGroupTooBigReason && !WaypointGroupsFit(0, 32768, &why),
	      "a group of 32,768 doesn't fit: the game's count of it would wrap round to below zero");
	Check(!WaypointGroupsFit((size_t)-1, 0, &why), "the largest count there is doesn't fit either");
}

static void WaypointRoomIsTheGames()
{
	printf("A waypoint's box has to give room, worked out by the game's own sums (waypoint_room.h:59-82):\n");

	int32_t const lowest[3] = { -100, -100, -100 };
	int32_t const highest[3] = { 100, 100, 100 };
	int32_t const centre[3] = { 0, 0, 0 };
	char const *why = 0;

	Check(WaypointBoxHasRoom(lowest, highest, centre, 1.0, &why), "a box 200 across, at scale 1, has room on every axis");

	int32_t const flatLowest[3] = { 0, -100, -100 };
	int32_t const flatHighest[3] = { 0, 100, 100 };
	Check(WaypointBoxHasRoom(flatLowest, flatHighest, centre, 1.0, &why),
	      "a box with no size on one axis, exactly at its centre, has a room of -1 there, which the game allows (nine of its own waypoints do)");

	int32_t const thinLowest[3] = { -100, 0, -100 };
	int32_t const thinHighest[3] = { 100, 1, 100 };
	why = 0;
	Check(!WaypointBoxHasRoom(thinLowest, thinHighest, centre, 1.0, &why) && why == WaypointNoRoomReason,
	      "a box 1 across on one axis has a room of 0 there: refused, with the reason");

	why = 0;
	Check(!WaypointBoxHasRoom(lowest, highest, centre, std::numeric_limits<double>::quiet_NaN(), &why) && why == WaypointNoRoomReason,
	      "a scale that isn't a number is refused");
	Check(!WaypointBoxHasRoom(lowest, highest, centre, std::numeric_limits<double>::infinity(), &why), "an endless scale is refused");

	int32_t const farCorner[3] = { 16777216, 0, 0 };
	Check(!WaypointBoxHasRoom(lowest, farCorner, centre, 1024.0, &why),
	      "a corner too big for the game's whole numbers at the level's scale is refused: 2^24 at a scale of 1024");
}

static void WaypointValuesAreChecked()
{
	printf("A waypoint's values are checked:\n");

	AiPart const *const waypoint = FindAiPart("WAYPOINT", "waypoint");
	AiPart const *const link = FindAiPart("WAYPOINT", "waypoint link");
	AiPart const *const moduleLink = FindAiPart("WAYPOINT", "module link");
	Field const *const lowest = waypoint ? FindAiField(*waypoint, "min") : 0;
	Field const *const highest = waypoint ? FindAiField(*waypoint, "max") : 0;
	Field const *const middle = waypoint ? FindAiField(*waypoint, "centre") : 0;
	Field const *const flags = waypoint ? FindAiField(*waypoint, "flags") : 0;
	Field const *const linkIndex = link ? FindAiField(*link, "index") : 0;
	Field const *const linkFlags = link ? FindAiField(*link, "flags") : 0;
	Field const *const moduleFlags = moduleLink ? FindAiField(*moduleLink, "flags") : 0;

	if (!lowest || !highest || !middle || !flags || !linkIndex || !linkFlags || !moduleFlags)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Field const *const corners[3] = { lowest, highest, middle };
	bool places = true;

	for (int i = 0; i < 3; i++)
	{
		places = strcmp(Verdict(*corners[i], 16777216), "") == 0 && strcmp(Verdict(*corners[i], -16777216), "") == 0 &&
		         Verdict(*corners[i], 16777217) == corners[i]->why && Verdict(*corners[i], -16777217) == corners[i]->why && places;
	}

	Check(places, "a waypoint's min, max and centre are each kept within 2^24 on each axis: the game refuses a level with one farther out");
	Check(lowest->limit == Limit::Loader && highest->limit == Limit::Loader && middle->limit == Limit::Loader && linkIndex->limit == Limit::Loader,
	      "a corner's place and a link's index are limits the game's loader has itself (chunk_waypoint.cpp:84-86 and :154), not only the editor's");
	Check(strcmp(Verdict(*flags, 0), "") == 0 && strcmp(Verdict(*flags, 1), "") == 0 && Verdict(*flags, 2) == DoesNotFitReason &&
	      Verdict(*flags, 1073741824.0) == DoesNotFitReason && Verdict(*flags, 2147483648.0) == DoesNotFitReason,
	      "a waypoint's flags take only its one bit, can crawl: the first-ground and centre bits aren't a mapper's choice");
	Check(Verdict(*linkIndex, -1) == linkIndex->why && strcmp(Verdict(*linkIndex, 0), "") == 0 &&
	      strcmp(Verdict(*linkIndex, 2147483647.0), "") == 0,
	      "a link's place in its list can't be below 0 (the game refuses it); the list's size is WaypointLinkTargetIsAllowed's to check");
	Check(strcmp(Verdict(*linkFlags, 15), "") == 0 && Verdict(*linkFlags, 16) == DoesNotFitReason,
	      "a link's flags take only its four bits: one way, one way reversed, aliens only, large target");
	Check(strcmp(Verdict(*moduleFlags, -2147483648.0), "") == 0 && strcmp(Verdict(*moduleFlags, 2147483647.0), "") == 0 &&
	      Verdict(*moduleFlags, 2147483648.0) == DoesNotFitReason,
	      "a module link's flags take any whole number: the game never uses them");
}

static void WaypointSettersWriteOnlyTheirPart()
{
	printf("Setters write only their own part:\n");

	AiPart const *const waypoint = FindAiPart("WAYPOINT", "waypoint");
	AiPart const *const link = FindAiPart("WAYPOINT", "waypoint link");
	AiPart const *const moduleLink = FindAiPart("WAYPOINT", "module link");
	Field const *const middle = waypoint ? FindAiField(*waypoint, "centre") : 0;
	Field const *const flags = waypoint ? FindAiField(*waypoint, "flags") : 0;
	Field const *const linkIndex = link ? FindAiField(*link, "index") : 0;
	Field const *const moduleFlags = moduleLink ? FindAiField(*moduleLink, "flags") : 0;

	if (!middle || !flags || !linkIndex || !moduleFlags)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	std::vector<uint8_t> bytes = WaypointSample();
	WaypointPlace first = WaypointPlace();
	WaypointPlace second = WaypointPlace();
	size_t linkOrigin = 0, nameAt = 0, moduleOrigin = 0;

	if (!WaypointOrigin(bytes.data(), bytes.size(), 0, &first) || !WaypointOrigin(bytes.data(), bytes.size(), 1, &second) ||
	    !WaypointLinkOrigin(first, 0, &linkOrigin) || !ModuleLinkOrigin(bytes.data(), bytes.size(), second, 0, &nameAt, &moduleOrigin))
	{
		Check(false, "the chunk these checks use is the right size");
		return;
	}

	Check(first.start == 4 && linkOrigin == 52 && moduleOrigin == 120,
	      "the chunk these checks use has waypoint 0 at 4, its link at 52, and waypoint 1's module link flags at 120");

	char const *why = 0;
	std::vector<uint8_t> before = bytes;

	Check(SetAiFieldAxis(*waypoint, *middle, bytes.data(), bytes.size(), first.start, 2, 12345, &why) && GetInt(bytes, 36) == 12345 &&
	      ChangedOnly(before, bytes, 36, 40),
	      "setting one axis of waypoint 0's centre changes that axis alone");

	PutInt(bytes, 40, INT32_MIN + 0x40000000);   // flags with the first-ground and centre bits, which the editor keeps as the file has them
	before = bytes;
	Check(SetAiField(*waypoint, *flags, bytes.data(), bytes.size(), first.start, 1, &why) && GetInt(bytes, 40) == INT32_MIN + 0x40000000 + 1 &&
	      ChangedOnly(before, bytes, 40, 44),
	      "setting a waypoint's flags changes only the bit it owns, and keeps the bits the editor has no name for");

	before = bytes;
	Check(SetAiField(*link, *linkIndex, bytes.data(), bytes.size(), linkOrigin, 0, &why) && GetInt(bytes, 52) == 0 &&
	      ChangedOnly(before, bytes, 52, 56),
	      "setting a link's place in its list changes that number alone");

	before = bytes;
	Check(SetAiField(*moduleLink, *moduleFlags, bytes.data(), bytes.size(), moduleOrigin, 9, &why) && GetInt(bytes, 120) == 9 &&
	      ChangedOnly(before, bytes, 120, 124),
	      "setting a module link's flags changes them where the reader finds them, after the name");

	before = bytes;
	why = 0;
	Check(!SetAiFieldAxis(*waypoint, *middle, bytes.data(), bytes.size(), first.start, 0, 16777217, &why) && why == middle->why &&
	      bytes == before,
	      "a centre past 2^24 is refused, with the field's own reason, and nothing changes");
	why = 0;
	Check(!SetAiField(*link, *linkIndex, bytes.data(), bytes.size(), linkOrigin, -1, &why) && why == linkIndex->why && bytes == before,
	      "a link's place below 0 is refused, with the field's own reason, and nothing changes");
	why = 0;
	Check(!SetAiField(*waypoint, *linkIndex, bytes.data(), bytes.size(), first.start, 0, &why) && why == NotThisPartsReason &&
	      bytes == before,
	      "a field of another part is refused, rather than written at its own offset in this one");
}

static void WaypointNamesAndBitsAreKnown()
{
	printf("A waypoint's names and bits have the old editor's words:\n");

	NameKind const *const name = FindNameKind("WAYPOINT", "module link name");

	Check(name && !name->required && name->most == NameMostWithNoGameLimit,
	      "the name table has a module link's name, which the game can do without");
	Check(strcmp(BitLabel("WAYPOINT", "waypoint", "flags", 0x1), "Can Crawl") == 0,
	      "the waypoint's one flag bit is named as the old editor names it: \"Can Crawl\"");
	Check(strcmp(BitLabel("WAYPOINT", "waypoint link", "flags", 0x1), "One way") == 0 &&
	      strcmp(BitLabel("WAYPOINT", "waypoint link", "flags", 0x2), "One way, the other direction") == 0 &&
	      strcmp(BitLabel("WAYPOINT", "waypoint link", "flags", 0x4), "Aliens only") == 0 &&
	      strcmp(BitLabel("WAYPOINT", "waypoint link", "flags", 0x8), "Large target") == 0,
	      "a link's four flag bits are named as the game names them (the old editor's words for them aren't known)");
}

// ---- The AI modules, what a module can see, the entry points and the zones ----

// One item of a module's list of what it can see, as the game writes it (win95/chunk_object.cpp:675-689): object_index, branch_no, flags and
// spare. (The game's struct keeps the four in another order, so this follows the reader, not the struct.)
struct TestVModule
{
	int32_t objectIndex;
	int32_t branch;
	int32_t flags;
	int32_t spare;
};

// One item of a module's list of adjacent modules, as the game writes it (win95/chunk_object.cpp:767-780): object_index, flags and the entry
// point's x, y and z.
struct TestAdjacent
{
	int32_t objectIndex;
	int32_t flags;
	int32_t x, y, z;
};

static std::vector<uint8_t> BuildVModules(std::vector<TestVModule> const &items)
{
	std::vector<uint8_t> bytes;

	Add(bytes, (int32_t)items.size());

	for (size_t i = 0; i < items.size(); i++)
	{
		Add(bytes, items[i].objectIndex);
		Add(bytes, items[i].branch);
		Add(bytes, items[i].flags);
		Add(bytes, items[i].spare);
	}

	return bytes;
}

static std::vector<uint8_t> BuildAdjacent(std::vector<TestAdjacent> const &items)
{
	std::vector<uint8_t> bytes;

	Add(bytes, (int32_t)items.size());

	for (size_t i = 0; i < items.size(); i++)
	{
		Add(bytes, items[i].objectIndex);
		Add(bytes, items[i].flags);
		Add(bytes, items[i].x);
		Add(bytes, items[i].y);
		Add(bytes, items[i].z);
	}

	return bytes;
}

// An AIMODSLA payload written out by hand (win95/chunk_waypoint.cpp:513-518): MasterModuleIndex 7.
static uint8_t const SlaveSampleBytes[] = { 0x07, 0x00, 0x00, 0x00 };

// A MODZONE payload written out by hand (win95/chunk_object.cpp:872-878): Zone 3, spare 9.
static uint8_t const ZoneSampleBytes[] = { 0x03, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00 };

// A VMDARRAY payload written out by hand (win95/chunk_object.cpp:701-743): a count of 2; item 0 (at 4): object_index 10, branch_no 2, flags 0,
// spare 0; item 1 (at 20): object_index 11, branch_no 0, flags 0, spare 5.
static uint8_t const VModuleSampleBytes[] =
{
	0x02, 0x00, 0x00, 0x00,   // the count: 2 items
	0x0A, 0x00, 0x00, 0x00,   // item 0: object_index 10
	0x02, 0x00, 0x00, 0x00,   // branch_no 2: jumps forward to the end of the list
	0x00, 0x00, 0x00, 0x00,   // flags 0
	0x00, 0x00, 0x00, 0x00,   // spare 0
	0x0B, 0x00, 0x00, 0x00,   // item 1: object_index 11
	0x00, 0x00, 0x00, 0x00,   // branch_no 0: no jump
	0x00, 0x00, 0x00, 0x00,   // flags 0
	0x05, 0x00, 0x00, 0x00,   // spare 5
};

// An ADJMDLEP payload written out by hand (win95/chunk_object.cpp:791-817): a count of 2; item 0 (at 4): object_index 20, flags 2, entry point
// 1, 2, 3; item 1 (at 24): object_index 21, flags 8, entry point -4, -5, -6.
static uint8_t const AdjacentSampleBytes[] =
{
	0x02, 0x00, 0x00, 0x00,                                                  // the count: 2 items
	0x14, 0x00, 0x00, 0x00,                                                  // item 0: object_index 20
	0x02, 0x00, 0x00, 0x00,                                                  // flags 2: inside the adjacent module
	0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  // entry point 1, 2, 3
	0x15, 0x00, 0x00, 0x00,                                                  // item 1: object_index 21
	0x08, 0x00, 0x00, 0x00,                                                  // flags 8: vertical
	0xFC, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0xFF, 0xFF, 0xFA, 0xFF, 0xFF, 0xFF,  // entry point -4, -5, -6
};

static std::vector<uint8_t> ArrayBytes(uint8_t const *from, size_t count)
{
	return std::vector<uint8_t>(from, from + count);
}

static void FlatAiChunksAreTheGames()
{
	printf("The AI module chunks and the zone chunk are a few fixed bytes (win95/chunk_waypoint.cpp:412-545, win95/chunk_object.cpp:864-894):\n");

	AiChunk const *const master = FindAiChunk("AIMODMAS");
	AiChunk const *const slave = FindAiChunk("AIMODSLA");
	AiChunk const *const zone = FindAiChunk("MODZONE");
	AiPart const *const slavePart = FindAiPart("AIMODSLA", "AI module slave");
	AiPart const *const zonePart = FindAiPart("MODZONE", "zone");

	if (!master || !slave || !zone || !slavePart || !zonePart)
	{
		Check(false, "the AI module chunks, the zone chunk and their parts are in the tables");
		return;
	}

	static ExpectedField const slavePlaces[] = { { "MasterModuleIndex", 0, Storage::Int32 } };
	static ExpectedField const zonePlaces[] = { { "Zone", 0, Storage::Int32 }, { "spare", 4, Storage::Int32 } };
	bool const slaveRight = !slavePart->item && slavePart->origin == AiOrigin::RecordStart && slavePart->bytes == 4 &&
	                        PlacesAre(*slavePart, slavePlaces, 1);
	bool const zoneRight = !zonePart->item && zonePart->origin == AiOrigin::RecordStart && zonePart->bytes == 8 &&
	                       PlacesAre(*zonePart, zonePlaces, 2);

	Check(strcmp(master->parent, "MODULEDT") == 0 && master->leastBytes == 0 && !FindAiPart("AIMODMAS", "AI module master"),
	      "the master chunk is read only inside a module's data, holds no numbers and has no smallest size (chunk_waypoint.cpp:412-417)");
	Check(strcmp(slave->parent, "MODULEDT") == 0 && slave->leastBytes == 4,
	      "the slave chunk is read only inside a module's data, and holds at least 4 bytes (chunk_waypoint.cpp:511)");
	Check(zone->parent[0] == 0 && zone->leastBytes == 8, "the zone chunk is read anywhere, and holds at least 8 bytes (chunk_object.cpp:864)");
	Check(slaveRight, "a slave's one number, MasterModuleIndex, is at +0");
	Check(zoneRight, "a zone's two numbers are Zone at +0 and spare at +4");

	if (!slaveRight || !zoneRight)
	{
		return;
	}

	Check(CheckFlatAiChunkSize(*master, 0) == SizeCheck::Right && CheckFlatAiChunkSize(*master, 10) == SizeCheck::Right,
	      "a master chunk of any size is read, as the game ignores its bytes");
	Check(CheckFlatAiChunkSize(*slave, 3) == SizeCheck::TooShort && CheckFlatAiChunkSize(*slave, 4) == SizeCheck::Right &&
	      CheckFlatAiChunkSize(*slave, 10) == SizeCheck::Right,
	      "a slave chunk under 4 bytes is refused as too short; 4 bytes, or more, are read");
	Check(CheckFlatAiChunkSize(*zone, 7) == SizeCheck::TooShort && CheckFlatAiChunkSize(*zone, 8) == SizeCheck::Right,
	      "a zone chunk under 8 bytes is refused as too short; 8 are read");

	std::vector<uint8_t> slaveBytes = ArrayBytes(SlaveSampleBytes, sizeof(SlaveSampleBytes));
	std::vector<uint8_t> zoneBytes = ArrayBytes(ZoneSampleBytes, sizeof(ZoneSampleBytes));
	Field const *const masterIndex = FindAiField(*slavePart, "MasterModuleIndex");
	Field const *const zoneNumber = FindAiField(*zonePart, "Zone");
	Field const *const zoneSpare = FindAiField(*zonePart, "spare");
	double value = -1;
	char const *why = 0;

	Check(ReadAiField(*slavePart, *masterIndex, slaveBytes.data(), slaveBytes.size(), 0, &value, &why) && value == 7,
	      "the hand-written slave reads MasterModuleIndex 7");
	Check(ReadAiField(*zonePart, *zoneNumber, zoneBytes.data(), zoneBytes.size(), 0, &value, &why) && value == 3 &&
	      ReadAiField(*zonePart, *zoneSpare, zoneBytes.data(), zoneBytes.size(), 0, &value, &why) && value == 9,
	      "the hand-written zone reads Zone 3 and spare 9");
	Check(FindAiChunk("MODZONE") == zone && FindAiChunk("MODZONE\0") == zone && FindAiChunk("MODZONEX") == 0,
	      "MODZONE has 7 letters and a zero: it's found by its 7 letters, with or without the zero, and not by a longer name that starts with them");

	std::vector<uint8_t> before = zoneBytes;
	Check(SetAiField(*zonePart, *zoneSpare, zoneBytes.data(), zoneBytes.size(), 0, 4, &why) && GetInt(zoneBytes, 4) == 4 &&
	      ChangedOnly(before, zoneBytes, 4, 8),
	      "setting a zone's spare changes that number alone");
	Check(SetAiField(*slavePart, *masterIndex, slaveBytes.data(), slaveBytes.size(), 0, -5, &why) && GetInt(slaveBytes, 0) == -5,
	      "setting a slave's master index takes any whole number: the game looks the object up by it");
	before = slaveBytes;
	Check(!SetAiField(*slavePart, *masterIndex, slaveBytes.data(), 3, 0, 9, &why) && slaveBytes == before,
	      "a slave payload of 3 bytes has no room for its number: refused, and nothing is written");
}

static void VirtualModulesAreTheGames()
{
	printf("A module's list of what it can see is laid out as the game's reader reads it (win95/chunk_object.cpp:701-743):\n");

	AiChunk const *const chunk = FindAiChunk("VMDARRAY");
	AiPart const *const part = FindAiPart("VMDARRAY", "virtual module");

	if (!chunk || !part)
	{
		Check(false, "the VMDARRAY chunk and its part are in the tables");
		return;
	}

	static ExpectedField const places[] =
	{
		{ "object_index", 0, Storage::Int32 }, { "branch_no", 4, Storage::Int32 }, { "flags", 8, Storage::Int32 }, { "spare", 12, Storage::Int32 },
	};
	bool const right = part->item && part->origin == AiOrigin::RecordStart && part->bytes == 16 && PlacesAre(*part, places, 4);

	Check(chunk->parent[0] == 0 && chunk->leastBytes == 0, "the game registers the list to be read anywhere, with no smallest size (chunk_object.cpp:642)");
	Check(right, "an item is 16 bytes: object_index +0, branch_no +4, flags +8, spare +12 (the reader's order, not the game's struct's)");

	if (!right)
	{
		return;
	}

	std::vector<uint8_t> const bytes = ArrayBytes(VModuleSampleBytes, sizeof(VModuleSampleBytes));
	size_t count = 0, origin = 0;
	double value = -1;
	char const *why = 0;
	Field const *const objectIndex = FindAiField(*part, "object_index");
	Field const *const branch = FindAiField(*part, "branch_no");
	Field const *const flags = FindAiField(*part, "flags");
	Field const *const spare = FindAiField(*part, "spare");

	Check(CheckVModuleArraySize(bytes.data(), bytes.size(), &count) == SizeCheck::Right && count == 2,
	      "the hand-written list of 2 items is the right size");
	Check(VModuleOrigin(2, 0, &origin) && origin == 4 && VModuleOrigin(2, 1, &origin) && origin == 20 && !VModuleOrigin(2, 2, &origin),
	      "item 0 starts at 4, item 1 at 20, and there is no item 2");
	Check(ReadAiField(*part, *objectIndex, bytes.data(), bytes.size(), 4, &value, &why) && value == 10 &&
	      ReadAiField(*part, *branch, bytes.data(), bytes.size(), 4, &value, &why) && value == 2 &&
	      ReadAiField(*part, *flags, bytes.data(), bytes.size(), 4, &value, &why) && value == 0 &&
	      ReadAiField(*part, *spare, bytes.data(), bytes.size(), 4, &value, &why) && value == 0,
	      "item 0 reads as object_index 10, branch_no 2, flags 0, spare 0");
	Check(ReadAiField(*part, *objectIndex, bytes.data(), bytes.size(), 20, &value, &why) && value == 11 &&
	      ReadAiField(*part, *branch, bytes.data(), bytes.size(), 20, &value, &why) && value == 0 &&
	      ReadAiField(*part, *flags, bytes.data(), bytes.size(), 20, &value, &why) && value == 0 &&
	      ReadAiField(*part, *spare, bytes.data(), bytes.size(), 20, &value, &why) && value == 5,
	      "item 1 reads as object_index 11, branch_no 0, flags 0, spare 5");

	std::vector<TestVModule> items;
	TestVModule const first = { 10, 2, 0, 0 };
	TestVModule const second = { 11, 0, 0, 5 };
	items.push_back(first);
	items.push_back(second);
	Check(BuildVModules(items) == bytes, "the test's own writer lays the same list out in the same 36 bytes");
	Check(branch->limit == Limit::Loader && flags->limit == Limit::Loader,
	      "a jump's lowest and an item's flags are limits the game's loader has itself (chunk_object.cpp:722 and :733), not only the editor's");

	// Hostile counts.
	Check(CheckVModuleArraySize(0, 0, 0) == SizeCheck::CountCutOff && CheckVModuleArraySize(bytes.data(), 3, 0) == SizeCheck::CountCutOff,
	      "an empty payload, or 3 bytes, has no count: refused");

	std::vector<uint8_t> const noItems = BuildVModules(std::vector<TestVModule>());
	count = 99;
	Check(CheckVModuleArraySize(noItems.data(), noItems.size(), &count) == SizeCheck::Right && count == 0 && noItems.size() == 4,
	      "a count of 0 and nothing after it is the right size");

	std::vector<uint8_t> hostile = bytes;
	PutInt(hostile, 0, -1);
	Check(CheckVModuleArraySize(hostile.data(), hostile.size(), 0) == SizeCheck::CountNegative, "a count below zero is refused");
	PutInt(hostile, 0, INT32_MIN);
	Check(CheckVModuleArraySize(hostile.data(), hostile.size(), 0) == SizeCheck::CountNegative, "the most negative count is refused too");
	PutInt(hostile, 0, 3);
	Check(CheckVModuleArraySize(hostile.data(), hostile.size(), 0) == SizeCheck::CountTooBig, "a count of 3 items with room for 2 is refused as too big");
	PutInt(hostile, 0, INT32_MAX);
	Check(CheckVModuleArraySize(hostile.data(), hostile.size(), 0) == SizeCheck::CountTooBig, "the biggest count there is is refused as too big");
	PutInt(hostile, 0, 268435457);
	Check(CheckVModuleArraySize(hostile.data(), hostile.size(), 0) == SizeCheck::CountTooBig,
	      "a count of 268,435,457 (16 times it is 4,294,967,312, which wraps round to 16 in 32 bits) is refused: the sum doesn't wrap");

	std::vector<uint8_t> cut = bytes;
	cut.pop_back();
	Check(CheckVModuleArraySize(cut.data(), cut.size(), 0) == SizeCheck::CountTooBig, "a count of 2 with one byte too few for the second item is refused");

	std::vector<uint8_t> longer = bytes;

	for (int i = 0; i < 5; i++)
	{
		longer.push_back(0xAA);
	}

	count = 99;
	Check(CheckVModuleArraySize(longer.data(), longer.size(), &count) == SizeCheck::Right && count == 2,
	      "bytes after the last item are ignored, as the game ignores them");

	// The jump an item makes (chunk_object.cpp:720-725).
	Check(VModuleBranchIsAllowed(0, 0, 3, &why) && VModuleBranchIsAllowed(1, 0, 3, &why) && VModuleBranchIsAllowed(3, 0, 3, &why) &&
	      VModuleBranchIsAllowed(2, 1, 3, &why) && VModuleBranchIsAllowed(3, 2, 3, &why) && VModuleBranchIsAllowed(0, 2, 3, &why),
	      "a branch is 0, no jump, or a place after the item's own and no further than the end of the list: in a list of 3 the first item may jump to 1, 2 or 3");
	why = 0;
	Check(!VModuleBranchIsAllowed(4, 0, 3, &why) && why == branch->why && !VModuleBranchIsAllowed(1, 1, 3, &why) &&
	      !VModuleBranchIsAllowed(2, 2, 3, &why) && !VModuleBranchIsAllowed(-1, 0, 3, &why) && !VModuleBranchIsAllowed(INT32_MIN, 0, 3, &why),
	      "a jump past the end of the list, back to the item's own place or before it, or below zero is refused, with the field's own reason");

	Check(Verdict(*flags, -1) == flags->why && strcmp(Verdict(*flags, 0), "") == 0 && Verdict(*flags, 1) == flags->why,
	      "an item's flags have to be 0: the game refuses a level whose item has any (chunk_object.cpp:733)");
	Check(Verdict(*branch, -1) == branch->why && strcmp(Verdict(*branch, 0), "") == 0 && strcmp(Verdict(*branch, 2147483647.0), "") == 0,
	      "a branch can't be below 0; where its upper end lies depends on the item's place and the list's size (VModuleBranchIsAllowed)");

	std::vector<uint8_t> edited = bytes;
	std::vector<uint8_t> before = edited;

	Check(SetAiField(*part, *branch, edited.data(), edited.size(), 20, 2, &why) && GetInt(edited, 24) == 2 && ChangedOnly(before, edited, 24, 28),
	      "setting the second item's branch changes that number alone, 4 bytes into the item");
	before = edited;
	why = 0;
	Check(!SetAiField(*part, *flags, edited.data(), edited.size(), 20, 1, &why) && why == flags->why && edited == before,
	      "a flag the game refuses is refused, with the field's own reason, and nothing changes");
}

static void AdjacentModulesAreTheGames()
{
	printf("A module's list of adjacent modules is laid out as the game's reader reads it (win95/chunk_object.cpp:791-817):\n");

	AiChunk const *const chunk = FindAiChunk("ADJMDLEP");
	AiPart const *const part = FindAiPart("ADJMDLEP", "adjacent module");

	if (!chunk || !part)
	{
		Check(false, "the ADJMDLEP chunk and its part are in the tables");
		return;
	}

	static ExpectedField const places[] =
	{
		{ "object_index", 0, Storage::Int32 }, { "flags", 4, Storage::UInt32 }, { "entry_point", 8, Storage::IntTriple },
	};
	bool const right = part->item && part->origin == AiOrigin::RecordStart && part->bytes == 20 && PlacesAre(*part, places, 3);

	Check(chunk->parent[0] == 0 && chunk->leastBytes == 0, "the game registers the list to be read anywhere, with no smallest size (chunk_object.cpp:751)");
	Check(right, "an item is 20 bytes: object_index +0, flags +4, entry_point +8 (the reader's order, not the game's struct's)");

	if (!right)
	{
		return;
	}

	std::vector<uint8_t> const bytes = ArrayBytes(AdjacentSampleBytes, sizeof(AdjacentSampleBytes));
	size_t count = 0, origin = 0;
	double value = -1;
	char const *why = 0;
	int32_t x = 0, y = 0, z = 0;
	Field const *const objectIndex = FindAiField(*part, "object_index");
	Field const *const flags = FindAiField(*part, "flags");
	Field const *const entry = FindAiField(*part, "entry_point");

	Check(CheckAdjacentModulesSize(bytes.data(), bytes.size(), &count) == SizeCheck::Right && count == 2,
	      "the hand-written list of 2 items is the right size");
	Check(AdjacentModuleOrigin(2, 0, &origin) && origin == 4 && AdjacentModuleOrigin(2, 1, &origin) && origin == 24 &&
	      !AdjacentModuleOrigin(2, 2, &origin),
	      "item 0 starts at 4, item 1 at 24, and there is no item 2");
	Check(ReadAiField(*part, *objectIndex, bytes.data(), bytes.size(), 4, &value, &why) && value == 20 &&
	      ReadAiField(*part, *flags, bytes.data(), bytes.size(), 4, &value, &why) && value == 2 &&
	      ReadTriple(*part, *entry, bytes, 4, &x, &y, &z) && x == 1 && y == 2 && z == 3,
	      "item 0 reads as object_index 20, flags 2, entry point 1, 2, 3");
	Check(ReadAiField(*part, *objectIndex, bytes.data(), bytes.size(), 24, &value, &why) && value == 21 &&
	      ReadAiField(*part, *flags, bytes.data(), bytes.size(), 24, &value, &why) && value == 8 &&
	      ReadTriple(*part, *entry, bytes, 24, &x, &y, &z) && x == -4 && y == -5 && z == -6,
	      "item 1 reads as object_index 21, flags 8, entry point -4, -5, -6");

	std::vector<TestAdjacent> items;
	TestAdjacent const first = { 20, 2, 1, 2, 3 };
	TestAdjacent const second = { 21, 8, -4, -5, -6 };
	items.push_back(first);
	items.push_back(second);
	Check(BuildAdjacent(items) == bytes, "the test's own writer lays the same list out in the same 44 bytes");
	Check(entry->limit == Limit::Loader, "an entry point's place is a limit the game's loader has itself (chunk_object.cpp:809), not only the editor's");

	// Hostile counts.
	Check(CheckAdjacentModulesSize(0, 0, 0) == SizeCheck::CountCutOff && CheckAdjacentModulesSize(bytes.data(), 3, 0) == SizeCheck::CountCutOff,
	      "an empty payload, or 3 bytes, has no count: refused");

	std::vector<uint8_t> const noItems = BuildAdjacent(std::vector<TestAdjacent>());
	count = 99;
	Check(CheckAdjacentModulesSize(noItems.data(), noItems.size(), &count) == SizeCheck::Right && count == 0 && noItems.size() == 4,
	      "a count of 0 and nothing after it is the right size");

	std::vector<uint8_t> hostile = bytes;
	PutInt(hostile, 0, -1);
	Check(CheckAdjacentModulesSize(hostile.data(), hostile.size(), 0) == SizeCheck::CountNegative, "a count below zero is refused");
	PutInt(hostile, 0, INT32_MIN);
	Check(CheckAdjacentModulesSize(hostile.data(), hostile.size(), 0) == SizeCheck::CountNegative, "the most negative count is refused too");
	PutInt(hostile, 0, 3);
	Check(CheckAdjacentModulesSize(hostile.data(), hostile.size(), 0) == SizeCheck::CountTooBig, "a count of 3 items with room for 2 is refused as too big");
	PutInt(hostile, 0, INT32_MAX);
	Check(CheckAdjacentModulesSize(hostile.data(), hostile.size(), 0) == SizeCheck::CountTooBig, "the biggest count there is is refused as too big");
	PutInt(hostile, 0, 214748365);
	Check(CheckAdjacentModulesSize(hostile.data(), hostile.size(), 0) == SizeCheck::CountTooBig,
	      "a count of 214,748,365 (20 times it is 4,294,967,300, which wraps round to 4 in 32 bits) is refused: the sum doesn't wrap");

	std::vector<uint8_t> cut = bytes;
	cut.pop_back();
	Check(CheckAdjacentModulesSize(cut.data(), cut.size(), 0) == SizeCheck::CountTooBig, "a count of 2 with one byte too few for the second item is refused");

	std::vector<uint8_t> longer = bytes;

	for (int i = 0; i < 5; i++)
	{
		longer.push_back(0xAA);
	}

	count = 99;
	Check(CheckAdjacentModulesSize(longer.data(), longer.size(), &count) == SizeCheck::Right && count == 2,
	      "bytes after the last item are ignored, as the game ignores them");

	Check(strcmp(Verdict(*flags, 15), "") == 0 && Verdict(*flags, 16) == DoesNotFitReason,
	      "an item's flags take only their four bits: entry point set by hand, inside the adjacent module, adjacent module inside this one, vertical");
	Check(strcmp(Verdict(*entry, 16777216), "") == 0 && strcmp(Verdict(*entry, -16777216), "") == 0 &&
	      Verdict(*entry, 16777217) == entry->why && Verdict(*entry, -16777217) == entry->why,
	      "an entry point is kept within 2^24 of the level's middle on each axis: the game refuses a level with one farther out");
	Check(strcmp(BitLabel("ADJMDLEP", "adjacent module", "flags", 0x1), "Entry point set by hand") == 0 &&
	      strcmp(BitLabel("ADJMDLEP", "adjacent module", "flags", 0x2), "Inside the adjacent module") == 0 &&
	      strcmp(BitLabel("ADJMDLEP", "adjacent module", "flags", 0x4), "Adjacent module inside this one") == 0 &&
	      strcmp(BitLabel("ADJMDLEP", "adjacent module", "flags", 0x8), "Vertical, for aliens only") == 0,
	      "the four flag bits are named as the game names them (the old editor's words for them aren't known)");

	std::vector<uint8_t> edited = bytes;
	std::vector<uint8_t> before = edited;

	Check(SetAiFieldAxis(*part, *entry, edited.data(), edited.size(), 24, 2, 777, &why) && GetInt(edited, 40) == 777 &&
	      ChangedOnly(before, edited, 40, 44),
	      "setting one axis of the second item's entry point changes that axis alone");

	PutInt(edited, 28, 0x18);   // the second item's flags, with a bit the editor has no name for (0x10)
	before = edited;
	Check(SetAiField(*part, *flags, edited.data(), edited.size(), 24, 2, &why) && GetInt(edited, 28) == 0x12 && ChangedOnly(before, edited, 28, 32),
	      "setting an item's flags changes only the four bits they own, and keeps the bit the editor has no name for");

	before = edited;
	why = 0;
	Check(!SetAiFieldAxis(*part, *entry, edited.data(), edited.size(), 24, 0, 16777217, &why) && why == entry->why && edited == before,
	      "an entry point past 2^24 is refused, with the field's own reason, and nothing changes");
}

// The lookups take a chunk's name as a level file holds it: its 8 letters and straight after them the chunk's size, with no zero between
// (MODZONE, of 7 letters, has its zero), and a name that isn't there is refused without being read.
static void AiLookupsTakeNamesAsALevelHoldsThem()
{
	printf("The lookups take names as a level file holds them:\n");

	char const path[12] = { 'A', 'V', 'P', 'P', 'A', 'T', 'H', '2', 24, 0, 0, 0 };
	char const wrongLast[12] = { 'A', 'V', 'P', 'P', 'A', 'T', 'H', '3', 24, 0, 0, 0 };
	char const zone[12] = { 'M', 'O', 'D', 'Z', 'O', 'N', 'E', 0, 8, 0, 0, 0 };

	Check(FindAiChunk(path) != 0 && FindAiChunk(path) == FindAiChunk("AVPPATH2") && FindAiChunk(wrongLast) == 0 && FindAiChunk("AVPPATH") == 0,
	      "a chunk is found from the 8 letters of its name with no zero after them, and all 8 letters are compared");
	Check(FindAiChunk(zone) != 0 && FindAiChunk(zone) == FindAiChunk("MODZONE"), "MODZONE is found from its 7 letters and the zero after them");
	Check(FindAiPart(path, "path") != 0 && FindAiPart(path, "path") == FindAiPart("AVPPATH2", "path") && FindAiPart(wrongLast, "path") == 0,
	      "a part is found from its chunk's name as a level file holds it, and a name that differs in its 8th letter finds none");

	AiPart const *const head = FindAiPart("AVPPATH2", "path");
	Check(FindAiChunk(0) == 0 && FindAiPart(0, "path") == 0 && FindAiPart("AVPPATH2", 0) == 0 && head && FindAiField(*head, 0) == 0,
	      "a lookup with no name finds nothing, and doesn't try to read one");
}

int main()
{
	TablesHoldTogether();
	PathIsLaidOutAsTheGameReadsIt();
	PathSizesAreTheGames();
	EveryLimitRefusesJustOutside();
	PathValuesAreChecked();
	PathSettersWriteOnlyTheirPart();
	PathNumbersAreUnique();
	PathNamesAndBitsAreKnown();
	WaypointIsLaidOutAsTheGameReadsIt();
	WaypointSizesAreTheGames();
	WaypointGroupsAreTheGames();
	WaypointRoomIsTheGames();
	WaypointValuesAreChecked();
	WaypointSettersWriteOnlyTheirPart();
	WaypointNamesAndBitsAreKnown();
	FlatAiChunksAreTheGames();
	VirtualModulesAreTheGames();
	AdjacentModulesAreTheGames();
	AiLookupsTakeNamesAsALevelHoldsThem();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
