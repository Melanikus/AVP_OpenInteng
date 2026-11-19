// waypoint_model.h: a module's WAYPOINT chunk as inteng's Waypoint mode holds it, and the edits the mode makes. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE CHUNK (avpx/3dc/win95/chunk_waypoint.cpp, the game's own reader and writer): heading (8 letters, size), the number of waypoints, then per waypoint: min (3 ints: the box's lower corner),
// max (3 ints), centre (3 ints: the place aliens aim for), flags, spare2, the number of links and each link (the waypoint it leads to, counted from the start of ITS OWN list, and flags), the number
// of module links and each (a name, zero ended, padded to 4 bytes, and flags). The waypoints are the alien ones first, then the ground ones; the first ground waypoint has the flag
// 0x40000000 in the file (WaypointFlag_FirstGroundWaypoint), which is not kept in memory.
//
// THE EDITS (Waypoint mode, FUN_00584ee0 / FUN_00585750; the decompile of each is named at its function): Add Link variants (FUN_00586b40), Delete Link (FUN_00586d00), Delete Waypoint
// (FUN_00587c10), the Can Crawl flag (commands 0x16, 0x17, 0x27), Create Waypoint (command 5). Nothing here reads or writes a level: waypoint_edit.h does.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	int32_t const kWaypointFlagCentreByUser = (int32_t)0x80000000;   // WaypointFlag_CentreDefinedByUser
	int32_t const kWaypointFlagFirstGround = 0x40000000;             // WaypointFlag_FirstGroundWaypoint (in the file only)
	int32_t const kWaypointCanCrawl = 1;                             // the flag bit Can Crawl (commands 0x16 / 0x17 / 0x27: flags bit 0)

	// A link's flags as inteng's Add Link makes them (FUN_00586b40's third argument): the bits are One way (1), One way the other direction (2), Aliens only (4), Large target / deviation (8)
	int32_t const kLinkOneWay = 1;
	int32_t const kLinkOneWayBack = 2;
	int32_t const kLinkAlien = 4;
	int32_t const kLinkDeviation = 8;

	struct WaypointLinkRec
	{
		int32_t index;   // the waypoint it leads to, counted from the start of its own list
		int32_t flags;
	};

	struct WaypointModuleLink
	{
		std::string name;
		int32_t flags;
	};

	struct WaypointRec
	{
		int32_t min[3];       // the box's lower corner (absolute, the file's units: measured on the stock levels; the game's own comment says relative to the centre, the data does not)
		int32_t max[3];
		int32_t centre[3];    // the place aliens aim for (absolute): inside the box in most waypoints
		int32_t flags;
		int32_t spare2;
		std::vector<WaypointLinkRec> links;
		std::vector<WaypointModuleLink> moduleLinks;
	};

	struct WaypointSet
	{
		std::vector<WaypointRec> alien;
		std::vector<WaypointRec> ground;
	};

	// Reads a whole WAYPOINT chunk (its heading and all). Every count is measured against the bytes, a name has to end inside the chunk, a link has to lead to a place in its own list, and
	// a place beyond 2^24 millimetres from the middle is refused (the game's reader does the same; the game's loader checks the rest when the level is read again). False, with the reason.
	bool Waypoints_Parse(uint8_t const *bytes, size_t size, WaypointSet *set, std::string *why);

	// The chunk's bytes: what the game's writer writes (the first ground waypoint flagged). An empty set gives a chunk with no waypoint (callers take the chunk out instead).
	std::vector<uint8_t> Waypoints_Serialize(WaypointSet const &set);

	// Add Link (FUN_00586b40(a, b, flags)): in a's list of links the link to b gets the flags ORed in (made when there is none), and when it then has both One way and One way the other
	// direction both are cleared; then the same for b's link to a, with the flags' One way turned into One way the other direction. False when a or b is not in the list.
	bool Waypoints_AddLink(std::vector<WaypointRec> *list, int32_t a, int32_t b, int32_t flags);

	// Delete Link (FUN_00586d00(a, b)): a's link to b is taken out, then b's link to a. False when neither was there.
	bool Waypoints_DeleteLink(std::vector<WaypointRec> *list, int32_t a, int32_t b);

	// OURS (Waypoint by mouse, P4, 11-18; Add Link only ORs bits in): a's link to b gets EXACTLY these flags (made when there is none), b's link to a their mirror (One way and One way
	// the other direction swapped); both of those set at once clear both, as Add Link does. False when a or b is not in the list, or they are the same.
	bool Waypoints_SetLinkFlags(std::vector<WaypointRec> *list, int32_t a, int32_t b, int32_t flags);

	// Delete Waypoint (FUN_00587c10): the links to it are taken out of every waypoint of the list, it is taken out, and the waypoints after it are renumbered (every link past it counts one less).
	bool Waypoints_Delete(std::vector<WaypointRec> *list, int32_t at);

	// Create Waypoint (command 5, FUN_00584ee0 0x58f51): a new waypoint at the end of the list at the picked module's place. inteng makes it with all three corners at the place (an empty
	// box that the Edit Extents screen then gives its size); NAMED: the game's loader refuses a box with no room, so the new box is half a metre (500 mm) each way from its centre.
	// Gives the new waypoint's number.
	int32_t Waypoints_Create(std::vector<WaypointRec> *list, int32_t const place[3]);

	// Add Module's waypoint transfer (Module_Waypoint_Chunk::TransferWaypointData, chunk_waypoint.cpp): *into gets from's alien waypoints after its own alien ones and from's ground
	// ones after its own ground ones, each moved waypoint's links counting from the start of its new place in its list. Does nothing (false) when from has no waypoint.
	bool Waypoints_Transfer(WaypointSet *into, WaypointSet const &from);
}
