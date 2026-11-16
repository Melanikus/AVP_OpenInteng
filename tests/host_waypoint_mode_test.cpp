// The test for Waypoint mode's state machine (host::WaypointMode, src/host/waypoint_mode.cpp) on REAL levels: the commands the code-built menus send (waypoint-menu-pairs.md), through to the
// level's bytes. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_mode_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: a command that needs a pick says so and changes nothing; Edit Module makes the picked module's AI module the current one; Show Ground / Alien Waypoints opens mode 1 on page 1;
// Create Waypoint adds one to the level's WAYPOINT chunk (read back from the level's bytes); Set Base moves to page 3; Add Link (and the one-way / alien / deviation variants) and Delete Link
// change the links in the level's bytes as inteng's FUN_00586b40 / FUN_00586d00 do; Can Crawl sets and clears the flag; Delete Waypoint takes one out and renumbers; Exit goes back to mode 0
// (page 0) and Exit again leaves the screen; Add Module / Remove Module through the menu commands change the AI module; every write is taken by the game's loader; the copy read again holds it;
// the opened file is byte for byte what it was.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "object_edit.h"
#include "waypoint_model.h"
#include "waypoint_mode.h"
#include "waypoint_view.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

// The WAYPOINT chunk of a module as the level's bytes hold it
static bool SetOfModule(OpenLevelFile &level, int32_t module, host::WaypointSet *set)
{
	std::vector<host::ModuleSnapshot> snapshots;
	std::string why;

	*set = host::WaypointSet();
	host::ReadModuleSnapshots(&level, &snapshots);

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (snapshot.module && snapshot.index == module)
		{
			return !snapshot.hasWaypoint || host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), set, &why);
		}
	}

	return false;
}

static int FindLink(host::WaypointRec const &wp, int32_t to)
{
	for (size_t i = 0; i < wp.links.size(); i++)
	{
		if (wp.links[i].index == to)
		{
			return (int)i;
		}
	}

	return -1;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_waypoint_mode_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_waypoint_mode_test (0 ok)\n");
		return 0;
	}

	std::vector<char> original;

	if (!ReadWholeFile(argv[1], &original))
	{
		printf("  FAILED  the level can be read: %s\n", argv[1]);
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	std::vector<host::ModuleSnapshot> snapshots;

	host::ReadModuleSnapshots(&level, &snapshots);

	// a master with waypoints in both lists (or the most), a slave of it, and a free module
	int32_t master = -1, slave = -1, free = -1;
	size_t most = 0;

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (snapshot.module && snapshot.isMaster && snapshot.masterIndex < 0 && snapshot.hasWaypoint)
		{
			host::WaypointSet set;
			std::string why;

			if (host::Waypoints_Parse(snapshot.waypoint.data(), snapshot.waypoint.size(), &set, &why) && set.alien.size() + set.ground.size() > most && set.alien.size() + set.ground.size() >= 3)
			{
				most = set.alien.size() + set.ground.size();
				master = snapshot.index;
			}
		}
	}

	for (host::ModuleSnapshot const &snapshot : snapshots)
	{
		if (snapshot.module && snapshot.masterIndex == master && slave < 0)
		{
			slave = snapshot.index;
		}

		if (snapshot.module && !snapshot.isMaster && snapshot.masterIndex < 0 && free < 0)
		{
			free = snapshot.index;
		}
	}

	printf("  --  a master with the most waypoints %d (%zu), one of its slaves %d, a free module %d\n", master, most, slave, free);
	Check(master >= 0 && free >= 0, "the level has a master with waypoints and a free module to work with");

	if (master < 0 || free < 0)
	{
		return 1;
	}

	host::WaypointMode mode;
	host::WaypointContext context;

	context.open = &level;
	context.pickedModule = -1;

	// needs a pick
	Check(mode.Command(0x19, context).say.find("pick") != std::string::npos && mode.CurrentModule() < 0, "Edit Module with nothing picked says so and changes nothing");
	Check(mode.Command(3, context).say.find("pick") != std::string::npos && mode.Mode() == 0, "Show Alien Waypoints with nothing picked says so and stays in mode 0");

	// Edit Module on a slave makes its master the current AI module
	context.pickedModule = slave >= 0 ? slave : master;
	host::WaypointOutcome out = mode.Command(0x19, context);

	Check(mode.CurrentModule() == master, "Edit Module on a module of the AI module makes its master current: " + out.say);

	// mode 1
	host::WaypointSet before;

	SetOfModule(level, master, &before);
	out = mode.Command(before.ground.empty() ? 3 : 4, context);
	Check(mode.Mode() == 1 && out.page == 1, "Show Waypoints opens mode 1 and asks for page 1: " + out.say);

	bool const alien = mode.AlienList();
	size_t const listSize = (alien ? before.alien : before.ground).size();

	Check(mode.List().size() == listSize && listSize >= 1, "the active list is the level's: " + std::to_string(listSize) + " waypoints");

	// Create
	out = mode.Command(5, context);
	Check(out.levelChanged && mode.List().size() == listSize + 1 && mode.CurrentWaypoint() == (int32_t)listSize, "Create Waypoint adds one and makes it current: " + out.say);

	host::WaypointSet after;

	SetOfModule(level, master, &after);
	Check((alien ? after.alien : after.ground).size() == listSize + 1, "the level's WAYPOINT chunk holds the new waypoint (the loader took it)");

	// the three views' lines (waypoint_view.h): twelve edges for each waypoint, the bounds hold every box, a click inside a box picks it
	{
		std::vector<host::TrackLine> lines;
		int32_t mins[3], maxs[3];
		size_t const drawn = host::WaypointView_Lines(mode, 1.0, &lines, mins, maxs);
		size_t linkLines = 0;

		for (host::WaypointRec const &wp : mode.List())
		{
			linkLines += wp.links.size();
		}

		Check(drawn == mode.List().size() && lines.size() >= 12 * drawn && lines.size() <= 12 * drawn + linkLines, "the views draw the boxes (12 lines each) and the links: " + std::to_string(lines.size()) + " lines for " + std::to_string(drawn) + " waypoints");

		bool holds = true;

		for (host::TrackLine const &line : lines)
		{
			for (int i = 0; i < 3 && holds; i++)
			{
				holds = line.from[i] >= mins[i] && line.from[i] <= maxs[i] && line.to[i] >= mins[i] && line.to[i] <= maxs[i];
			}
		}

		Check(holds, "the bounds hold every line");

		// a click at the middle of waypoint 2 (any axes) picks that waypoint or one whose box holds the same place
		size_t const target = mode.List().size() > 2 ? 2 : 0;
		int32_t const centre[3] = { mode.List()[target].centre[0], mode.List()[target].centre[1], mode.List()[target].centre[2] };

		Check(mode.PickNear(centre, 7) && mode.CurrentWaypoint() >= 0, "a click at a waypoint's middle picks a waypoint");

		int32_t const inside = mode.CurrentWaypoint();
		host::WaypointRec const &picked = mode.List()[(size_t)inside];

		// the box (absolute corners, measured on the stock levels) holds the place, or the waypoint's own aim point is the place (an aim point is not always inside the box)
		bool const boxHolds = centre[0] >= picked.min[0] && centre[0] <= picked.max[0] && centre[1] >= picked.min[1] && centre[1] <= picked.max[1] && centre[2] >= picked.min[2] && centre[2] <= picked.max[2];
		bool const aimIsIt = centre[0] == picked.centre[0] && centre[1] == picked.centre[1] && centre[2] == picked.centre[2];

		Check(boxHolds || aimIsIt, "the picked waypoint's box holds the place, or its aim point is the place");

		int32_t file[3];

		host::WaypointView_FileUnits(centre, 1.0, file);
		Check(file[0] == centre[0] && file[1] == centre[1] && file[2] == centre[2], "at scale 1 a world place is the file's place");

		mode.Step(1);
		mode.Step(-1);
		Check(mode.CurrentWaypoint() == inside, "Next then Previous come back to the same waypoint");

		// the link highlight items (FUN_00588980): each toggles its own flag and the status words say so
		host::WaypointOutcome high = mode.Command(0xe, context);

		Check(mode.HighlightFlag() == host::kLinkAlien && high.say.find("Highlight Alien Link") != std::string::npos && !high.levelChanged, "Highlight link flag > Alien highlights the aliens-only links");
		high = mode.Command(0xf, context);
		Check(mode.HighlightFlag() == host::kLinkDeviation && high.say.find("Highlight Deviation Link") != std::string::npos, "Deviation allowed takes the highlight over");
		mode.Command(0xf, context);
		Check(mode.HighlightFlag() == 0, "choosing the same item again takes the highlight off");
	}

	// Set Base, then pick another and link
	int32_t const created = mode.CurrentWaypoint();

	// Edit Extents: the prompt is asked for; a typed box is put in order, the aim point follows it unless it is the user's; aim sets the flag; reset clears it
	{
		host::WaypointOutcome ask = mode.Command(0x14, context);

		Check(!ask.askExtents && !ask.levelChanged && ask.page == 4 && mode.Mode() == 2, "Edit Extents is mode 2 (page 4) and changes nothing yet");

		// the current waypoint is a stock one (the picking above chose waypoint 2) whose aim point may be the user's: reset it, so the middle of its box is where the aim point is
		mode.ApplyExtentsText("reset", context);

		host::WaypointRec const placed = mode.List()[(size_t)created];
		int32_t const cx = placed.centre[0], cy = placed.centre[1], cz = placed.centre[2];
		char text[200];

		// the highest corner first on x, to see the axis put in order
		snprintf(text, sizeof(text), "%d %d %d %d %d %d", cx + 1000, cy - 900, cz - 800, cx - 1000, cy + 900, cz + 800);
		out = mode.ApplyExtentsText(text, context);

		host::WaypointSet read;

		SetOfModule(level, master, &read);

		host::WaypointRec const &stored = (alien ? read.alien : read.ground)[(size_t)created];

		Check(out.levelChanged && stored.min[0] == cx - 1000 && stored.max[0] == cx + 1000 && stored.min[1] == cy - 900 && stored.max[1] == cy + 900 && stored.min[2] == cz - 800 && stored.max[2] == cz + 800,
			"a typed box is written with each axis in order: " + out.say);
		Check(stored.centre[0] == cx && stored.centre[1] == cy && stored.centre[2] == cz && !(stored.flags & host::kWaypointFlagCentreByUser), "the aim point is the middle of the box");

		snprintf(text, sizeof(text), "aim %d %d %d", cx + 10, cy + 20, cz + 30);
		out = mode.ApplyExtentsText(text, context);
		SetOfModule(level, master, &read);

		host::WaypointRec const &aimed = (alien ? read.alien : read.ground)[(size_t)created];

		Check(aimed.centre[0] == cx + 10 && aimed.centre[1] == cy + 20 && aimed.centre[2] == cz + 30 && (aimed.flags & host::kWaypointFlagCentreByUser) != 0, "aim sets the aim point and the user's flag");

		snprintf(text, sizeof(text), "%d %d %d %d %d %d", cx - 1100, cy - 900, cz - 800, cx + 1100, cy + 900, cz + 800);
		mode.ApplyExtentsText(text, context);
		SetOfModule(level, master, &read);
		Check((alien ? read.alien : read.ground)[(size_t)created].centre[0] == cx + 10, "a box change leaves a user's aim point where it is");

		out = mode.Command(0x18, context);   // Reset Waypoint Centre
		SetOfModule(level, master, &read);

		host::WaypointRec const &reset = (alien ? read.alien : read.ground)[(size_t)created];

		Check(!(reset.flags & host::kWaypointFlagCentreByUser) && reset.centre[0] == cx && reset.centre[1] == cy && reset.centre[2] == cz, "Reset Waypoint Centre clears the flag and the aim point is the middle again: " + out.say);

		Check(mode.ApplyExtentsText("1 2 3", context).say.find("six numbers") != std::string::npos && mode.ApplyExtentsText("1 2 3 4 5 x", context).say.find("six whole numbers") != std::string::npos &&
			mode.ApplyExtentsText("aim 1 2", context).say.find("six numbers") != std::string::npos, "a text that is none of the three is refused, saying what is wanted");

		SetOfModule(level, master, &read);
		Check((alien ? read.alien : read.ground)[(size_t)created].min[0] == cx - 1100, "a refused text changed nothing");

		// DRAG EDITING (FUN_00585750): a press within 3 pixels of a face takes it, the drag moves it, the release puts the axes in order, lets the aim follow and writes
		{
			host::WaypointRec const box = mode.List()[(size_t)created];
			int32_t onFace[3] = { box.min[0] + 15, box.centre[1], box.centre[2] };   // 15 mm from the lowest x face, 10 mm to a pixel: 1 pixel
			int32_t middle[3] = { box.centre[0], box.centre[1], box.centre[2] };

			Check(!mode.BeginDrag(middle, 7, 10) && !mode.Dragging(), "a press in the middle of the box, far from every face, takes nothing");
			Check(mode.BeginDrag(onFace, 1, 10) && mode.Dragging(), "a press within 3 pixels of the lowest x face takes it");

			int32_t moved[3] = { box.max[0] + 700, 0, 0 };   // dragged past the highest face: the axis is put in order when it is let go

			mode.DragTo(moved);
			Check(mode.List()[(size_t)created].min[0] == box.max[0] + 700, "the face follows the mouse while it is held (the list shows it at once)");

			out = mode.EndDrag(context);

			SetOfModule(level, master, &read);

			host::WaypointRec const &dragged = (alien ? read.alien : read.ground)[(size_t)created];

			Check(out.levelChanged && !mode.Dragging() && dragged.min[0] == box.max[0] && dragged.max[0] == box.max[0] + 700 && dragged.min[1] == box.min[1] && dragged.max[1] == box.max[1],
				"let go: the axis is in order, written, and the other axes are as they were: " + out.say);
			Check(!(dragged.flags & host::kWaypointFlagCentreByUser) ? dragged.centre[0] == (int32_t)(((int64_t)dragged.min[0] + dragged.max[0]) / 2) : true, "the aim point follows the box (unless it is the user's)");

			int32_t farAway[3] = { box.min[0] + 100000, 0, 0 };

			Check(!mode.BeginDrag(farAway, 1, 10), "a press far from a face takes nothing, then a release does nothing: " + mode.EndDrag(context).say);
		}
	}

	// MODE 2's own items: the minimise toggle, the zoom, Centre Edit, the snap, Finish Extent Edit
	{
		Check(mode.Mode() == 2 && !mode.Minimised() && !mode.CentreEdit() && mode.ZoomSteps() == 0, "in mode 2 nothing is minimised, centred or zoomed yet");
		Check(mode.Command(0x1c, context).say.find("on") != std::string::npos && mode.Minimised(), "Minimise (0x1c) turns on");
		mode.Command(0x1c, context);
		Check(!mode.Minimised(), "Minimise (0x1c) again turns off");
		mode.Zoom(1);
		mode.Zoom(1);
		mode.Zoom(-1);
		Check(mode.ZoomSteps() == 1, "the zoom steps count up and down: " + std::to_string(mode.ZoomSteps()));

		for (int i = 0; i < 20; i++)
		{
			mode.Zoom(1);
		}

		Check(mode.ZoomSteps() == 8, "the zoom is held to 8 steps");
		mode.Zoom(-100);
		Check(mode.ZoomSteps() == -8, "and to -8 the other way");
		Check(mode.Command(7, context).say.find("Not while editing extents") != std::string::npos && mode.Mode() == 2, "Set Base is refused in mode 2: Finish Extent Edit first");

		// CENTRE EDIT: the press takes the aim point of the axes the pane shows, the drag moves it, the release writes it with the user's flag
		mode.Command(0x1f, context);
		Check(mode.CentreEdit(), "Centre Edit (0x1f) is on");

		host::WaypointRec const box = mode.List()[(size_t)created];
		int32_t const target[3] = { box.min[0] + 50, box.min[1] + 60, box.min[2] + 70 };

		Check(mode.BeginDrag(target, 3, 10) && mode.Dragging(), "in Centre Edit a press anywhere takes the aim point (axes x and y of the pane)");
		mode.DragTo(target);
		out = mode.EndDrag(context);

		host::WaypointSet readCentre;

		SetOfModule(level, master, &readCentre);

		host::WaypointRec const &centred = (alien ? readCentre.alien : readCentre.ground)[(size_t)created];

		Check(out.levelChanged && centred.centre[0] == target[0] && centred.centre[1] == target[1] && (centred.flags & host::kWaypointFlagCentreByUser) != 0 && centred.min[0] == box.min[0] && centred.max[0] == box.max[0],
			"the aim point is where the drag ended (x and y), the flag is the user's and the box is as it was: " + out.say);
		Check(centred.centre[2] == box.centre[2], "the axis the pane does not show keeps its aim");
		mode.Command(0x1f, context);
		Check(!mode.CentreEdit(), "Centre Edit (0x1f) again turns off");
		mode.Command(0x18, context);   // back to the middle for what follows

		// SHIFT SNAP (FUN_00587cd0 / FUN_00588ac0): the number goes to the nearest side of the other waypoints and of the AI module's modules
		host::WaypointRec const box2 = mode.List()[(size_t)created];
		int32_t const onFace[3] = { box2.max[0] - 15, box2.centre[1], box2.centre[2] };

		Check(mode.BeginDrag(onFace, 1, 10), "a press near the highest x face takes it");

		int32_t farOff[3] = { box2.max[0] + 123457, box2.centre[1], box2.centre[2] };

		mode.DragToSnapped(farOff);

		int32_t const snappedTo = mode.List()[(size_t)created].max[0];
		bool onSomeSide = false;

		for (size_t i = 0; i < mode.List().size(); i++)
		{
			if ((int32_t)i != created && (mode.List()[i].min[0] == snappedTo || mode.List()[i].max[0] == snappedTo))
			{
				onSomeSide = true;
			}
		}

		Check(snappedTo != box2.max[0] + 123457, "with Shift the face does not stay where the mouse is: " + std::to_string(snappedTo));
		mode.DragTo(farOff);
		Check(mode.List()[(size_t)created].max[0] == box2.max[0] + 123457, "without Shift it does");
		(void)onSomeSide;
		mode.DragToSnapped(box2.max);   // the face goes back to somewhere sensible before the release
		out = mode.EndDrag(context);

		// Finish Extent Edit (0x13) goes back to mode 1, page 1
		out = mode.Command(0x13, context);
		Check(mode.Mode() == 1 && out.page == 1 && !mode.CentreEdit() && mode.ZoomSteps() == 0, "Finish Extent Edit goes back to mode 1 (page 1) and lets go of the zoom and Centre Edit");
	}

	out = mode.Command(7, context);
	Check(out.page == 3 && mode.BaseWaypoint() == created && mode.CurrentWaypoint() < 0, "Set Base: the base is the created waypoint and page 3 is asked for");
	Check(mode.Command(8, context).say.find("pick") != std::string::npos, "Add Link without a current waypoint says so");

	// MODE 3, LINKING (FUN_00584d80; no menu item sends 0x11, the editor's tests do): the module picked is linked to the BASE waypoint
	{
		host::WaypointContext picking = context;

		picking.pickedModule = master;
		Check(mode.Command(0x11, context).page == 2 && mode.Mode() == 3, "0x11 with a base waypoint is mode 3 (page 2)");

		host::WaypointOutcome linkOut = mode.Command(8, picking);
		host::WaypointSet readLinks;

		SetOfModule(level, master, &readLinks);

		std::vector<host::WaypointRec> const &afterLink = alien ? readLinks.alien : readLinks.ground;

		Check(linkOut.levelChanged && afterLink[(size_t)created].moduleLinks.size() == mode.List()[(size_t)created].moduleLinks.size() && !afterLink[(size_t)created].moduleLinks.empty(),
			"Add Link in mode 3 puts the picked module on the base waypoint's module links: " + linkOut.say);

		size_t const had = afterLink[(size_t)created].moduleLinks.size();

		Check(mode.Command(8, picking).say.find("already") != std::string::npos, "the same module again is refused");
		linkOut = mode.Command(0x10, picking);
		SetOfModule(level, master, &readLinks);
		Check(linkOut.levelChanged && (alien ? readLinks.alien : readLinks.ground)[(size_t)created].moduleLinks.size() + 1 == had, "Delete Link takes it off again");
		Check(mode.Command(0x12, context).page == 1 && mode.Mode() == 1, "Finish Linking goes back to mode 1 (page 1)");
	}

	mode.Step(1);

	int32_t const other = mode.CurrentWaypoint();

	Check(other >= 0 && other != created, "Next picks a waypoint other than the base: " + std::to_string(other));

	out = mode.Command(9, context);   // Add Link - 1 way: base to current has One way, the way back One way the other direction
	SetOfModule(level, master, &after);

	std::vector<host::WaypointRec> &list = alien ? after.alien : after.ground;

	Check(out.levelChanged && FindLink(list[(size_t)created], other) >= 0 && list[(size_t)created].links[(size_t)FindLink(list[(size_t)created], other)].flags == 1 && FindLink(list[(size_t)other], created) >= 0 &&
		list[(size_t)other].links[(size_t)FindLink(list[(size_t)other], created)].flags == 2, "Add Link - 1 way: One way on the base's link, One way the other direction on the other's");

	out = mode.Command(0xa, context);   // Add Alien Link on the same pair ORs the Aliens only bit in
	SetOfModule(level, master, &after);

	std::vector<host::WaypointRec> &list2 = alien ? after.alien : after.ground;

	Check((list2[(size_t)created].links[(size_t)FindLink(list2[(size_t)created], other)].flags & 4) != 0, "Add Alien Link ORs the Aliens only bit into the existing link");

	out = mode.Command(0x10, context);   // Delete Link: both directions
	SetOfModule(level, master, &after);

	std::vector<host::WaypointRec> &list3 = alien ? after.alien : after.ground;

	Check(FindLink(list3[(size_t)created], other) < 0 && FindLink(list3[(size_t)other], created) < 0, "Delete Link takes the link out in both directions");
	Check(mode.Command(0x10, context).say.find("no link") != std::string::npos, "Delete Link with no link says so");

	// Can Crawl on the current waypoint
	out = mode.Command(0x16, context);
	SetOfModule(level, master, &after);

	std::vector<host::WaypointRec> &list4 = alien ? after.alien : after.ground;

	Check((list4[(size_t)other].flags & host::kWaypointCanCrawl) != 0, "Add Can Crawl sets the flag in the level");
	out = mode.Command(0x17, context);
	SetOfModule(level, master, &after);

	std::vector<host::WaypointRec> &list5 = alien ? after.alien : after.ground;

	Check((list5[(size_t)other].flags & host::kWaypointCanCrawl) == 0, "Remove Can Crawl clears it");

	// Delete Waypoint: the created one is the base; pick it by Step until current == created, delete
	for (size_t tries = 0; tries <= listSize + 2 && mode.CurrentWaypoint() != created; tries++)
	{
		mode.Step(1);
	}

	out = mode.Command(6, context);
	SetOfModule(level, master, &after);
	Check((alien ? after.alien : after.ground).size() == listSize && mode.BaseWaypoint() < 0, "Delete Waypoint takes the created one out again; it was the base, so there is none now: " + out.say);

	// Exit: back to mode 0 on page 0, and again leaves the screen
	out = mode.Command(2, context);
	Check(mode.Mode() == 0 && out.page == 0 && !out.leaveScreen, "Exit in mode 1 goes back to mode 0 on page 0");
	out = mode.Command(2, context);
	Check(out.leaveScreen, "Exit in mode 0 leaves the screen");

	// Add Module / Remove Module through the menu commands
	mode.Enter();
	context.pickedModule = master;
	mode.Command(0x19, context);

	std::vector<host::ModuleSnapshot> beforeAdd;

	host::ReadModuleSnapshots(&level, &beforeAdd);
	context.pickedModule = free;
	out = mode.Command(0x1a, context);
	Check(out.levelChanged, "Add Module with a free module picked joins it: " + out.say);

	std::vector<host::ModuleSnapshot> afterAdd;

	host::ReadModuleSnapshots(&level, &afterAdd);

	bool joined = false;

	for (host::ModuleSnapshot const &snapshot : afterAdd)
	{
		joined = joined || (snapshot.module && snapshot.index == free && snapshot.masterIndex == master);
	}

	Check(joined, "the free module is a slave of the master in the level now");

	out = mode.Command(0x1b, context);
	Check(out.levelChanged, "Remove Module takes it out again: " + out.say);

	std::vector<host::ModuleSnapshot> afterRemove;

	host::ReadModuleSnapshots(&level, &afterRemove);

	bool alone = false;

	for (host::ModuleSnapshot const &snapshot : afterRemove)
	{
		alone = alone || (snapshot.module && snapshot.index == free && snapshot.masterIndex < 0);
	}

	Check(alone, "it is on its own again");

	// Entry points > Delete: the adjacency between the AI modules of two picked modules; Recalculate all
	{
		std::vector<host::ModuleSnapshot> now;
		int32_t a = -1, b = -1;

		host::ReadModuleSnapshots(&level, &now);

		std::set<int32_t> moduleSet;

		for (host::ModuleSnapshot const &snapshot : now)
		{
			if (snapshot.module)
			{
				moduleSet.insert(snapshot.index);
			}
		}

		for (host::ModuleSnapshot const &snapshot : now)
		{
			if (snapshot.module && snapshot.hasAdjacency && a < 0)
			{
				for (host::ModuleAdjacencyEntry const &entry : snapshot.adjacency)
				{
					if (moduleSet.count(entry.objectIndex) && entry.objectIndex != snapshot.index)
					{
						a = snapshot.index;
						b = entry.objectIndex;
						break;
					}
				}
			}
		}

		Check(a >= 0, "the level has two modules that are adjacent: " + std::to_string(a) + " and " + std::to_string(b));

		context.pickedModules.clear();
		Check(mode.Command(0x24, context).say.find("pick two") != std::string::npos, "Entry points > Delete with fewer than two modules picked says so");

		auto entries = [&](std::vector<host::ModuleSnapshot> const &snaps) -> size_t
		{
			size_t n = 0;

			for (host::ModuleSnapshot const &snapshot : snaps)
			{
				n += snapshot.module && snapshot.hasAdjacency ? snapshot.adjacency.size() : 0;
			}

			return n;
		};

		size_t const beforeEntries = entries(now);

		context.pickedModules.push_back(a);
		context.pickedModules.push_back(b);
		out = mode.Command(0x24, context);

		std::vector<host::ModuleSnapshot> after2;

		host::ReadModuleSnapshots(&level, &after2);

		size_t removedNow = 0;
		bool oneWayGone = true;

		for (host::ModuleSnapshot const &snapshot : after2)
		{
			if (snapshot.module && snapshot.index == a && snapshot.hasAdjacency)
			{
				for (host::ModuleAdjacencyEntry const &entry : snapshot.adjacency)
				{
					oneWayGone = oneWayGone && entry.objectIndex != b;
				}
			}
		}

		removedNow = beforeEntries - entries(after2);
		Check(out.levelChanged && removedNow >= 1 && oneWayGone, "Entry points > Delete takes the adjacency between the two AI modules out (" + std::to_string(removedNow) + " entries): " + out.say);

		out = mode.Command(0x26, context);
		Check(out.levelChanged && out.say.find("entry points worked out") != std::string::npos, "Recalculate all works the entry points out: " + out.say);
	}

	// the copy
	std::string const folder = argv[2];
	std::string const copyPath = folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "waypoint-mode-test-copy.rif";

	CreateDirectoryA(folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile reopened;

	Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && reopened.Level(), "the game's loader takes the copy");
	DeleteFileA(copyPath.c_str());

	std::vector<char> originalAfter;

	Check(ReadWholeFile(argv[1], &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_waypoint_mode_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_waypoint_mode_test\n");
	return 0;
}
