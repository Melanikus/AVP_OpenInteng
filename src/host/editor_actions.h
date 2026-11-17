// editor_actions.h: the editor's one table of actions: every command the menu bar offers, its words, its key in each of the two key
// layouts (key_layout.h), and what it does; and the camera's keys in each layout. Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". (Modified 2026-11-14 by Melanikus
// (Dietrich), F3: the two key layouts, Classic inteng and Modern WASD.)
//
// The menu bar (src/ui/menu_bar.h) is made from this table, so a command and its menu item can never say different things. The
// table is Dietrich's; Frost adds the View menu's draw modes and toggles, and Hicks's side the level opening, each as a line here.
// - Each action's number is above 0 and used once, its menu is one of the four, and its words are the editor's own.
// - A key is the keyboard's own key (WM_KEYDOWN's virtual key), with Ctrl and Shift held or not. In the Classic layout the numeric
//   keypad's 7 and the main row's 7 are different keys that do different things, which the letter a key types (WM_CHAR) can't tell
//   apart. Alt is never part of a key: Alt and a key are Windows' (Alt+F4 closes the window) and the menu bar's.
// - The words beside an item are made from its key in the layout in use, so they can't name a key the item doesn't have. Keys Windows
//   does itself, such as Alt+F4, are only shown.
// - EditorActions_CheckTable is run on the table at start, as the test runs it: in each layout, no key does two things, and no action
//   takes a key the camera holds. A table it refuses is a mistake in the editor's own code, and the editor doesn't start.
// - An action changes the editor only through EditorHostState, which the loop then acts on: whether it keeps running, what the status
//   line says, and which key layout is in use.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "key_layout.h"
#include "menu_bar.h"

// The menus, in their order on the bar
enum class EditorMenu
{
	File,
	View,
	Mode,
	Help,
	Count
};

// What an action may change, which the loop acts on after it
struct EditorHostState
{
	bool running;              // false: the editor closes, as when its window is closed
	const char *statusWords;   // what the status line says now, or null to leave it as it was: the editor's own words only
	KeyLayout layout;          // the key layout in use. When an action changes it, the loop saves it in the .ini and remakes the menus.
	size_t screen;             // the old editor's screen in use, as a place in kIntengScreens (inteng_menus.h): its bar is the one shown.
	                           // When a pick changes it, the loop makes that screen's bar (editor_bar.h). (Added 2026-11-15 by Melanikus
	                           // (Dietrich), A6.)
	bool toggleObjectList;     // true: the object list opens, or closes if it is open (the panels are drawn over the level only while open).
	                           // (Added 2026-11-15 by Melanikus (Dietrich), L-1.)
	bool deleteSelectedObject; // true: the selected object is taken out of the level (Object Placement's Delete; object_edit.h). The loop does
	                           // it, as it holds the open level and the selection. (Added 2026-11-15 by Melanikus (Drake), S5.)
	bool saveCopy;             // true: the level is saved as a COPY in the editor's own folder (save_copy.h): never over the file that was
	                           // opened. (Added 2026-11-15 by Melanikus (Drake), S3d.)
	bool levelSettings;        // true: the field panel shows the level's own settings (L), editable, instead of what's selected. (Added
	                           // 2026-11-15 by Melanikus (Drake), L.)
	bool createLikeSelected;   // true: a copy of the selected object is made at the view's place (Object Placement's Create, With shape:
	                           // Selected). (Added 2026-11-15 by Melanikus (Drake), S5.)
	bool deleteModuleData;     // true: Delete Module's Data on the selected object (module_data_edit.h)
	bool mergeObjects;         // true: Merge Objects over the selected objects (shape_merge_edit.h)
	bool findNotDone;         // true: the modules without adjacent modules are listed (Adjacent Modules, Find Not Done)
	bool recalcEntryPoints;    // true: every entry point between adjacent modules is worked out again (Adjacent Modules, Recalculate Entry Points)
	bool showConstructionKeys; // true: Construction's controls are shown as a text over the level (Keys - Mode, inteng_help.h)
	bool showPlacementKeys;    // true: Object Placement's Keys text (FUN_004d9af0 0x53: the string at 0x5e853c, FUN_004724f0) is shown over the level
	bool showLinkHelp;         // true: Module Linking's Help text is shown over the level (inteng_help.h)
	int32_t waypointCommand;   // an item of Waypoint mode's code-built menus was chosen: its inteng command number (waypoint_mode.h), or 0
	bool waypointAsked;        // true: waypointCommand is to be done
	bool paintWhite;           // true: every polygon is painted white (Mode > No Textures: Strategy Data entered with flag 0x80, inteng's FUN_00522ab0)
	bool copyTrack;            // true: Copy Track (track_edit.h). (Added 2026-11-15 by Melanikus (Dietrich), P10.)
	bool exitMode;             // true: the mode's Exit (inteng: the mode's write-back, the level saved, "Camera position saved", then the hub; Strategy asks "Would you like to save first?")
	bool pasteTrackAbsolute;   // true: Paste Track Absolute (track_edit.h)
	bool pasteTrackRelative;   // true: Paste Track Relative (track_edit.h: the pasted track is first turned and moved onto the object)
	bool moveRelToPoly;        // true: Move rel. to poly: the next click picks a polygon of another object (move_rel_poly_edit.h). (Added 2026-11-15 by Melanikus (Dietrich).)
	int32_t moveCentre;        // 1: Move centre to top, 2: to bottom, over every selected object's shape (shape_edit.h ShapeEdit_Recentre). (Added 2026-11-15 by Melanikus (Dietrich).)
	bool moduleOverlaps;       // true: Module Overlaps switched (module_overlaps.h). (Added 2026-11-15 by Melanikus (Hicks).)
	bool recalculateNormals;   // true: Recalculate Normals and Extents over the level (shape_recalc_edit.h). (Added 2026-11-15 by Melanikus (Dietrich).)
	bool takeScreenshot;       // true: the window is saved as a picture in the editor's own folder (Screenshot, screenshot_name.h). (Added 2026-11-15
	                           // by Melanikus (Dietrich), P6.)
	int32_t linkOp;            // a Module Linking action for the loop to do: one of the kLink numbers below, or 0
	int32_t viewMode;          // a screen's View Mode item: 1 Polygon, 2 Polyline, 3 Filled Polyline (the editor's draw modes, view_toggles.h), or 0
	int32_t chooserOp;         // the Shape Chooser: one of the kChooser numbers below, or 0
	int32_t adjacentOp;      // an Adjacent Modules action for the loop to do: one of the kAdj numbers below, or 0
	bool setBaseModule;        // true: the selected object becomes the base object (Construction, Set Base Module)
	int32_t changeType;        // Construction's Object, Change Object Type: 1 = Module (0x12d), 2 = Placed (0x12e), for every selected object, or 0 (object_type_edit.h)
	int32_t optionToggle;      // an Options switch was chosen: 1 Select by polygons, 2 Prefer .BMP over .RIM, 3 Use Polygon View Distance (editor_options.h), or 0
	int32_t pathCommand;       // a Path mode item's command number (3 Add Module, 4 Delete Module, 5 Create Path, 6 Select Path, 7 Delete Path, 8 Keys, 9 Properties: path-menu-pairs.md), or 0 (path_mode.h)
	int32_t trackCommand;      // a Track Edit item's command number (0x514 to 0x52b: track-menu-pairs.md), or 0 (track_editor.h)
	int32_t acousticCommand;   // an Acoustic Mode item's command number (0xa Exit, 0xbb8 .. 0xbbd: reassess/object-placement.md AC-3 .. AC-8), or 0 (acoustic_mode.h)
	int32_t strategyCommand;   // a Strategy mode item's command number (0xc Delete Strategy, ...: strategy-menu-pairs.md), or 0 (strategy_mode.h)
	int32_t altLocation;       // an Alt Locations item's command number (0x64 to 0x69), or 0
	int32_t hierarchyCommand;  // a Hierarchy mode item's command number (hierarchy_mode.h), or 0
	int32_t placedHierarchyCommand;   // the current placed hierarchy's pop-up item (0x7d0 Play, 0x7d1 Stop, ...: reassess/object-placement.md PH-2..PH-13), or 0
	int32_t fixTears;          // Construction, Miscellaneous, Fix tears: 1 = the two selected objects (0x5a), 2 = all (0x5b)
	bool recentrePolygon;      // Texture Map, Recentre Shape (0xd6): wait for the click on a polygon
	bool splitScreen;          // Object Placement, Misc, Toggle Split Screen (0x52)
	bool moveRelativeModule;   // Object Placement, Object, Move relative (0x1c)
	bool gravityToggle;        // Object Placement, Misc, Toggle Gravity (0x58)
	bool snapToVertex;         // Object Placement, Misc, Snap To Vertex (0x4e2): wait for the click
	bool placementPolyCounts;  // Object Placement, Misc, Show Polygon Counts (0x5a): toggled
	int32_t specialCommand;    // an Object Placement item for the special objects (0x34 AVP generator, 0x35 Bad Guy, 0x3a Player start, 0x37 Sound, 0x39 Particle Generator, 0x18 Edit, special_mode.h), or 0
	int32_t textureCommand;    // a Texture Map mode item's command number (texture-map-menu-pairs.md), or 0
	int32_t quickCreate;       // a Quick-create item's command number: the shape it names is made into an object at the view (P3), or 0
	int32_t lightWork;         // something Light Sourcing does that the loop does, as it holds the level: one of the kLight numbers below, or 0 (M-3)
	int32_t prompt;            // a prompt to open for a typed answer: one of the kPrompt numbers below, or 0 for none (M-1)
	int32_t clickWants;        // a "command, then click" (click_mode.h): what the next click in the view has to find, as a host::ClickWants number,
	                           // or 0 to start none. Then clickCommand is the number handed back with the answer and clickWords the command's name
	                           // for the status line. (Added 2026-11-15 by Melanikus (Dietrich), P4.)
	int32_t clickCommand;
	const char *clickWords;
};

// The prompts an action can ask the loop to open (M-1, object placement): each is a line the user types, then Enter
int32_t const kPromptNone = 0;
int32_t const kPromptFind = 1;       // an object's name or ID: the view flies to it
int32_t const kPromptMove = 2;       // how far to move the selected object: three whole numbers
// The Module Linking actions an item can ask the loop for
int32_t const kLinkNone = 0;
int32_t const kLinkSetCurrent = 1;
int32_t const kLinkNormal = 2;
int32_t const kLinkBranch = 3;
int32_t const kLinkDelete = 4;
int32_t const kLinkClose = 5;
int32_t const kLinkReverseModule = 10;   // AutoLink > Add Reverse Links: Module
int32_t const kLinkReverseAll = 11;      // AutoLink > Add Reverse Links: Environment
int32_t const kLinkDeleteLinks = 9;   // AutoLink > Delete Links: Module (the picked modules' links are deleted)
int32_t const kLinkView = 7;   // View Branch
int32_t const kLinkHide = 8;   // Hide Branch
int32_t const kLinkOpen = 12;  // Open Branch
int32_t const kLinkAutoModule = 13;       // AutoLink > Module (0xa1): the picked modules, their old links kept
int32_t const kLinkAutoEnvironment = 14;  // AutoLink > Environment (0xa2): every module, old links kept
int32_t const kLinkAutoModuleNew = 15;    // AutoLink > Module (delete old) (0xa5)
int32_t const kLinkAutoEnvironmentNew = 16;   // AutoLink > Environment (delete old) (0xa6)
int32_t const kLinkExitNew = 18;          // Mode > Exit + New Adjacencies (0x66): leave, then work out the adjacency of the modules that have none (adjacency_generate.h)
int32_t const kLinkExitAll = 19;          // Mode > Exit + All Adjacencies (0x65): leave, then ask, then work out every module's adjacency
int32_t const kLinkLoadFile = 17;         // AutoLink > Load from file (0xa3): the level's .mlf (autolink_mlf.h)
int32_t const kLinkHideObjects = 20;     // Object > Hide (0x96): the picked modules are hidden (hidden_objects.h)
int32_t const kLinkUnhideLast = 21;      // Object > Un-hide last (0x9a)
int32_t const kLinkUnhideAll = 22;       // Object > Un-hide all (0x98)
int32_t const kLinkPolyCounts = 23;      // Base > Show Poly Counts (0xcc, FUN_004c0950): the list of polygon counts (poly_counts.h)
int32_t const kLinkCentres = 24;         // Base > Select With Centres (0xcd): the click takes the module whose centre is nearest (FUN_0046ddb0)
int32_t const kLinkMixed = 25;          // Mixed (0x4a), Dietrich: the base used it without a definition here
int32_t const kLinkShowAll = 26;        // Show All (0xca), Dietrich
int32_t const kLinkViewDistance = 27;   // Toggle View Distance (0x49), Dietrich
int32_t const kLinkOnlyLinked = 28;     // Only Draw Linked Modules (0x4b), Dietrich
int32_t const kLinkExit = 6;  // Module Linking, Exit: the links are written, the screen's state is let go of
// The Shape Chooser's actions (shape_chooser.h)
int32_t const kChooserNone = 0;
int32_t const kChooserStart = 1;      // Replace shape: the chooser opens for the selected object
int32_t const kChooserWith = 2;       // Exit with shape (0xc)
int32_t const kChooserWithout = 3;    // Exit without shape (0xa)
int32_t const kChooserStartCreate = 4;   // Create > With shape > Choose: the chooser opens for a NEW object
// The Adjacent Modules actions an item can ask the loop for (adjacent_list.h)
int32_t const kAdjNone = 0;
int32_t const kAdjAdd = 1;
int32_t const kAdjDelete = 2;
int32_t const kAdjSetCurrent = 3;
int32_t const kAdjExit = 4;      // Mode > Exit: the list is written back, the screen's state is let go of (the item also goes to the hub)
int32_t const kAdjViewDistance = 6;   // Base > Toggle View Distance
int32_t const kAdjShowAll = 5;   // Base > Show All: modules with an ADJMDLEP blue, the others white

int32_t const kPromptAsc = 5;        // Write Asc File: the name of the file to write, without ".asc" (inteng: "Type filename (without .asc extension)")
int32_t const kPromptAscDoors = 6;   // Write Asc File (Doors closed)
int32_t const kPromptMoveRelPoly = 10; // Move rel. to poly: the distance from the polygon, in metres (inteng: "distance to move from the poly in metres :")

int32_t const kPromptName = 3;       // a new name for the selected object
int32_t const kPromptAltGroup = 4;   // Alt Locations, Assign Group Number: the group number (0 means no group)
int32_t const kPromptAltIndex = 7;   // Alt Locations, Change shown location: the index for every object
int32_t const kPromptSmallLimit = 8;  // Find Small Modules: the limit, in metres
int32_t const kPromptSmallPick = 9;   // Find Small Modules: which of the small modules to go to (a number from the list)
int32_t const kPromptVisibility = 17;   // AutoLink > Set Visibility (0xa4): the visible distance in whole metres (0 keeps every module) (registry number 17)
int32_t const kPromptWaypointExtents = 31;   // Waypoint mode, Edit Extents (0x14): the current waypoint's box, or its aim point, or reset (registry number 31; Drake wrote 19, which is Frost's)
int32_t const kPromptRegenAdjacency = 18;   // Mode > Exit + All Adjacencies (0x65): "Do you want to regenerate adjacent module data?" and a typed yes
int32_t const kPromptDestroyPick = 34;      // Strategy mode, Destroy Virtual Object (0x40): which virtual object (a number from the list)
int32_t const kPromptDestroyAsk = 35;       // Destroy Virtual Object: "Delete this object?" (yes or no)
int32_t const kPromptAssignKind = 32;       // Strategy mode, Assign Strategy (0x8): the number of the strategy kind in the list
int32_t const kPromptAssignReplace = 33;    // Assign Strategy: "Object already has a strategy. Would you like to replace it?" (yes or no)
int32_t const kPromptDeleteMulti = 36;      // Strategy mode, Delete Multiple Strategies (0x42): the numbers of the groups to delete
int32_t const kPromptVirtualName = 29;     // Strategy mode, Create Virtual Object (0x39-0x3f): "Type name for new virtual object"
int32_t const kPromptStability = 28;       // Strategy mode, Stability flags (0x37): the six faces of the picked module, typed as six letters r (red: stable) or g (green: unstable)
int32_t const kPromptExitSave = 47;        // Strategy Data's Exit (0x5) when the level changed: "Would you like to save first?" (yes or no)
int32_t const kPromptTrackTime = 37;       // Track Edit, Properties > Section (0x51b): "Time for section", in seconds
int32_t const kPromptTrackFlags = 38;      // Track Edit, Properties > Track (0x527): the letters of the flags that are on
int32_t const kPromptTrackSound = 39;      // Track Edit, Sound / Start Sound / End Sound > Edit (0x524 0x528 0x52a): the file name and numbers
int32_t const kPromptTrackDelete = 40;     // Track Edit, Delete > Track (0x520): "Delete this track?" (yes or no)
int32_t const kPromptTrackTurn = 46;       // Track Edit, Turn End (ours, 0x5f0): the degrees of yaw, pitch and roll from the start turn
int32_t const kPromptPathName = 24;        // Path mode, Create Path (5): "Name of path (optional)"
int32_t const kPromptPathPick = 25;        // Path mode, Select Path (6): which of the level's paths to work on (a number from the list)
int32_t const kPromptPathDelete = 26;      // Path mode, Delete Path (7): "Delete this path?" (yes or no)
int32_t const kPromptPathProps = 27;       // Path mode, Properties (9): "Path Properties": Loop path or Go back and forth
int32_t const kPromptStrategyMessage = 23;   // Strategy mode, Module flags > Message (0x2c): "Type in a message number from 1 to 127" (the number goes in the modules' flag bits)
int32_t const kPromptPolyPick = 21;        // Module Linking, Show Poly Counts (0xcc): a page of the list, a row number to go to, n or p for the next or previous page
int32_t const kPromptFindSelect = 22;       // Construction's Find Object (0x52): the same typed name as Find, but the object found is selected too
int32_t const kPromptStrategyDelete = 19;  // Strategy mode, Delete Strategy (0xc): "Are you sure you want to delete the strategy on this object?" (yes or no)
int32_t const kPromptStrategyFans = 20;    // Strategy mode, Reset fan wind speed (0x43): "Would you like to reset the wind strength for all fans" (yes or no)
int32_t const kPromptSoftEdge = 41;  // Light Sourcing, Edge > Soft: the maximum angle for soft edges (shape_soften_edit.h)
int32_t const kPromptDeleteAllLinks = 12;   // AutoLink > Delete Links: Environment: a yes before every module's links are deleted
int32_t const kPromptTypeLinks = 13;  // Change Object Type: Placed: "<name> has module linking data. Do you want to delete it?" (yes or no), once per object that has some
int32_t const kPromptRescale = 14;   // Rescale: the new size of the selected object's shape (shape_rescale_edit.h)
int32_t const kPromptChooserPick = 15;   // the Shape Chooser's "Select shape from list": a place in the list or part of a name (registry number 15)
int32_t const kPromptTears = 44;   // Fix tears (0x5a / 0x5b): the tolerance in mm ("Type in tolerance level for tear fixing (in mm)"). Taken 2026-11-15
int32_t const kPromptMoveRelModule = 45;   // Move relative (0x1c): the three offsets in metres. Taken 2026-11-15
int32_t const kPromptSpecial = 42;   // Object Placement's special-object dialogs (special_mode.h): one number for all, which field is open is the mode's. Taken 2026-11-15
int32_t const kPromptTextureMap = 43;   // Texture Map mode's typed answers (Select by Angle, the arrow step sizes, ...): which one is open is the mode's (taken on the board 2026-11-15)
int32_t const kPromptHierarchy = 30;   // Hierarchy mode's questions (hierarchy_mode.h); one number for all, which one is open is the mode's. Taken on the board 2026-11-15
int32_t const kPromptCreateName = 16;    // Create > With shape > Choose: the name of the new object ("Name Object"; registry number 16)

int32_t const kLightNone = 0;
int32_t const kLightSelectLights = 3;    // a click picks a light
int32_t const kLightSelectObjects = 4;   // a click picks an object
int32_t const kLightHideAll = 6;         // every light gets LOFlag_Invisible
int32_t const kLightUnhideAll = 7;       // every light loses it
int32_t const kLightHelp = 5;            // the status line says what Light Sourcing does so far
int32_t const kLightHierarchy = 14;      // Hierarchy > Hierarchy: select a hierarchy, then the vertex normals are made again from the polygons
int32_t const kLightSequence = 15;       // Hierarchy > Sequence
int32_t const kLightDoorsClose = 8;      // Doors > Close: the morph shapes are the closed state (light_state.h)
int32_t const kLightDoorsOpen = 9;       // Doors > Open
int32_t const kLightToggle = 10;         // Lights > Toggle on/off: the selected light's state
int32_t const kLightSwitchAllOn = 11;    // Lights > Switch all on
int32_t const kLightSaveGame = 12;       // Save Intensities for Game
int32_t const kLightSaveTools = 13;      // Save Intensities for Tools
int32_t const kLightExit = 16;           // Mode > Exit: the write-back of FUN_004aa910 (every light un-hidden, bits 0x300 out of the engine flags), the copy saved when the level changed, the hub

typedef void (*EditorActionRun)(EditorHostState *host);

// A key that does an action, when no menu is open: a key of the keyboard, with Ctrl and Shift held or not
struct EditorKey
{
	uint8_t virtualKey;        // Windows' number for the key (VK_...): 'A' to 'Z' and '0' to '9' for the main keys. 0 for no key.
	bool ctrl;
	bool shift;
};

struct EditorAction
{
	int32_t id;                // above 0, each once
	EditorMenu menu;
	const char *name;          // the item's words
	EditorKey classic;         // its key in the Classic inteng layout, or { 0 } for none
	EditorKey modern;          // its key in the Modern WASD layout, or { 0 } for none
	const char *windowsKeys;   // keys Windows does itself, only shown beside the item (Quit's Alt+F4), or null. Never with a key of its own.
	EditorActionRun run;
};

// The actions' numbers. Each menu has its own range, so a line added to one never takes another's number.
int32_t const kActionOpenLevel = 1;
int32_t const kActionQuit = 2;
int32_t const kActionSaveCopy = 3;               // Save a copy: <name>-edited.rif in the editor's own folder (save_copy.h), never over the opened file
// The View menu, inteng's own draw modes and Gouraud switch (view_toggles.h; view-state-table.md). (Modified 2026-11-14 by Melanikus
// (Frost): reshaped to inteng's. 102 (Fully lit, now the Gouraud switch off), 111 and 112 (the green wireframes, now Polyline with the
// switch on or off) are RETIRED: never used again, so a saved key layout naming one is refused as unknown, never taken as something else.)
int32_t const kActionViewPolyline = 101;         // inteng's 8: every polygon a white line. Off while ViewToggles_OutlinesCanBeShown says false
int32_t const kActionViewBackFaces = 103;        // off while ViewToggles_BackFacesCanBeShown says false (view_toggles_game.h)
int32_t const kActionViewBrighter = 104;
int32_t const kActionViewDarker = 105;
int32_t const kActionViewPolygon = 106;          // inteng's 7: the polygons solid
int32_t const kActionViewFilledPolyline = 107;   // inteng's 9: the polygons with white lines. Off as Polyline is
int32_t const kActionViewGouraud = 108;          // the Gouraud switch, on or off (keys.txt's B)
int32_t const kActionViewHeadlight = 109;        // inteng's Ctrl+B
int32_t const kActionViewNameObject = 113;       // ours: a new name for the selected object (M-1)
int32_t const kActionViewObjectList = 111;        // ours: the object list, over the level, open or closed (L-1)
int32_t const kActionViewWireTextured = 110;     // ours, his request: the game's own lines in their textures' colour
int32_t const kActionViewTextures = 115;          // Textured / Untextured, for every draw mode (view_toggles.h; inteng's DAT_005fd6b4, run 25)
int32_t const kActionViewGouraudLevelOnly = 114;  // inteng's Gouraud state 2 (command 0xcc): only the level's own gouraud polygons stay shaded. Off, [wip]: not built
int32_t const kActionViewKeysClassic = 151;      // the two key layouts: the one in use is shown switched off
int32_t const kActionViewKeysModern = 152;
int32_t const kActionLevelSettings = 201;         // the Mode menu's: the level's settings in the field panel, READ-ONLY (inteng has no edit for them; Bishop's audit 2026-11-15)
int32_t const kActionHelpKeys = 301;
int32_t const kActionHelpAbout = 302;

extern const EditorAction kEditorActions[];
extern const size_t kEditorActionCount;

// The camera's moves. Their keys are held, not pressed once: the camera moves each frame while its key is down.
enum class EditorMove
{
	Forward,
	Back,
	Left,
	Right,
	Up,
	Down,
	TurnLeft,
	TurnRight,
	LookUp,
	LookDown,
	RollLeft,    // keypad 7 (inteng's Movement help: "7/9  Roll left/right")
	RollRight,   // keypad 9
	Count
};

struct EditorMoveKeys
{
	EditorMove move;
	const char *name;          // the move's words, for Help, Keys
	uint8_t classic;           // its key (VK_...) in the Classic inteng layout
	uint8_t modern;            // and in the Modern WASD layout
};

// One line for each EditorMove, in its order
extern const EditorMoveKeys kEditorMoves[];

// The words the status line shows for Help, About, for picking a key layout, and for an action that isn't there yet
extern const char kEditorAboutWords[];
extern const char kEditorClassicNowWords[];
extern const char kEditorModernNowWords[];
extern const char kEditorOpenNotYetWords[];

// The room a key's words take beside an item, with their 0: "Ctrl+Shift+Num 9", the longest, is 16 letters. The menu bar takes up to
// kMenuKeysMost.
size_t const kEditorKeyWordsRoom = 24;
static_assert(kEditorKeyWordsRoom - 1 <= ui::kMenuKeysMost, "a key's words fit beside a menu item");

struct EditorKeyWords
{
	char text[kEditorKeyWordsRoom];
};

// A key's words, such as "Ctrl+O", "7", "Num 4" or "Left", into words. Gives back false, with empty words, for no key, a key the editor
// has no name for, or too little room (and nothing at all for no room).
bool EditorActions_KeyWords(const EditorKey &key, char *words, size_t size);

// An action's key in a layout
const EditorKey &EditorActions_KeyIn(const EditorAction &action, KeyLayout layout);

// A move's key in a layout, or 0 for a move that isn't one of EditorMove's
uint8_t EditorActions_MoveKey(KeyLayout layout, EditorMove move);

// Help, Keys: the camera's keys in a layout, made from kEditorMoves, so they always say what the keys do. The words stay as they are
// until the next call.
const char *EditorActions_KeysWords(KeyLayout layout);

// Checks a table of actions and moves, as the editor does its own at start. Gives back null when it's sound, or else the first thing
// wrong, in the editor's own words. Sound means: every action's number is above 0 and used once, its menu is one of the four, it has
// words and something it does, and it shows at most one kind of key; there's a line for each move, in order, with a key in both layouts;
// every key has a name; and in each layout no two actions share a key, no two moves share one, no action takes a move's key (with Ctrl,
// Shift or neither: a held key counts whatever else is held), and none takes Ctrl+V, which stays the paste's.
const char *EditorActions_CheckTable(const EditorAction *actions, size_t actionCount, const EditorMoveKeys *moves, size_t moveCount);

// The menu bar's menus, made from a table of actions (the editor's own is kEditorActions) for a layout: one for each EditorMenu, in that
// order, with their items in the table's order, each with its key's words in that layout. menus has room for EditorMenu::Count menus,
// and items and words for actionCount each; all three have to stay where they are while the bar uses them. Gives back false, filling
// nothing, for no table, or if any of them has too little room.
bool EditorActions_MakeMenus(const EditorAction *actions, size_t actionCount, KeyLayout layout, ui::Menu *menus, size_t menuRoom,
	ui::MenuItem *items, size_t itemRoom, EditorKeyWords *words, size_t wordsRoom);

// The action with this number, or null
const EditorAction *EditorActions_Find(int32_t id);

// The action a key does in a layout, in a table of actions (the editor's own is kEditorActions), when no menu is open, or null.
// virtualKey is the key WM_KEYDOWN gives. With Alt held (or AltGr, which Windows gives as Ctrl and Alt together) it's none, as those
// keys are Windows', the menu bar's and typing's.
const EditorAction *EditorActions_ForKey(const EditorAction *actions, size_t actionCount, KeyLayout layout, uint8_t virtualKey, bool shiftHeld,
	bool ctrlHeld, bool altHeld);
