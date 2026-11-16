// The test for src/model/tracks_readers.h: ReadTrackField, ReadTrackSectionField and ReadTrackSectionFieldAxis, which read a track's numbers
// out of a payload to show them. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds track payloads in memory as the game's own writer lays them out (the Game...
// structures below are the game's, as model_tracks_test.cpp has them), some of them hostile, and checks that each reader gives back what the
// file holds, at the place the game reads it, and refuses what it must. A check that would crash if a guard were lost runs under Survives
// (crash_guard.h), so it FAILS by name instead of stopping the program. The program gives back a non-zero result if any check failed.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "crash_guard.h"
#include "real_numbers.h"
#include "tracks_fields.h"
#include "tracks_readers.h"

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

// The game's own layout of a track section (win95/chunk_type.hpp:43-47 and :191-194, win95/chunk_object.hpp:374-390), 76 bytes
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

// A track as the game writes it (win95/chunk_object.cpp:962-988): the count, the sections, the flags and the timer's start
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

// Section number k, with a number of its own in every place, so a number read from the wrong section or the wrong place can't pass for the right
static GameTrackSection SectionNumber(int k)
{
	GameTrackSection s = {};

	s.quat_start = GameQuat{ 0.25f + (float)k, 0.5f + (float)k, 0.75f + (float)k, 1.0f + (float)k };
	s.quat_end = GameQuat{ -0.25f - (float)k, -0.5f - (float)k, -0.75f - (float)k, -1.0f - (float)k };
	s.pivot_start = GameVectorInt{ 100 * k + 1, 100 * k + 2, 100 * k + 3 };
	s.pivot_end = GameVectorInt{ -(100 * k + 4), -(100 * k + 5), -(100 * k + 6) };
	s.object_offset = GameVectorInt{ 7 + k, 8 + k, 9 + k };
	s.time_for_section = 1000 * (k + 1) + 65;
	s.spare = 77 + k;
	return s;
}

struct ScalarSaid
{
	char const *name;
	double value;
};

struct TripleSaid
{
	char const *name;
	GameVectorInt value;
};

// What the game holds in section k, field by field, by the field's name: the numbers a reader of one number gives, and those of a place
static std::vector<ScalarSaid> ScalarsOf(GameTrackSection const &s)
{
	std::vector<ScalarSaid> said;

	said.push_back(ScalarSaid{ "quat_start_x", s.quat_start.x });
	said.push_back(ScalarSaid{ "quat_start_y", s.quat_start.y });
	said.push_back(ScalarSaid{ "quat_start_z", s.quat_start.z });
	said.push_back(ScalarSaid{ "quat_start_w", s.quat_start.w });
	said.push_back(ScalarSaid{ "quat_end_x", s.quat_end.x });
	said.push_back(ScalarSaid{ "quat_end_y", s.quat_end.y });
	said.push_back(ScalarSaid{ "quat_end_z", s.quat_end.z });
	said.push_back(ScalarSaid{ "quat_end_w", s.quat_end.w });
	said.push_back(ScalarSaid{ "time_for_section", (double)s.time_for_section });
	said.push_back(ScalarSaid{ "spare", (double)s.spare });
	return said;
}

static std::vector<TripleSaid> TriplesOf(GameTrackSection const &s)
{
	std::vector<TripleSaid> said;

	said.push_back(TripleSaid{ "pivot_start", s.pivot_start });
	said.push_back(TripleSaid{ "pivot_end", s.pivot_end });
	said.push_back(TripleSaid{ "object_offset", s.object_offset });
	return said;
}

static int32_t const Flags = (int32_t)0x80000005u;   // the game's own bit 31, and "Loop Track" and "Loop back and forth"
static int32_t const TimerStart = 1234567;

static std::vector<GameTrackSection> ThreeSections()
{
	std::vector<GameTrackSection> sections;

	for (int k = 0; k < 3; k++)
	{
		sections.push_back(SectionNumber(k));
	}

	return sections;
}

static void ReadsWhatTheGameWrote()
{
	printf("Each reader gives back what the game wrote, where it wrote it:\n");

	std::vector<uint8_t> const bytes = GameTrack(ThreeSections(), Flags, TimerStart);
	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");
	Field const *const count = FindTrackField("num_sections");

	Check(flags && timer && count, "the track's own fields are in the table");

	if (!flags || !timer || !count)
	{
		return;
	}

	double value = -1;

	Check(ReadTrackField(*timer, bytes.data(), bytes.size(), &value) && value == TimerStart, "the timer's start is read after the last section");
	Check(ReadTrackField(*count, bytes.data(), bytes.size(), &value) && value == 3, "the count of sections is read from the start");
	Check(ReadTrackField(*flags, bytes.data(), bytes.size(), &value) && value == 5,
	      "the flags are read with the four bits a mapper chooses: bit 31, the game's own, is left out (the mask is applied, nothing is shifted)");

	std::vector<ScalarSaid> const sampleScalars = ScalarsOf(SectionNumber(0));
	std::vector<TripleSaid> const sampleTriples = TriplesOf(SectionNumber(0));

	Check(sampleScalars.size() + sampleTriples.size() == TrackSectionFieldCount,
	      "the test says every field of a section (a field added to the table is said here too, or this fails)");

	bool scalarsRight = true;
	bool triplesRight = true;

	for (int k = 0; k < 3; k++)
	{
		for (ScalarSaid const &said : ScalarsOf(SectionNumber(k)))
		{
			Field const *const field = FindTrackSectionField(said.name);
			double got = -12345;

			scalarsRight = scalarsRight && field && ReadTrackSectionField(*field, bytes.data(), bytes.size(), (size_t)k, &got) && got == said.value;
		}

		for (TripleSaid const &said : TriplesOf(SectionNumber(k)))
		{
			Field const *const field = FindTrackSectionField(said.name);
			int32_t got[3] = { -1, -1, -1 };
			bool read = field != 0;

			for (int axis = 0; read && axis < 3; axis++)
			{
				read = ReadTrackSectionFieldAxis(*field, bytes.data(), bytes.size(), (size_t)k, axis, &got[axis]);
			}

			triplesRight = triplesRight && read && got[0] == said.value.x && got[1] == said.value.y && got[2] == said.value.z;
		}
	}

	Check(scalarsRight, "each of the 3 sections' " + std::to_string(sampleScalars.size()) +
	      " one-number fields reads as that section holds it (a turn's eight parts, the time, the spare)");
	Check(triplesRight, "each section's " + std::to_string(sampleTriples.size()) + " places read x, y and z as that section holds them, section by section");

	// The same payload with bytes after the timer: the game ignores them, and so do the readers
	std::vector<uint8_t> longer = bytes;
	longer.resize(longer.size() + 16, 0xEE);
	Field const *const time = FindTrackSectionField("time_for_section");

	Check(time && ReadTrackField(*timer, longer.data(), longer.size(), &value) && value == TimerStart &&
	      ReadTrackSectionField(*time, longer.data(), longer.size(), 2, &value) && value == 1000 * 3 + 65,
	      "bytes after the timer's start are ignored, as the game ignores them");
}

static void EveryFieldReadsAsWhatItIs()
{
	printf("Every field of the tables reads as the kind of number it is:\n");

	std::vector<uint8_t> const bytes = GameTrack(ThreeSections(), Flags, TimerStart);
	bool ownOk = true;
	bool sectionOk = true;
	bool tripleOk = true;

	for (size_t f = 0; f < TrackFieldCount; f++)
	{
		double value = 0;

		// every field of the track's own table is one number
		ownOk = ownOk && ReadTrackField(TrackFields[f], bytes.data(), bytes.size(), &value);
	}

	for (size_t f = 0; f < TrackSectionFieldCount; f++)
	{
		Field const &field = TrackSectionFields[f];
		double value = 0;
		int32_t axis = 0;
		bool const isTriple = field.storage == Storage::IntTriple;

		// A field of three numbers is read by its axes and not as one number; every other field is read as one number and has no axes
		sectionOk = sectionOk && ReadTrackSectionField(field, bytes.data(), bytes.size(), 1, &value) == !isTriple;
		tripleOk = tripleOk && ReadTrackSectionFieldAxis(field, bytes.data(), bytes.size(), 1, 0, &axis) == isTriple;
	}

	Check(ownOk, "all " + std::to_string(TrackFieldCount) + " of the track's own fields read as one number");
	Check(sectionOk, "a section's field of three numbers isn't read as one number, and every other one is");
	Check(tripleOk, "a section's field that isn't of three numbers has no axes, and each of the three-number ones has");
}

static void RefusesWhatItMust()
{
	printf("What the readers refuse (and leave the value alone):\n");

	std::vector<uint8_t> const bytes = GameTrack(ThreeSections(), Flags, TimerStart);
	Field const *const flags = FindTrackField("flags");
	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const pivot = FindTrackSectionField("pivot_start");

	if (!flags || !time || !pivot)
	{
		Check(false, "the fields this part reads are in the tables");
		return;
	}

	double value = 77;
	int32_t axis = 55;

	Check(!ReadTrackField(*time, bytes.data(), bytes.size(), &value) && value == 77,
	      "a section's field isn't one of the track's own, and is refused by the track's reader");
	Check(!ReadTrackSectionField(*flags, bytes.data(), bytes.size(), 0, &value) && value == 77,
	      "the track's own field isn't a section's, and is refused by the section reader (it would be read at its own offset inside the first section)");
	Check(!ReadTrackSectionFieldAxis(*flags, bytes.data(), bytes.size(), 0, 0, &axis) && axis == 55,
	      "and so is it by the axis reader");

	Field const copyOfFlags = *flags;
	Field const copyOfTime = *time;

	Check(!ReadTrackField(copyOfFlags, bytes.data(), bytes.size(), &value) && !ReadTrackSectionField(copyOfTime, bytes.data(), bytes.size(), 0, &value) &&
	      value == 77, "a copy of a field of the table is no field of the table: refused, so no field of another table is read by its look-alike");

	Check(!ReadTrackSectionField(*time, bytes.data(), bytes.size(), 3, &value) && !ReadTrackSectionField(*time, bytes.data(), bytes.size(), 1000, &value) &&
	      !ReadTrackSectionField(*time, bytes.data(), bytes.size(), (size_t)-1, &value) && value == 77,
	      "a section the track doesn't hold (3 of 3, 1000, the most a size can say) is refused");
	Check(!ReadTrackSectionFieldAxis(*pivot, bytes.data(), bytes.size(), 3, 0, &axis) && !ReadTrackSectionFieldAxis(*pivot, bytes.data(), bytes.size(), (size_t)-1, 0, &axis) &&
	      axis == 55, "and so is it by the axis reader");
	Check(!ReadTrackSectionFieldAxis(*pivot, bytes.data(), bytes.size(), 0, 3, &axis) && !ReadTrackSectionFieldAxis(*pivot, bytes.data(), bytes.size(), 0, -1, &axis) &&
	      axis == 55, "a fourth axis, and one below the first, are refused");

	// A payload that isn't a size the game reads
	std::vector<uint8_t> shortBy1(bytes.begin(), bytes.end() - 1);
	Field const *const timer = FindTrackField("timer_start");

	Check(!ReadTrackField(*timer, shortBy1.data(), shortBy1.size(), &value) && !ReadTrackField(*flags, shortBy1.data(), shortBy1.size(), &value) &&
	      !ReadTrackSectionField(*time, shortBy1.data(), shortBy1.size(), 0, &value) && value == 77,
	      "a track one byte short of its timer's start isn't a size the game reads: nothing in it is read");
	Check(!ReadTrackField(*flags, bytes.data(), 0, &value) && !ReadTrackSectionField(*time, bytes.data(), 0, 0, &value) && value == 77,
	      "an empty payload is refused");

	// Nothing handed over. Each check that would read through a null pointer if its guard were gone runs under Survives, so a lost guard makes
	// ITS check FAILED, by name, instead of stopping the program.
	bool refused = false;
	bool ran = Survives([&] { refused = !ReadTrackField(*flags, 0, bytes.size(), &value); });
	Check(ran && refused && value == 77, "no payload, with a length said, is refused by the track's reader");
	refused = false;
	ran = Survives([&] { refused = !ReadTrackSectionField(*time, 0, bytes.size(), 0, &value); });
	Check(ran && refused && value == 77, "and by the section reader");
	refused = false;
	ran = Survives([&] { refused = !ReadTrackSectionFieldAxis(*pivot, 0, bytes.size(), 0, 0, &axis); });
	Check(ran && refused && axis == 55, "and by the axis reader");

	refused = false;
	ran = Survives([&] { refused = !ReadTrackField(*flags, bytes.data(), bytes.size(), 0); });
	Check(ran && refused, "no place for the value is refused by the track's reader");
	refused = false;
	ran = Survives([&] { refused = !ReadTrackSectionField(*time, bytes.data(), bytes.size(), 0, 0); });
	Check(ran && refused, "and by the section reader");
	refused = false;
	ran = Survives([&] { refused = !ReadTrackSectionFieldAxis(*pivot, bytes.data(), bytes.size(), 0, 0, 0); });
	Check(ran && refused, "and by the axis reader");
}

// Counts a level could hold that a size can't carry, in payloads of every size from nothing to past a track of 3 sections: no reader reads
// past the payload, and each gives a value exactly when the track is a size the game reads and holds the section
static void HostileCountsAndSizes()
{
	printf("Hostile counts, in payloads of every size:\n");

	int32_t const counts[] = { 0, 1, 2, 3, 4, -1, (int32_t)0x80000000u, 0x7FFFFFFF, 0x40000001, 56512728, 119304648, 0x00FFFFFF };
	size_t const sections[] = { 0, 1, 2, 3, 4, 1000, (size_t)-1 };
	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");
	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const pivot = FindTrackSectionField("pivot_end");

	if (!flags || !timer || !time || !pivot)
	{
		Check(false, "the fields this part reads are in the tables");
		return;
	}

	bool survived = true;
	bool agreed = true;
	size_t tried = 0;

	for (int32_t const count : counts)
	{
		for (size_t length = 0; length <= 4 + 3 * sizeof(GameTrackSection) + 8 + 12; length++)
		{
			std::vector<uint8_t> bytes(length, 0x5A);

			if (length >= 4)
			{
				PutInt(bytes, 0, count);
			}

			size_t held = 0;
			bool const sizeRight = TrackSectionCount(bytes.data(), bytes.size(), &held);

			survived = survived && Survives([&]
			{
				double value = 0;
				int32_t one = 0;

				agreed = agreed && ReadTrackField(*flags, bytes.data(), bytes.size(), &value) == sizeRight;
				agreed = agreed && ReadTrackField(*timer, bytes.data(), bytes.size(), &value) == sizeRight;

				for (size_t section : sections)
				{
					bool const there = sizeRight && section < held;

					agreed = agreed && ReadTrackSectionField(*time, bytes.data(), bytes.size(), section, &value) == there;
					agreed = agreed && ReadTrackSectionFieldAxis(*pivot, bytes.data(), bytes.size(), section, 2, &one) == there;
					tried += 2;
				}
			});
		}
	}

	Check(survived, "no reader stopped the program, for " + std::to_string(sizeof(counts) / sizeof(counts[0])) + " counts, in payloads of every size up to past a track of 3 sections");
	Check(agreed && tried > 1000, "each reader gave a value exactly when the track is a size the game reads and holds that section (" + std::to_string(tried) + " section reads)");
}

static void ANumberThatIsNotARealOneIsKept()
{
	printf("What the file holds is given back as it is:\n");

	std::vector<GameTrackSection> sections = ThreeSections();
	uint32_t const notANumber = 0x7FC00001u;
	uint32_t const endless = 0x7F800000u;

	memcpy(&sections[0].quat_start.x, &notANumber, 4);
	memcpy(&sections[0].quat_start.y, &endless, 4);
	sections[0].time_for_section = -5;

	std::vector<uint8_t> const bytes = GameTrack(sections, (int32_t)0xFFFFFFFFu, -9);
	Field const *const x = FindTrackSectionField("quat_start_x");
	Field const *const y = FindTrackSectionField("quat_start_y");
	Field const *const time = FindTrackSectionField("time_for_section");
	Field const *const flags = FindTrackField("flags");
	Field const *const timer = FindTrackField("timer_start");

	if (!x || !y || !time || !flags || !timer)
	{
		Check(false, "the fields this part reads are in the tables");
		return;
	}

	double value = 0;

	Check(ReadTrackSectionField(*x, bytes.data(), bytes.size(), 0, &value) && IsNaN(value), "a turn's part that is 'not a number' is read as it is");
	Check(ReadTrackSectionField(*y, bytes.data(), bytes.size(), 0, &value) && IsNaNOrEndless(value) && !IsNaN(value), "and an endless one");
	Check(ReadTrackSectionField(*time, bytes.data(), bytes.size(), 0, &value) && value == -5,
	      "a time below the loader's least (the loader would refuse the level) is read as it is");
	Check(ReadTrackField(*timer, bytes.data(), bytes.size(), &value) && value == -9, "so is a timer's start below 0");
	Check(ReadTrackField(*flags, bytes.data(), bytes.size(), &value) && value == 15,
	      "flags with every bit set read as the four bits a mapper chooses (the mask is applied, and bit 31 is left out)");
}

int main()
{
	ReadsWhatTheGameWrote();
	EveryFieldReadsAsWhatItIs();
	RefusesWhatItMust();
	HostileCountsAndSizes();
	ANumberThatIsNotARealOneIsKept();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
