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
// NAMED: (1) the waypoints are written into the level after EACH change, not when the mode is left (the level the editor holds is always the level it saves); (2) in mode 1 a waypoint is picked as
// inteng picks it (the box nearest the click on the screen, PickBox; Vasquez 11-17), in Edit Extents by a click in the three views (W-5 is to build); (3) a new waypoint (an empty box at the AI module's
// master's place, as inteng's) is written when its sizing is done, and a box the game's loader refuses then (an axis with no room) gets its axes free again; (4) Add / Remove Module with nothing to work on say so (inteng does nothing).
//
// MODE 0's PICK AND CURRENT, 1:1 (Vasquez 2026-11-17, reassess/waypoint-closeout-vasquez.md W-1 / W-8; FUN_00583d00): inteng keeps two modules. The PICKED one (DAT_0091bed4): a click on a module of
// the current AI module picks that module, a click on a module of another AI module picks THAT AI MODULE'S MASTER (0x58402a-0x5841e4), a click on nothing picks none. The CURRENT AI module
// (DAT_0091bedc, a master). Edit Module (0x19) makes the picked one's AI module current and lets the pick go; with nothing picked it leaves no AI module current. Add Module (0x1a) and Remove
// Module (0x1b) let the pick go. Show Alien / Ground Waypoints (3 / 4) open the PICKED module's AI module (FUN_00583b40: the current one is let go, the pick becomes the master); with nothing
// picked they do Edit Module's work (0x583daf / 0x583dd4 jump to 0x583df3), so no AI module is current after them. Mode 1's Exit (FUN_00583c10) comes back with that AI module picked.
// THE MODULE COLOURS of the AI modules mode (FUN_00588e60 paints a whole AI module, FUN_0046da70 one module; true colour): on entry every AI module is GREEN when its master's MODULEDT holds a
// WAYPOINT (FUN_00581430), else WHITE (FUN_00581650 0x58169b-0x581714); the picked module, or the picked AI module, is RED; the current AI module is 0xffffffff (white); a pick let go gets the
// colour it had (green or white by its own WAYPOINT, or 0xffffffff in the current AI module); mode 1's Exit paints the AI module RED. ModulePaint() is that paint, one colour per module.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "waypoint_model.h"

class OpenLevelFile;

namespace host
{
	// The AI modules mode's colours (true colour, as inteng paints them on his D3D inteng: (DAT_00d876d8 != 0)): white 0xffffff, green 0x00ff00, red 0xff0000, and the current AI module's -1 =
	// 0xffffffff, which is white too. Here as the view's ARGB.
	uint32_t const kWaypointModuleWhite = 0xFFFFFFFFu;     // (& 0xffff07) + 0xf8
	uint32_t const kWaypointModuleGreen = 0xFF00FF00u;     // (& 0xfe03) + 0xfd
	uint32_t const kWaypointModuleRed = 0xFFFF0000u;       // (& 0xfeff06) + 0xfa
	uint32_t const kWaypointModuleCurrent = 0xFFFFFFFFu;   // (DAT_00d876d8 != 0) + -2 = -1

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
		bool offerAirduct = false;   // THE CAN CRAWL OFFER (the owner 11-21): Can Crawl was just turned ON (Add Can Crawl, or W to on): the editor offers the module's Airduct flag
		                             // instead (can_crawl_offer.h), as the bit itself does nothing in the game (read only inside an "#if 0", bh_waypoint.cpp 870-883)
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

		// Does what the menu item with that inteng command number does. Commands this does not know are refused with "not built yet". In mode 0 a context.pickedModule other than the
		// last one seen is taken as a click on it first (Click).
		WaypointOutcome Command(int32_t command, WaypointContext const &context);

		// Mode 0: a left click in the 3D view on the module with that object index, or on no module (-1): the pick and the colours as inteng's click (see MODE 0's PICK above)
		void Click(OpenLevelFile *open, int32_t module);

		// Mode 0: the picked module (inteng's DAT_0091bed4: a module of the current AI module, or another AI module's master), or -1
		int32_t PickedModule() const { return picked; }

		// MULTI-MODULE SELECTION (Vasquez 2026-11-19, the owner's ask through Bishop: "select multiple modules and apply to them", "Inteng hides unselected modules"; a named difference):
		// - mode 0's Ctrl+click (SelectToggle) adds the clicked module's AI module (its master) to the SET, or takes it out; the first one starts the set from the pick's AI module. A plain
		//   click (Click) clears the set, and so does the screen's entry; Shift is not touched. The set's AI modules are painted RED, as inteng paints its pick.
		// - Show Alien / Ground Waypoints (3 / 4) opens the picked AI module as inteng's, and SHOWS every other AI module of the set with it: its modules are not hidden (ShownGroup) and its
		//   list of the same kind is drawn (ShownNets: read as the waypoints are shown). Only the current AI module's list is edited. Mode 1's Exit keeps the set.
		// SelectToggle gives false when nothing changed (not mode 0, no level, no module there).
		struct ShownNet
		{
			int32_t master;                  // the AI module's master (object index)
			std::string name;                // its name
			std::vector<WaypointRec> list;   // its list of the shown kind (alien or ground)
		};

		bool SelectToggle(OpenLevelFile *open, int32_t module);
		std::vector<int32_t> const &Selection() const { return selection; }   // the set's masters, in the order added
		std::string SelectionWords() const;                                   // their names joined by '|' (the state line's "waypointselection")
		std::vector<int32_t> ShownGroup() const;                              // modes 1 to 3: the current AI module's modules, then each shown one's
		std::vector<ShownNet> const &ShownNets() const { return shownNets; }
		std::string NetsWords() const;   // modes 1 to 3: "<name> boxes <n> links <n>" for the current AI module, then each shown one, joined by '|' (the state line's "waypointnets")

		// The module colours of the AI modules mode by object index (the entry's paint made on the first call after Enter)
		std::map<int32_t, uint32_t> const &ModulePaint(OpenLevelFile *open);

		// Edit Extents's answer (inteng drags the box's faces in the three views, FUN_00585750; here a typed answer, named): "x0 y0 z0 x1 y1 z1" (the lowest corner, then the highest, in millimetres, each axis put in
		// order as inteng does when the drag ends), "aim x y z" (the place aliens aim for, which sets the flag 0x80000000: the centre is then the user's), or "reset" (Reset Waypoint Centre, command 0x18:
		// the flag is cleared and the aim point goes back to the middle of the box, FUN_00586e90). Applies to the current waypoint.
		WaypointOutcome ApplyExtentsText(std::string const &typed, WaypointContext const &context);

		// DRAG EDITING of the box (FUN_00585750 0x585778-0x5860ff; Vasquez 11-17, waypoint-closeout-vasquez.md W-5): a press at a place (the file's units, the two coordinates the pane shows: axisMask
		// bits 0 x, 1 y, 2 z), in one of the three views:
		// - SIZING (after Create Waypoint, while an axis of the new box is free): the free axes the pane shows get min, max and the aim point on the place; DragTo moves max on them; EndDrag fixes the
		//   pane's axes and puts them in order; when all three are fixed the box is written;
		// - Centre Edit on: the aim point's shown axes go to the place (the user's aim point, flag 0x80000000) and it is written; no drag (false);
		// - else the press takes ONE face of the current box within 3 pixels (pixelFile file units each): for x, then y, then z of the shown axes the low side, else the high side; the last that
		//   matched wins (inteng ASSIGNS _DAT_0091bf48). DragTo moves it; EndDrag puts each axis in order, lets the aim point follow the box and writes it (the lists go back as they were when the
		//   game's loader refuses it).
		bool BeginDrag(int32_t const place[3], int axisMask, int32_t pixelFile);
		void DragTo(int32_t const place[3]);
		WaypointOutcome EndDrag(WaypointContext const &context);
		bool Dragging() const { return dragFaces != 0 || sizingMask != 0; }

		// Edit Extents' sizing of a new box: true while one of its axes is free (inteng reads no menu then, FUN_00585750 0x585778); which axes are fixed
		bool Sizing() const { return mode == 2 && !(axisFixed[0] && axisFixed[1] && axisFixed[2]); }
		bool AxisFixed(int axis) const { return axis >= 0 && axis < 3 && axisFixed[axis]; }

		// Edit Extents state (mode 2): Centre Edit (the press moves the aim point), the minimise toggle (0x1c)
		bool CentreEdit() const { return centreEdit; }
		bool Minimised() const { return minimised; }

		// EDIT EXTENTS' VIEWS (FUN_005846b0, FUN_005880e0): the three views are centred on the AI module's master's place (ViewsCentre, the file's units; set each time mode 2 is entered: ViewsMade
		// counts it) with ViewScale of inteng's units a pixel (100 to the world's millimetre): 5000 from the screen's start (FUN_00581650 0x582d0f), 0x20 (>) takes 1000 off while it is above 1000,
		// 0x21 (<) adds 1000. Zoom(+1) is 0x20, Zoom(-1) 0x21.
		int32_t ViewScale() const { return viewScale; }
		unsigned ViewsMade() const { return viewsMade; }
		void ViewsCentre(int32_t file[3]) const { file[0] = viewCentre[0]; file[1] = viewCentre[1]; file[2] = viewCentre[2]; }
		void Zoom(int delta);   // mode 2 only

		// Shift held while dragging: every axis is put on the nearest of the snap places (see the header's mode 2); with no place to snap to the number is kept
		void DragToSnapped(int32_t const place[3]);

		// Mode 1: the waypoint nearest to a place (the file's units) is made the current one; false when the list is empty. The place is the two coordinates the view shows (mask bit 0 x, 1 y,
		// 2 z: the ones that count) and the cursor's. Picks the box that holds the place, else the nearest centre.
		bool PickNear(int32_t const place[3], int axisMask);

		// Mode 1's click in the 3D view (FUN_00584ee0 0x58542d-0x58555d): the waypoint whose box the click is nearest to on the screen (the caller works that out: WaypointView_BoxCentre,
		// pick_centre.h) becomes the current one, unless it is the current one already or the base; false when nothing changes
		bool PickBox(int32_t index);

		// THE WAYPOINT DISPLAY'S CLICK (Vasquez 2026-11-19, the owner's "see and be able to edit all alien paths and volumes across all modules"; named): mode 0 shows every AI module's
		// alien network; a click on one of its boxes opens that AI module as inteng's pick + Show Alien Waypoints would (Click(master), command 3: mode 1, the alien list, the pick's colours,
		// the others hidden), with that box the CURRENT waypoint. Outside mode 0, or a master that is not one, nothing changes and the words say why; the outcome's page is 1 when it opened.
		WaypointOutcome OpenAt(OpenLevelFile *open, int32_t master, int32_t box);

		// WAYPOINT BY MOUSE (the owner's addition 11-18; Hicks's plan section 6, waypoint_pick.h; the ALIEN list only, the one the game loads): mode 1's click takes ANY waypoint of the
		// list, the current one and the base too (inteng's PickBox refuses those), or -1 lets the current go (the base stays); false when nothing changes, outside mode 1 or on the
		// ground list, or for a number not in the list
		bool MousePick(int32_t index);

		// Item 1 by mouse: the box (the file's units, waypoint_pick.h WaypointPick_NewBox) put at the end of the alien list in mode 1, made the current one and WRITTEN at once (Write: the
		// game's loader checks it; refused, the lists, the current and the base go back as they were and the words say why)
		WaypointOutcome MouseCreate(OpenLevelFile *open, WaypointRec const &box);

		// Item 3 by mouse, a DRAG from box a to box b: Add Link (Waypoints_AddLink, inteng's FUN_00586b40: the flags ORed in) from a to b with flags (0 both ways, kLinkAlien, kLinkOneWay,
		// or both), written at once as MouseCreate; then a is the base and b the current one, and the link between them is SELECTED (LinkSelected)
		WaypointOutcome MouseLink(OpenLevelFile *open, int32_t a, int32_t b, int32_t flags);

		// Item 3b: a CLICK on the link between a and b (alien list, mode 1) selects it: a the base, b the current one; nothing is written. MouseSetLinkFlags gives the selected link exactly
		// these flags (Waypoints_SetLinkFlags, base to current; the mirror back), written at once as MouseCreate. LinkFlagsOf: a's link to b's flags, -1 for none.
		WaypointOutcome MouseSelectLink(int32_t a, int32_t b);
		WaypointOutcome MouseSetLinkFlags(OpenLevelFile *open, int32_t flags);
		int32_t LinkFlagsOf(int32_t a, int32_t b) const;

		// AUTO-PLOT ALIEN WAYS (the owner's addition 11-18, autoplot_plan.h): AI module `master`'s ALIEN list replaced by `alien` and written at once (Write: the game's loader checks it); its
		// ground list kept. The mode's own state (the AI module it works on, its lists, current, base, the selected link) comes back as it was, read again when it was that AI module.
		// Refused (not a module of the level, its waypoints unreadable, the loader): nothing changed, the words say why.
		WaypointOutcome AutoPlotWrite(OpenLevelFile *open, int32_t master, std::vector<WaypointRec> const &alienList);

		// THE WAYPOINT-BOX GIZMO (Frost 11-18, waypoint_box_gizmo.h; mode 1, the alien list; defined in waypoint_box_mode.cpp): MouseShowBox puts box's corners, aim point and flags (its
		// links stay) on waypoint `index` in memory only, so the 3D view draws it as a drag goes (nothing is written; the drag's start is put back the same way). MouseSetBox puts them there
		// and WRITES it at once, as MouseCreate (refused: the lists as they were, and the words say why); `what` is the edit's words ("the box moved 120 along x").
		bool MouseShowBox(int32_t index, WaypointRec const &box);
		WaypointOutcome MouseSetBox(OpenLevelFile *open, int32_t index, WaypointRec const &box, std::string const &what);

		// The link selected by mouse: base to current, while they are still the pair it was selected with (any other change of either ends it)
		bool LinkSelected() const { return linkFrom >= 0 && linkFrom == wpBase && linkTo == wpCurrent; }

		// Mode 1: the next / previous waypoint of the list is the current one (wraps)
		void Step(int delta);

		// The active list as it is now (mode 1), the whole set, and the module the lists belong to
		std::vector<WaypointRec> const &List() const;
		WaypointSet const &Set() const { return set; }

		// The modules of the current AI module (the master and its slaves) as object indexes, for the view's colours; empty with no current module
		std::vector<int32_t> CurrentGroup() const { return group; }

		// W-6: how many module links of the list read named no module of the level and were dropped (FUN_005867c0); the mode's Exit writes the list without them (FUN_00587210)
		size_t DroppedModuleLinks() const { return droppedModuleLinks; }

		// W-4: the place (the file's units, its object's location) of the level's module of exactly this name, as the list was read; false for a name of no module (a module link to it was dropped, W-6)
		bool ModulePlace(std::string const &name, int32_t place[3]) const;

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
		int32_t viewScale;        // DAT_0091bef4
		unsigned viewsMade;
		int32_t viewCentre[3];    // DAT_0091bee8 as the views were made (the file's units)
		bool axisFixed[3];        // DAT_0091bf3c / bf40 / bf44: the new box's axes the sizing has fixed
		int sizingMask;           // the axes a sizing press is moving (the pane's), 0 for none
		OpenLevelFile *lastOpen;   // the level of the last command: the snap places are read from it
		int dragFaces;            // inteng's _DAT_0091bf48: 1 / 2 the lowest / highest x, 4 / 8 y, 0x10 / 0x20 z; 0 when no face is taken
		WaypointSet dragBackup;
		std::vector<int32_t> group;
		int32_t picked;           // DAT_0091bed4 (mode 0)
		int32_t lastSeenPick;     // the context's pickedModule last taken as a click
		int32_t linkFrom, linkTo;   // the link selected by mouse (LinkSelected), -1 for none
		std::map<int32_t, uint32_t> paint;
		bool paintMade;
		size_t droppedModuleLinks;   // W-6: module links of names of no module, dropped when the list was read (written out at the mode's Exit)
		std::map<std::string, std::vector<int32_t> > modulePlaces;   // W-4: each module's name and place (x y z, the file's units), as the list was read: the module-link lines end there
		std::vector<int32_t> selection;                // the set (multi-module selection): masters in the order added
		std::map<int32_t, std::string> selectionNames;   // their names
		std::string currentName;                       // the current AI module's master's name (NetsWords)
		std::vector<int32_t> shownGroup;               // modes 1 to 3: the other selected AI modules' modules
		std::vector<ShownNet> shownNets;               // and their lists

		int32_t SnapTo(int axis, int32_t value) const;
		void ShowSelection(OpenLevelFile *open, bool alienList);
		void ClearSelection(OpenLevelFile *open);
		void MakeEntryPaint(OpenLevelFile *open);
		void PaintAiModule(OpenLevelFile *open, int32_t module, uint32_t colour);
		WaypointOutcome EditModule(OpenLevelFile *open);
		void MakeViews(OpenLevelFile *open);
		bool Load(OpenLevelFile *open, int32_t master, std::string *why);
		bool Write(OpenLevelFile *open, std::string *why);
		std::vector<WaypointRec> &Active();
		void RefreshGroup(OpenLevelFile *open);
	};

	// For the commands that work on the set (Dietrich's "Auto-plot selected AI modules"): the editor's Waypoint mode's set (its masters' object indexes, in the order added); with none,
	// the picked module alone (inteng's pick); empty when nothing is picked. Defined beside the editor's waypointMode (construction_modes_screen.cpp).
	std::vector<int32_t> WaypointMode_SelectedModules();
}
