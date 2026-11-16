// The tables for the AI chunks (see ai_fields.h) and the rules that find their parts in a payload. Written 2026-11-14 by Melanikus for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every layout and limit was read from the game's own reader at the dev pin the editor builds against, AvPx 491deb22 (branch
// editor-dev-pin-491deb22 of the AvP repository); paths are under its 3dc folder. The old
// editor's words come from a study of its program (vasquez-ui.md, section 7). It draws most of the AI by hand, so most numbers here have
// no label: the editor keeps their bytes as they are, and shows them nowhere.
//
// A level can come from anyone, so a payload is read as hostile: every function checks the payload as the game's reader does before it
// trusts a count or a place, and works out places without adding past the end.

#include <string.h>

#include "ai_fields.h"
#include "field_bytes.h"
#include "field_model.h"
#include "../../avpx/3dc/waypoint_room.h"

namespace model
{
	using namespace detail;

	namespace
	{
		double const PositionMost = 16777216.0;   // 2^24

		char const PositionWhy[] =
			"the loader refuses a level with a point more than 2^24 game units from the level's middle along any axis (CheckPlace)";

		char const PathNumberWhy[] =
			"a path's number is its place in the game's list of paths, and a switch can only name a path up to 255: the game refuses a level "
			"with a path numbered outside 0-255";

		unsigned const PathPointBytes = 24;           // sizeof(ChunkPathPoint): avp/win95/path_chunk.hpp:14-19
		uint32_t const PathBits = 0x1 | 0x2;          // PathFlag_LoopPath, PathFlag_BackAndForth: avp/win95/path_chunk.hpp:22-23
		uint32_t const PathPointBits = 0x1 | 0x2;     // PathPointFlag_InterpolatedPoint, PathPointFlag_LocationValid: path_chunk.hpp:11-12

		const Anchor S = Anchor::Start;
	}

	// AVPPATH2: a patrol path. Its payload starts with its name (name_model.h: "AVPPATH2", "path name"), then come these numbers, then the
	// count of its points: 16 bytes in all, counted from the end of the name's padding. Then the points.
	static Field const PathFields[] =
	{
		{ "PathID", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, 255, PathNumberWhy, false, 0,
		  "avp/win95/path_chunk.cpp:34, :36-40; avp/win95/path_chunk.hpp:25-26; win95/environment_chunk.cpp:337-364 (each number once)" },
		{ "flags", "", S, 4, Storage::UInt32, Unit::Flags, PathBits, Limit::None, 0, 0, "", false, 0,
		  "avp/win95/path_chunk.cpp:42; avp/win95/path_chunk.hpp:22-23; avp/win95/projload.cpp:232, :258" },
		{ "spare2", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "avp/win95/path_chunk.cpp:43" },
	};

	// One point of a path: 24 bytes (ChunkPathPoint), counted from the start of the point.
	static Field const PathPointFields[] =
	{
		{ "module_index", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0,
		  "avp/win95/path_chunk.hpp:16; avp/win95/projload.cpp:241-243 (found by number; passed over when no object has it)" },
		{ "location", "", S, 4, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0,
		  "avp/win95/path_chunk.hpp:17; limit: CheckPlace (win95/chunk.hpp:492-510), called at avp/win95/path_chunk.cpp:52" },
		{ "flags", "", S, 16, Storage::UInt32, Unit::Flags, PathPointBits, Limit::None, 0, 0, "", false, 0,
		  "avp/win95/path_chunk.hpp:11-12, :18" },
		{ "spare2", "", S, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "avp/win95/path_chunk.hpp:18" },
	};

	// id, label, parent, leastBytes, reader
	AiChunk const AiChunks[] =
	{
		{ "AVPPATH2", "patrol path", "", 0, "avp/win95/path_chunk.cpp:14, :26-55" },
		{ "WAYPOINT", "waypoints of a module", "", 0, "win95/chunk_waypoint.cpp:43, :45-161" },
		{ "AIMODMAS", "AI module master", "MODULEDT", 0, "win95/chunk_waypoint.cpp:412-417" },
		{ "AIMODSLA", "AI module slave", "MODULEDT", 4, "win95/chunk_waypoint.cpp:511-518" },
		{ "ADJMDLEP", "entry points to adjacent modules", "", 0, "win95/chunk_object.cpp:751, :791-817" },
		{ "VMDARRAY", "modules a module can see", "", 0, "win95/chunk_object.cpp:642, :701-743" },
		{ "MODZONE", "module zone", "", 8, "win95/chunk_object.cpp:864, :872-878" },
	};

	size_t const AiChunkCount = sizeof(AiChunks) / sizeof(AiChunks[0]);

	namespace
	{
		double const AdjacentPlaceMost = 16777216.0;   // 2^24: FarthestPlace, win95/chunk.hpp:492

		char const AdjacentPlaceWhy[] =
			"the game refuses a level with an entry point farther than 2^24 game units from the level's middle on any axis";

		char const VModuleBranchWhy[] =
			"a closed door makes the game jump along this list to the place named here, and it walks the list until the end, so a jump has to go "
			"forward, to a later place and no further than the end of the list, or be 0, no jump: the game refuses a level whose jump doesn't";

		char const VModuleFlagsWhy[] =
			"an item's flags belong to the game while it runs, so a level gives none: the game refuses a level whose item has any";

		uint32_t const AdjacentBits = 0x1 | 0x2 | 0x4 | 0x8;   // win95/chunk_object.hpp:261-264
	}

	// AIMODSLA: an AI module's master. 4 bytes.
	static Field const SlaveFields[] =
	{
		{ "MasterModuleIndex", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_waypoint.cpp:517, :552-578 (the level is refused unless it names a master module: that needs the other chunks)" },
	};

	// MODZONE: a module's zone. 8 bytes.
	static Field const ZoneFields[] =
	{
		{ "Zone", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:875 (the game never uses it)" },
		{ "spare", "", S, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:877" },
	};

	// VMDARRAY: one item of the list of what a module can see: 16 bytes, counted from the start of the item, in the file's order.
	static Field const VModuleFields[] =
	{
		{ "object_index", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_object.cpp:714; avp/win95/projload.cpp:2423 (found by number; passed over when no object has it)" },
		{ "branch_no", "", S, 4, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, 2147483647, VModuleBranchWhy, false, 0,
		  "win95/chunk_object.cpp:717, :720-725; avp/win95/projload.cpp:2443" },
		{ "flags", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, 0, VModuleFlagsWhy, false, 0,
		  "win95/chunk_object.cpp:727, :730-736; avp/win95/projload.cpp:2451; include/module.h:89-96" },
		{ "spare", "", S, 12, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:738" },
	};

	// ADJMDLEP: one item of the list of adjacent modules: 20 bytes, counted from the start of the item.
	static Field const AdjacentFields[] =
	{
		{ "object_index", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_object.cpp:802; avp/win95/projload.cpp:2510-2512, :2557 (found by number; passed over when no object has it)" },
		{ "flags", "", S, 4, Storage::UInt32, Unit::Flags, AdjacentBits, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_object.cpp:805; win95/chunk_object.hpp:261-264; avp/win95/projload.cpp:2525, :2529, :2536" },
		{ "entry_point", "", S, 8, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -AdjacentPlaceMost, AdjacentPlaceMost, AdjacentPlaceWhy, false,
		  0, "win95/chunk_object.cpp:808-809; win95/chunk.hpp:488-510" },
	};

	namespace
	{
		double const WaypointPlaceMost = 16777216.0;   // 2^24: FarthestPlace, win95/chunk.hpp:492

		char const WaypointPlaceWhy[] =
			"the game refuses a level with a waypoint corner or centre farther than 2^24 game units from the level's middle on any axis";

		char const WaypointLinkWhy[] =
			"a link names a waypoint by its place in its own list, so it can't be below 0, and it has to be less than the number of waypoints in "
			"that list: the game refuses a level whose link isn't";

		uint32_t const WaypointBits = 0x1;                          // wayflag_cancrawl: avp/bh_waypoint.h:36
		uint32_t const WaypointLinkBits = 0x1 | 0x2 | 0x4 | 0x8;    // linkflag_oneway, _reversed_oneway, _alienonly, _largetarget: avp/bh_waypoint.h:16-19
	}

	// WAYPOINT: the waypoints of a module. The payload starts with their count (4 bytes); then each waypoint: these numbers (44 bytes), the
	// count of its links (4), its links (8 each), the count of its module links (4), and its module links. The part's 48 bytes are the
	// numbers and the count of links, counted from the waypoint's start.
	static Field const WaypointFields[] =
	{
		{ "min", "", S, 0, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -WaypointPlaceMost, WaypointPlaceMost, WaypointPlaceWhy, false, 0,
		  "win95/chunk_waypoint.cpp:79, :84; win95/chunk.hpp:488-510; win95/chunk_waypoint.hpp:39" },
		{ "max", "", S, 12, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -WaypointPlaceMost, WaypointPlaceMost, WaypointPlaceWhy, false, 0,
		  "win95/chunk_waypoint.cpp:81, :85; win95/chunk.hpp:488-510; win95/chunk_waypoint.hpp:39" },
		{ "centre", "", S, 24, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -WaypointPlaceMost, WaypointPlaceMost, WaypointPlaceWhy, false, 0,
		  "win95/chunk_waypoint.cpp:83, :86; win95/chunk.hpp:488-510; win95/chunk_waypoint.hpp:40" },
		{ "flags", "", S, 36, Storage::UInt32, Unit::Flags, WaypointBits, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_waypoint.cpp:89, :127-131; win95/chunk_waypoint.hpp:31-32; avp/bh_waypoint.h:36; avp/bh_waypoint.cpp:886-887; avp/win95/projload.cpp:2638" },
		{ "spare2", "", S, 40, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_waypoint.cpp:91" },
	};

	// One link from a waypoint to a waypoint of its own list: 8 bytes (WaypointLink), counted from the start of the link.
	static Field const WaypointLinkFields[] =
	{
		{ "index", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, 2147483647, WaypointLinkWhy, false, 0,
		  "win95/chunk_waypoint.cpp:101-105, :147-159; win95/chunk_waypoint.hpp:17; avp/win95/projload.cpp:2647" },
		{ "flags", "", S, 4, Storage::UInt32, Unit::Flags, WaypointLinkBits, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_waypoint.cpp:103; win95/chunk_waypoint.hpp:18; avp/bh_waypoint.h:16-19; avp/win95/projload.cpp:2646" },
	};

	// One link from a waypoint to a module: its name, then its flags (4 bytes), counted from the end of the name's padding.
	static Field const ModuleLinkFields[] =
	{
		{ "flags", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_waypoint.cpp:124" },
	};

	// chunk, label, item, origin, bytes, fields, fieldCount, reader
	AiPart const AiParts[] =
	{
		{ "AVPPATH2", "path", false, AiOrigin::AfterName, 16, PathFields, sizeof(PathFields) / sizeof(PathFields[0]),
		  "avp/win95/path_chunk.cpp:29-44" },
		{ "AVPPATH2", "path point", true, AiOrigin::RecordStart, 24, PathPointFields, sizeof(PathPointFields) / sizeof(PathPointFields[0]),
		  "avp/win95/path_chunk.cpp:46-54" },
		{ "WAYPOINT", "waypoint", true, AiOrigin::RecordStart, 48, WaypointFields, sizeof(WaypointFields) / sizeof(WaypointFields[0]),
		  "win95/chunk_waypoint.cpp:76-94" },
		{ "WAYPOINT", "waypoint link", true, AiOrigin::RecordStart, 8, WaypointLinkFields, sizeof(WaypointLinkFields) / sizeof(WaypointLinkFields[0]),
		  "win95/chunk_waypoint.cpp:101-105" },
		{ "WAYPOINT", "module link", true, AiOrigin::AfterName, 4, ModuleLinkFields, sizeof(ModuleLinkFields) / sizeof(ModuleLinkFields[0]),
		  "win95/chunk_waypoint.cpp:118-125" },
		{ "AIMODSLA", "AI module slave", false, AiOrigin::RecordStart, 4, SlaveFields, sizeof(SlaveFields) / sizeof(SlaveFields[0]),
		  "win95/chunk_waypoint.cpp:513-518" },
		{ "MODZONE", "zone", false, AiOrigin::RecordStart, 8, ZoneFields, sizeof(ZoneFields) / sizeof(ZoneFields[0]),
		  "win95/chunk_object.cpp:872-878" },
		{ "VMDARRAY", "virtual module", true, AiOrigin::RecordStart, 16, VModuleFields, sizeof(VModuleFields) / sizeof(VModuleFields[0]),
		  "win95/chunk_object.cpp:711-741" },
		{ "ADJMDLEP", "adjacent module", true, AiOrigin::RecordStart, 20, AdjacentFields, sizeof(AdjacentFields) / sizeof(AdjacentFields[0]),
		  "win95/chunk_object.cpp:798-810" },
	};

	size_t const AiPartCount = sizeof(AiParts) / sizeof(AiParts[0]);

	// chunk, part, field, bit, label
	AiBitName const AiBits[] =
	{
		{ "AVPPATH2", "path", "flags", 0x1, "Loop path" },
		{ "AVPPATH2", "path", "flags", 0x2, "Go back and forth" },
		{ "AVPPATH2", "path point", "flags", 0x1, "Interpolated point" },
		{ "AVPPATH2", "path point", "flags", 0x2, "Location valid" },
		{ "WAYPOINT", "waypoint", "flags", 0x1, "Can Crawl" },
		{ "WAYPOINT", "waypoint link", "flags", 0x1, "One way" },
		{ "WAYPOINT", "waypoint link", "flags", 0x2, "One way, the other direction" },
		{ "WAYPOINT", "waypoint link", "flags", 0x4, "Aliens only" },
		{ "WAYPOINT", "waypoint link", "flags", 0x8, "Large target" },
		{ "ADJMDLEP", "adjacent module", "flags", 0x1, "Entry point set by hand" },
		{ "ADJMDLEP", "adjacent module", "flags", 0x2, "Inside the adjacent module" },
		{ "ADJMDLEP", "adjacent module", "flags", 0x4, "Adjacent module inside this one" },
		{ "ADJMDLEP", "adjacent module", "flags", 0x8, "Vertical, for aliens only" },
	};

	size_t const AiBitCount = sizeof(AiBits) / sizeof(AiBits[0]);

	char const NotThisPartsReason[] = "this field isn't one of the part's own";
	char const PathNumberTakenReason[] = "another path has this number, and the game refuses a level where two paths share one";

	namespace
	{
		size_t const NoPlace = (size_t)-1;

		// Whether the field is one of that part's own, so a field of another part can't be read or written at its own offset here.
		bool IsFieldOfPart(AiPart const &part, Field const &field)
		{
			for (size_t i = 0; i < part.fieldCount; i++)
			{
				if (&part.fields[i] == &field)
				{
					return true;
				}
			}

			return false;
		}

		// Where the field starts: its part's origin plus its offset, or NoPlace when that is past the end of the payload. Worked out
		// without adding past the end, so a huge origin can't wrap round to a small place. (Whether the whole field fits is asked next.)
		size_t PlaceOf(Field const &field, size_t length, size_t origin)
		{
			return (origin <= length && field.offset <= length - origin) ? origin + field.offset : NoPlace;
		}

		// Where the field starts, when the whole of it lies inside the payload.
		bool ReadPlace(Field const &field, size_t length, size_t origin, size_t *at)
		{
			size_t const place = PlaceOf(field, length, origin);

			if (place == NoPlace || length - place < StorageBytes(field.storage))
			{
				return false;
			}

			*at = place;
			return true;
		}

		bool RefuseOtherPartsField(char const **why)
		{
			if (why)
			{
				*why = NotThisPartsReason;
			}

			return false;
		}
	}

	AiChunk const *FindAiChunk(char const *id)
	{
		if (!id)
		{
			return 0;
		}

		for (size_t i = 0; i < AiChunkCount; i++)
		{
			if (strncmp(AiChunks[i].id, id, 8) == 0)
			{
				return &AiChunks[i];
			}
		}

		return 0;
	}

	AiPart const *FindAiPart(char const *chunk, char const *label)
	{
		if (!chunk || !label)
		{
			return 0;
		}

		for (size_t i = 0; i < AiPartCount; i++)
		{
			if (strncmp(AiParts[i].chunk, chunk, 8) == 0 && strcmp(AiParts[i].label, label) == 0)
			{
				return &AiParts[i];
			}
		}

		return 0;
	}

	Field const *FindAiField(AiPart const &part, char const *name)
	{
		if (!name)
		{
			return 0;
		}

		for (size_t i = 0; i < part.fieldCount; i++)
		{
			if (strcmp(part.fields[i].name, name) == 0)
			{
				return &part.fields[i];
			}
		}

		return 0;
	}

	char const *AiSizeCheckWords(SizeCheck check)
	{
		switch (check)
		{
		case SizeCheck::Right: return "the chunk is laid out as the game reads it";
		case SizeCheck::TooShort: return "the chunk ends before the numbers the game reads from it";
		case SizeCheck::CountCutOff: return "the chunk ends before a list's count";
		case SizeCheck::CountNegative: return "a list in the chunk has a count below zero";
		case SizeCheck::CountTooBig: return "a list in the chunk counts more items than the rest of the chunk holds";
		case SizeCheck::WrongSize: return "the chunk isn't the size its counts add up to";
		case SizeCheck::NameCutOff: return "a name in the chunk has no ending zero inside the chunk";
		}

		return "the chunk can't be checked";
	}

	bool ReadAiField(AiPart const &part, Field const &field, uint8_t const *payload, size_t length, size_t origin, double *value,
	                 char const **why)
	{
		size_t at = 0;

		if (!IsFieldOfPart(part, field))
		{
			return RefuseOtherPartsField(why);
		}

		if (!ReadPlace(field, length, origin, &at))
		{
			return false;
		}

		if (field.storage == Storage::Int32)
		{
			*value = (double)LoadInt32(payload + at);
			return true;
		}

		if (field.storage == Storage::UInt32)
		{
			uint32_t const bits = LoadUInt32(payload + at);

			*value = (double)(field.mask ? (bits & field.mask) : bits);
			return true;
		}

		return false;
	}

	bool ReadAiFieldAxis(AiPart const &part, Field const &field, uint8_t const *payload, size_t length, size_t origin, int axis,
	                     int32_t *value, char const **why)
	{
		size_t at = 0;

		if (!IsFieldOfPart(part, field))
		{
			return RefuseOtherPartsField(why);
		}

		if (field.storage != Storage::IntTriple || axis < 0 || axis > 2 || !ReadPlace(field, length, origin, &at))
		{
			return false;
		}

		*value = LoadInt32(payload + at + 4 * (size_t)axis);
		return true;
	}

	bool SetAiField(AiPart const &part, Field const &field, uint8_t *payload, size_t length, size_t origin, double value,
	                char const **why)
	{
		if (!IsFieldOfPart(part, field))
		{
			return RefuseOtherPartsField(why);
		}

		return SetFieldAt(field, payload, length, PlaceOf(field, length, origin), value, why);
	}

	bool SetAiFieldAxis(AiPart const &part, Field const &field, uint8_t *payload, size_t length, size_t origin, int axis, int32_t value,
	                    char const **why)
	{
		if (!IsFieldOfPart(part, field))
		{
			return RefuseOtherPartsField(why);
		}

		return SetAxisAt(field, payload, length, PlaceOf(field, length, origin), axis, value, why);
	}

	SizeCheck CheckPathSize(uint8_t const *payload, size_t length, PathParts *parts)
	{
		// The name ends at its first zero byte, and the zero has to be inside the payload (CopyNameFromChunk, win95/chunk.hpp:433-455).
		uint8_t const *const zero = length ? (uint8_t const *)memchr(payload, 0, length) : 0;

		if (!zero)
		{
			return SizeCheck::NameCutOff;
		}

		size_t const nameLength = (size_t)(zero - payload);

		// The numbers start after the name's zero and the padding up to a multiple of 4: (name_length+4)&~3 (path_chunk.cpp:32).
		size_t const headAt = (nameLength + 4) & ~(size_t)3;

		// PathID, flags and spare2 have to be there, one after another (ReadFromChunk, win95/chunk.hpp:404-418).
		if (headAt > length || length - headAt < 12)
		{
			return SizeCheck::TooShort;
		}

		// Then the count of points: there, not below 0, and no more points than the bytes after it hold at 24 each (ReadListCount,
		// win95/chunk.hpp:384-398). ReadCount works it out in numbers wide enough not to wrap.
		size_t count = 0;
		const SizeCheck counted = ReadCount(payload, length, headAt + 12, PathPointBytes, &count);

		if (counted != SizeCheck::Right)
		{
			return counted;
		}

		if (parts)
		{
			parts->nameLength = nameLength;
			parts->headAt = headAt;
			parts->countAt = headAt + 12;
			parts->pointCount = count;
			parts->pointsAt = headAt + 16;
		}

		return SizeCheck::Right;
	}

	bool PathPointOrigin(PathParts const &parts, size_t index, size_t *origin)
	{
		if (index >= parts.pointCount)
		{
			return false;
		}

		// index is below the count, and the count's bytes fit in the payload, so this can't wrap round.
		*origin = parts.pointsAt + index * PathPointBytes;
		return true;
	}

	bool PathNumberIsAllowed(int32_t number, int32_t const *others, size_t othersCount, char const **why)
	{
		// The range and its reason are the "PathID" field's own (the first of PathFields), so the two can never disagree.
		if (!ValueIsAllowed(PathFields[0], (double)number, why))
		{
			return false;
		}

		for (size_t i = 0; others && i < othersCount; i++)
		{
			if (others[i] == number)
			{
				if (why)
				{
					*why = PathNumberTakenReason;
				}

				return false;
			}
		}

		return true;
	}

	char const WaypointGroupTooBigReason[] =
		"the game keeps the number of waypoints in each group, the alien ones and the ground ones, in a 16-bit number, so a group can't hold "
		"more than 32,767";

	char const WaypointNoRoomReason[] =
		"a waypoint's box has to give an alien room to aim for on every axis at the level's scale: the game refuses a level whose box gives "
		"none, or whose corners or scale aren't numbers it can work with";

	namespace
	{
		unsigned const WaypointLeastBytes = 52;     // the least a waypoint takes: three corners, flags, spare2 and two counts
		unsigned const WaypointLinkBytes = 8;       // sizeof(WaypointLink): win95/chunk_waypoint.hpp:15-19
		unsigned const ModuleLinkLeastBytes = 8;    // a module link's name takes at least 4 bytes, and its flags 4: chunk_waypoint.cpp:107-108
		unsigned const WaypointFlagsAt = 36;        // where a waypoint's flags are, from its start
		unsigned const WaypointLinkCountAt = 44;    // where a waypoint's count of links is, from its start
		uint32_t const WaypointFirstGround = 0x40000000;   // WaypointFlag_FirstGroundWaypoint: win95/chunk_waypoint.hpp:32

		// Where the name that starts at "at" ends: after its zero and the padding up to a multiple of 4 (CopyNameFromChunk, win95/chunk.hpp:433-455,
		// and chunk_waypoint.cpp:122-123). False when there's no zero inside the payload. The place it gives can be past the end of the payload.
		bool PaddedNameEnd(uint8_t const *payload, size_t length, size_t at, size_t *after)
		{
			if (at >= length)
			{
				return false;
			}

			uint8_t const *const zero = (uint8_t const *)memchr(payload + at, 0, length - at);

			if (!zero)
			{
				return false;
			}

			size_t const nameLength = (size_t)(zero - (payload + at));

			*after = at + ((nameLength + 4) & ~(size_t)3);
			return true;
		}

		// Checks the waypoint that starts at "at" as the game's reader reads one (chunk_waypoint.cpp:70-125), and gives where its parts are.
		SizeCheck StepWaypoint(uint8_t const *payload, size_t length, size_t at, WaypointPlace *place)
		{
			// Every waypoint takes at least 52 bytes, and a waypoint's own lists can use up the room the count of waypoints was measured
			// against (chunk_waypoint.cpp:70-74).
			if (at > length || length - at < WaypointLeastBytes)
			{
				return SizeCheck::TooShort;
			}

			// Its links: counted at +44, and no more than the bytes after the count hold at 8 each (chunk_waypoint.cpp:94).
			size_t links = 0;
			const SizeCheck linksCounted = ReadCount(payload, length, at + WaypointLinkCountAt, WaypointLinkBytes, &links);

			if (linksCounted != SizeCheck::Right)
			{
				return linksCounted;
			}

			// The count was measured against the bytes there are, so the links end inside the payload.
			size_t const linksAt = at + WaypointLinkCountAt + 4;
			size_t const moduleLinkCountAt = linksAt + links * WaypointLinkBytes;
			size_t moduleLinks = 0;
			const SizeCheck moduleLinksCounted = ReadCount(payload, length, moduleLinkCountAt, ModuleLinkLeastBytes, &moduleLinks);

			if (moduleLinksCounted != SizeCheck::Right)
			{
				return moduleLinksCounted;
			}

			size_t position = moduleLinkCountAt + 4;
			size_t const moduleLinksAt = position;

			// Each module link: a name with its zero inside the payload, then its flags (chunk_waypoint.cpp:118-125).
			for (size_t i = 0; i < moduleLinks; i++)
			{
				size_t after = 0;

				if (!PaddedNameEnd(payload, length, position, &after))
				{
					return SizeCheck::NameCutOff;
				}

				if (after > length || length - after < 4)
				{
					return SizeCheck::TooShort;
				}

				position = after + 4;
			}

			place->start = at;
			place->linkCount = links;
			place->linksAt = linksAt;
			place->moduleLinkCount = moduleLinks;
			place->moduleLinksAt = moduleLinksAt;
			place->end = position;
			return SizeCheck::Right;
		}
	}

	bool WaypointGroupsFit(size_t alienCount, size_t groundCount, char const **why)
	{
		if (alienCount > MostWaypointsInAGroup || groundCount > MostWaypointsInAGroup)
		{
			if (why)
			{
				*why = WaypointGroupTooBigReason;
			}

			return false;
		}

		return true;
	}

	SizeCheck CheckWaypointSize(uint8_t const *payload, size_t length, WaypointParts *parts)
	{
		// The count of waypoints: there, not below 0, and no more than the bytes after it hold at 52 each (chunk_waypoint.cpp:50-52).
		size_t count = 0;
		const SizeCheck counted = ReadCount(payload, length, 0, WaypointLeastBytes, &count);

		if (counted != SizeCheck::Right)
		{
			return counted;
		}

		size_t at = 4;
		bool anyGround = false;
		size_t firstGround = 0;

		for (size_t i = 0; i < count; i++)
		{
			WaypointPlace place = WaypointPlace();
			const SizeCheck stepped = StepWaypoint(payload, length, at, &place);

			if (stepped != SizeCheck::Right)
			{
				return stepped;
			}

			// The waypoint the file marks as the first ground one; when several are marked, the last mark wins (chunk_waypoint.cpp:127-131).
			if (LoadUInt32(payload + at + WaypointFlagsAt) & WaypointFirstGround)
			{
				anyGround = true;
				firstGround = i;
			}

			at = place.end;
		}

		if (parts)
		{
			parts->count = count;
			parts->alienCount = anyGround ? firstGround : count;
			parts->groundCount = count - parts->alienCount;
		}

		return SizeCheck::Right;
	}

	bool WaypointOrigin(uint8_t const *payload, size_t length, size_t index, WaypointPlace *place)
	{
		size_t count = 0;

		if (ReadCount(payload, length, 0, WaypointLeastBytes, &count) != SizeCheck::Right || index >= count)
		{
			return false;
		}

		size_t at = 4;

		for (size_t i = 0; i <= index; i++)
		{
			if (StepWaypoint(payload, length, at, place) != SizeCheck::Right)
			{
				return false;
			}

			if (i == index)
			{
				return true;
			}

			at = place->end;
		}

		return false;
	}

	bool WaypointLinkOrigin(WaypointPlace const &place, size_t index, size_t *origin)
	{
		if (index >= place.linkCount)
		{
			return false;
		}

		// index is below the count, and the count's bytes fit in the payload, so this can't wrap round.
		*origin = place.linksAt + index * WaypointLinkBytes;
		return true;
	}

	bool ModuleLinkOrigin(uint8_t const *payload, size_t length, WaypointPlace const &place, size_t index, size_t *nameAt, size_t *origin)
	{
		if (index >= place.moduleLinkCount)
		{
			return false;
		}

		size_t position = place.moduleLinksAt;

		for (size_t i = 0; i <= index; i++)
		{
			size_t after = 0;

			if (!PaddedNameEnd(payload, length, position, &after) || after > length || length - after < 4)
			{
				return false;
			}

			if (i == index)
			{
				*nameAt = position;
				*origin = after;
				return true;
			}

			position = after + 4;
		}

		return false;
	}

	bool WaypointLinkTargetIsAllowed(int32_t target, size_t listSize, char const **why)
	{
		// The lower end and its reason are the link's "index" field's own (the first of WaypointLinkFields), so the two can never disagree.
		if (!ValueIsAllowed(WaypointLinkFields[0], (double)target, why))
		{
			return false;
		}

		// target isn't below 0 here. The upper end is the size of the list (chunk_waypoint.cpp:150, :154).
		if ((size_t)target >= listSize)
		{
			if (why)
			{
				*why = WaypointLinkFields[0].why;
			}

			return false;
		}

		return true;
	}

	bool WaypointBoxHasRoom(int32_t const *lowest, int32_t const *highest, int32_t const *centre, double scale, char const **why)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			int32_t room = 0;

			// The game's own sums (waypoint_room.h), as the reader asks them (chunk_waypoint.cpp:215-218): a room of 0 is refused.
			if (!WaypointRoom(lowest[axis], highest[axis], centre[axis], scale, &room) || room == 0)
			{
				if (why)
				{
					*why = WaypointNoRoomReason;
				}

				return false;
			}
		}

		return true;
	}

	namespace
	{
		unsigned const VModuleBytes = 16;       // an item of VMDARRAY: win95/chunk_object.cpp:705
		unsigned const AdjacentModuleBytes = 20;   // an item of ADJMDLEP: 2 numbers and a place, win95/chunk_object.cpp:795

			// CountedItemOrigin divides by an item's size, so a size of 0 would be a division by zero. Both are constants above 0, and a
			// constant can't be tested at run time, so it's checked here, when the program is built (Apone's review of 08df80b and 65cb4c1).
			static_assert(VModuleBytes > 0 && AdjacentModuleBytes > 0, "an item of a counted list takes bytes: CountedItemOrigin divides by its size");

		// The count at the start of the payload, measured as the game's ReadListCount does, for items of itemBytes each.
		SizeCheck CheckCountedList(uint8_t const *payload, size_t length, size_t itemBytes, size_t *count)
		{
			size_t items = 0;
			const SizeCheck counted = ReadCount(payload, length, 0, itemBytes, &items);

			if (counted == SizeCheck::Right && count)
			{
				*count = items;
			}

			return counted;
		}

		// Where item number index starts, when the items are itemBytes each and start at 4. Worked out without wrapping round.
		bool CountedItemOrigin(size_t count, size_t itemBytes, size_t index, size_t *origin)
		{
			size_t const most = ((size_t)-1 - 4) / itemBytes;

			if (index >= count || index > most)
			{
				return false;
			}

			*origin = 4 + index * itemBytes;
			return true;
		}
	}

	SizeCheck CheckFlatAiChunkSize(AiChunk const &chunk, size_t length)
	{
		return length >= chunk.leastBytes ? SizeCheck::Right : SizeCheck::TooShort;
	}

	SizeCheck CheckVModuleArraySize(uint8_t const *payload, size_t length, size_t *count)
	{
		return CheckCountedList(payload, length, VModuleBytes, count);
	}

	bool VModuleOrigin(size_t count, size_t index, size_t *origin)
	{
		return CountedItemOrigin(count, VModuleBytes, index, origin);
	}

	bool VModuleBranchIsAllowed(int32_t branch, size_t index, size_t count, char const **why)
	{
		// 0 is no jump. Any other has to be after the item's own place, and no further than the end of the list (chunk_object.cpp:722).
		if (branch == 0 || (branch > 0 && (size_t)branch > index && (size_t)branch <= count))
		{
			return true;
		}

		if (why)
		{
			// The reason is the "branch_no" field's own (the second of VModuleFields), so the two can never disagree.
			*why = VModuleFields[1].why;
		}

		return false;
	}

	SizeCheck CheckAdjacentModulesSize(uint8_t const *payload, size_t length, size_t *count)
	{
		return CheckCountedList(payload, length, AdjacentModuleBytes, count);
	}

	bool AdjacentModuleOrigin(size_t count, size_t index, size_t *origin)
	{
		return CountedItemOrigin(count, AdjacentModuleBytes, index, origin);
	}
}
