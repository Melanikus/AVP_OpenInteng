// The test for the setters and the old tracks' upgrade in src/model's OBJTRAK2 and ALTLOCAT rules (tracks_fields.h): putting one checked
// number into a moving object's track or into an object's alternate locations, and the game's own upgrade of an old track, done the same
// way. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds payloads in memory, some of them hostile, sets numbers in them, and checks
// every byte. The program gives back a non-zero result if any check failed. Each check's words say what it proves.
//
// THE GAME'S OWN LAYOUT AND UPGRADE are copied into this test from the dev pin the editor builds against, AvPx 491deb22 (paths under its
// 3dc folder): the Game... structures from win95/chunk_object.hpp and win95/chunk_type.hpp, and the game's upgrade of an old track
// (GameUpgrade, from win95/chunk_object.cpp:1045-1062). The game's reader copies each section and each location straight out of the
// payload into its own structure (win95/chunk_object.cpp:1011 and :1168), so a number a setter writes is read back here the same way, and
// the editor's upgrade is compared with the game's own, not with a second copy of the editor's.
//
// EVERY PAYLOAD HAS 16 BYTES AFTER IT that aren't part of it (Payload, below). A setter is told only the payload's own length, so one that
// wrote past the end would change those bytes, and the checks, which compare every byte, would see it.
//
// A CHECK THAT COULD CRASH if its guard were taken out runs under Survives (crash_guard.h): the crash then makes that check FAILED, by
// name, and the other checks still run.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "crash_guard.h"
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

// ---- Numbers in bytes, low byte first, as the game wrote them on the PC

static void PutUInt(std::vector<uint8_t> &bytes, size_t at, uint32_t bits)
{
	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

static void PutInt(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	PutUInt(bytes, at, bits);
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

static int32_t GetInt(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t const bits = GetUInt(bytes, at);
	int32_t value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static float GetFloat(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t const bits = GetUInt(bytes, at);
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static uint32_t FloatBits(float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

// ---- The game's own structures, at the pin 491deb22: win95/chunk_type.hpp:43-47 (ChunkVectorInt) and :191-194 (ChunkQuat), and
// win95/chunk_object.hpp:374-390 (ChunkTrackSection) and :448-453 (AltObjectLocation)

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

struct GameAlternateLocation
{
	GameVectorInt position;
	GameQuat orientation;
	int spare1, spare2;
};

static_assert(sizeof(GameTrackSection) == 76, "the game's ChunkTrackSection is 76 bytes (win95/chunk_object.hpp:374-390)");
static_assert(sizeof(GameAlternateLocation) == 36, "the game's AltObjectLocation is 36 bytes (win95/chunk_object.hpp:448-453)");

// TrackFlag_QuatProblemSorted (win95/chunk_object.hpp:396)
static uint32_t const QuatProblemSorted = 0x80000000u;

// The game's own upgrade of an old track, typed as its reader has it (win95/chunk_object.cpp:1045-1062): only the structures' names are
// this test's. It works on the sections as the game holds them in memory, as fractions, with the game's own minus signs.
static void GameUpgrade(std::vector<GameTrackSection> &sections, uint32_t &flags)
{
	int const num_sections = (int)sections.size();

	if (!(flags & QuatProblemSorted))
	{
		for (int i = 0; i < num_sections; i++)
		{
			GameQuat temp = sections[i].quat_start;
			sections[i].quat_start.x = -temp.w;
			sections[i].quat_start.y = temp.x;
			sections[i].quat_start.z = temp.y;
			sections[i].quat_start.w = -temp.z;

			temp = sections[i].quat_end;
			sections[i].quat_end.x = -temp.w;
			sections[i].quat_end.y = temp.x;
			sections[i].quat_end.z = temp.y;
			sections[i].quat_end.w = -temp.z;
		}
		flags |= QuatProblemSorted;
	}
}

// ---- Payloads

// A payload in memory, with 16 bytes after it that aren't part of it, each 0xEE. A setter is told only `length`, the payload's own size,
// so a setter that wrote past the end would change those 16 bytes, and a check that compares every byte would see it.
struct Payload
{
	std::vector<uint8_t> bytes;   // the payload, then the 16 bytes after it
	size_t length;                // the payload's own size

	explicit Payload(std::vector<uint8_t> const &payload) : bytes(payload), length(payload.size())
	{
		bytes.insert(bytes.end(), (size_t)16, (uint8_t)0xEE);
	}

	uint8_t *data()
	{
		return bytes.data();
	}
};

// Whether `after` is `before` with nothing changed but the `count` bytes from `at`
static bool OnlyChanged(std::vector<uint8_t> const &before, std::vector<uint8_t> const &after, size_t at, size_t count)
{
	if (before.size() != after.size())
	{
		return false;
	}

	for (size_t i = 0; i < before.size(); i++)
	{
		if ((i < at || i >= at + count) && before[i] != after[i])
		{
			return false;
		}
	}

	return true;
}

// A track payload of that many bytes, all zero, whose count says `count` sections (when there's room for the count).
static std::vector<uint8_t> Track(int32_t count, size_t length)
{
	std::vector<uint8_t> bytes(length, 0);

	if (length >= 4)
	{
		PutInt(bytes, 0, count);
	}

	return bytes;
}

// A track as the game writes it (Object_Track_Chunk2::fill_data_block, win95/chunk_object.cpp:962-988): the count, the sections as the
// game holds them in memory, the flags and the timer's start.
static std::vector<uint8_t> GameTrack(std::vector<GameTrackSection> const &sections, uint32_t flags, int32_t timerStart)
{
	size_t const flagsAt = 4 + sections.size() * sizeof(GameTrackSection);
	std::vector<uint8_t> bytes(flagsAt + 8, 0);

	PutInt(bytes, 0, (int32_t)sections.size());

	for (size_t i = 0; i < sections.size(); i++)
	{
		memcpy(bytes.data() + 4 + i * sizeof(GameTrackSection), &sections[i], sizeof(GameTrackSection));
	}

	PutUInt(bytes, flagsAt, flags);
	PutInt(bytes, flagsAt + 4, timerStart);
	return bytes;
}

// Three sections in which every number is different and no part of a turn is 0, so a number moved to the wrong place, or a minus sign
// lost or added, changes a byte. Every part is a multiple of 1/8 from -1.25 to 1.125, which a fraction holds exactly.
static std::vector<GameTrackSection> ThreeSections()
{
	std::vector<GameTrackSection> sections(3);
	memset(sections.data(), 0, sections.size() * sizeof(GameTrackSection));

	for (int k = 0; k < 3; k++)
	{
		float const eighths = (float)k / 8;

		sections[k].quat_start = { 0.125f + eighths, -0.25f - eighths, 0.375f + eighths, -0.5f - eighths };
		sections[k].quat_end = { 0.625f + eighths, -0.75f - eighths, 0.875f + eighths, -1.0f - eighths };
		sections[k].pivot_start = { 100 * k + 1, 100 * k + 2, 100 * k + 3 };
		sections[k].pivot_end = { 100 * k + 4, 100 * k + 5, 100 * k + 6 };
		sections[k].object_offset = { 100 * k + 7, 100 * k + 8, 100 * k + 9 };
		sections[k].time_for_section = 6553 + k;
		sections[k].spare = 9000 + k;
	}

	return sections;
}

// Where the game reads section k: after the count and the k sections before it (win95/chunk_object.cpp:1002-1013). The flags and the
// timer's start come straight after the last section (:1015-1016), so a track of n sections has its flags at SectionAt(n).
static size_t SectionAt(size_t k)
{
	return 4 + k * sizeof(GameTrackSection);
}

// Section k as the game's reader takes it: its 76 bytes copied straight into the game's own structure (win95/chunk_object.cpp:1011)
static GameTrackSection GameSection(std::vector<uint8_t> const &bytes, size_t k)
{
	GameTrackSection section = {};
	memcpy(&section, bytes.data() + SectionAt(k), sizeof(section));
	return section;
}

// An alternate-locations payload of that many bytes, all zero, whose count (at +8) says `count` locations (when there's room for it).
static std::vector<uint8_t> Locations(int32_t count, size_t length)
{
	std::vector<uint8_t> bytes(length, 0);

	if (length >= 12)
	{
		PutInt(bytes, 8, count);
	}

	return bytes;
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

// Two locations in which every number is different
static std::vector<GameAlternateLocation> TwoLocations()
{
	std::vector<GameAlternateLocation> locations(2);
	memset(locations.data(), 0, locations.size() * sizeof(GameAlternateLocation));

	for (int k = 0; k < 2; k++)
	{
		float const eighths = (float)k / 8;

		locations[k].position = { 100 * k + 1, 100 * k + 2, 100 * k + 3 };
		locations[k].orientation = { 0.125f + eighths, -0.25f - eighths, 0.375f + eighths, -0.5f - eighths };
		locations[k].spare1 = 7000 + k;
		locations[k].spare2 = 8000 + k;
	}

	return locations;
}

// Where the game reads location k: after the group, the spare, the count and the k locations before it (win95/chunk_object.cpp:1154-1172)
static size_t LocationAt(size_t k)
{
	return 12 + k * sizeof(GameAlternateLocation);
}

// Location k as the game's reader takes it: its 36 bytes copied straight into the game's own structure (win95/chunk_object.cpp:1168)
static GameAlternateLocation GameLocation(std::vector<uint8_t> const &bytes, size_t k)
{
	GameAlternateLocation location = {};
	memcpy(&location, bytes.data() + LocationAt(k), sizeof(location));
	return location;
}

// A value a field takes that the payloads in the sweeps below don't already hold there, so a write shows: 5 for the track's flags (loop
// and loop back and forth, bits 0x1 and 0x4), the lowest allowed value for a field with a limit, and 12345 for one without.
static double NewValueFor(Field const &field)
{
	if (field.unit == Unit::Flags)
	{
		return 0x5;
	}

	return field.limit != Limit::None ? field.lowest : 12345;
}

// Whether the 4 bytes at `at` hold `value` as the field stores it: a fraction, a masked word's own bits, or a whole number
static bool HoldsValue(Field const &field, std::vector<uint8_t> const &bytes, size_t at, double value)
{
	switch (field.storage)
	{
	case Storage::Float32:
		return GetFloat(bytes, at) == (float)value;

	case Storage::UInt32:
		return (GetUInt(bytes, at) & (field.mask ? field.mask : 0xFFFFFFFFu)) == (uint32_t)value;

	default:
		return GetInt(bytes, at) == (int32_t)value;
	}
}

// ---- The tests

static void TrackOwnNumbersAreSet()
{
	printf("A track's own numbers are set where the game reads them:\n");

	Field const *const count = FindTrackField("num_sections");
	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");

	if (!count || !flags || !timer)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	// Two sections; the flags say loop (0x1), bit 31 (the game's own) and bits 8 to 11, which the game doesn't use; the timer starts at 4096
	std::vector<GameTrackSection> sections = ThreeSections();
	sections.resize(2);
	Payload track(GameTrack(sections, 0x80000F01u, 4096));
	size_t const flagsAt = SectionAt(2);
	size_t const timerAt = flagsAt + 4;
	std::vector<uint8_t> before = track.bytes;
	char const *why = 0;

	Check(SetTrackField(*timer, track.data(), track.length, 65536, &why) && GetInt(track.bytes, timerAt) == 65536 &&
	      OnlyChanged(before, track.bytes, timerAt, 4),
	      "the timer's start is set in the 4 bytes after the flags, where the game reads it, and nothing else changes");

	before = track.bytes;
	Check(SetTrackField(*flags, track.data(), track.length, 0x2 | 0x8, &why) && GetUInt(track.bytes, flagsAt) == 0x80000F0Au &&
	      OnlyChanged(before, track.bytes, flagsAt, 4),
	      "setting the flags to playing at start and smoothing clears loop, and keeps bit 31 and the bits the game doesn't use as they were");
	Check(SetTrackField(*flags, track.data(), track.length, 0, &why) && GetUInt(track.bytes, flagsAt) == 0x80000F00u,
	      "setting no flags clears only the four a mapper may choose");
	Check(SetTrackField(*timer, track.data(), track.length, 0, &why) && GetInt(track.bytes, timerAt) == 0 &&
	      SetTrackField(*timer, track.data(), track.length, 16777216, &why) && GetInt(track.bytes, timerAt) == 16777216,
	      "a timer start of 0 and one of 2^24 are both set: the loader takes them");

	before = track.bytes;
	why = 0;
	Check(!SetTrackField(*timer, track.data(), track.length, 16777217, &why) && why == timer->why && track.bytes == before,
	      "a timer start past 2^24 is refused with the timer's own reason, and nothing changes");
	why = 0;
	Check(!SetTrackField(*timer, track.data(), track.length, -1, &why) && why == timer->why && track.bytes == before,
	      "a timer start below 0 is refused with the timer's own reason, and nothing changes");
	why = 0;
	Check(!SetTrackField(*timer, track.data(), track.length, 100.5, &why) && why == NotWholeReason && track.bytes == before,
	      "a timer start that isn't whole is refused, and nothing changes");
	why = 0;
	Check(!SetTrackField(*timer, track.data(), track.length, std::numeric_limits<double>::quiet_NaN(), &why) && why == NotANumberReason &&
	      track.bytes == before,
	      "a timer start that isn't a number is refused, and nothing changes");
	why = 0;
	Check(!SetTrackField(*flags, track.data(), track.length, 0x80000000u, &why) && why == DoesNotFitReason && track.bytes == before,
	      "bit 31 can't be set through the flags (the game sets it itself), and nothing changes");
	why = 0;
	Check(!SetTrackField(*flags, track.data(), track.length, 0x10, &why) && why == DoesNotFitReason && track.bytes == before,
	      "a bit the game doesn't use can't be set through the flags, and nothing changes");
	why = 0;
	Check(!SetTrackField(*count, track.data(), track.length, 2, &why) && why == ListCountReason && track.bytes == before,
	      "the count of sections can't be set, not even to the count it has: it changes only when sections are added or taken away");
	Check(!SetTrackField(*timer, track.data(), track.length, -1, 0) && track.bytes == before,
	      "a value that isn't allowed is refused just the same when there's nowhere to put the reason");
}

static void SectionNumbersAreSet()
{
	printf("A section's numbers are set in that section, where the game reads them:\n");

	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const startX = FindTrackSectionField("quat_start_x");
	Field const *const endW = FindTrackSectionField("quat_end_w");
	Field const *const spare = FindTrackSectionField("spare");
	Field const *const pivotStart = FindTrackSectionField("pivot_start");

	if (!time || !startX || !endW || !spare || !pivotStart)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Payload track(GameTrack(ThreeSections(), QuatProblemSorted, 0));
	std::vector<uint8_t> before = track.bytes;
	char const *why = 0;

	Check(SetTrackSectionField(*time, track.data(), track.length, 1, 19660, &why) &&
	      OnlyChanged(before, track.bytes, SectionAt(1) + offsetof(GameTrackSection, time_for_section), 4),
	      "section 1's time is set, and nothing else changes");
	Check(GameSection(track.bytes, 1).time_for_section == 19660 && GameSection(track.bytes, 0).time_for_section == 6553 &&
	      GameSection(track.bytes, 2).time_for_section == 6555,
	      "the game, taking section 1 as its own structure, finds that time in it, and sections 0 and 2 keep theirs");

	before = track.bytes;
	Check(SetTrackSectionField(*endW, track.data(), track.length, 2, -1.5, &why) &&
	      OnlyChanged(before, track.bytes, SectionAt(2) + offsetof(GameTrackSection, quat_end) + offsetof(GameQuat, w), 4) &&
	      GameSection(track.bytes, 2).quat_end.w == -1.5f,
	      "the w of section 2's end turn is set to -1.5, where the game finds it, and nothing else changes");
	Check(SetTrackSectionField(*startX, track.data(), track.length, 0, 2.0, &why) && GameSection(track.bytes, 0).quat_start.x == 2.0f &&
	      SetTrackSectionField(*startX, track.data(), track.length, 0, -2.0, &why) && GameSection(track.bytes, 0).quat_start.x == -2.0f,
	      "a part of a turn of 2 and one of -2 are both set: the loader takes them");
	Check(SetTrackSectionField(*time, track.data(), track.length, 0, 65, &why) && GameSection(track.bytes, 0).time_for_section == 65,
	      "a section's time of 65 is set: the loader takes it");
	Check(SetTrackSectionField(*spare, track.data(), track.length, 2, -123456, &why) && GameSection(track.bytes, 2).spare == -123456,
	      "a section's spare number takes any whole number");

	before = track.bytes;
	why = 0;
	Check(!SetTrackSectionField(*startX, track.data(), track.length, 0, 2.0001, &why) && why == startX->why && track.bytes == before,
	      "a part of a turn past 2 is refused with the turn's own reason, and nothing changes");
	why = 0;
	Check(!SetTrackSectionField(*startX, track.data(), track.length, 0, std::numeric_limits<double>::infinity(), &why) && why == NotANumberReason &&
	      track.bytes == before,
	      "a part of a turn that's endless is refused, and nothing changes");
	why = 0;
	Check(!SetTrackSectionField(*time, track.data(), track.length, 0, 64, &why) && why == time->why && track.bytes == before,
	      "a section's time of 64 is refused with the time's own reason, and nothing changes");
	why = 0;
	Check(!SetTrackSectionField(*pivotStart, track.data(), track.length, 0, 5, &why) && why == 0 && track.bytes == before,
	      "a place can't be set from one number (its axis setter does it), and nothing changes");
	why = 0;
	Check(!SetTrackSectionField(*time, track.data(), track.length, 3, 100, &why) && why == 0 && track.bytes == before,
	      "section 3 of a track of three isn't there: nothing is set, and the value, which is allowed, gives no reason");
	Check(!SetTrackSectionField(*time, track.data(), track.length, (size_t)-1, 100, &why) && track.bytes == before,
	      "nor is a section numbered past any track's end");
	why = 0;
	Check(!SetTrackSectionField(*time, track.data(), track.length, 3, 64, &why) && why == time->why && track.bytes == before,
	      "a value that isn't allowed gives its own reason even in a section that isn't there: the value is checked first, as in every setter");
}

static void SectionPlacesAreSetAnAxisAtATime()
{
	printf("A section's places are set one axis at a time:\n");

	Field const *const pivotEnd = FindTrackSectionField("pivot_end");
	Field const *const offset = FindTrackSectionField("object_offset");
	Field const *const time = FindTrackSectionField("time_for_section");

	if (!pivotEnd || !offset || !time)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Payload track(GameTrack(ThreeSections(), QuatProblemSorted, 0));
	std::vector<uint8_t> before = track.bytes;
	size_t const pivotEndAt = SectionAt(1) + offsetof(GameTrackSection, pivot_end);
	char const *why = 0;

	Check(SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 1, 2, -16777216, &why) &&
	      OnlyChanged(before, track.bytes, pivotEndAt + 8, 4),
	      "axis 2 of section 1's end pivot, its z, is set, and nothing else changes, not even its x and y");
	Check(GameSection(track.bytes, 1).pivot_end.x == 104 && GameSection(track.bytes, 1).pivot_end.y == 105 &&
	      GameSection(track.bytes, 1).pivot_end.z == -16777216,
	      "the game, taking section 1 as its own structure, finds the new z and the old x and y");

	before = track.bytes;
	Check(SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 1, 0, 16777216, &why) && OnlyChanged(before, track.bytes, pivotEndAt, 4) &&
	      GameSection(track.bytes, 1).pivot_end.x == 16777216,
	      "axis 0 is x: 2^24 is set in the place's first 4 bytes, as the loader takes it");

	before = track.bytes;
	Check(SetTrackSectionFieldAxis(*offset, track.data(), track.length, 0, 1, -7, &why) &&
	      OnlyChanged(before, track.bytes, SectionAt(0) + offsetof(GameTrackSection, object_offset) + 4, 4) &&
	      GameSection(track.bytes, 0).object_offset.y == -7,
	      "axis 1 is y: section 0's object offset gets its y, and nothing else changes");

	before = track.bytes;
	why = 0;
	Check(!SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 1, 2, 16777217, &why) && why == pivotEnd->why && track.bytes == before,
	      "an axis past 2^24 is refused with the place's own reason, and nothing changes");
	why = 0;
	Check(!SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 1, 0, std::numeric_limits<int32_t>::min(), &why) &&
	      why == pivotEnd->why && track.bytes == before,
	      "and so is the most negative whole number");
	Check(!SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 1, -1, 5, &why) &&
	      !SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 1, 3, 5, &why) && track.bytes == before,
	      "axis -1 and axis 3 aren't axes: nothing is set before the place or after it");
	why = 0;
	Check(!SetTrackSectionFieldAxis(*time, track.data(), track.length, 0, 0, 100, &why) && why == 0 && track.bytes == before,
	      "the axis setter refuses a field that isn't a place, and nothing changes");
	Check(!SetTrackSectionFieldAxis(*pivotEnd, track.data(), track.length, 3, 0, 5, &why) && track.bytes == before,
	      "a place in a section the track doesn't hold isn't set");
}

// Every field of the track's tables, in every section, set once: the setter writes exactly where the place function says, and nowhere else
static void EveryTrackFieldIsSetAtItsPlace()
{
	printf("Every field of a track is set exactly where its place function puts it:\n");

	bool exact = true;
	size_t setsMade = 0;

	for (size_t f = 0; f < TrackFieldCount; f++)
	{
		Field const &field = TrackFields[f];

		if (field.unit == Unit::ListCount)
		{
			continue;
		}

		Payload track(GameTrack(ThreeSections(), QuatProblemSorted, 4096));
		std::vector<uint8_t> const before = track.bytes;
		double const value = NewValueFor(field);
		size_t place = 0;

		exact = TrackFieldPlace(field, track.data(), track.length, &place) && SetTrackField(field, track.data(), track.length, value, 0) &&
		        HoldsValue(field, track.bytes, place, value) && OnlyChanged(before, track.bytes, place, StorageBytes(field.storage)) && exact;
		setsMade++;
	}

	for (size_t k = 0; k < 3; k++)
	{
		for (size_t f = 0; f < TrackSectionFieldCount; f++)
		{
			Field const &field = TrackSectionFields[f];
			Payload track(GameTrack(ThreeSections(), QuatProblemSorted, 4096));
			size_t place = 0;

			if (!TrackSectionFieldPlace(field, track.data(), track.length, k, &place))
			{
				exact = false;
				continue;
			}

			if (field.storage == Storage::IntTriple)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					std::vector<uint8_t> const before = track.bytes;
					int32_t const value = -16777216 + 1000 * (int32_t)k + axis;   // within 2^24, and no section holds it

					exact = SetTrackSectionFieldAxis(field, track.data(), track.length, k, axis, value, 0) &&
					        GetInt(track.bytes, place + 4 * axis) == value && OnlyChanged(before, track.bytes, place + 4 * axis, 4) && exact;
					setsMade++;
				}
			}
			else
			{
				std::vector<uint8_t> const before = track.bytes;
				double const value = NewValueFor(field);

				exact = SetTrackSectionField(field, track.data(), track.length, k, value, 0) && HoldsValue(field, track.bytes, place, value) &&
				        OnlyChanged(before, track.bytes, place, StorageBytes(field.storage)) && exact;
				setsMade++;
			}
		}
	}

	Check(exact && setsMade == 59,
	      "all 59 sets (the flags, the timer, and in each of 3 sections its 10 single numbers and its 3 places an axis at a time) wrote "
	      "exactly the field's own bytes at the place function's place, and nothing else");
}

static void TrackSettersRefuseWhatIsntTheirs()
{
	printf("The track's setters refuse another table's field, a payload the game wouldn't read, and no payload:\n");

	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");
	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const pivotStart = FindTrackSectionField("pivot_start");
	Field const *const position = FindAlternateLocationEntryField("position");

	if (!flags || !timer || !time || !pivotStart || !position)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Payload track(GameTrack(ThreeSections(), QuatProblemSorted, 0));
	std::vector<uint8_t> const before = track.bytes;
	char const *why = 0;

	// A field of another table would be written at its own offset, in a part of the payload it doesn't belong to
	Check(!SetTrackField(*time, track.data(), track.length, 100, &why) && why == NotThisTablesReason && track.bytes == before,
	      "a section's field isn't one of the track's own: the track's setter refuses it, with the reason, and nothing changes");
	why = 0;
	Check(!SetTrackSectionField(*timer, track.data(), track.length, 0, 100, &why) && why == NotThisTablesReason && track.bytes == before,
	      "a track's own field isn't one of a section's: the section setter refuses it, with the reason");
	why = 0;
	Check(!SetTrackSectionFieldAxis(*position, track.data(), track.length, 0, 0, 5, &why) && why == NotThisTablesReason && track.bytes == before,
	      "an alternate location's place isn't one of a section's: the axis setter refuses it, with the reason");
	why = 0;
	Field const copy = *timer;
	Check(!SetTrackField(copy, track.data(), track.length, 100, &why) && why == NotThisTablesReason && track.bytes == before,
	      "a copy of the timer's field isn't the table's own, so it's refused, with the reason");
	why = 0;
	Check(!SetTrackField(StrategyBaseFields[0], track.data(), track.length, 0, &why) && why == NotThisTablesReason && track.bytes == before,
	      "a strategy's field isn't one of the track's: refused, with the reason");

	bool refused = false;
	bool ran = Survives([&] { refused = !SetTrackField(*time, track.data(), track.length, 100, 0); });
	Check(ran && refused && track.bytes == before, "another table's field is refused just the same when there's nowhere to put the reason");

	// Payloads the game's reader would refuse for their size: more sections than the bytes hold, the flags and timer cut off, a negative
	// count, a count whose size wraps round in 32 bits, no room for the count. No setter writes anything in any of them.
	struct Hostile
	{
		int32_t count;
		size_t length;
	};

	Hostile const hostiles[] = { { 4, 240 }, { 3, 239 }, { 0, 11 }, { -1, 240 }, { 0x40000001, 240 }, { 0, 3 } };
	bool untouched = true;

	for (size_t h = 0; h < sizeof(hostiles) / sizeof(hostiles[0]); h++)
	{
		Payload bad(Track(hostiles[h].count, hostiles[h].length));
		std::vector<uint8_t> const was = bad.bytes;

		untouched = !SetTrackField(*timer, bad.data(), bad.length, 100, &why) && !SetTrackField(*flags, bad.data(), bad.length, 1, &why) &&
		            !SetTrackSectionField(*time, bad.data(), bad.length, 0, 100, &why) &&
		            !SetTrackSectionFieldAxis(*pivotStart, bad.data(), bad.length, 0, 0, 5, &why) && bad.bytes == was && untouched;
	}

	Check(untouched, "in a track the game's reader would refuse for its size, no setter writes a byte");

	refused = false;
	ran = Survives([&] { refused = !SetTrackField(*timer, 0, 88, 100, &why); });
	Check(ran && refused, "the track's own setter refuses no payload with a length said, and reads nothing");
	refused = false;
	ran = Survives([&] { refused = !SetTrackSectionField(*time, 0, 88, 0, 100, &why); });
	Check(ran && refused, "the section setter refuses no payload with a length said, and reads nothing");
	refused = false;
	ran = Survives([&] { refused = !SetTrackSectionFieldAxis(*pivotStart, 0, 88, 0, 0, 5, &why); });
	Check(ran && refused, "and so does the section's axis setter");
}

static void AlternateLocationNumbersAreSet()
{
	printf("An object's alternate locations are set where the game reads them:\n");

	Field const *const group = FindAlternateLocationField("group");
	Field const *const spareOwn = FindAlternateLocationField("spare2");
	Field const *const count = FindAlternateLocationField("num_locations");
	Field const *const position = FindAlternateLocationEntryField("position");
	Field const *const turnX = FindAlternateLocationEntryField("orientation_x");
	Field const *const spare1 = FindAlternateLocationEntryField("spare1");

	if (!group || !spareOwn || !count || !position || !turnX || !spare1)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Payload chunk(GameLocations(77, 66, TwoLocations()));
	std::vector<uint8_t> before = chunk.bytes;
	char const *why = 0;

	Check(SetAlternateLocationField(*group, chunk.data(), chunk.length, 0, &why) && GetInt(chunk.bytes, 0) == 0 && OnlyChanged(before, chunk.bytes, 0, 4),
	      "the group is set to 0 (no group) in the payload's first 4 bytes, where the game reads it, and nothing else changes");
	Check(SetAlternateLocationField(*group, chunk.data(), chunk.length, std::numeric_limits<int32_t>::max(), &why) &&
	      GetInt(chunk.bytes, 0) == std::numeric_limits<int32_t>::max() &&
	      SetAlternateLocationField(*group, chunk.data(), chunk.length, std::numeric_limits<int32_t>::min(), &why) &&
	      GetInt(chunk.bytes, 0) == std::numeric_limits<int32_t>::min(),
	      "a group takes any whole number: the game's loader doesn't check it");

	before = chunk.bytes;
	Check(SetAlternateLocationField(*spareOwn, chunk.data(), chunk.length, -5, &why) && GetInt(chunk.bytes, 4) == -5 && OnlyChanged(before, chunk.bytes, 4, 4),
	      "the chunk's spare number is set at +4, and nothing else changes");

	before = chunk.bytes;
	why = 0;
	Check(!SetAlternateLocationField(*group, chunk.data(), chunk.length, 1.5, &why) && why == NotWholeReason && chunk.bytes == before,
	      "a group that isn't whole is refused, and nothing changes");
	why = 0;
	Check(!SetAlternateLocationField(*count, chunk.data(), chunk.length, 2, &why) && why == ListCountReason && chunk.bytes == before,
	      "the count of locations can't be set: it changes only when locations are added or taken away");

	Check(SetAlternateLocationEntryField(*turnX, chunk.data(), chunk.length, 1, 0.75, &why) &&
	      OnlyChanged(before, chunk.bytes, LocationAt(1) + offsetof(GameAlternateLocation, orientation) + offsetof(GameQuat, x), 4) &&
	      GameLocation(chunk.bytes, 1).orientation.x == 0.75f,
	      "location 1's turn's x is set to 0.75, where the game finds it, and nothing else changes");

	before = chunk.bytes;
	Check(SetAlternateLocationEntryField(*spare1, chunk.data(), chunk.length, 0, 99, &why) &&
	      OnlyChanged(before, chunk.bytes, LocationAt(0) + offsetof(GameAlternateLocation, spare1), 4) && GameLocation(chunk.bytes, 0).spare1 == 99,
	      "location 0's first spare is set, and nothing else changes");

	before = chunk.bytes;
	Check(SetAlternateLocationEntryFieldAxis(*position, chunk.data(), chunk.length, 0, 1, 16777216, &why) &&
	      OnlyChanged(before, chunk.bytes, LocationAt(0) + offsetof(GameAlternateLocation, position) + 4, 4),
	      "axis 1 of location 0's place, its y, is set, and nothing else changes");
	Check(GameLocation(chunk.bytes, 0).position.x == 1 && GameLocation(chunk.bytes, 0).position.y == 16777216 &&
	      GameLocation(chunk.bytes, 0).position.z == 3,
	      "the game, taking location 0 as its own structure, finds the new y and the old x and z");

	before = chunk.bytes;
	why = 0;
	Check(!SetAlternateLocationEntryField(*turnX, chunk.data(), chunk.length, 0, 2.5, &why) && why == turnX->why && chunk.bytes == before,
	      "a part of a location's turn past 2 is refused with the turn's own reason, and nothing changes");
	why = 0;
	Check(!SetAlternateLocationEntryField(*position, chunk.data(), chunk.length, 0, 5, &why) && why == 0 && chunk.bytes == before,
	      "a location's place can't be set from one number, and nothing changes");
	why = 0;
	Check(!SetAlternateLocationEntryFieldAxis(*position, chunk.data(), chunk.length, 0, 2, -16777217, &why) && why == position->why &&
	      chunk.bytes == before,
	      "an axis past 2^24 is refused with the place's own reason, and nothing changes");
	Check(!SetAlternateLocationEntryFieldAxis(*position, chunk.data(), chunk.length, 0, 3, 5, &why) &&
	      !SetAlternateLocationEntryFieldAxis(*turnX, chunk.data(), chunk.length, 0, 0, 1, &why) && chunk.bytes == before,
	      "axis 3 isn't an axis, and a part of a turn isn't a place: the axis setter refuses both");
	Check(!SetAlternateLocationEntryField(*turnX, chunk.data(), chunk.length, 2, 0.5, &why) &&
	      !SetAlternateLocationEntryFieldAxis(*position, chunk.data(), chunk.length, 2, 0, 5, &why) && chunk.bytes == before,
	      "location 2 of two isn't there: neither setter sets anything in it");
}

// Every field of the alternate locations' tables, in every location, set once: the setter writes exactly where the place function says
static void EveryAlternateLocationFieldIsSetAtItsPlace()
{
	printf("Every field of an object's alternate locations is set exactly where its place function puts it:\n");

	bool exact = true;
	size_t setsMade = 0;

	for (size_t f = 0; f < AlternateLocationFieldCount; f++)
	{
		Field const &field = AlternateLocationFields[f];

		if (field.unit == Unit::ListCount)
		{
			continue;
		}

		Payload chunk(GameLocations(77, 66, TwoLocations()));
		std::vector<uint8_t> const before = chunk.bytes;
		double const value = NewValueFor(field);
		size_t place = 0;

		exact = AlternateLocationFieldPlace(field, chunk.data(), chunk.length, &place) &&
		        SetAlternateLocationField(field, chunk.data(), chunk.length, value, 0) && HoldsValue(field, chunk.bytes, place, value) &&
		        OnlyChanged(before, chunk.bytes, place, StorageBytes(field.storage)) && exact;
		setsMade++;
	}

	for (size_t k = 0; k < 2; k++)
	{
		for (size_t f = 0; f < AlternateLocationEntryFieldCount; f++)
		{
			Field const &field = AlternateLocationEntryFields[f];
			Payload chunk(GameLocations(77, 66, TwoLocations()));
			size_t place = 0;

			if (!AlternateLocationEntryFieldPlace(field, chunk.data(), chunk.length, k, &place))
			{
				exact = false;
				continue;
			}

			if (field.storage == Storage::IntTriple)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					std::vector<uint8_t> const before = chunk.bytes;
					int32_t const value = -16777216 + 1000 * (int32_t)k + axis;   // within 2^24, and no location holds it

					exact = SetAlternateLocationEntryFieldAxis(field, chunk.data(), chunk.length, k, axis, value, 0) &&
					        GetInt(chunk.bytes, place + 4 * axis) == value && OnlyChanged(before, chunk.bytes, place + 4 * axis, 4) && exact;
					setsMade++;
				}
			}
			else
			{
				std::vector<uint8_t> const before = chunk.bytes;
				double const value = NewValueFor(field);

				exact = SetAlternateLocationEntryField(field, chunk.data(), chunk.length, k, value, 0) &&
				        HoldsValue(field, chunk.bytes, place, value) && OnlyChanged(before, chunk.bytes, place, StorageBytes(field.storage)) &&
				        exact;
				setsMade++;
			}
		}
	}

	Check(exact && setsMade == 20,
	      "all 20 sets (the group, the spare, and in each of 2 locations its 6 single numbers and its place an axis at a time) wrote exactly "
	      "the field's own bytes at the place function's place, and nothing else");
}

static void AlternateSettersRefuseWhatIsntTheirs()
{
	printf("The alternate locations' setters refuse another table's field, a payload the game wouldn't read, and no payload:\n");

	Field const *const group = FindAlternateLocationField("group");
	Field const *const position = FindAlternateLocationEntryField("position");
	Field const *const turnX = FindAlternateLocationEntryField("orientation_x");
	Field const *const timer = FindTrackField("timer_start");
	Field const *const pivotStart = FindTrackSectionField("pivot_start");

	if (!group || !position || !turnX || !timer || !pivotStart)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Payload chunk(GameLocations(77, 66, TwoLocations()));
	std::vector<uint8_t> const before = chunk.bytes;
	char const *why = 0;

	Check(!SetAlternateLocationField(*turnX, chunk.data(), chunk.length, 0.5, &why) && why == NotThisTablesReason && chunk.bytes == before,
	      "a location's field isn't one of the chunk's own: the chunk's setter refuses it, with the reason, and nothing changes");
	why = 0;
	Check(!SetAlternateLocationField(*timer, chunk.data(), chunk.length, 100, &why) && why == NotThisTablesReason && chunk.bytes == before,
	      "a track's field isn't one of the chunk's own either: refused, with the reason");
	why = 0;
	Check(!SetAlternateLocationEntryField(*group, chunk.data(), chunk.length, 0, 1, &why) && why == NotThisTablesReason && chunk.bytes == before,
	      "the chunk's own group isn't one of a location's: the location setter refuses it, with the reason");
	why = 0;
	Check(!SetAlternateLocationEntryFieldAxis(*pivotStart, chunk.data(), chunk.length, 0, 0, 5, &why) && why == NotThisTablesReason &&
	      chunk.bytes == before,
	      "a track section's place isn't one of a location's: the axis setter refuses it, with the reason");

	// Payloads the game's reader would refuse for their size: under the 8 bytes it reads at least, no room for the count, more locations
	// than the bytes hold, a negative count, a count whose size wraps round in 32 bits. No setter writes anything in any of them.
	struct Hostile
	{
		int32_t count;
		size_t length;
	};

	Hostile const hostiles[] = { { 0, 7 }, { 0, 11 }, { 3, 119 }, { -1, 48 }, { 119304648, 48 } };
	bool untouched = true;

	for (size_t h = 0; h < sizeof(hostiles) / sizeof(hostiles[0]); h++)
	{
		Payload bad(Locations(hostiles[h].count, hostiles[h].length));
		std::vector<uint8_t> const was = bad.bytes;

		untouched = !SetAlternateLocationField(*group, bad.data(), bad.length, 5, &why) &&
		            !SetAlternateLocationEntryField(*turnX, bad.data(), bad.length, 0, 0.5, &why) &&
		            !SetAlternateLocationEntryFieldAxis(*position, bad.data(), bad.length, 0, 0, 5, &why) && bad.bytes == was && untouched;
	}

	Check(untouched, "in alternate locations the game's reader would refuse for their size, no setter writes a byte");

	bool refused = false;
	bool ran = Survives([&] { refused = !SetAlternateLocationField(*group, 0, 48, 5, &why); });
	Check(ran && refused, "the chunk's own setter refuses no payload with a length said, and reads nothing");
	refused = false;
	ran = Survives([&] { refused = !SetAlternateLocationEntryField(*turnX, 0, 48, 0, 0.5, &why); });
	Check(ran && refused, "the location setter refuses no payload with a length said, and reads nothing");
	refused = false;
	ran = Survives([&] { refused = !SetAlternateLocationEntryFieldAxis(*position, 0, 48, 0, 0, 5, &why); });
	Check(ran && refused, "and so does the location's axis setter");
}

static void OldTracksAreUpgradedAsTheGameDoes()
{
	printf("An old track is upgraded exactly as the game's reader upgrades it:\n");

	// An old track (bit 31 clear) with three sections, loop, playing at start and smoothing (0xB), and one byte after the timer, which
	// the game never reads
	std::vector<GameTrackSection> sections = ThreeSections();
	uint32_t flags = 0xBu;
	std::vector<uint8_t> old = GameTrack(sections, flags, 4096);
	old.push_back(0x5A);
	Payload track(old);

	// What the game's reader makes of the same track: its own upgrade, on its own structures, then written as the game writes a track
	GameUpgrade(sections, flags);
	std::vector<uint8_t> upgraded = GameTrack(sections, flags, 4096);
	upgraded.push_back(0x5A);
	Payload expected(upgraded);

	Check(UpgradeOldTrack(track.data(), track.length), "an old track, whose bit 31 is clear, is upgraded, and the editor says it changed it");
	Check(track.bytes == expected.bytes,
	      "every byte is what the game's reader makes of it: in each of the three sections, each turn's new x is minus its old w, y is x, z "
	      "is y and w is minus z, and bit 31 is set");
	Check(GetUInt(track.bytes, SectionAt(3)) == 0x8000000Bu, "the flags keep loop, playing at start and smoothing, and gain bit 31");

	// Nothing but the turns (each section's first 32 bytes) and the flags changed
	bool onlyTurnsAndFlags = true;

	for (size_t i = 0; i < old.size(); i++)
	{
		bool const inSections = i >= SectionAt(0) && i < SectionAt(3);
		bool const isTurn = inSections && (i - SectionAt(0)) % sizeof(GameTrackSection) < 32;
		bool const isFlags = i >= SectionAt(3) && i < SectionAt(3) + 4;

		onlyTurnsAndFlags = (isTurn || isFlags || track.bytes[i] == old[i]) && onlyTurnsAndFlags;
	}

	Check(onlyTurnsAndFlags, "nothing else changed: the count, the places, the times, the spares, the timer and the byte after it");

	std::vector<uint8_t> const once = track.bytes;
	Check(!UpgradeOldTrack(track.data(), track.length) && track.bytes == once,
	      "an upgraded track isn't upgraded again: the game's reader leaves a track with bit 31 as it is");

	Payload sorted(GameTrack(ThreeSections(), 0x80000001u, 0));
	std::vector<uint8_t> const sortedBefore = sorted.bytes;
	Check(!UpgradeOldTrack(sorted.data(), sorted.length) && sorted.bytes == sortedBefore, "a track that has bit 31 from the start is left exactly as it is");

	Payload empty(GameTrack(std::vector<GameTrackSection>(), 0x1u, 0));
	Check(UpgradeOldTrack(empty.data(), empty.length) && empty.length == 12 && GetInt(empty.bytes, 0) == 0 &&
	      GetUInt(empty.bytes, 4) == 0x80000001u && GetInt(empty.bytes, 8) == 0,
	      "an old track with no sections gets bit 31 and nothing else, as the game's reader gives it the bit with no turns to upgrade");

	// A minus 0. The turn (0, 0, 0, 1) becomes (-1, 0, 0, minus 0): a minus sign on a fraction turns its sign bit over, so the minus of 0
	// is a 0 with that bit set, 0x80000000.
	std::vector<GameTrackSection> plain(1);
	memset(plain.data(), 0, sizeof(GameTrackSection));
	plain[0].quat_start = { 0.0f, 0.0f, 0.0f, 1.0f };
	plain[0].quat_end = { 0.0f, 0.0f, 0.0f, 1.0f };
	plain[0].time_for_section = 65536;
	Payload zeros(GameTrack(plain, 0, 0));
	Check(UpgradeOldTrack(zeros.data(), zeros.length) && GetUInt(zeros.bytes, 4) == FloatBits(-1.0f) && GetUInt(zeros.bytes, 8) == 0 &&
	      GetUInt(zeros.bytes, 12) == 0 && GetUInt(zeros.bytes, 16) == 0x80000000u && GetUInt(zeros.bytes, 20) == FloatBits(-1.0f) &&
	      GetUInt(zeros.bytes, 32) == 0x80000000u,
	      "the turn (0, 0, 0, 1) becomes (-1, 0, 0, minus 0) in both turns: the minus of 0 is 0 with its sign bit set, as a minus sign makes "
	      "it on the PC");

	// Payloads the game's reader would refuse for their size are left as they are
	struct Hostile
	{
		int32_t count;
		size_t length;
	};

	Hostile const hostiles[] = { { 2, 88 }, { 1, 87 }, { 0, 11 }, { -1, 88 }, { 0x40000001, 88 }, { 56512728, 88 }, { 0, 3 } };
	bool untouched = true;

	for (size_t h = 0; h < sizeof(hostiles) / sizeof(hostiles[0]); h++)
	{
		Payload bad(Track(hostiles[h].count, hostiles[h].length));
		std::vector<uint8_t> const was = bad.bytes;

		untouched = !UpgradeOldTrack(bad.data(), bad.length) && bad.bytes == was && untouched;
	}

	Check(untouched, "a track the game's reader would refuse for its size (more sections than its bytes hold, the flags and timer cut off, "
	      "a negative count, counts that wrap round in 32 bits, no room for the count) isn't upgraded, and not a byte of it changes");

	bool changed = true;
	bool ran = Survives([&] { changed = UpgradeOldTrack(0, 88); });
	Check(ran && !changed, "with no payload, but a length said, nothing is upgraded and nothing is read");
}

static void NoSetterNeedsAChoiceRule()
{
	printf("The four tables need no rule for choices:\n");

	bool noChoice = true;

	for (size_t f = 0; f < TrackFieldCount; f++)
	{
		noChoice = TrackFields[f].unit != Unit::Choice && noChoice;
	}

	for (size_t f = 0; f < TrackSectionFieldCount; f++)
	{
		noChoice = TrackSectionFields[f].unit != Unit::Choice && noChoice;
	}

	for (size_t f = 0; f < AlternateLocationFieldCount; f++)
	{
		noChoice = AlternateLocationFields[f].unit != Unit::Choice && noChoice;
	}

	for (size_t f = 0; f < AlternateLocationEntryFieldCount; f++)
	{
		noChoice = AlternateLocationEntryFields[f].unit != Unit::Choice && noChoice;
	}

	Check(noChoice, "no field of the four tables is a choice, so no setter here needs SetField's rule that a choice takes only the values it names");
}

int main()
{
	TrackOwnNumbersAreSet();
	SectionNumbersAreSet();
	SectionPlacesAreSetAnAxisAtATime();
	EveryTrackFieldIsSetAtItsPlace();
	TrackSettersRefuseWhatIsntTheirs();
	AlternateLocationNumbersAreSet();
	EveryAlternateLocationFieldIsSetAtItsPlace();
	AlternateSettersRefuseWhatIsntTheirs();
	OldTracksAreUpgradedAsTheGameDoes();
	NoSetterNeedsAChoiceRule();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
