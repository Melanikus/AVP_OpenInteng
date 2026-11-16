// waypoint_mode.h: inteng's WAYPOINT MODE (command 0xcd; FUN_00586450 dispatches it by DAT_0091becc to FUN_00583d00 / FUN_00584ee0 / FUN_00585750 / FUN_00584d80) as a state machine on
// the open level, with no window: the editor gives it the commands the code-built menus send (waypoint-menu-pairs.md) and shows what it says. Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Facts: AvP-docs\engine\inteng\drake-waypoint-facts.md.
//
// THE MODES (page numbers are inteng's menu method +0x18 argument):
//  0  AI MODULES (page 0, FUN_00583d00): the module picked in the view; Edit Module (0x19) makes the AI module the picked module belongs to the current one; Add Module (0x1a) puts the picked
//     module (or, when it is another AI module's master, that whole AI module) in the current one (ai_modules_edit.h); Remove Module (0x1b) takes a module of the current AI module out;
//     Show Alien Waypoints (3) and Show Ground Waypoints (4) open the AI module's waypoints (mode 1); Exit leaves the screen.
//  1  WAYPOINTS (page 1, FUN_00584ee0, after FUN_00583b40 loaded the AI module's WAYPOINT chunk into the alien or the ground list): the waypoint picked in the view is the CURRENT one; Create
//     Waypoint (5), Delete Waypoint (6), Set Base (7: the current becomes the BASE and the links page, 3, shows), Add Link and its variants (8..0xd: base to current), Delete Link (0x10),
//     Add / Remove Can Crawl (0x16 / 0x17); Exit goes back to mode 0 (FUN_00583c10 writes the waypoints into the module's WAYPOINT chunk, FUN_00587210).
//  2  EXTENTS (page 4, FUN_00585750; Edit Extents 0x14 enters it, Finish Extent Edit 0x13 goes back to mode 1): a press within 3 pixels of a side of the current box takes it and the drag moves it; Shift while
//     dragging snaps to the nearest side of the other waypoints and of the AI module's modules (FUN_00587cd0 makes the three sorted lists, FUN_00588ac0 picks); Create / Delete Waypoint, Reset Waypoint Centre (0x18)
//     and the minimise toggle (0x1c) work in it too; the zoom keys < > (0x20 / 0x21) change the views' scale. Edit centre (0x1f, "Centre Edit") moves the aim point instead of the sides.
//  3  LINKING (page 2, FUN_00584d80, entered by command 0x11 with a base waypoint set; NO menu item or help key sends 0x11, so the screen cannot reach it: unfinished in inteng): the module picked is linked to
//     the BASE waypoint (Add Link 0x8, FUN_00586c60) or unlinked (Delete Link 0x10, FUN_00586db0); Finish Linking (0x12) goes back to mode 1.
//
// NAMED: (1) the waypoints are written into the level after EACH change, not when the mode is left (the level the editor holds is always the level it saves); (2) a waypoint is picked by the
// editor (a click in the three views, waypoint_view.h) or by Next / Previous, not by inteng's nearest-box-in-the-3D-view test; (3) a new waypoint is made at the AI module's master's place with a
// box of half a metre each way (inteng makes an empty box the Edit Extents screen then sizes); (4) a command that needs a module the editor has not picked says so and changes nothing.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "waypoint_model.h"

class OpenLevelFile;

namespace host
{
	// What the editor knows when it hands a command over
	struct WaypointContext
	{
		OpenLevelFile *open;
		int32_t pickedModule;    // the object index of the module picked in the view, or -1
		std::vector<int32_t> pickedModules;   // every module picked (Shift+click adds), in the order picked: Entry points > Delete takes the first two
	};

	// What a command did, for the editor to show and act on
	struct WaypointOutcome
	{
		std::string say;         // the status line
		int32_t page;            // the menu page to show now, or -1 to leave it
		bool leaveScreen;        // Exit of mode 0: back to Construction
		bool levelChanged;       // the level's bytes were swapped: what the editor made of the old one has to be made again
		bool askExtents;         // (not used by the editor any more: Edit Extents is mode 2; ApplyExtentsText stays for the tests)
	};

	class WaypointMode
	{
	public:
		WaypointMode();

		// The screen is entered: mode 0, nothing current, nothing picked
		void Enter();

		int Mode() const { return mode; }
		int32_t CurrentModule() const { return current; }
		bool AlienList() const { return alien; }
		int32_t CurrentWaypoint() const { return wpCurrent; }
		int32_t BaseWaypoint() const { return wpBase; }
		int32_t HighlightFlag() const { return highlight; }   // 0, kLinkAlien or kLinkDeviation: the links with that flag are drawn highlighted (items Alien / Deviation allowed, FUN_00588980)

		// Does what the menu item with that inteng command number does. Commands this does not know are refused with "not built yet".
		WaypointOutcome Command(int32_t command, WaypointContext const &context);

		// Edit Extents's answer (inteng drags the box's faces in the three views, FUN_00585750; here a typed answer, named): "x0 y0 z0 x1 y1 z1" (the lowest corner, then the highest, in millimetres, each axis put in
		// order as inteng does when the drag ends), "aim x y z" (the place aliens aim for, which sets the flag 0x80000000: the centre is then the user's), or "reset" (Reset Waypoint Centre, command 0x18:
		// the flag is cleared and the aim point goes back to the middle of the box, FUN_00586e90). Applies to the current waypoint.
		WaypointOutcome ApplyExtentsText(std::string const &typed, WaypointContext const &context);

		// DRAG EDITING of the box (FUN_00585750, the extents help: "Left click and drag on side to alter extents"): a press at a place (the file's units, the two coordinates the pane shows: axisMask bits 0 x, 1 y,
		// 2 z) within 3 pixels (pixelFile file units each) of a face of the CURRENT waypoint's box takes that face (the faces of both shown axes when it is at a corner); false when there is none.
		// DragTo moves the taken faces to the place; EndDrag puts each axis in order, lets the aim point follow the box and writes it (the lists go back as they were when the game's loader refuses it).
		bool BeginDrag(int32_t const place[3], int axisMask, int32_t pixelFile);
		void DragTo(int32_t const place[3]);
		WaypointOutcome EndDrag(WaypointContext const &context);
		bool Dragging() const { return dragFaces != 0; }

		// Edit Extents state (mode 2): Centre Edit (the drag moves the aim point), the minimise toggle (0x1c: only the current box is drawn), the zoom steps (< > : each step is a quarter of the view)
		bool CentreEdit() const { return centreEdit; }
		bool Minimised() const { return minimised; }
		int ZoomSteps() const { return zoomSteps; }
		void Zoom(int delta);   // mode 2 only

		// Shift held while dragging: every axis is put on the nearest of the snap places (see the header's mode 2); with no place to snap to the number is kept
		void DragToSnapped(int32_t const place[3]);

		// Mode 1: the waypoint nearest to a place (the file's units) is made the current one; false when the list is empty. The place is the two coordinates the view shows (mask bit 0 x, 1 y,
		// 2 z: the ones that count) and the cursor's. Picks the box that holds the place, else the nearest centre.
		bool PickNear(int32_t const place[3], int axisMask);

		// Mode 1: the next / previous waypoint of the list is the current one (wraps)
		void Step(int delta);

		// The active list as it is now (mode 1), the whole set, and the module the lists belong to
		std::vector<WaypointRec> const &List() const;
		WaypointSet const &Set() const { return set; }

		// The modules of the current AI module (the master and its slaves) as object indexes, for the view's colours; empty with no current module
		std::vector<int32_t> CurrentGroup() const { return group; }

		// The one-line words of the state, for the status line
		std::string Words() const;

	private:
		int mode;
		int32_t current;          // the AI module being worked on: its master's object index, or -1
		bool alien;               // the active list is the alien one
		WaypointSet set;
		int32_t wpCurrent;
		int32_t wpBase;
		int32_t highlight;
		bool centreEdit;
		bool minimised;
		int zoomSteps;
		OpenLevelFile *lastOpen;   // the level of the last command: the snap places are read from it
		int dragFaces;            // inteng's _DAT_0091bf48: 1 / 2 the lowest / highest x, 4 / 8 y, 0x10 / 0x20 z; 0 when no face is taken
		WaypointSet dragBackup;
		std::vector<int32_t> group;

		int32_t SnapTo(int axis, int32_t value) const;
		bool Load(OpenLevelFile *open, int32_t master, std::string *why);
		bool Write(OpenLevelFile *open, std::string *why);
		std::vector<WaypointRec> &Active();
		void RefreshGroup(OpenLevelFile *open);
	};
}
