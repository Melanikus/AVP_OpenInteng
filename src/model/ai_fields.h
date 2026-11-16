// The field model for the AI chunks: the patrol paths (AVPPATH2), the waypoints (WAYPOINT), the AI modules (AIMODMAS and AIMODSLA),
// the entry points between modules (ADJMDLEP), the modules a module can see (VMDARRAY) and the zones (MODZONE). Written 2026-11-14 by
// Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT THIS IS. field_model.h describes a chunk as numbers counted from the start of its payload, with at most one list at the end.
// The AI chunks don't fit that: a path starts with its name, so its numbers come after a name of any length, and most of the others
// are lists whose items have numbers of their own. So here a chunk's numbers are described in PARTS. A part is the numbers at the
// head of a payload, or the numbers inside one item of a list. Each part is a table of the same Field entries as everywhere else, whose
// offsets are counted from the part's ORIGIN: the byte where the part starts. The functions below find a part's origin in a payload,
// after checking the payload the way the game's reader does. A field is then at its part's origin plus its offset.
//
// A level can come from anyone, so every payload is hostile: a count is never used until it has been measured against the bytes that
// are really there, and a place is never worked out by adding past the end of the payload.
//
// ALL NUMBERS ARE LITTLE-ENDIAN, as the game wrote them on the PC. Nothing here reads a file or uses the game's globals.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "field_model.h"

namespace model
{
	// Where a part's offsets are counted from.
	enum class AiOrigin
	{
		RecordStart,  // the first byte of the record the part is in: the chunk's payload, or one item of a list
		AfterName,    // the first byte after the name the record starts with: its letters, its zero, and the padding up to a multiple
		              // of 4 bytes (the game reads a name as "its length plus 4, rounded down to a multiple of 4")
	};

	// A group of numbers that sit together: the numbers at the head of a chunk's payload, or the numbers inside one item of a list.
	struct AiPart
	{
		char const *chunk;      // the chunk it's in: its 8-letter name
		char const *label;      // what it is, in words ("path", "path point"); no two parts of a chunk have the same
		bool item;              // false: the numbers at the head of the payload. true: the numbers inside one item of a list
		AiOrigin origin;        // where every field's offset is counted from (every field's anchor is Anchor::Start)
		unsigned bytes;         // how far its numbers reach from the origin, with a count that comes straight after them included
		Field const *fields;
		size_t fieldCount;
		char const *reader;     // where the game reads it, at the editor's pin (AvPx 491deb22)
	};

	// One of the AI chunks.
	struct AiChunk
	{
		char const *id;         // the chunk's name: 8 letters (MODZONE is 7, and then a zero)
		char const *label;      // what it is, in words
		char const *parent;     // the chunk it has to be inside for the game to read it, or "" when the game reads it anywhere
		unsigned leastBytes;    // the smallest payload the game reads, from its registration (0 when it has none)
		char const *reader;     // where the game reads it, at the editor's pin (AvPx 491deb22)
	};

	// A name for one bit of an AI field whose unit is Flags.
	struct AiBitName
	{
		char const *chunk;
		char const *part;       // the part's label
		char const *field;      // the Field's name
		uint32_t bit;
		char const *label;      // in words, as the old editor showed it where it did, otherwise the game's own name for the bit
	};

	extern AiChunk const AiChunks[];
	extern size_t const AiChunkCount;

	extern AiPart const AiParts[];
	extern size_t const AiPartCount;

	extern AiBitName const AiBits[];
	extern size_t const AiBitCount;

	// The chunk with that id, or 0 if it isn't one of these (or id is 0). id is the chunk's name as a level file holds it: its 8 bytes, with
	// no zero after them, or for MODZONE its 7 letters and the zero after them. At most 8 bytes are compared, and no more are read.
	AiChunk const *FindAiChunk(char const *id);

	// The part of that chunk with that label, or 0 (also when chunk or label is 0). chunk is as for FindAiChunk.
	AiPart const *FindAiPart(char const *chunk, char const *label);

	// The field of that part with that name, or 0 (also when name is 0).
	Field const *FindAiField(AiPart const &part, char const *name);

	// The reason, in plain words, for showing a mapper why an AI chunk was refused (SizeCheck is field_model.h's).
	char const *AiSizeCheckWords(SizeCheck check);

	// ---- Reading and setting a part's fields ----

	// What the readers and setters give as the reason when the field isn't one of the part's own: it would be read or written at its own
	// offset, in a part it doesn't belong to.
	extern char const NotThisPartsReason[];

	// Reads one field as one number, as stored: for a masked field, after the mask and not shifted. origin is where the part's offsets are
	// counted from in this payload (the functions below that check a chunk give it). Gives back false, leaving *value alone, when the field
	// isn't the part's own (*why says so), doesn't fit in the payload at its place, or isn't one number: a field of three numbers has
	// ReadAiFieldAxis. Only the storages the AI chunks use are read: a whole number, signed or unsigned.
	bool ReadAiField(AiPart const &part, Field const &field, uint8_t const *payload, size_t length, size_t origin, double *value,
	                 char const **why);

	// Reads one number of a field of three numbers: axis 0, 1 or 2 (x, y or z).
	bool ReadAiFieldAxis(AiPart const &part, Field const &field, uint8_t const *payload, size_t length, size_t origin, int axis,
	                     int32_t *value, char const **why);

	// Sets one field after checking the value with ValueIsAllowed, as SetChunkField does for the other chunks. Nothing changes if the value
	// isn't allowed (*why says why), the field isn't the part's own, or it doesn't fit in the payload at its place. A field of three numbers
	// can't be set from one number: SetAiFieldAxis sets one of its numbers and leaves the other two as they were.
	bool SetAiField(AiPart const &part, Field const &field, uint8_t *payload, size_t length, size_t origin, double value,
	                char const **why);
	bool SetAiFieldAxis(AiPart const &part, Field const &field, uint8_t *payload, size_t length, size_t origin, int axis, int32_t value,
	                    char const **why);

	// ---- AVPPATH2: a patrol path ----

	// Where the parts of a path's payload are, found as the game's reader finds them (avp/win95/path_chunk.cpp:29-54).
	struct PathParts
	{
		size_t nameLength;   // the name's letters, not counting its zero
		size_t headAt;       // where the path's numbers start: after the name, its zero and its padding. The origin of the "path" part
		size_t countAt;      // where the count of points is (headAt + 12)
		size_t pointCount;   // how many points the payload holds
		size_t pointsAt;     // where point 0 starts (headAt + 16). The origin of the "path point" part for point 0
	};

	// Checks a path's payload as the game's reader does, and when it's right gives the places of its parts in *parts (when not 0). The name
	// needs a zero inside the payload; the three numbers and the count need room; the count can't be below 0 or more than the bytes after
	// it hold at 24 a point. Bytes after the last point are ignored, as the game ignores them. It doesn't check the values (ValueIsAllowed
	// does that, for each field).
	SizeCheck CheckPathSize(uint8_t const *payload, size_t length, PathParts *parts);

	// Where point number index starts: the origin of the "path point" part for that point. False when the path has no such point.
	bool PathPointOrigin(PathParts const &parts, size_t index, size_t *origin);

	// Whether number may be the number of a path when the other paths in the same SPECLOBJ have the numbers others[0] to
	// others[othersCount - 1]: 0-255 (the "PathID" field's own limit and reason), and no other path there has it (the game refuses a
	// level where two paths of one SPECLOBJ share a number: win95/environment_chunk.cpp:339-364, its own AVPPATH2 children at :344,
	// refused at :357). When it may not, *why (when not 0) says why. others may be 0 when othersCount is 0.
	bool PathNumberIsAllowed(int32_t number, int32_t const *others, size_t othersCount, char const **why);

	// The reason PathNumberIsAllowed gives when another path has the number.
	extern char const PathNumberTakenReason[];

	// ---- WAYPOINT: the waypoints of a module ----

	// The most waypoints the editor lets one group hold, the alien ones or the ground ones. The game keeps each group's count in a 16-bit
	// whole number (win95/chunk_waypoint.hpp:74-75), and a count past 32,767 wraps round to a number below zero.
	size_t const MostWaypointsInAGroup = 32767;

	// Whether the two groups of a chunk's waypoints, the alien ones (first in the chunk) and the ground ones (from the one the file marks
	// as the first), each fit the game's 16-bit count. When one doesn't, *why (when not 0) says why.
	bool WaypointGroupsFit(size_t alienCount, size_t groundCount, char const **why);

	// The reason WaypointGroupsFit gives.
	extern char const WaypointGroupTooBigReason[];

	// How a chunk's waypoints are grouped, found as the game's reader finds them (win95/chunk_waypoint.cpp:127-143).
	struct WaypointParts
	{
		size_t count;         // how many waypoints the chunk holds
		size_t alienCount;    // the alien ones: the first alienCount, counted from 0
		size_t groundCount;   // the ground ones: the rest, from the one the file marks as the first
	};

	// Checks a waypoint chunk's payload as the game's reader does: the count of waypoints; for each waypoint, room for its fixed part, its
	// count of links, its count of module links, and each module link's name and flags; each count not below 0 and no more than the bytes
	// after it hold. When it's right gives the groups in *parts (when not 0). Bytes after the last waypoint are ignored, as the game ignores
	// them. It doesn't check the values (ValueIsAllowed does that for each field), or that each link leads to a place in its list
	// (WaypointLinkTargetIsAllowed does).
	SizeCheck CheckWaypointSize(uint8_t const *payload, size_t length, WaypointParts *parts);

	// Where one waypoint's parts are in the payload.
	struct WaypointPlace
	{
		size_t start;            // where the waypoint starts: the origin of the "waypoint" part
		size_t linkCount;        // how many links it has
		size_t linksAt;          // where its first link starts: the origin of the "waypoint link" part for link 0 (a link is 8 bytes)
		size_t moduleLinkCount;  // how many module links it has
		size_t moduleLinksAt;    // where its first module link's name starts
		size_t end;              // where the next waypoint starts
	};

	// Finds waypoint number index, checking every waypoint up to it as CheckWaypointSize does. False when the payload isn't right that far,
	// or has no such waypoint. place can't be 0.
	bool WaypointOrigin(uint8_t const *payload, size_t length, size_t index, WaypointPlace *place);

	// Where link number index of a waypoint starts: the origin of the "waypoint link" part. False when the waypoint has no such link.
	bool WaypointLinkOrigin(WaypointPlace const &place, size_t index, size_t *origin);

	// Where module link number index of a waypoint is: *nameAt is where its name starts, and *origin where its flags are, after the name's zero
	// and padding: the origin of the "module link" part. False when the waypoint has no such module link, or the payload isn't right there.
	bool ModuleLinkOrigin(uint8_t const *payload, size_t length, WaypointPlace const &place, size_t index, size_t *nameAt, size_t *origin);

	// Whether a link may lead to waypoint target of its own list, which holds listSize waypoints (the alien list for an alien waypoint's link,
	// the ground list for a ground waypoint's): a place in it, from 0 up to listSize - 1. When it may not, *why (when not 0) says why.
	bool WaypointLinkTargetIsAllowed(int32_t target, size_t listSize, char const **why);

	// Whether a waypoint's box gives an alien room to aim for on every axis at the level's scale, as the game's reader requires
	// (win95/chunk_waypoint.cpp:203-226). It calls the game's own sums (avpx/3dc/waypoint_room.h), so the editor and the game can't
	// disagree. lowest, highest and centre each point to the x, y and z of that corner as the file holds them; scale is the level's (1 when
	// its environment gives none). A room of 0 on any axis is refused, and so is a scale or a corner the sums can't use (NaN, endless, or
	// too big for the game's whole numbers at that scale). A room of -1, a box with no size exactly at its centre, is allowed, as the
	// game's own levels need. When it isn't, *why (when not 0) says why.
	bool WaypointBoxHasRoom(int32_t const *lowest, int32_t const *highest, int32_t const *centre, double scale, char const **why);

	// The reason WaypointBoxHasRoom gives.
	extern char const WaypointNoRoomReason[];

	// ---- AIMODMAS, AIMODSLA and MODZONE: a few fixed bytes ----

	// Checks the payload of a chunk that is a few fixed bytes (AIMODMAS: none, AIMODSLA: 4, MODZONE: 8) as the game does: one shorter than
	// the chunk's smallest size is refused (TooShort), and any bytes after it are ignored. chunk is one of AiChunks.
	SizeCheck CheckFlatAiChunkSize(AiChunk const &chunk, size_t length);

	// ---- VMDARRAY: the modules a module can see ----

	// Checks a payload that is a count of items and then the items, 16 bytes each, as the game's reader does (win95/chunk_object.cpp:705-707):
	// the count has to be there, not below 0, and no more than the bytes after it hold. When it's right gives the count in *count (when not 0).
	SizeCheck CheckVModuleArraySize(uint8_t const *payload, size_t length, size_t *count);

	// Where item number index starts: the origin of the "virtual module" part. count is the count CheckVModuleArraySize gave. False when
	// there's no such item.
	bool VModuleOrigin(size_t count, size_t index, size_t *origin);

	// Whether an item at place index (counted from 0) of a list of count items may have this branch_no: 0 (no jump), or a place after its own
	// and no further than the end of the list, as the game's reader insists (win95/chunk_object.cpp:720-725). When not, *why (when not 0) says
	// why.
	bool VModuleBranchIsAllowed(int32_t branch, size_t index, size_t count, char const **why);

	// ---- ADJMDLEP: the entry points to adjacent modules ----

	// The same for a count and then items of 20 bytes each (win95/chunk_object.cpp:795-796).
	SizeCheck CheckAdjacentModulesSize(uint8_t const *payload, size_t length, size_t *count);

	// Where item number index starts: the origin of the "adjacent module" part. False when there's no such item.
	bool AdjacentModuleOrigin(size_t count, size_t index, size_t *origin);
}
