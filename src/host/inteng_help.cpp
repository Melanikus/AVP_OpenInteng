// inteng_help.cpp: the old editor's help texts, copied from inteng.exe's own strings. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A6),
// by a script from AvP-docs\engine\inteng\raw\inteng-strings.txt, not by hand. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "inteng_help.h"

// Construction's Miscellaneous > Keys - Mode: FUN_00445280 (out\map-1-strings.txt:8719) copies the string at 0x5cf54c and shows it (FUN_004724f0).
// Exactly its letters, tabs and new lines, up to the 0 that ends it.
const char kIntengConstructionKeysText[] =
	"\t\t\tMouse Controls\n"
	"\n"
	"Left click\t\tSelect object.\n"
	"Shift+LMB\tSelect multiple objects\n"
	"\n"
	"\t\t\tKeyboard Controls\n"
	"\n"
	"L\t\tDeselect all objects\n"
	"H\t\tIncrease View Distance\n"
	"N\t\tDecrease View Distance\n"
	"G\t\tGoto next object in list\n"
	"Shift G\t\tGoto previous object in list\n"
	"R\t\tRotate object 90 degrees\n"
	"\n";

// Module Linking's Help (command 0xd2, FUN_004c0ed0): copies the 0x6e words at 0x5e37f0 and shows them (FUN_004724f0). Exactly its letters, tabs and new lines (AvP-docs\engine\inteng\raw\inteng-strings.txt:1927).
// Added 2026-11-15 by Melanikus (Drake), by hand from that line.
const char kIntengLinkHelpText[] =
	"Hold down shift for multiple selection\n"
	"Hold down delete to deselect individual modules\n"
	"H\t\tHide object\n"
	"U\t\tUnhide object\n"
	"L\t\tUnhide last\n"
	"S\t\tSet base\n"
	"N\t\tNormal link\n"
	"B\t\tBranch link\n"
	"C\t\tEnd branch link\n"
	"O\t\tOpen branch link\n"
	"D\t\tDelete link\n"
	"7\t\tFilled polygons\n"
	"8\t\tPolyline\n"
	"9\t\tFilled polyline\n"
	"0\t\tLinked - Filled polyline\n"
	"\t\tUnlinked - Polyline\n"
	"J,M\t\tAlter view distance\n"
	"V\t\tOnly draw linked modules\n"
	"Shift+S\t\tContinously set base to module that you are in\n";

// Texture Map's Help > General (command 0x27, FUN_00557320): the three blocks it shows, inteng.exe's .data at 0x5fff54, 0x5ffb4c and 0x5ffa44, one after another (the letters are inteng's own, spelling included)
const char kIntengTextureMapHelpText[] =
	"\t\t\tMouse Controls\n\nLeft click\t\tSelect polygon\nShift+Left click\tDeselect polygon\nHold  \\ or 0 with above to select / deselect all polys in object.\n\n\t\t\tKeyboard Controls\n\nTab\t\tChange to texture mode.\nCtrl+\\ or 0\tSelect all polygons.\nL\t\tDeselect all polygons.\nReturn\t\tSave changes.\nBackspace\tUndo changes.\nT\t\tWrap texture over selected polygons.\nShift+T\t\tTile texture over selected polygons.\nCtrl+T\t\tStop texture wrap/tile mode.\nShift+N\t\tSelect number of repeats of texture to use.\nU\t\tCopy UV Coordinates from one polygon onto selected polygons.\nShift+U\t\tAs above, but copy animations as well.\nH\t\tIncrease View distance.\nN\t\tDecrease View distance.\nQ\t\tSelect as triangles\\quads\nW\t\tChange how non-selected polys are shown\n\t\t(only when non-selected polys are shown textured)\n7\t\tUse flat shaded polygons.\n8\t\tUse polyline.\n9\t\tUse filled polyline.\n\n"
	"X\t\tX-Flip nodes.\nY\t\tY-Flip nodes.\nE\t\tRotate nodes 90 degrees anti-clockwise.\nR\t\tRotate nodes 90 degrees clockwise.\nI\t\tAlign texture on polygon with adjacent polygon.\nM\t\t'Multiple nodes'-In texture mode one node will be shown\n\t\tfor each vertex selected.\nCtrl+R\t\tSelect part of texture to be used.\nShift+R\t\tUse all of texture.\nCtrl+K\t\tApply current colour to selected polys.\nShift+K\t\tSelect colour and use it on selected polys.\nB\t\tToggle show gouraud\nCtrl+B\t\tTurn headlight on and off (requires gouraud polygons)\nShift+B\t\tSelect base polygon.(Used for determining orientation\n\t\twhen using UV Coordinates on more than one polygon)\nP\t\tShow\\Hide text\n<\t\tUndo.\n>\t\tRedo.\nShift+<\t\tUndo Poly Selection.\nShift+>\t\tRedo Poly Selection.\nG\t\tGoto next poly.(used with find textured/purple poly)\nShift+G\t\tGoto previous poly.\nO\t\tCycle nodes in selected polys.\nShift+O\t\tAs above,but alter all frames of animations as well.\nV\t\tReverse nodes in selected polys.\nShift+V\t\tAs above,but alter all frames of animations as well.\nCtrl+X\t\tExit texture mapper.\n\n"
	"1,2\t\tPrevious / next animation frame\nshift+1,2\t\tPrevious / next animation sequence\n-,+\t\tPrevious /next hierarchy frame\n\nF\t\tAdd viewed flag to selected polygons.\nShift+F\t\tRemove viewed flag from selected polygons.\nCtrl + Z\t\tShow texture counts by zone (The Mummy)\n";

// Texture Map's Help > Movement (command 0x28, FUN_005573c0): the camera keys, inteng.exe's .data at 0x600394
const char kIntengTextureMapMovementText[] =
	"2/8\t\tPitch up/down.\n4/6\t\tTurn left/right.\n7/9\t\tRoll left/right.\n/ (Held down)\tAbove controls rotate environment instead\n\t\tof changing your facing.\nA\t\tMove forwards.\nZ\t\tMove backwards.\n1/3\t\tMove left/right.\n-/+\t\tMove up/down (+,- on numeric keypad).\nShift+A\t\tAccelerate.\nShift+Z\t\tDecelerate.\nSpace\t\tStop movement.\n\nF-Keys\t\tRestore camera position\nShift+F-Keys\tSave camera position\n]\t\tIncrease speed.\n[\t\tDecrease speed.\n\n\n\t\t\tNumLock must be on for the keys to work.\n\nInsert\t\tZoom : Left click to choose centre of zoom,then after\n\t\tchoosing area,left click to zoom in or right click to zoom out.\n\t\t(escape cancels process)\nHome\t\tReset to default zoom level.\n";

// Strategy Data's Help (command 0x36, FUN_00525700 0x525c6d -> FUN_00522950): copies the 0xcf words at 0x5ed824 and shows them (FUN_004724f0). Exactly its letters, tabs and new lines (829 letters,
// read from the exe's data: AvP-docs\engine\inteng\exit-and-strategy-leftovers.md). Added 2026-11-15 by Melanikus (Frost).
const char kIntengStrategyHelpText[] =
	"2/8\t\tPitch up/down.\n"
	"4/6\t\tTurn left/right.\n"
	"7/9\t\tRoll left/right.\n"
	"/ (Held down)\tAbove controls rotate environment instead\n"
	"\t\tof changing your facing.\n"
	"A\t\tMove forwards.\n"
	"Z\t\tMove backwards.\n"
	"1/3\t\tMove left/right.\n"
	"-/+\t\tMove up/down (+,- on numeric keypad).\n"
	"Shift+A\t\tAccelerate.\n"
	"Shift+Z\t\tDecelerate.\n"
	"Space\t\tStop movement.\n"
	"\n"
	"F-Keys\t\tRestore camera position\n"
	"Shift+F-Keys\tSave camera position\n"
	"]\t\tIncrease speed.\n"
	"[\t\tDecrease speed.\n"
	"\t\t\tNumLock must be on for the above keys to work.\n"
	"\n"
	"Insert\t\tZoom : Left click to choose centre of zoom,then after\n"
	"\t\tchoosing area,left click to zoom in or right click to zoom out.\n"
	"\t\t(escape cancels process)\n"
	"Home\t\tReset to default zoom level.\n"
	"\n"
	"Left button to select objects\n"
	"Shift + left button to deselect objects\n"
	"L\t\tDeselects all objects\n"
	"S+Left button\tSelect all objects using this texture\n"
	"M\t\tSelect all modules\n";

// Light Sourcing's Help > General (0x64, FUN_004aab60 case 100 -> FUN_004aa420): TWO texts, one after the other (FUN_004724f0 twice: each waits for a key); read from inteng.exe's bytes (drake-alpha-out/cstr.js)
// 622 letters at 0x5e1ea8 of inteng.exe
const char kIntengLightHelpGeneralText[] =
	"Help for Light Sourcing\n"
	"\n"
	"\n"
	"General keys\n"
	"\n"
	"B:\tTurn headlight on and off\n"
	"S:\tSwitch selected light on\n"
	"X:\tSwitch selected light off\n"
	"D:\tDrop light at current position\n"
	"\n"
	"V:\tPaste copied light at current position\n"
	"Use the mouse to select a light, then\n"
	"R:\tRemove light\n"
	"L:\tFly light on / off\n"
	"<,>:\tSelect light attribute to change\n"
	"+,-:\tChange light attribute\n"
	"C:\tCopy Light\n"
	"O:\tCopy clipboard light's data over selected light\n"
	"I:\tToggle Selected lights visibility\n"
	"U:\tUpdate intensities in light - Stops the light rotating, see below\n"
	"\n"
	"T:\tToggle between up/down colour  (Placed object lights)\n"
	"P:\tEdit light properties (Placed object lights).";

// Path mode's Keys (FUN_004fa110 shows the 95 letters at 0x5e9844 of inteng.exe through FUN_004724f0)
const char kIntengPathKeysText[] = "8\t\tPolyline\n9\t\tFilled Polyline\nL\t\tAdd selected module to path\nD\t\tDelete last module from path\n";

// Waypoint mode's three helps, read from inteng.exe's bytes (sha256 c52294569560217e45eb2bca0efbd3327339a1d1be5628c70eaef6a48d3a22cb): 0x602720 (the AI modules, FUN_00584e70), 0x6027f0 (the waypoints, FUN_00584ea0), 0x602978 (the extents, FUN_00585720)
const char kIntengWaypointModulesHelpText[] = "S\t\tEdit selected AI module\nL\t\tAdd module to AI module\nR\t\tRemove module from AI module\nF\t\tToggle fly mode for selected entry point\nDelete\t\tDelete selected entry point\n8\t\tSet Polyline\n9\t\tSet Filled Polyline\n";
const char kIntengWaypointWaypointsHelpText[] = "C\t\tCreate waypoint\nS\t\tSet base waypoint (for adding links)\nD\t\tDelete link\nM\t\tToggle waypoint minimize mode\n8\t\tSet Polyline\n9\t\tSet Filled Polyline\nL\t\tAdd link\nShift+L\t\tAdd alien link\nV\t\tAdd deviation allowed link\nW\t\tToggle can crawl flag\nY\t\tCycle link flag highlight mode (yellow links)\n";
const char kIntengWaypointExtentsHelpText[] = "Left click and drag on side to alter extents\n\nHold down shift while dragging to snap extents to nearest other\nwaypoint or module edge\n\nLeft click in bottom left view to select waypoint\n\nShift+Right click+drag mouse - scroll views around\n\n<\t\tZoom out\n>\t\tZoom in\nC\t\tCreate waypoint\nR\t\tToggle edit waypoint centre\nM\t\tToggle waypoint minimize mode\n8\t\tSet Polyline\n9\t\tSet Filled Polyline\n";

// 573 letters at 0x5e1c68 of inteng.exe
const char kIntengLightHelpStatesText[] =
	"Help for Light Sourcing\n"
	"\n"
	"\n"
	"Light states\n"
	"Flashing:\tThis light is currently selected\n"
	"Rotating:\tThe light effect of this light is calculated per frame (slower).  A light\n"
	"\twill revert to this state when it is changed in any way. Key U should be pressed\n"
	"\tafter changes have been made to a light.\n"
	"\n"
	"The Save option in the menu does not save the light positions, it saves the vertex\n"
	"intensities for each shape.  This will only save the lighting for the lights that\n"
	"have had their intensities updated (press U).  It only needs to be done at the end\n"
	"before the RIF is put in the game.";

// Object Placement's Keys (0x53, FUN_004d9af0: FUN_004724f0 over the string at 0x5e853c, read from inteng.exe's bytes)
const char kIntengPlacementKeysText[] =
	"Backspace\tDeselect object\nT\t\tTurn text on and off\nS\t\tToggle split screen mode\nD\t\tCentre view on object (split screen mode)\nE\t\tEdit selected object\n"
	"P\t\tCreate Duplicate of selected object (not while editing track)\nF\t\tFly selected object\nV\t\tSnap object to vertex\nQ\t\tSet particle generator parent\n\n"
	"Tab\t\tEnter Strategy Mode\n7/8\t\tShow as polygons/wireframe respectively\n\n\t\t\tFollowing work while flying an object\n\n"
	"L\t\tToggle collisions\nR\t\tToggle camera link to object\nShift+R\t\tCamera link to object , with camera at centre of object\nHold C\t\t Move camera relative to object (while camera linked)\n"
	"X\t\tToggle movement relative to axes \nB\t\tToggle bounding box increase\n\n\t\t\tFollowing work while editing track positions\n\n"
	"P\t\tSwitch between rotation about pivot and about centre\nO\t\tToggle display of section start position\n<\t\tDecrese rotation angle\n>\t\tIncrease rotation angle\n";
