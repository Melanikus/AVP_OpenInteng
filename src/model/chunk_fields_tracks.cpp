// The tables and the size and place rules for the chunks that hold a list of items: OBJTRAK2 (a moving object's track) and ALTLOCAT (the
// other places an object may start in). Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code". tracks_fields.h says what is here, and why these chunks have tables of their own.
//
// Every layout and limit was read from the game's own reader at the dev pin the editor builds against, AvPx 491deb22 (branch
// editor-dev-pin-491deb22 of the AvP repository); paths are under its 3dc folder. The labels are
// the old editor's own words where it had them (vasquez-ui.md, sections 2.4 and 2.6, from the strings of Rebellion's editor); a field it
// never labelled has the label "", which the panel skips, and its bytes are always kept. The old editor's starting values for a new track
// or location aren't known, so no field here claims one.
//
// A LIMIT is labelled by where it comes from. Limit::Loader is a rule that pin's loader has: it refuses a level that breaks it. Every
// limit in these tables is one of those.

#include <string.h>

#include "field_bytes.h"
#include "tracks_fields.h"

namespace model
{
	namespace
	{
		double const PositionMost = 16777216.0;   // 2^24 game units from the level's middle, along each axis
		double const TurnMost = 2.0;              // each part of a turn is from -2 to 2
		double const TimerMost = 16777216.0;      // 1 << 24: the loader's latest start for a track's timer (chunk_object.cpp:1022)
		double const IntMost = 2147483647.0;      // the most a stored whole number holds

		char const SectionTimeWhy[] =
			"the loader refuses a section shorter than 65 (about a thousandth of a second): the game works through the timer one section at "
			"a time, so one that took no time would hang it";
		char const TimerStartWhy[] =
			"the loader refuses a timer start below 0 or past 2^24 (256 seconds), since a negative or huge start could keep the game "
			"working through the sections for ever";
		char const PlaceWhy[] =
			"a place is within 2^24 game units of the level's middle on each axis: the loader refuses a level "
			"with a farther one (CheckPlace)";
		char const TurnWhy[] =
			"each part of a turn is a real number from -2 to 2 (a real turn's are from -1 to 1): the loader refuses a level with any "
			"other (CheckTurn)";
		char const TrackFlagsWhy[] =
			"the game sets bit 31 (QuatProblemSorted) itself when it loads a track (chunk_object.cpp:1045-1062), so the editor keeps it "
			"exactly as the file has it; a mapper chooses only the four named bits";

		char const TrackCountSource[] = "win95/chunk_object.cpp:1002; the count's rule is ReadListCount, win95/chunk.hpp:384-398";
		char const TrackFlagsSource[] = "win95/chunk_object.cpp:1015; the bits are win95/chunk_object.hpp:392-396";
		char const TrackTimerSource[] = "win95/chunk_object.cpp:1016, :1022, :1040-1043";
		char const SectionTimeSource[] = "win95/chunk_object.cpp:1018-1029";
		char const SectionSpareSource[] = "win95/chunk_object.hpp:387; the game never reads it";
		char const SectionPlaceSource[] =
			"layout: win95/chunk_object.hpp:380-383; limit: CheckPlace (win95/chunk.hpp:492-510), called at "
			"win95/chunk_object.cpp:1033-1035";
		char const SectionTurnSource[] =
			"layout: win95/chunk_object.hpp:377-378 (a turn is win95/chunk_type.hpp:191-194); limit: CheckTurn (win95/chunk.hpp:529-546), "
			"called at win95/chunk_object.cpp:1036-1037";

		char const AlternateGroupSource[] =
			"win95/chunk_object.cpp:1154; win95/chunk_object.hpp:470 (objects with the same group, other than 0, share one die roll)";
		char const AlternateSpareSource[] =
			"win95/chunk_object.cpp:1156; win95/chunk_object.hpp:472; the game never reads it (avp/win95/object_setup.cpp:626-650 uses only the "
			"group, the count, the places and the turns)";
		char const AlternateCountSource[] = "win95/chunk_object.cpp:1159; the count's rule is ReadListCount, win95/chunk.hpp:384-398";
		char const LocationPlaceSource[] =
			"layout: win95/chunk_object.hpp:450; limit: CheckPlace (win95/chunk.hpp:492-510), called at "
			"win95/chunk_object.cpp:1169";
		char const LocationTurnSource[] =
			"layout: win95/chunk_object.hpp:451 (a turn is win95/chunk_type.hpp:191-194); limit: CheckTurn (win95/chunk.hpp:529-546), "
			"called at win95/chunk_object.cpp:1170";
		char const LocationSpareSource[] =
			"win95/chunk_object.hpp:452; the game never reads it (avp/win95/object_setup.cpp:626-650 uses only the group, the count, the places "
			"and the turns)";

		// The bits of a track's flags a mapper may choose. Bit 31 isn't one: the game sets it itself (TrackFlagsWhy), so it's kept as it is.
		uint32_t const TrackChoiceBits = 0x1 | 0x2 | 0x4 | 0x8;   // TrackFlag_Loop, _PlayingAtStart, _LoopBackAndForth, _UseTrackSmoothing

		const Anchor S = Anchor::Start;
		const Anchor AfterList = Anchor::AfterFirstList;

		// How one of these chunks is laid out: where its list's count is, how big an item is, and what the game reads around them.
		struct ListLayout
		{
			size_t leastBytes;     // the smallest payload the game reads, from its registration
			size_t countAt;        // where the list's count is
			size_t itemBytes;      // the size of one item
			size_t trailerBytes;   // the bytes the game reads straight after the last item
		};

		// OBJTRAK2 isn't registered with a smallest size; its count is first, and the game reads its flags and the timer's start after the
		// sections (chunk_object.cpp:947, :1002, :1015-1016).
		ListLayout const TrackLayout = { 0, 0, TrackSectionBytes, 8 };

		// ALTLOCAT is registered with a smallest size of 8 (chunk_object.cpp:1147); its count is the third number, at +8, and the game reads
		// nothing after the locations (chunk_object.cpp:1149-1173).
		ListLayout const AlternateLocationsLayout = { 8, 8, AlternateLocationBytes, 0 };

		bool IsOneOf(Field const *table, size_t count, Field const &field)
		{
			for (size_t i = 0; i < count; i++)
			{
				if (&table[i] == &field)
				{
					return true;
				}
			}

			return false;
		}

		Field const *FindIn(Field const *table, size_t count, char const *name)
		{
			if (!name)
			{
				return 0;
			}

			for (size_t i = 0; i < count; i++)
			{
				if (strcmp(table[i].name, name) == 0)
				{
					return &table[i];
				}
			}

			return 0;
		}

		// The game's checks of a list's size, in the order its reader makes them: the smallest size it's registered with, then the count
		// (ReadListCount: there, not negative, no more than the bytes after it hold, worked out so nothing wraps round), then whatever it
		// reads straight after the last item (ReadFromChunk: refused when it isn't there). *count is set when the size is right.
		SizeCheck CheckListSize(ListLayout const &layout, uint8_t const *payload, size_t length, size_t *count)
		{
			if (length < layout.leastBytes)
			{
				return SizeCheck::TooShort;
			}

			size_t found = 0;
			const SizeCheck counted = detail::ReadCount(payload, length, layout.countAt, layout.itemBytes, &found);

			if (counted != SizeCheck::Right)
			{
				return counted;
			}

			// ReadCount has shown that found items fit in the bytes after the count, so this sum can't be past the payload's end.
			size_t const listEnd = layout.countAt + 4 + found * layout.itemBytes;

			if (length - listEnd < layout.trailerBytes)
			{
				return SizeCheck::TooShort;
			}

			if (count)
			{
				*count = found;
			}

			return SizeCheck::Right;
		}

		// Where item number `item` starts. Refuses a payload the game would refuse, and an item the list doesn't hold.
		bool ItemPlace(ListLayout const &layout, uint8_t const *payload, size_t length, size_t item, size_t *place)
		{
			size_t count = 0;

			if (CheckListSize(layout, payload, length, &count) != SizeCheck::Right || item >= count)
			{
				return false;
			}

			// item is below count, and count items fit in the payload, so this can't wrap round.
			*place = layout.countAt + 4 + item * layout.itemBytes;
			return true;
		}

		// Where one of the chunk's own fields starts: counted from the payload's start, or from the end of the list.
		bool OwnFieldPlace(ListLayout const &layout, Field const *table, size_t tableCount, Field const &field, uint8_t const *payload,
		                   size_t length, size_t *place)
		{
			size_t count = 0;

			if (!IsOneOf(table, tableCount, field) || CheckListSize(layout, payload, length, &count) != SizeCheck::Right)
			{
				return false;
			}

			size_t at = 0;

			switch (field.anchor)
			{
			case Anchor::Start:
				at = field.offset;
				break;

			case Anchor::AfterFirstList:
				at = layout.countAt + 4 + count * layout.itemBytes + field.offset;
				break;

			default:
				return false;
			}

			// No check that the field ends inside the payload is needed: CheckListSize has shown the payload holds the count and the
			// bytes the game reads after the last item, and the test shows every field of the tables lies inside those (and that no place
			// it gives is ever past the payload's end, for payloads of every size and hostile counts).
			*place = at;
			return true;
		}

		// Where one of an item's fields starts. Every field of an item table lies inside one item (the test shows it), so it can never be
		// found in the next one, and an item the list holds lies inside the payload.
		bool ItemFieldPlace(ListLayout const &layout, Field const *table, size_t tableCount, Field const &field, uint8_t const *payload,
		                    size_t length, size_t item, size_t *place)
		{
			size_t start = 0;

			if (!IsOneOf(table, tableCount, field) || !ItemPlace(layout, payload, length, item, &start))
			{
				return false;
			}

			*place = start + field.offset;
			return true;
		}
	}

	// OBJTRAK2: the track of a moving object. The payload is the count, the sections, the flags and the timer's start.
	Field const TrackFields[] =
	{
		{ "num_sections", "", S, 0, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0, "", false, 0, TrackCountSource },
		{ "flags", "", AfterList, 0, Storage::UInt32, Unit::Flags, TrackChoiceBits, Limit::None, 0, 0, TrackFlagsWhy, false, 0, TrackFlagsSource },
		{ "timer_start", "Timer start (seconds)", AfterList, 4, Storage::Int32, Unit::Fixed16Seconds, 0, Limit::Loader, 0, TimerMost,
		  TimerStartWhy, false, 0, TrackTimerSource },
	};

	size_t const TrackFieldCount = sizeof(TrackFields) / sizeof(TrackFields[0]);

	// One section of a track, 76 bytes: two turns, three places, how long it takes, and a spare number. Offsets are from the section's start.
	Field const TrackSectionFields[] =
	{
		{ "quat_start_x", "", S, 0, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_start_y", "", S, 4, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_start_z", "", S, 8, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_start_w", "", S, 12, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_end_x", "", S, 16, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_end_y", "", S, 20, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_end_z", "", S, 24, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "quat_end_w", "", S, 28, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, SectionTurnSource },
		{ "pivot_start", "", S, 32, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PlaceWhy, false, 0,
		  SectionPlaceSource },
		{ "pivot_end", "", S, 44, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PlaceWhy, false, 0,
		  SectionPlaceSource },
		{ "object_offset", "", S, 56, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PlaceWhy, false, 0,
		  SectionPlaceSource },
		{ "time_for_section", "Time for section", S, 68, Storage::Int32, Unit::Fixed16Seconds, 0, Limit::Loader, 65, IntMost, SectionTimeWhy,
		  false, 0, SectionTimeSource },
		{ "spare", "", S, 72, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, SectionSpareSource },
	};

	size_t const TrackSectionFieldCount = sizeof(TrackSectionFields) / sizeof(TrackSectionFields[0]);

	// The old editor's words for the track's check boxes (vasquez-ui.md, 2.6). It had no box for "playing at start": those are plain words.
	ChunkBitName const TrackBits[] =
	{
		{ "OBJTRAK2", "flags", 0x1, "Loop Track" },
		{ "OBJTRAK2", "flags", 0x2, "Playing at start" },
		{ "OBJTRAK2", "flags", 0x4, "Loop back and forth" },
		{ "OBJTRAK2", "flags", 0x8, "Use Track Smoothing" },
	};

	size_t const TrackBitCount = sizeof(TrackBits) / sizeof(TrackBits[0]);

	// ALTLOCAT: the other places an object may start in. The payload is a group number, a spare number, the count, and the locations.
	Field const AlternateLocationFields[] =
	{
		{ "group", "Group number (0 means no group)", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, AlternateGroupSource },
		{ "spare2", "", S, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, AlternateSpareSource },
		{ "num_locations", "", S, 8, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0, "", false, 0, AlternateCountSource },
	};

	size_t const AlternateLocationFieldCount = sizeof(AlternateLocationFields) / sizeof(AlternateLocationFields[0]);

	// One alternate location, 36 bytes: a place, a turn and two spare numbers. Offsets are from the location's start.
	Field const AlternateLocationEntryFields[] =
	{
		{ "position", "", S, 0, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PlaceWhy, false, 0,
		  LocationPlaceSource },
		{ "orientation_x", "", S, 12, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, LocationTurnSource },
		{ "orientation_y", "", S, 16, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, LocationTurnSource },
		{ "orientation_z", "", S, 20, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, LocationTurnSource },
		{ "orientation_w", "", S, 24, Storage::Float32, Unit::Plain, 0, Limit::Loader, -TurnMost, TurnMost, TurnWhy, false, 0, LocationTurnSource },
		{ "spare1", "", S, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LocationSpareSource },
		{ "spare2", "", S, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LocationSpareSource },
	};

	size_t const AlternateLocationEntryFieldCount = sizeof(AlternateLocationEntryFields) / sizeof(AlternateLocationEntryFields[0]);

	Field const *FindTrackField(char const *name)
	{
		return FindIn(TrackFields, TrackFieldCount, name);
	}

	Field const *FindTrackSectionField(char const *name)
	{
		return FindIn(TrackSectionFields, TrackSectionFieldCount, name);
	}

	SizeCheck CheckTrackSize(uint8_t const *payload, size_t length)
	{
		return CheckListSize(TrackLayout, payload, length, 0);
	}

	bool TrackSectionCount(uint8_t const *payload, size_t length, size_t *count)
	{
		return CheckListSize(TrackLayout, payload, length, count) == SizeCheck::Right;
	}

	bool TrackSectionPlace(uint8_t const *payload, size_t length, size_t section, size_t *place)
	{
		return ItemPlace(TrackLayout, payload, length, section, place);
	}

	bool TrackFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t *place)
	{
		return OwnFieldPlace(TrackLayout, TrackFields, TrackFieldCount, field, payload, length, place);
	}

	bool TrackSectionFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t section, size_t *place)
	{
		return ItemFieldPlace(TrackLayout, TrackSectionFields, TrackSectionFieldCount, field, payload, length, section, place);
	}

	Field const *FindAlternateLocationField(char const *name)
	{
		return FindIn(AlternateLocationFields, AlternateLocationFieldCount, name);
	}

	Field const *FindAlternateLocationEntryField(char const *name)
	{
		return FindIn(AlternateLocationEntryFields, AlternateLocationEntryFieldCount, name);
	}

	SizeCheck CheckAlternateLocationsSize(uint8_t const *payload, size_t length)
	{
		return CheckListSize(AlternateLocationsLayout, payload, length, 0);
	}

	bool AlternateLocationCount(uint8_t const *payload, size_t length, size_t *count)
	{
		return CheckListSize(AlternateLocationsLayout, payload, length, count) == SizeCheck::Right;
	}

	bool AlternateLocationPlace(uint8_t const *payload, size_t length, size_t location, size_t *place)
	{
		return ItemPlace(AlternateLocationsLayout, payload, length, location, place);
	}

	bool AlternateLocationFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t *place)
	{
		return OwnFieldPlace(AlternateLocationsLayout, AlternateLocationFields, AlternateLocationFieldCount, field, payload, length, place);
	}

	bool AlternateLocationEntryFieldPlace(Field const &field, uint8_t const *payload, size_t length, size_t location, size_t *place)
	{
		return ItemFieldPlace(AlternateLocationsLayout, AlternateLocationEntryFields, AlternateLocationEntryFieldCount, field, payload, length,
		                      location, place);
	}

	// ---- Setting one number (tracks_fields.h says the rules) ----

	char const NotThisTablesReason[] = "this field isn't one of this table's own, so it has no place here";

	namespace
	{
		// A place no payload has. A setter is given it when the field has no place in this payload: the setter still checks the value
		// first, and gives the value's own reason when it isn't allowed, and then refuses, writing nothing (FitsAt, strategy_rules.cpp).
		size_t const NoPlace = (size_t)-1;

		bool RefuseOtherTablesField(char const **why)
		{
			if (why)
			{
				*why = NotThisTablesReason;
			}

			return false;
		}

		// Whether a field is one of a section's, or one of a location's. A section and a location each have two setters, one for a single
		// number and one for one axis of a place, and both ask here.
		bool IsSectionField(Field const &field)
		{
			return IsOneOf(TrackSectionFields, TrackSectionFieldCount, field);
		}

		bool IsLocationField(Field const &field)
		{
			return IsOneOf(AlternateLocationEntryFields, AlternateLocationEntryFieldCount, field);
		}

		// Where a setter writes: exactly the place the matching place function gives, or NoPlace when there's no payload, when the game's
		// reader would refuse the payload's size, or when the payload doesn't hold that section or location. Each place function checks
		// the size itself, so a count from the file is never trusted here.
		size_t TrackOwnSetterPlace(Field const &field, uint8_t const *payload, size_t length)
		{
			size_t at = 0;

			if (payload && TrackFieldPlace(field, payload, length, &at))
			{
				return at;
			}

			return NoPlace;
		}

		size_t SectionSetterPlace(Field const &field, uint8_t const *payload, size_t length, size_t section)
		{
			size_t at = 0;

			if (payload && TrackSectionFieldPlace(field, payload, length, section, &at))
			{
				return at;
			}

			return NoPlace;
		}

		size_t AlternateLocationsOwnSetterPlace(Field const &field, uint8_t const *payload, size_t length)
		{
			size_t at = 0;

			if (payload && AlternateLocationFieldPlace(field, payload, length, &at))
			{
				return at;
			}

			return NoPlace;
		}

		size_t LocationSetterPlace(Field const &field, uint8_t const *payload, size_t length, size_t location)
		{
			size_t at = 0;

			if (payload && AlternateLocationEntryFieldPlace(field, payload, length, location, &at))
			{
				return at;
			}

			return NoPlace;
		}
	}

	bool SetTrackField(Field const &field, uint8_t *payload, size_t length, double value, char const **why)
	{
		if (!IsOneOf(TrackFields, TrackFieldCount, field))
		{
			return RefuseOtherTablesField(why);
		}

		return detail::SetFieldAt(field, payload, length, TrackOwnSetterPlace(field, payload, length), value, why);
	}

	bool SetTrackSectionField(Field const &field, uint8_t *payload, size_t length, size_t section, double value, char const **why)
	{
		if (!IsSectionField(field))
		{
			return RefuseOtherTablesField(why);
		}

		return detail::SetFieldAt(field, payload, length, SectionSetterPlace(field, payload, length, section), value, why);
	}

	bool SetTrackSectionFieldAxis(Field const &field, uint8_t *payload, size_t length, size_t section, int axis, int32_t value,
	                              char const **why)
	{
		if (!IsSectionField(field))
		{
			return RefuseOtherTablesField(why);
		}

		return detail::SetAxisAt(field, payload, length, SectionSetterPlace(field, payload, length, section), axis, value, why);
	}

	bool SetAlternateLocationField(Field const &field, uint8_t *payload, size_t length, double value, char const **why)
	{
		if (!IsOneOf(AlternateLocationFields, AlternateLocationFieldCount, field))
		{
			return RefuseOtherTablesField(why);
		}

		return detail::SetFieldAt(field, payload, length, AlternateLocationsOwnSetterPlace(field, payload, length), value, why);
	}

	bool SetAlternateLocationEntryField(Field const &field, uint8_t *payload, size_t length, size_t location, double value,
	                                    char const **why)
	{
		if (!IsLocationField(field))
		{
			return RefuseOtherTablesField(why);
		}

		return detail::SetFieldAt(field, payload, length, LocationSetterPlace(field, payload, length, location), value, why);
	}

	bool SetAlternateLocationEntryFieldAxis(Field const &field, uint8_t *payload, size_t length, size_t location, int axis,
	                                        int32_t value, char const **why)
	{
		if (!IsLocationField(field))
		{
			return RefuseOtherTablesField(why);
		}

		return detail::SetAxisAt(field, payload, length, LocationSetterPlace(field, payload, length, location), axis, value, why);
	}

	// ---- The upgrade of an old track (tracks_fields.h, UpgradeOldTrack) ----

	namespace
	{
		// TrackFlag_QuatProblemSorted (win95/chunk_object.hpp:396): a track that has it holds its turns in the form the game uses now
		uint32_t const QuatProblemSorted = 0x80000000u;

		// A fraction's top bit is its sign, in the form the file keeps it (IEEE 754, the PC's own): turning that bit over is what a minus
		// sign does to it
		uint32_t const SignBit = 0x80000000u;

		// Where a section's two turns are: quat_start is its first 16 bytes, and quat_end the 16 after them (win95/chunk_object.hpp:377-378)
		size_t const StartTurnAt = 0;
		size_t const EndTurnAt = 16;

		// Rewrites one turn, the four fractions x, y, z and w that start at `at`, exactly as the game's reader does
		// (win95/chunk_object.cpp:1049-1053 and :1055-1059): the new x is minus the old w, the new y is the old x, the new z is the old y,
		// and the new w is minus the old z. Each fraction is moved as its 4 bytes, and a minus turns its sign bit over, so the result can't
		// change with how the editor is built: it's the number the game's own minus sign gives, for every real number.
		void UpgradeTurn(uint8_t *at)
		{
			uint32_t const x = detail::LoadUInt32(at);
			uint32_t const y = detail::LoadUInt32(at + 4);
			uint32_t const z = detail::LoadUInt32(at + 8);
			uint32_t const w = detail::LoadUInt32(at + 12);

			detail::StoreUInt32(at, w ^ SignBit);
			detail::StoreUInt32(at + 4, x);
			detail::StoreUInt32(at + 8, y);
			detail::StoreUInt32(at + 12, z ^ SignBit);
		}
	}

	bool UpgradeOldTrack(uint8_t *payload, size_t length)
	{
		size_t count = 0;

		// No payload, or one whose size the game's reader would refuse, is left as it is. A size that's right means every section, and
		// the flags and the timer's start after them, lie inside the payload (TrackSectionCount checks it as CheckTrackSize does).
		if (!payload || !TrackSectionCount(payload, length, &count))
		{
			return false;
		}

		// The sections start straight after the count, one after another, and the flags come straight after the last one
		// (win95/chunk_object.cpp:1002-1015): the places TrackSectionPlace and TrackFieldPlace give.
		size_t const sectionsAt = TrackLayout.countAt + 4;
		size_t const flagsAt = sectionsAt + count * TrackLayout.itemBytes;
		uint32_t const flagBits = detail::LoadUInt32(payload + flagsAt);

		// A track that has the bit holds its turns in the form the game uses now, and the game's reader leaves it as it is (:1045)
		if (flagBits & QuatProblemSorted)
		{
			return false;
		}

		for (size_t section = 0; section < count; section++)
		{
			uint8_t *const sectionStart = payload + sectionsAt + section * TrackLayout.itemBytes;

			UpgradeTurn(sectionStart + StartTurnAt);
			UpgradeTurn(sectionStart + EndTurnAt);
		}

		// Then the bit is set, as the game's reader sets it (:1061), even in a track with no sections. Every other bit is kept.
		detail::StoreUInt32(payload + flagsAt, flagBits | QuatProblemSorted);
		return true;
	}
}
