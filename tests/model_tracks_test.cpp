// The test for src/model's OBJTRAK2 and ALTLOCAT tables and rules (tracks_fields.h): a moving object's track, and the other places an
// object may start in. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds payloads in memory, some of them hostile, and checks what the model says about
// them. The program gives back a non-zero result if any check failed. Each check's words say what it proves.
//
// THE GAME'S OWN LAYOUT is copied into this test (the Game... structures below, from the game's win95/chunk_object.hpp at the dev pin
// the editor builds against, AvPx 491deb22), so a table's offset is compared with where the game really keeps the number, not with a second copy of the table.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "tracks_fields.h"

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

// win95/chunk_type.hpp:43-47 (ChunkVectorInt) and :191-194 (ChunkQuat), and win95/chunk_object.hpp:374-390 (ChunkTrackSection).
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

static void PutInt(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

static uint32_t GetUInt(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t bits = 0;

	for (int i = 0; i < 4; i++)
	{
		bits |= (uint32_t)bytes.at(at + i) << (8 * i);
	}

	return bits;
}

static uint32_t FloatBits(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

// Whether a source names a file and a line: ".cpp:" or ".hpp:" and then a digit, anywhere in it.
static bool NamesAFileAndLine(char const *where)
{
	for (size_t i = 1; where && where[i]; i++)
	{
		bool const afterName = where[i - 1] == 'p' || where[i - 1] == 'h';

		if (where[i] == ':' && afterName && where[i + 1] >= '0' && where[i + 1] <= '9')
		{
			return true;
		}
	}

	return false;
}

// Whether a value is allowed in a field, and the reason when it isn't ("" when it is).
static char const *Verdict(Field const &field, double value)
{
	char const *why = 0;
	return ValueIsAllowed(field, value, &why) ? "" : (why ? why : "refused with no reason");
}

// A track payload of that many bytes, all zero, whose count says `count` sections.
static std::vector<uint8_t> Track(int32_t count, size_t length)
{
	std::vector<uint8_t> bytes(length, 0);

	if (length >= 4)
	{
		PutInt(bytes, 0, count);
	}

	return bytes;
}

static SizeCheck TrackSizeOf(std::vector<uint8_t> const &bytes)
{
	return CheckTrackSize(bytes.data(), bytes.size());
}

// A track as the game writes it (Object_Track_Chunk2::fill_data_block, win95/chunk_object.cpp:962-988): the count, the sections as the
// game holds them in memory, the flags and the timer's start.
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

static void TrackTablesHoldTogether()
{
	printf("The track's tables hold together:\n");

	std::set<std::string> ownNames, sectionNames;
	bool namesUnique = true, sources = true, whys = true, limits = true, labels = true, noCreations = true, spares = true;

	for (size_t f = 0; f < TrackFieldCount; f++)
	{
		Field const &field = TrackFields[f];
		double lowest = 0, highest = 0;
		StorageRange(field.storage, &lowest, &highest);

		namesUnique = ownNames.insert(field.name).second && namesUnique;
		sources = NamesAFileAndLine(field.source) && sources;
		// A field with a limit says why. One with none says nothing, except the flags, which say why a bit isn't a choice.
		whys = (field.limit == Limit::None ? (field.why[0] == 0 || field.unit == Unit::Flags) : field.why[0] != 0) && whys;
		limits = (field.limit == Limit::None || (field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest)) && limits;
		labels = strlen(field.label) <= 48 && strlen(field.why) <= 200 && labels;
		noCreations = !field.hasCreation && noCreations;
	}

	for (size_t f = 0; f < TrackSectionFieldCount; f++)
	{
		Field const &field = TrackSectionFields[f];
		double lowest = 0, highest = 0;
		StorageRange(field.storage, &lowest, &highest);

		namesUnique = sectionNames.insert(field.name).second && namesUnique;
		sources = NamesAFileAndLine(field.source) && sources;
		whys = (field.limit == Limit::None ? field.why[0] == 0 : field.why[0] != 0) && whys;
		limits = (field.limit == Limit::None || (field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest)) && limits;
		labels = strlen(field.label) <= 48 && strlen(field.why) <= 200 && labels;
		// A field with a label is shown in a box of its own, so it's one number, not a triple.
		labels = (field.label[0] == 0 || field.storage != Storage::IntTriple) && labels;
		noCreations = !field.hasCreation && noCreations;
		spares = (strcmp(field.name, "spare") != 0 || (field.label[0] == 0 && field.limit == Limit::None)) && spares;
	}

	Check(namesUnique, "no table has two fields of one name");
	Check(sources, "every field says where its facts come from, as a file and a line");
	Check(whys, "a field with a limit says why, and one without says nothing (but the flags, which say why bit 31 isn't a choice)");
	Check(limits, "every limit is a range its storage can hold");
	Check(labels, "every label fits the panel's 48 bytes and every reason its 200, and a labelled field is one number");
	Check(noCreations, "no field claims a starting value (the old editor's aren't known)");
	Check(spares, "the section's spare number has no label and no limit: it's hidden, and its bytes are kept");

	// The track's own numbers: the count first, then the flags and the timer's start taking exactly the 8 bytes the game reads after the sections.
	Field const *const count = FindTrackField("num_sections");
	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");

	if (!count || !flags || !timer || TrackFieldCount != 3)
	{
		Check(false, "the track's three own fields are in the table");
		return;
	}

	Check(count->anchor == Anchor::Start && count->offset == 0 && count->unit == Unit::ListCount && count->storage == Storage::Int32,
	      "the count of sections is the first number of the payload, and a list's count");
	Check(flags->anchor == Anchor::AfterFirstList && flags->offset == 0 && timer->anchor == Anchor::AfterFirstList && timer->offset == 4 &&
	      StorageBytes(flags->storage) == 4 && StorageBytes(timer->storage) == 4,
	      "the flags and the timer's start are the 8 bytes straight after the last section");

	// A section's fields lie inside it, never overlap, and leave no byte out: sorted by place, each starts where the one before ended.
	std::vector<bool> taken(TrackSectionBytes, false);
	bool inside = true, overlapping = false;
	size_t bytesNamed = 0;

	for (size_t f = 0; f < TrackSectionFieldCount; f++)
	{
		Field const &field = TrackSectionFields[f];
		size_t const end = (size_t)field.offset + StorageBytes(field.storage);

		inside = field.anchor == Anchor::Start && end <= TrackSectionBytes && inside;

		for (size_t at = field.offset; at < end && at < TrackSectionBytes; at++)
		{
			overlapping = taken[at] || overlapping;
			taken[at] = true;
		}

		bytesNamed += StorageBytes(field.storage);
	}

	Check(inside, "every section field lies inside the section's 76 bytes");
	Check(!overlapping && bytesNamed == TrackSectionBytes, "the section's fields never overlap, and between them name all 76 bytes");

	// Where the game keeps each number, from its own structure.
	GameTrackSection game = {};
	game.quat_start = { 1.5f, 2.5f, 3.5f, 4.5f };
	game.quat_end = { 5.5f, 6.5f, 7.5f, 8.5f };
	game.pivot_start = { 11, 12, 13 };
	game.pivot_end = { 21, 22, 23 };
	game.object_offset = { 31, 32, 33 };
	game.time_for_section = 4040;
	game.spare = 5050;

	struct Where
	{
		char const *name;
		size_t at;
		uint32_t bits;
	};

	Where const wheres[] =
	{
		{ "quat_start_x", offsetof(GameTrackSection, quat_start) + offsetof(GameQuat, x), FloatBits(game.quat_start.x) },
		{ "quat_start_y", offsetof(GameTrackSection, quat_start) + offsetof(GameQuat, y), FloatBits(game.quat_start.y) },
		{ "quat_start_z", offsetof(GameTrackSection, quat_start) + offsetof(GameQuat, z), FloatBits(game.quat_start.z) },
		{ "quat_start_w", offsetof(GameTrackSection, quat_start) + offsetof(GameQuat, w), FloatBits(game.quat_start.w) },
		{ "quat_end_x", offsetof(GameTrackSection, quat_end) + offsetof(GameQuat, x), FloatBits(game.quat_end.x) },
		{ "quat_end_y", offsetof(GameTrackSection, quat_end) + offsetof(GameQuat, y), FloatBits(game.quat_end.y) },
		{ "quat_end_z", offsetof(GameTrackSection, quat_end) + offsetof(GameQuat, z), FloatBits(game.quat_end.z) },
		{ "quat_end_w", offsetof(GameTrackSection, quat_end) + offsetof(GameQuat, w), FloatBits(game.quat_end.w) },
		{ "pivot_start", offsetof(GameTrackSection, pivot_start), (uint32_t)game.pivot_start.x },
		{ "pivot_end", offsetof(GameTrackSection, pivot_end), (uint32_t)game.pivot_end.x },
		{ "object_offset", offsetof(GameTrackSection, object_offset), (uint32_t)game.object_offset.x },
		{ "time_for_section", offsetof(GameTrackSection, time_for_section), (uint32_t)game.time_for_section },
		{ "spare", offsetof(GameTrackSection, spare), (uint32_t)game.spare },
	};

	size_t const whereCount = sizeof(wheres) / sizeof(wheres[0]);
	bool offsetsAreTheGames = TrackSectionFieldCount == whereCount;
	bool bitsAreTheGames = offsetsAreTheGames;

	for (size_t w = 0; w < whereCount; w++)
	{
		Field const *const field = FindTrackSectionField(wheres[w].name);
		offsetsAreTheGames = field && field->offset == wheres[w].at && offsetsAreTheGames;

		// The number really is there in the bytes the game writes.
		if (field)
		{
			std::vector<uint8_t> const bytes = GameTrack(std::vector<GameTrackSection>(1, game), 0, 0);
			bitsAreTheGames = GetUInt(bytes, 4 + field->offset) == wheres[w].bits && bitsAreTheGames;
		}
	}

	Check(offsetsAreTheGames, "every section field starts where the game's own structure keeps that number");
	Check(bitsAreTheGames, "each section field finds, in a section the game wrote, the number the game put in that member");
}

static void TrackSizesAreTheGames()
{
	printf("A track's size is checked as the game's reader checks it:\n");

	bool cutOff = true;

	for (size_t length = 0; length < 4; length++)
	{
		cutOff = TrackSizeOf(Track(0, length)) == SizeCheck::CountCutOff && cutOff;
	}

	Check(cutOff, "a track of 0 to 3 bytes has no room for its count, which the game refuses");
	Check(TrackSizeOf(Track(0, 4)) == SizeCheck::TooShort && TrackSizeOf(Track(0, 11)) == SizeCheck::TooShort,
	      "a track with no sections and under 12 bytes has no flags or timer, which the game refuses");
	Check(TrackSizeOf(Track(0, 12)) == SizeCheck::Right, "a track with no sections is the right size at 12 bytes");
	Check(TrackSizeOf(Track(0, 100)) == SizeCheck::Right, "bytes after the timer are ignored, as the game does");

	Check(TrackSizeOf(Track(1, 80)) == SizeCheck::TooShort && TrackSizeOf(Track(1, 87)) == SizeCheck::TooShort,
	      "a track whose one section fits, but whose flags and timer don't, is refused");
	Check(TrackSizeOf(Track(1, 88)) == SizeCheck::Right && TrackSizeOf(Track(1, 90)) == SizeCheck::Right,
	      "a track with one section is the right size at 88 bytes");
	Check(TrackSizeOf(Track(1, 79)) == SizeCheck::CountTooBig, "a count of 1 with less than a section's room is refused as too big");

	Check(TrackSizeOf(Track(-1, 88)) == SizeCheck::CountNegative && TrackSizeOf(Track(std::numeric_limits<int32_t>::min(), 88)) == SizeCheck::CountNegative,
	      "a negative count is refused");
	Check(TrackSizeOf(Track(2, 88)) == SizeCheck::CountTooBig, "a count of 2 with room for 1 is refused as too big");
	Check(TrackSizeOf(Track(2, 164)) == SizeCheck::Right && TrackSizeOf(Track(2, 163)) == SizeCheck::TooShort,
	      "a count of 2 is right at 164 bytes, and one byte short of that the flags and timer are cut off");

	Check(TrackSizeOf(Track(std::numeric_limits<int32_t>::max(), 1000)) == SizeCheck::CountTooBig &&
	      TrackSizeOf(Track(std::numeric_limits<int32_t>::max(), 12)) == SizeCheck::CountTooBig,
	      "the biggest count is refused as too big, at any size");
	Check(TrackSizeOf(Track(0x40000001, 88)) == SizeCheck::CountTooBig,
	      "a count whose size wraps round in 32 bits to one section (2^30 + 1 sections of 76 bytes) is refused as too big");
	Check(TrackSizeOf(Track(56512728, 88)) == SizeCheck::CountTooBig,
	      "a count whose size wraps round in 32 bits to 32 bytes (56,512,728 sections of 76 bytes) is refused as too big");

	size_t count = 99;
	Check(TrackSectionCount(Track(3, 240).data(), 240, &count) && count == 3, "the count of a track of a right size is given");
	count = 99;
	Check(!TrackSectionCount(Track(3, 239).data(), 239, &count) && count == 99,
	      "the count of a track of a wrong size isn't given, and what was there is left alone");
}

static void TrackPlacesAreTheGames()
{
	printf("Places in a track are where the game reads them:\n");

	std::vector<uint8_t> const three = Track(3, 240);
	size_t place = 7;

	Check(TrackSectionPlace(three.data(), three.size(), 0, &place) && place == 4, "the first section starts at +4, straight after the count");
	Check(TrackSectionPlace(three.data(), three.size(), 1, &place) && place == 80 && TrackSectionPlace(three.data(), three.size(), 2, &place) &&
	      place == 156,
	      "each section starts 76 bytes after the one before");

	place = 7;
	Check(!TrackSectionPlace(three.data(), three.size(), 3, &place) && place == 7 &&
	      !TrackSectionPlace(three.data(), three.size(), (size_t)-1, &place) && place == 7,
	      "a section the track doesn't hold has no place, however big its number, and nothing is written to the place");

	std::vector<uint8_t> const wrong = Track(4, 240);
	Check(!TrackSectionPlace(wrong.data(), wrong.size(), 0, &place) && !TrackSectionPlace(wrong.data(), wrong.size(), 3, &place),
	      "no section has a place when the count is more than the track holds");

	Field const *const count = FindTrackField("num_sections");
	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");
	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const pivotEnd = FindTrackSectionField("pivot_end");
	Field const *const spare = FindTrackSectionField("spare");

	if (!count || !flags || !timer || !time || !pivotEnd || !spare)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(TrackFieldPlace(*count, three.data(), three.size(), &place) && place == 0, "the count is at +0");
	Check(TrackFieldPlace(*flags, three.data(), three.size(), &place) && place == 232 && TrackFieldPlace(*timer, three.data(), three.size(), &place) &&
	      place == 236,
	      "three sections' flags are at +232 and the timer's start at +236, straight after the last section");

	std::vector<uint8_t> const longer = Track(3, 245);
	Check(TrackFieldPlace(*flags, longer.data(), longer.size(), &place) && place == 232,
	      "bytes after the timer don't move the flags: the game reads them straight after the sections, not from the end");

	std::vector<uint8_t> const empty = Track(0, 12);
	Check(TrackFieldPlace(*flags, empty.data(), empty.size(), &place) && place == 4 && TrackFieldPlace(*timer, empty.data(), empty.size(), &place) &&
	      place == 8,
	      "with no sections the flags are at +4 and the timer's start at +8");

	Check(TrackSectionFieldPlace(*time, three.data(), three.size(), 0, &place) && place == 4 + 68, "section 0's time is at +72");
	Check(TrackSectionFieldPlace(*pivotEnd, three.data(), three.size(), 1, &place) && place == 4 + 76 + 44,
	      "section 1's end pivot is at +124: its section's start, plus 44");
	Check(TrackSectionFieldPlace(*spare, three.data(), three.size(), 2, &place) && place == 228 && place + 4 == 232,
	      "the last section's spare ends exactly where the flags begin");

	place = 7;
	Check(!TrackSectionFieldPlace(*time, three.data(), three.size(), 3, &place) && !TrackSectionFieldPlace(*time, wrong.data(), wrong.size(), 0, &place) &&
	      place == 7,
	      "a field of a section the track doesn't hold, or of a track of a wrong size, has no place");
	Check(!TrackFieldPlace(*time, three.data(), three.size(), &place) && !TrackSectionFieldPlace(*flags, three.data(), three.size(), 0, &place),
	      "a section's field isn't one of the track's, and a track's isn't one of a section's: neither is found at the other's offset");

	Field const copy = *flags;
	Check(!TrackFieldPlace(copy, three.data(), three.size(), &place) && place == 7, "a copy of a field isn't the table's own, so it's refused");
	Check(!TrackFieldPlace(*flags, three.data(), 0, &place) && !TrackSectionFieldPlace(*time, three.data(), 3, 0, &place),
	      "a payload that's cut short has no places at all");

	// A track as the game writes it: each field's place holds the number the game put in that member of that section.
	std::vector<GameTrackSection> sections(3);
	memset(sections.data(), 0, sections.size() * sizeof(GameTrackSection));
	bool found = true;

	for (int k = 0; k < 3; k++)
	{
		sections[k].quat_start = { 0.5f + k, 1.5f + k, 2.5f + k, 3.5f + k };
		sections[k].quat_end = { 4.5f + k, 5.5f + k, 6.5f + k, 7.5f + k };
		sections[k].pivot_start = { 100 * k + 1, 100 * k + 2, 100 * k + 3 };
		sections[k].pivot_end = { 100 * k + 4, 100 * k + 5, 100 * k + 6 };
		sections[k].object_offset = { 100 * k + 7, 100 * k + 8, 100 * k + 9 };
		sections[k].time_for_section = 65 + k;
		sections[k].spare = 9000 + k;
	}

	std::vector<uint8_t> const written = GameTrack(sections, 0x8000000B, 4096);

	for (int k = 0; k < 3; k++)
	{
		size_t at = 0;
		found = TrackSectionFieldPlace(*FindTrackSectionField("time_for_section"), written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == (uint32_t)sections[k].time_for_section && found;
		found = TrackSectionFieldPlace(*FindTrackSectionField("pivot_end"), written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == (uint32_t)sections[k].pivot_end.x && GetUInt(written, at + 4) == (uint32_t)sections[k].pivot_end.y &&
		        GetUInt(written, at + 8) == (uint32_t)sections[k].pivot_end.z && found;
		found = TrackSectionFieldPlace(*FindTrackSectionField("quat_end_w"), written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == FloatBits(sections[k].quat_end.w) && found;
		found = TrackSectionFieldPlace(*spare, written.data(), written.size(), k, &at) && GetUInt(written, at) == (uint32_t)sections[k].spare && found;
	}

	found = TrackFieldPlace(*flags, written.data(), written.size(), &place) && GetUInt(written, place) == 0x8000000B && found;
	found = TrackFieldPlace(*timer, written.data(), written.size(), &place) && GetUInt(written, place) == 4096 && found;
	found = TrackSectionCount(written.data(), written.size(), &place) && place == 3 && found;
	Check(found, "in a track as the game writes it, every place finds the number the game put there, for each of three sections");
}

static void TrackValuesAreChecked()
{
	printf("A track's values are checked:\n");

	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const timer = FindTrackField("timer_start");
	Field const *const flags = FindTrackField("flags");
	Field const *const spare = FindTrackSectionField("spare");

	if (!time || !timer || !flags || !spare)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(Verdict(*time, 64) == time->why && strcmp(Verdict(*time, 65), "") == 0,
	      "a section's time of 64 is refused and 65 is allowed: the game's loader refuses anything shorter than 65");
	Check(Verdict(*time, 0) == time->why && Verdict(*time, -1) == time->why && Verdict(*time, std::numeric_limits<int32_t>::min()) == time->why,
	      "a section that takes no time, or less, is refused: it would hang the game's timer");
	Check(strcmp(Verdict(*time, std::numeric_limits<int32_t>::max()), "") == 0, "a section's time has no top but what a whole number holds");
	Check(Verdict(*time, 2147483648.0) == DoesNotFitReason && Verdict(*time, 65.5) == NotWholeReason &&
	      Verdict(*time, std::numeric_limits<double>::quiet_NaN()) == NotANumberReason,
	      "a section's time that doesn't fit a whole number, isn't whole or isn't a number is refused");
	Check(time->limit == Limit::Loader && time->lowest == 65, "the section's time limit is the loader's own");

	Check(Verdict(*timer, -1) == timer->why && strcmp(Verdict(*timer, 0), "") == 0,
	      "a timer start of -1 is refused and 0 is allowed");
	Check(strcmp(Verdict(*timer, 16777216), "") == 0 && Verdict(*timer, 16777217) == timer->why,
	      "a timer start of 2^24 is allowed and one more is refused: the game's loader refuses anything later");
	Check(Verdict(*timer, std::numeric_limits<int32_t>::max()) == timer->why, "the biggest timer start is refused");
	Check(timer->limit == Limit::Loader && timer->highest == (double)(1 << 24), "the timer start's limit is the loader's own");

	Check(strcmp(Verdict(*flags, 0x1), "") == 0 && strcmp(Verdict(*flags, 0x2), "") == 0 && strcmp(Verdict(*flags, 0x4), "") == 0 &&
	      strcmp(Verdict(*flags, 0x8), "") == 0 && strcmp(Verdict(*flags, 0xF), "") == 0,
	      "each of the four flags a mapper may choose can be set, alone or all together");
	Check(Verdict(*flags, 0x80000000u) == DoesNotFitReason && Verdict(*flags, 0x10) == DoesNotFitReason,
	      "bit 31 and the bits the game doesn't use aren't a mapper's choice");
	Check(strstr(flags->why, "bit 31") != 0 && strstr(flags->why, "chunk_object.cpp:1045-1062") != 0,
	      "the flags say, in words, that the game sets bit 31 itself and the editor keeps it as the file has it");
	Check((flags->mask & 0x80000000u) == 0, "bit 31 is outside the flags' mask, so setting the flags never changes it");

	Check(strcmp(Verdict(*spare, std::numeric_limits<int32_t>::min()), "") == 0 && strcmp(Verdict(*spare, std::numeric_limits<int32_t>::max()), "") == 0,
	      "a spare number takes any whole number");

	// The eight parts of the two turns: from -2 to 2, and real numbers only.
	char const *const parts[] = { "quat_start_x", "quat_start_y", "quat_start_z", "quat_start_w", "quat_end_x", "quat_end_y", "quat_end_z", "quat_end_w" };
	bool ranges = true, notNumbers = true, fromLoader = true;

	for (size_t p = 0; p < sizeof(parts) / sizeof(parts[0]); p++)
	{
		Field const *const part = FindTrackSectionField(parts[p]);

		if (!part)
		{
			ranges = false;
			continue;
		}

		ranges = strcmp(Verdict(*part, 2.0), "") == 0 && strcmp(Verdict(*part, -2.0), "") == 0 && strcmp(Verdict(*part, 0.0), "") == 0 &&
		         Verdict(*part, 2.0001) == part->why && Verdict(*part, -2.0001) == part->why && Verdict(*part, 3.0e38) == part->why && ranges;
		notNumbers = Verdict(*part, std::numeric_limits<double>::quiet_NaN()) == NotANumberReason &&
		             Verdict(*part, std::numeric_limits<double>::infinity()) == NotANumberReason &&
		             Verdict(*part, -std::numeric_limits<double>::infinity()) == NotANumberReason && notNumbers;
		fromLoader = part->limit == Limit::Loader && fromLoader;
	}

	Check(ranges, "each part of each turn is allowed from -2 to 2, and refused beyond it");
	Check(notNumbers, "each part of each turn refuses NaN and endless: they aren't real numbers");
	Check(fromLoader, "the turns' limit is the loader's own (CheckTurn), not only the editor's");

	// The three places: each axis within 2^24 either way.
	char const *const places[] = { "pivot_start", "pivot_end", "object_offset" };
	bool placeRanges = true, placesFromLoader = true;

	for (size_t p = 0; p < sizeof(places) / sizeof(places[0]); p++)
	{
		Field const *const place = FindTrackSectionField(places[p]);

		if (!place)
		{
			placeRanges = false;
			continue;
		}

		placeRanges = strcmp(Verdict(*place, 16777216), "") == 0 && strcmp(Verdict(*place, -16777216), "") == 0 &&
		              Verdict(*place, 16777217) == place->why && Verdict(*place, -16777217) == place->why &&
		              Verdict(*place, std::numeric_limits<int32_t>::max()) == place->why && Verdict(*place, 2147483648.0) == DoesNotFitReason &&
		              placeRanges;
		placesFromLoader = place->limit == Limit::Loader && placesFromLoader;
	}

	Check(placeRanges, "each axis of each place is allowed within 2^24 either way, and refused beyond it");
	Check(placesFromLoader, "the places' limit is the loader's own (CheckPlace), not only the editor's");
}

// The label of a named bit of the track's flags, or "" when it has no name.
static char const *TrackBitLabel(uint32_t bit)
{
	for (size_t i = 0; i < TrackBitCount; i++)
	{
		if (TrackBits[i].bit == bit && strcmp(TrackBits[i].chunk, "OBJTRAK2") == 0 && strcmp(TrackBits[i].field, "flags") == 0)
		{
			return TrackBits[i].label;
		}
	}

	return "";
}

static void TrackWordsAreTheOldEditors()
{
	printf("The track's words are the old editor's (vasquez-ui.md, section 2.6):\n");

	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const timer = FindTrackField("timer_start");

	if (!time || !timer)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(strcmp(time->label, "Time for section") == 0 && strcmp(timer->label, "Timer start (seconds)") == 0,
	      "a section's time and the timer's start are labelled as the old editor labelled them");

	// Every other field has no label: the old editor never labelled it, so the panel skips it and its bytes are kept.
	size_t labelled = 0;

	for (size_t f = 0; f < TrackFieldCount; f++)
	{
		labelled += TrackFields[f].label[0] != 0;
	}

	for (size_t f = 0; f < TrackSectionFieldCount; f++)
	{
		labelled += TrackSectionFields[f].label[0] != 0;
	}

	Check(labelled == 2, "no other field has a label: the turns, the places, the spare, the flags and the count are skipped by the panel");

	Check(strcmp(TrackBitLabel(0x1), "Loop Track") == 0 && strcmp(TrackBitLabel(0x4), "Loop back and forth") == 0 &&
	      strcmp(TrackBitLabel(0x8), "Use Track Smoothing") == 0,
	      "the looping and smoothing flags are named as the old editor's check boxes were");
	Check(strcmp(TrackBitLabel(0x2), "Playing at start") == 0, "the playing-at-start flag, which the old editor had no box for, has plain words");

	// The named bits are exactly the four a mapper may choose: the flags' mask. Bit 31 has no name.
	Field const *const flags = FindTrackField("flags");
	uint32_t named = 0;
	bool singleBits = true;

	for (size_t i = 0; i < TrackBitCount; i++)
	{
		singleBits = TrackBits[i].bit != 0 && (TrackBits[i].bit & (TrackBits[i].bit - 1)) == 0 && singleBits;
		named |= TrackBits[i].bit;
	}

	Check(TrackBitCount == 4 && singleBits && flags && named == flags->mask && TrackBitLabel(0x80000000u)[0] == 0,
	      "the four named bits are single bits and are exactly the flags' mask, and bit 31 has no name");
}

// For payloads of every size up to 400 bytes, with counts that are honest and hostile, no place the model gives is past the payload's end.
static void TrackPlacesStayInside()
{
	printf("No place a track gives is ever past its payload:\n");

	int32_t const counts[] = { -1, 0, 1, 2, 3, 4, 5, 0x40000001, 56512728, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	size_t const sections[] = { 0, 1, 2, 3, 4, 5, (size_t)-1 };
	bool inside = true;
	size_t placesFound = 0;

	for (size_t length = 0; length <= 400; length++)
	{
		for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
		{
			std::vector<uint8_t> const bytes = Track(counts[c], length);

			for (size_t f = 0; f < TrackFieldCount; f++)
			{
				size_t place = 0;

				if (TrackFieldPlace(TrackFields[f], bytes.data(), bytes.size(), &place))
				{
					placesFound++;
					inside = place + StorageBytes(TrackFields[f].storage) <= length && inside;
				}
			}

			for (size_t s = 0; s < sizeof(sections) / sizeof(sections[0]); s++)
			{
				size_t place = 0;

				if (TrackSectionPlace(bytes.data(), bytes.size(), sections[s], &place))
				{
					placesFound++;
					inside = place + TrackSectionBytes <= length && inside;
				}

				for (size_t f = 0; f < TrackSectionFieldCount; f++)
				{
					if (TrackSectionFieldPlace(TrackSectionFields[f], bytes.data(), bytes.size(), sections[s], &place))
					{
						placesFound++;
						inside = place + StorageBytes(TrackSectionFields[f].storage) <= length && inside;
					}
				}
			}
		}
	}

	Check(inside, "a place the model gives always lies wholly inside the payload, for sizes 0 to 400 and counts of every kind");
	Check(placesFound > 1000, "the sweep did find places to check, so it isn't passing by finding none");
}

// win95/chunk_object.hpp:448-453 (AltObjectLocation), from the same structures as above.
struct GameAlternateLocation
{
	GameVectorInt position;
	GameQuat orientation;
	int spare1, spare2;
};

static_assert(sizeof(GameAlternateLocation) == 36, "the game's AltObjectLocation is 36 bytes (win95/chunk_object.hpp:448-453)");

// An alternate-locations payload of that many bytes, all zero, whose count (at +8) says `count` locations.
static std::vector<uint8_t> Locations(int32_t count, size_t length)
{
	std::vector<uint8_t> bytes(length, 0);

	if (length >= 12)
	{
		PutInt(bytes, 8, count);
	}

	return bytes;
}

static SizeCheck LocationsSizeOf(std::vector<uint8_t> const &bytes)
{
	return CheckAlternateLocationsSize(bytes.data(), bytes.size());
}

// The chunk as the game writes it (Object_Alternate_Locations_Chunk::fill_data_block, win95/chunk_object.cpp:1180-1203): the group, the
// spare, the count, and the locations as the game holds them in memory.
static std::vector<uint8_t> GameLocations(int32_t group, int32_t spare2, std::vector<GameAlternateLocation> const &locations)
{
	std::vector<uint8_t> bytes(12 + locations.size() * sizeof(GameAlternateLocation), 0);

	PutInt(bytes, 0, group);
	PutInt(bytes, 4, spare2);
	PutInt(bytes, 8, (int32_t)locations.size());

	for (size_t i = 0; i < locations.size(); i++)
	{
		memcpy(bytes.data() + 12 + i * sizeof(GameAlternateLocation), &locations[i], sizeof(GameAlternateLocation));
	}

	return bytes;
}

static void AlternateTablesHoldTogether()
{
	printf("The alternate locations' tables hold together:\n");

	std::set<std::string> ownNames, entryNames;
	bool namesUnique = true, sources = true, whys = true, limits = true, labels = true, noCreations = true, spares = true;

	for (size_t f = 0; f < AlternateLocationFieldCount; f++)
	{
		Field const &field = AlternateLocationFields[f];
		double lowest = 0, highest = 0;
		StorageRange(field.storage, &lowest, &highest);

		namesUnique = ownNames.insert(field.name).second && namesUnique;
		sources = NamesAFileAndLine(field.source) && sources;
		whys = (field.limit == Limit::None) == (field.why[0] == 0) && whys;
		limits = (field.limit == Limit::None || (field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest)) && limits;
		labels = strlen(field.label) <= 48 && strlen(field.why) <= 200 && labels;
		noCreations = !field.hasCreation && noCreations;
		spares = (strcmp(field.name, "spare2") != 0 || (field.label[0] == 0 && field.limit == Limit::None)) && spares;
	}

	for (size_t f = 0; f < AlternateLocationEntryFieldCount; f++)
	{
		Field const &field = AlternateLocationEntryFields[f];
		double lowest = 0, highest = 0;
		StorageRange(field.storage, &lowest, &highest);

		namesUnique = entryNames.insert(field.name).second && namesUnique;
		sources = NamesAFileAndLine(field.source) && sources;
		whys = (field.limit == Limit::None) == (field.why[0] == 0) && whys;
		limits = (field.limit == Limit::None || (field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest)) && limits;
		labels = strlen(field.label) <= 48 && strlen(field.why) <= 200 && (field.label[0] == 0 || field.storage != Storage::IntTriple) && labels;
		noCreations = !field.hasCreation && noCreations;
		spares = (strncmp(field.name, "spare", 5) != 0 || (field.label[0] == 0 && field.limit == Limit::None)) && spares;
	}

	Check(namesUnique, "no table has two fields of one name");
	Check(sources, "every field says where its facts come from, as a file and a line");
	Check(whys, "a field with a limit says why, and only those do");
	Check(limits, "every limit is a range its storage can hold");
	Check(labels, "every label fits the panel's 48 bytes and every reason its 200, and a labelled field is one number");
	Check(noCreations, "no field claims a starting value (the old editor's aren't known)");
	Check(spares, "the spare numbers have no label and no limit: they're hidden, and their bytes are kept");

	// The chunk's own numbers are the 12 bytes before the first location: the group, the spare and the count.
	bool ownTile = AlternateLocationFieldCount == 3;
	size_t ownBytes = 0;

	for (size_t f = 0; f < AlternateLocationFieldCount; f++)
	{
		ownTile = AlternateLocationFields[f].anchor == Anchor::Start && AlternateLocationFields[f].offset == ownBytes &&
		          StorageBytes(AlternateLocationFields[f].storage) == 4 && ownTile;
		ownBytes += StorageBytes(AlternateLocationFields[f].storage);
	}

	Field const *const count = FindAlternateLocationField("num_locations");
	Check(ownTile && ownBytes == 12 && count && count->offset == 8 && count->unit == Unit::ListCount,
	      "the group, the spare and the count of locations are the first 12 bytes, in that order, and the count is a list's count");

	// An entry's fields lie inside it, never overlap, and leave no byte out.
	std::vector<bool> taken(AlternateLocationBytes, false);
	bool inside = true, overlapping = false;
	size_t bytesNamed = 0;

	for (size_t f = 0; f < AlternateLocationEntryFieldCount; f++)
	{
		Field const &field = AlternateLocationEntryFields[f];
		size_t const end = (size_t)field.offset + StorageBytes(field.storage);

		inside = field.anchor == Anchor::Start && end <= AlternateLocationBytes && inside;

		for (size_t at = field.offset; at < end && at < AlternateLocationBytes; at++)
		{
			overlapping = taken[at] || overlapping;
			taken[at] = true;
		}

		bytesNamed += StorageBytes(field.storage);
	}

	Check(inside, "every location field lies inside the location's 36 bytes");
	Check(!overlapping && bytesNamed == AlternateLocationBytes, "the location's fields never overlap, and between them name all 36 bytes");

	// Where the game keeps each number, from its own structure.
	GameAlternateLocation game = {};
	game.position = { 11, 12, 13 };
	game.orientation = { 1.5f, 2.5f, 3.5f, 4.5f };
	game.spare1 = 7070;
	game.spare2 = 8080;

	struct Where
	{
		char const *name;
		size_t at;
		uint32_t bits;
	};

	Where const wheres[] =
	{
		{ "position", offsetof(GameAlternateLocation, position), (uint32_t)game.position.x },
		{ "orientation_x", offsetof(GameAlternateLocation, orientation) + offsetof(GameQuat, x), FloatBits(game.orientation.x) },
		{ "orientation_y", offsetof(GameAlternateLocation, orientation) + offsetof(GameQuat, y), FloatBits(game.orientation.y) },
		{ "orientation_z", offsetof(GameAlternateLocation, orientation) + offsetof(GameQuat, z), FloatBits(game.orientation.z) },
		{ "orientation_w", offsetof(GameAlternateLocation, orientation) + offsetof(GameQuat, w), FloatBits(game.orientation.w) },
		{ "spare1", offsetof(GameAlternateLocation, spare1), (uint32_t)game.spare1 },
		{ "spare2", offsetof(GameAlternateLocation, spare2), (uint32_t)game.spare2 },
	};

	size_t const whereCount = sizeof(wheres) / sizeof(wheres[0]);
	bool offsetsAreTheGames = AlternateLocationEntryFieldCount == whereCount;
	bool bitsAreTheGames = offsetsAreTheGames;

	for (size_t w = 0; w < whereCount; w++)
	{
		Field const *const field = FindAlternateLocationEntryField(wheres[w].name);
		offsetsAreTheGames = field && field->offset == wheres[w].at && offsetsAreTheGames;

		// The number really is there in the bytes the game writes.
		if (field)
		{
			std::vector<uint8_t> const bytes = GameLocations(0, 0, std::vector<GameAlternateLocation>(1, game));
			bitsAreTheGames = GetUInt(bytes, 12 + field->offset) == wheres[w].bits && bitsAreTheGames;
		}
	}

	Check(offsetsAreTheGames, "every location field starts where the game's own structure keeps that number");
	Check(bitsAreTheGames, "each location field finds, in a location the game wrote, the number the game put in that member");
}

static void AlternateSizesAreTheGames()
{
	printf("The alternate locations' size is checked as the game's reader checks it:\n");

	Check(LocationsSizeOf(Locations(0, 0)) == SizeCheck::TooShort && LocationsSizeOf(Locations(0, 7)) == SizeCheck::TooShort,
	      "a chunk under 8 bytes is refused as too short: the game won't read one that small");
	Check(LocationsSizeOf(Locations(0, 8)) == SizeCheck::CountCutOff && LocationsSizeOf(Locations(0, 11)) == SizeCheck::CountCutOff,
	      "a chunk of 8 to 11 bytes has no room for its count, which the game refuses");
	Check(LocationsSizeOf(Locations(0, 12)) == SizeCheck::Right && LocationsSizeOf(Locations(0, 100)) == SizeCheck::Right,
	      "a chunk with no locations is the right size at 12 bytes, and bytes after the locations are ignored");

	Check(LocationsSizeOf(Locations(1, 47)) == SizeCheck::CountTooBig && LocationsSizeOf(Locations(1, 12)) == SizeCheck::CountTooBig,
	      "a count of 1 with less than a location's room is refused as too big");
	Check(LocationsSizeOf(Locations(1, 48)) == SizeCheck::Right && LocationsSizeOf(Locations(1, 60)) == SizeCheck::Right,
	      "a chunk with one location is the right size at 48 bytes");
	Check(LocationsSizeOf(Locations(2, 48)) == SizeCheck::CountTooBig && LocationsSizeOf(Locations(2, 84)) == SizeCheck::Right,
	      "a count of 2 is refused with room for 1, and is right at 84 bytes");

	Check(LocationsSizeOf(Locations(-1, 48)) == SizeCheck::CountNegative &&
	      LocationsSizeOf(Locations(std::numeric_limits<int32_t>::min(), 48)) == SizeCheck::CountNegative,
	      "a negative count is refused");
	Check(LocationsSizeOf(Locations(std::numeric_limits<int32_t>::max(), 1000)) == SizeCheck::CountTooBig &&
	      LocationsSizeOf(Locations(std::numeric_limits<int32_t>::max(), 12)) == SizeCheck::CountTooBig,
	      "the biggest count is refused as too big, at any size");
	Check(LocationsSizeOf(Locations(0x40000001, 48)) == SizeCheck::CountTooBig,
	      "a count whose size wraps round in 32 bits to one location (2^30 + 1 locations of 36 bytes) is refused as too big");
	Check(LocationsSizeOf(Locations(119304648, 48)) == SizeCheck::CountTooBig,
	      "a count whose size wraps round in 32 bits to 32 bytes (119,304,648 locations of 36 bytes) is refused as too big");

	size_t count = 99;
	Check(AlternateLocationCount(Locations(3, 120).data(), 120, &count) && count == 3, "the count of a chunk of a right size is given");
	count = 99;
	Check(!AlternateLocationCount(Locations(3, 119).data(), 119, &count) && count == 99,
	      "the count of a chunk of a wrong size isn't given, and what was there is left alone");
}

static void AlternatePlacesAreTheGames()
{
	printf("Places in the alternate locations are where the game reads them:\n");

	std::vector<uint8_t> const three = Locations(3, 120);
	size_t place = 7;

	Check(AlternateLocationPlace(three.data(), three.size(), 0, &place) && place == 12, "the first location starts at +12, straight after the count");
	Check(AlternateLocationPlace(three.data(), three.size(), 1, &place) && place == 48 && AlternateLocationPlace(three.data(), three.size(), 2, &place) &&
	      place == 84,
	      "each location starts 36 bytes after the one before");

	place = 7;
	Check(!AlternateLocationPlace(three.data(), three.size(), 3, &place) && place == 7 &&
	      !AlternateLocationPlace(three.data(), three.size(), (size_t)-1, &place) && place == 7,
	      "a location the chunk doesn't hold has no place, however big its number, and nothing is written to the place");

	std::vector<uint8_t> const wrong = Locations(4, 120);
	Check(!AlternateLocationPlace(wrong.data(), wrong.size(), 0, &place) && !AlternateLocationPlace(wrong.data(), wrong.size(), 3, &place),
	      "no location has a place when the count is more than the chunk holds");

	Field const *const group = FindAlternateLocationField("group");
	Field const *const spareOwn = FindAlternateLocationField("spare2");
	Field const *const count = FindAlternateLocationField("num_locations");
	Field const *const position = FindAlternateLocationEntryField("position");
	Field const *const turnX = FindAlternateLocationEntryField("orientation_x");
	Field const *const spare1 = FindAlternateLocationEntryField("spare1");
	Field const *const spare2 = FindAlternateLocationEntryField("spare2");

	if (!group || !spareOwn || !count || !position || !turnX || !spare1 || !spare2)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(AlternateLocationFieldPlace(*group, three.data(), three.size(), &place) && place == 0 &&
	      AlternateLocationFieldPlace(*spareOwn, three.data(), three.size(), &place) && place == 4 &&
	      AlternateLocationFieldPlace(*count, three.data(), three.size(), &place) && place == 8,
	      "the group is at +0, the spare at +4 and the count at +8");

	Check(AlternateLocationEntryFieldPlace(*position, three.data(), three.size(), 0, &place) && place == 12, "location 0's place is at +12");
	Check(AlternateLocationEntryFieldPlace(*turnX, three.data(), three.size(), 0, &place) && place == 24, "location 0's turn starts at +24");
	Check(AlternateLocationEntryFieldPlace(*spare1, three.data(), three.size(), 0, &place) && place == 40 &&
	      AlternateLocationEntryFieldPlace(*spare2, three.data(), three.size(), 0, &place) && place == 44,
	      "location 0's spares are at +40 and +44");
	Check(AlternateLocationEntryFieldPlace(*position, three.data(), three.size(), 1, &place) && place == 48, "location 1's place is at +48");
	Check(AlternateLocationEntryFieldPlace(*spare2, three.data(), three.size(), 2, &place) && place == 116 && place + 4 == three.size(),
	      "the last location's second spare ends exactly where a chunk of that size ends");

	std::vector<uint8_t> const longer = Locations(3, 125);
	Check(AlternateLocationPlace(longer.data(), longer.size(), 2, &place) && place == 84,
	      "bytes after the last location don't move anything: the game ignores them");

	place = 7;
	Check(!AlternateLocationEntryFieldPlace(*position, three.data(), three.size(), 3, &place) &&
	      !AlternateLocationEntryFieldPlace(*position, wrong.data(), wrong.size(), 0, &place) && place == 7,
	      "a field of a location the chunk doesn't hold, or of a chunk of a wrong size, has no place");
	Check(!AlternateLocationFieldPlace(*position, three.data(), three.size(), &place) &&
	      !AlternateLocationEntryFieldPlace(*group, three.data(), three.size(), 0, &place),
	      "a location's field isn't one of the chunk's, and the chunk's isn't one of a location's: neither is found at the other's offset");

	Field const copy = *group;
	Check(!AlternateLocationFieldPlace(copy, three.data(), three.size(), &place) && place == 7,
	      "a copy of a field isn't the table's own, so it's refused");
	Check(!AlternateLocationFieldPlace(*group, three.data(), 11, &place) && !AlternateLocationEntryFieldPlace(*position, three.data(), 7, 0, &place),
	      "a payload that's cut short has no places at all");

	// A chunk as the game writes it: each field's place holds the number the game put in that member of that location.
	std::vector<GameAlternateLocation> locations(3);
	memset(locations.data(), 0, locations.size() * sizeof(GameAlternateLocation));
	bool found = true;

	for (int k = 0; k < 3; k++)
	{
		locations[k].position = { 100 * k + 1, 100 * k + 2, 100 * k + 3 };
		locations[k].orientation = { 0.5f + k, 1.5f + k, 2.5f + k, 3.5f + k };
		locations[k].spare1 = 7000 + k;
		locations[k].spare2 = 8000 + k;
	}

	std::vector<uint8_t> const written = GameLocations(77, 66, locations);

	for (int k = 0; k < 3; k++)
	{
		size_t at = 0;
		found = AlternateLocationEntryFieldPlace(*position, written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == (uint32_t)locations[k].position.x && GetUInt(written, at + 4) == (uint32_t)locations[k].position.y &&
		        GetUInt(written, at + 8) == (uint32_t)locations[k].position.z && found;
		found = AlternateLocationEntryFieldPlace(*FindAlternateLocationEntryField("orientation_w"), written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == FloatBits(locations[k].orientation.w) && found;
		found = AlternateLocationEntryFieldPlace(*spare1, written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == (uint32_t)locations[k].spare1 && found;
		found = AlternateLocationEntryFieldPlace(*spare2, written.data(), written.size(), k, &at) &&
		        GetUInt(written, at) == (uint32_t)locations[k].spare2 && found;
	}

	found = AlternateLocationFieldPlace(*group, written.data(), written.size(), &place) && GetUInt(written, place) == 77 && found;
	found = AlternateLocationFieldPlace(*spareOwn, written.data(), written.size(), &place) && GetUInt(written, place) == 66 && found;
	found = AlternateLocationCount(written.data(), written.size(), &place) && place == 3 && found;
	Check(found, "in a chunk as the game writes it, every place finds the number the game put there, for each of three locations");
}

// For payloads of every size up to 400 bytes, with counts that are honest and hostile, no place the model gives is past the payload's end.
static void AlternatePlacesStayInside()
{
	printf("No place an alternate location gives is ever past its payload:\n");

	int32_t const counts[] = { -1, 0, 1, 2, 3, 4, 5, 0x40000001, 119304648, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	size_t const indexes[] = { 0, 1, 2, 3, 4, 5, (size_t)-1 };
	bool inside = true;
	size_t placesFound = 0;

	for (size_t length = 0; length <= 400; length++)
	{
		for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
		{
			std::vector<uint8_t> const bytes = Locations(counts[c], length);

			for (size_t f = 0; f < AlternateLocationFieldCount; f++)
			{
				size_t place = 0;

				if (AlternateLocationFieldPlace(AlternateLocationFields[f], bytes.data(), bytes.size(), &place))
				{
					placesFound++;
					inside = place + StorageBytes(AlternateLocationFields[f].storage) <= length && inside;
				}
			}

			for (size_t s = 0; s < sizeof(indexes) / sizeof(indexes[0]); s++)
			{
				size_t place = 0;

				if (AlternateLocationPlace(bytes.data(), bytes.size(), indexes[s], &place))
				{
					placesFound++;
					inside = place + AlternateLocationBytes <= length && inside;
				}

				for (size_t f = 0; f < AlternateLocationEntryFieldCount; f++)
				{
					if (AlternateLocationEntryFieldPlace(AlternateLocationEntryFields[f], bytes.data(), bytes.size(), indexes[s], &place))
					{
						placesFound++;
						inside = place + StorageBytes(AlternateLocationEntryFields[f].storage) <= length && inside;
					}
				}
			}
		}
	}

	Check(inside, "a place the model gives always lies wholly inside the payload, for sizes 0 to 400 and counts of every kind");
	Check(placesFound > 1000, "the sweep did find places to check, so it isn't passing by finding none");
}

static void AlternateValuesAreChecked()
{
	printf("An alternate location's values are checked:\n");

	Field const *const group = FindAlternateLocationField("group");
	Field const *const spareOwn = FindAlternateLocationField("spare2");
	Field const *const position = FindAlternateLocationEntryField("position");
	Field const *const spare1 = FindAlternateLocationEntryField("spare1");

	if (!group || !spareOwn || !position || !spare1)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(strcmp(Verdict(*group, 0), "") == 0 && strcmp(Verdict(*group, std::numeric_limits<int32_t>::max()), "") == 0 &&
	      strcmp(Verdict(*group, std::numeric_limits<int32_t>::min()), "") == 0,
	      "a group takes any whole number, and 0 means none: the game's loader doesn't check it");
	Check(Verdict(*group, 1.5) == NotWholeReason && Verdict(*group, 2147483648.0) == DoesNotFitReason,
	      "a group that isn't whole, or doesn't fit a whole number, is refused");
	Check(strcmp(Verdict(*spareOwn, 5), "") == 0 && strcmp(Verdict(*spare1, std::numeric_limits<int32_t>::min()), "") == 0,
	      "a spare number takes any whole number");

	bool placeRanges = strcmp(Verdict(*position, 16777216), "") == 0 && strcmp(Verdict(*position, -16777216), "") == 0 &&
	                   Verdict(*position, 16777217) == position->why && Verdict(*position, -16777217) == position->why &&
	                   Verdict(*position, std::numeric_limits<int32_t>::max()) == position->why && Verdict(*position, 2147483648.0) == DoesNotFitReason;
	Check(placeRanges, "each axis of a location's place is allowed within 2^24 either way, and refused beyond it");
	Check(position->limit == Limit::Loader, "the place's limit is the loader's own (CheckPlace), not only the editor's");

	char const *const parts[] = { "orientation_x", "orientation_y", "orientation_z", "orientation_w" };
	bool ranges = true, notNumbers = true, fromLoader = true;

	for (size_t p = 0; p < sizeof(parts) / sizeof(parts[0]); p++)
	{
		Field const *const part = FindAlternateLocationEntryField(parts[p]);

		if (!part)
		{
			ranges = false;
			continue;
		}

		ranges = strcmp(Verdict(*part, 2.0), "") == 0 && strcmp(Verdict(*part, -2.0), "") == 0 && strcmp(Verdict(*part, 0.0), "") == 0 &&
		         Verdict(*part, 2.0001) == part->why && Verdict(*part, -2.0001) == part->why && Verdict(*part, 3.0e38) == part->why && ranges;
		notNumbers = Verdict(*part, std::numeric_limits<double>::quiet_NaN()) == NotANumberReason &&
		             Verdict(*part, std::numeric_limits<double>::infinity()) == NotANumberReason &&
		             Verdict(*part, -std::numeric_limits<double>::infinity()) == NotANumberReason && notNumbers;
		fromLoader = part->limit == Limit::Loader && fromLoader;
	}

	Check(ranges, "each part of a location's turn is allowed from -2 to 2, and refused beyond it");
	Check(notNumbers, "each part of a location's turn refuses NaN and endless: they aren't real numbers");
	Check(fromLoader, "the turn's limit is the loader's own (CheckTurn), not only the editor's");
}

static void AlternateWordsAreTheOldEditors()
{
	printf("The alternate locations' words are the old editor's (vasquez-ui.md, section 2.4):\n");

	Field const *const group = FindAlternateLocationField("group");

	Check(group && strcmp(group->label, "Group number (0 means no group)") == 0,
	      "the group is labelled as the old editor's prompt was, shortened to fit the panel's 48 bytes");

	// Every other field has no label: the old editor never labelled it, so the panel skips it and its bytes are kept.
	size_t labelled = 0;

	for (size_t f = 0; f < AlternateLocationFieldCount; f++)
	{
		labelled += AlternateLocationFields[f].label[0] != 0;
	}

	for (size_t f = 0; f < AlternateLocationEntryFieldCount; f++)
	{
		labelled += AlternateLocationEntryFields[f].label[0] != 0;
	}

	Check(labelled == 1, "no other field has a label: the places, the turns, the spares and the count are skipped by the panel");
}

// A lookup with no name finds nothing, and doesn't try to read one
static void FindersRefuseNoName()
{
	printf("The finders refuse a name that isn't there:\n");

	Check(FindTrackField(0) == 0 && FindTrackSectionField(0) == 0 && FindAlternateLocationField(0) == 0 && FindAlternateLocationEntryField(0) == 0,
	      "a finder given no name finds nothing, and doesn't try to read one");
	Check(FindTrackField("flags") != 0 && FindTrackSectionField("spare") != 0 && FindAlternateLocationField("group") != 0 &&
	      FindAlternateLocationEntryField("position") != 0,
	      "and each finds a field it has, by that name");
}

int main()
{
	TrackTablesHoldTogether();
	TrackSizesAreTheGames();
	TrackPlacesAreTheGames();
	TrackPlacesStayInside();
	TrackValuesAreChecked();
	TrackWordsAreTheOldEditors();
	AlternateTablesHoldTogether();
	AlternateSizesAreTheGames();
	AlternatePlacesAreTheGames();
	AlternatePlacesStayInside();
	AlternateValuesAreChecked();
	AlternateWordsAreTheOldEditors();
	FindersRefuseNoName();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
