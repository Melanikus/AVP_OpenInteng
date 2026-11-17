// The test for Waypoint mode's state machine (host::WaypointMode, src/host/waypoint_mode.cpp) on REAL levels: the commands the code-built menus send (waypoint-menu-pairs.md), through to the
// level's bytes. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_waypoint_mode_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.
// CHECKS: a command that needs a pick says so and changes nothing; the AI modules mode's pick and colours as inteng's (green / white on entry, a click on another AI module picks its master and paints it red, Edit Module paints the current 0xffffffff and lets the pick go, Show Waypoints with nothing picked lets the current go, Exit from mode 1 comes back with that AI module picked red: Vasquez 11-17); Edit Module makes the picked module's AI module the current one; Show Ground / Alien Waypoints opens mode 1 on page 1;
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

	// THE AI MODULES MODE'S PICK AND COLOURS, 1:1 (Vasquez 11-17, waypoint-closeout-vasquez.md W-1 / W-8; FUN_00581650 0x58169b, FUN_00583d00 0x58402a-0x5841e4, 0x583df3, FUN_00583b40)
	int32_t const member = slave >= 0 ? slave : master;
	{
		std::map<int32_t, uint32_t> const entry = mode.ModulePaint(&level);
		std::map<int32_t, uint32_t>::const_iterator const atMaster = entry.find(master);
		std::map<int32_t, uint32_t>::const_iterator const atFree = entry.find(free);

		Check(atMaster != entry.end() && atMaster->second == host::kWaypointModuleGreen && atFree != entry.end() && atFree->second == host::kWaypointModuleWhite,
			"on entry the AI module with waypoints is green and a module of none white (FUN_00581430 / FUN_00588e60)");

		mode.Click(&level, member);

		std::map<int32_t, uint32_t> const clicked = mode.ModulePaint(&level);
		std::map<int32_t, uint32_t>::const_iterator const red = clicked.find(member);
		std::map<int32_t, uint32_t>::const_iterator const redMaster = clicked.find(master);

		Check(mode.PickedModule() == master && red != clicked.end() && red->second == host::kWaypointModuleRed && redMaster != clicked.end() && redMaster->second == host::kWaypointModuleRed,
			"a click on a module of another AI module picks that AI module's MASTER and paints the whole AI module red (0x5841b5-0x5841e4)");
	}

	// Edit Module makes the picked one's AI module current (0xffffffff) and lets the pick go
	context.pickedModule = member;
	host::WaypointOutcome out = mode.Command(0x19, context);

	Check(mode.CurrentModule() == master, "Edit Module on a module of the AI module makes its master current: " + out.say);
	Check(mode.PickedModule() < 0 && mode.ModulePaint(&level).find(master)->second == host::kWaypointModuleCurrent, "Edit Module lets the pick go and paints the current AI module 0xffffffff (0x583e56-0x583e80)");

	// a click on a module of the CURRENT AI module picks that one module, red alone; a click on nothing lets it go, back to 0xffffffff (0x584166-0x5841aa, 0x5840d9-0x5840e4)
	if (slave >= 0)
	{
		mode.Click(&level, slave);
		Check(mode.PickedModule() == slave && mode.ModulePaint(&level).find(slave)->second == host::kWaypointModuleRed && mode.ModulePaint(&level).find(master)->second == host::kWaypointModuleCurrent,
			"a click on a module of the current AI module picks it alone, red; the rest stays 0xffffffff");
		mode.Click(&level, -1);
		context.pickedModule = -1;   // the editor's pick follows the click
		Check(mode.PickedModule() < 0 && mode.ModulePaint(&level).find(slave)->second == host::kWaypointModuleCurrent, "a click on nothing lets the pick go: the module is the current's colour again");
	}

	// Show Waypoints with nothing picked does Edit Module's work (0x583daf -> 0x583df3): no AI module is current, and it is green again
	out = mode.Command(4, context);
	Check(mode.Mode() == 0 && mode.CurrentModule() < 0 && mode.ModulePaint(&level).find(master)->second == host::kWaypointModuleGreen,
		"Show Waypoints with nothing picked lets the current AI module go, green again, and stays in mode 0: " + out.say);

	// a click on the module again, then Show Waypoints opens the PICKED module's AI module (FUN_00583b40)
	mode.Click(&level, member);
	context.pickedModule = member;

	// mode 1
	host::WaypointSet before;

	SetOfModule(level, master, &before);
	out = mode.Command(before.ground.empty() ? 3 : 4, context);
	Check(mode.Mode() == 1 && out.page == 1 && mode.CurrentModule() == master, "Show Waypoints opens the picked module's AI module in mode 1 and asks for page 1: " + out.say);

	bool const alien = mode.AlienList();
	size_t const listSize = (alien ? before.alien : before.ground).size();

	Check(mode.List().size() == listSize && listSize >= 1, "the active list is the level's: " + std::to_string(listSize) + " waypoints");

	// THE 3D VIEW OF MODE 1 as inteng draws it (Vasquez 11-17, waypoint-closeout-vasquez.md W-2 / W-3; waypoint_view.h WaypointView_WorldLines): twelve edges a box, white with nothing current;
	// one line of two halves for each pair of linked waypoints (a link and its link back share it), green, or red on the half a one-way link may not be gone along from
	{
		std::vector<host::WaypointRec> const &list = mode.List();
		size_t pairs = 0, closedHalves = 0;

		for (size_t w = 0; w < list.size(); w++)
		{
			for (host::WaypointLinkRec const &link : list[w].links)
			{
				if (link.index < 0 || (size_t)link.index >= list.size())
				{
					continue;
				}

				bool back = false;

				for (host::WaypointLinkRec const &other : list[(size_t)link.index].links)
				{
					back = back || other.index == (int32_t)w;
				}

				if ((size_t)link.index < w && back)
				{
					continue;
				}

				pairs++;
				closedHalves += ((link.flags & host::kLinkOneWay) ? 1 : 0) + ((link.flags & host::kLinkOneWayBack) ? 1 : 0);
			}
		}

		std::vector<host::TrackLine> world;

		host::WaypointView_WorldLines(mode, 1.0, &world);

		size_t white = 0, green = 0, red = 0;

		for (host::TrackLine const &line : world)
		{
			white += line.colour == host::kWaypointBoxPlain ? 1 : 0;
			green += line.colour == host::kWaypointLinkPlain ? 1 : 0;
			red += line.colour == host::kWaypointLinkClosed ? 1 : 0;
		}

		Check(world.size() == 12 * list.size() + 2 * pairs && white == 12 * list.size() && green + red == 2 * pairs && red == closedHalves,
			"mode 1's 3D view: " + std::to_string(list.size()) + " white boxes of 12 edges, " + std::to_string(pairs) + " link lines of two halves (" + std::to_string(green) + " green, " + std::to_string(red) + " red halves)");

		// the click: the box nearest on the screen becomes current, never the current one again (FUN_00584ee0 0x58549b)
		Check(mode.PickBox(0) && mode.CurrentWaypoint() == 0 && !mode.PickBox(0), "a click on a box makes it current; the current one again changes nothing");

		std::vector<host::TrackLine> picked;

		host::WaypointView_WorldLines(mode, 1.0, &picked);

		size_t redEdges = 0;

		for (size_t i = 0; i < 12 && i < picked.size(); i++)
		{
			redEdges += picked[i].colour == host::kWaypointBoxCurrent ? 1 : 0;
		}

		Check(redEdges == 12, "the current waypoint's box is red (0x585518)");
	}

	// CREATE, as inteng (Vasquez 11-17, waypoint-closeout-vasquez.md W-5; FUN_00584ee0 0x584f8f-0x5850f2, FUN_00585750 0x585778-0x5858c3): an EMPTY box at the master's place, straight into Edit
	// Extents (page 4) with its three axes free; no menu command is read until a drag in two views has sized it; then it is written
	out = mode.Command(5, context);
	Check(!out.levelChanged && out.page == 4 && mode.Mode() == 2 && mode.Sizing() && mode.List().size() == listSize + 1 && mode.CurrentWaypoint() == (int32_t)listSize,
		"Create Waypoint adds an empty box, makes it current and goes into Edit Extents to size it: " + out.say);

	{
		host::WaypointRec const fresh = mode.List()[listSize];

		Check(fresh.min[0] == fresh.max[0] && fresh.min[1] == fresh.max[1] && fresh.min[2] == fresh.max[2] && fresh.centre[0] == fresh.min[0], "the new box is empty, its aim point at its place");

		mode.Command(0x13, context);
		Check(mode.Mode() == 2 && mode.Sizing(), "a menu command is not read while the box is sized (Finish Extent Edit does nothing)");

		int32_t const start[3] = { fresh.min[0] + 100, fresh.min[1] + 200, fresh.min[2] };
		int32_t const corner[3] = { start[0] + 2000, start[1] + 1500, 0 };

		// the top view (x and y): the press puts both on the place, the hold drags max, the release fixes them
		Check(mode.BeginDrag(start, 3, 10), "a press in the top view starts the sizing on x and y");
		mode.DragTo(corner);
		out = mode.EndDrag(context);
		Check(mode.Sizing() && mode.AxisFixed(0) && mode.AxisFixed(1) && !mode.AxisFixed(2) && !out.levelChanged, "the release fixes x and y; z is still free, nothing written yet: " + out.say);

		// the side view (z and y): only z is free
		int32_t const startZ[3] = { 0, 99999, fresh.min[2] - 300 };
		int32_t const cornerZ[3] = { 0, 99999, fresh.min[2] + 900 };

		Check(mode.BeginDrag(startZ, 6, 10), "a press in the side view sizes z");
		mode.DragTo(cornerZ);
		out = mode.EndDrag(context);

		host::WaypointRec const sized = mode.List()[listSize];

		Check(!mode.Sizing() && out.levelChanged && sized.min[0] == start[0] && sized.max[0] == corner[0] && sized.min[1] == start[1] && sized.max[1] == corner[1] && sized.min[2] == startZ[2]
			&& sized.max[2] == cornerZ[2] && sized.centre[2] == (startZ[2] + cornerZ[2]) / 2, "the second release fixes z (y kept) and the box is written: " + out.say);

		out = mode.Command(0x13, context);
		Check(mode.Mode() == 1 && out.page == 1, "Finish Extent Edit goes back to mode 1 once the box is sized");
	}

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

			// at a CORNER inteng takes ONE face: _DAT_0091bf48 is assigned, so of x then y the last that matched, y's (0x585ef1-0x585f7b; Vasquez 11-17, W-5)
			host::WaypointRec const cornerBox = mode.List()[(size_t)created];
			int32_t const atCorner[3] = { cornerBox.min[0] + 5, cornerBox.min[1] + 5, cornerBox.centre[2] };
			int32_t const pulled[3] = { cornerBox.min[0] - 400, cornerBox.min[1] - 300, cornerBox.centre[2] };

			Check(mode.BeginDrag(atCorner, 3, 10), "a press at the corner of the lowest x and y faces takes a face");
			mode.DragTo(pulled);
			Check(mode.List()[(size_t)created].min[1] == pulled[1] && mode.List()[(size_t)created].min[0] == cornerBox.min[0], "only the y face follows (one face a press, the last axis that matched)");
			mode.DragTo(cornerBox.min);
			mode.EndDrag(context);
		}
	}

	// MODE 2's own items: the minimise toggle, the zoom, Centre Edit, the snap, Finish Extent Edit
	{
		Check(mode.Mode() == 2 && !mode.Minimised() && !mode.CentreEdit() && mode.ViewScale() == 5000, "in mode 2 nothing is minimised or centred; the views' scale is 5000 a pixel (0x582d0f)");
		Check(mode.Command(0x1c, context).say.find("on") != std::string::npos && mode.Minimised(), "Minimise (0x1c) turns on");
		mode.Command(0x1c, context);
		Check(!mode.Minimised(), "Minimise (0x1c) again turns off");

		// the zoom (W-5; 0x585a5f / 0x585a7f): > takes 1000 off a pixel while above 1000, < adds 1000
		mode.Zoom(1);
		mode.Zoom(1);
		mode.Zoom(-1);
		Check(mode.ViewScale() == 4000, "> > < : 5000 - 1000 - 1000 + 1000 = 4000: " + std::to_string(mode.ViewScale()));

		for (int i = 0; i < 20; i++)
		{
			mode.Zoom(1);
		}

		Check(mode.ViewScale() == 1000, "> never goes below 1000");
		mode.Zoom(-1);
		Check(mode.ViewScale() == 2000, "< adds 1000 again");
		Check(mode.Command(7, context).say.find("Not while editing extents") != std::string::npos && mode.Mode() == 2, "Set Base is refused in mode 2: Finish Extent Edit first");

		// CENTRE EDIT: the press takes the aim point of the axes the pane shows, the drag moves it, the release writes it with the user's flag
		mode.Command(0x1f, context);
		Check(mode.CentreEdit(), "Centre Edit (0x1f) is on");

		host::WaypointRec const box = mode.List()[(size_t)created];
		int32_t const target[3] = { box.min[0] + 50, box.min[1] + 60, box.min[2] + 70 };

		Check(!mode.BeginDrag(target, 3, 10) && mode.Dragging(), "in Centre Edit a press anywhere puts the aim point there (axes x and y of the pane), no drag (0x585e7e-0x585ee6)");
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
		Check(mode.Mode() == 1 && out.page == 1 && !mode.CentreEdit() && mode.ViewScale() == 2000, "Finish Extent Edit goes back to mode 1 (page 1), lets go of Centre Edit and keeps the scale (inteng sets it at the screen's start only)");
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
	Check(mode.CurrentModule() < 0 && mode.PickedModule() == master && mode.ModulePaint(&level).find(master)->second == host::kWaypointModuleRed,
		"Exit in mode 1 comes back with no AI module current and that AI module picked, red (FUN_00583c10 0x583cbc)");
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

	Check(mode.PickedModule() < 0, "Add Module lets the pick go (0x583eee)");

	// inteng's Remove Module works on the PICKED module: the module is clicked again (now a module of the current AI module, so it alone is picked)
	mode.Click(&level, free);
	out = mode.Command(0x1b, context);
	Check(out.levelChanged, "Remove Module takes it out again: " + out.say);
	Check(mode.PickedModule() < 0 && mode.ModulePaint(&level).find(free)->second == host::kWaypointModuleWhite, "Remove Module lets the pick go and paints the module by its own WAYPOINT (white: none)");

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

	// W-6 (Bishop 11-17: copy inteng's drop 1:1; FUN_005867c0 0x586a2d, FUN_00583c10 -> FUN_00587210): a module link naming no module of the level is dropped when the list is read and the
	// mode's Exit writes the list without it. Red first: before, the name stayed in the list and in the file.
	{
		host::WaypointSet withDead;

		SetOfModule(level, master, &withDead);

		std::vector<host::WaypointRec> &deadList = withDead.alien.empty() ? withDead.ground : withDead.alien;

		if (!deadList.empty())
		{
			deadList[0].moduleLinks.push_back({ "NoSuchModuleW6", 0 });

			host::ModuleChildEdit edit;
			std::vector<host::ModuleChildEdit> edits;
			std::vector<bool> wrote;

			edit.objectIndex = master;
			memset(edit.name, 0, sizeof(edit.name));
			memcpy(edit.name, "WAYPOINT", 8);
			edit.remove = false;
			edit.chunk = host::Waypoints_Serialize(withDead);
			edits.push_back(edit);

			bool const planted = host::SetModuleChildrenMany(&level, edits, &wrote) == host::ObjectEditResult::Done && wrote.size() == 1 && wrote[0];

			Check(planted, "a module link to a name of no module is put in the level (the game's loader takes it)");

			host::WaypointMode dead;
			host::WaypointContext deadContext;

			deadContext.open = &level;
			deadContext.pickedModule = master;
			dead.Command(withDead.alien.empty() ? 4 : 3, deadContext);

			bool kept = false;

			for (host::WaypointModuleLink const &named : dead.List()[0].moduleLinks)
			{
				kept = kept || named.name == "NoSuchModuleW6";
			}

			Check(dead.Mode() == 1 && dead.DroppedModuleLinks() == 1 && !kept, "the list read drops it (one dropped)");

			host::WaypointOutcome const left = dead.Command(2, deadContext);
			host::WaypointSet afterExit;

			SetOfModule(level, master, &afterExit);

			bool inFile = false;

			for (host::WaypointRec const &wp : (withDead.alien.empty() ? afterExit.ground : afterExit.alien))
			{
				for (host::WaypointModuleLink const &named : wp.moduleLinks)
				{
					inFile = inFile || named.name == "NoSuchModuleW6";
				}
			}

			Check(left.levelChanged && !inFile, "the mode's Exit writes the list without it");
		}
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
