// level_view.h: opening a level and drawing it, the game's own way (the phase 2 plan, F4). Written 2026-11-14 by Melanikus (Hicks) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The editor opens a level through the game's own loader and draws it with the game's own renderer, so what the editor shows is what
// the game will show. The game's own start, its level start and its frame are copied here call for call from its winmain.cpp
// (which the editor doesn't compile; docs/avpx-files.md), with three differences:
//   - nothing in the level moves: the game's frame runs UpdateGame (the AI, the physics, the player), and the editor never does;
//   - the player's eyes are the editor's fly camera: each frame the camera's place and turn are written into the player, which is where
//     the game's renderer takes its view from (view.cpp's UpdateCamera);
//   - no sound, no music, no network, no input of the game's own, and nothing written into the player's game settings (the editor
//     keeps the game's settings in memory only: src/host/edge_own.cpp).
//
// The order is: the window and the Direct3D device (F3, src/host), then LevelView_Start once, then LevelView_Open, LevelView_Draw for
// each frame, and LevelView_Close before another level opens.

#pragma once

#include "three_view.h"
#include "track_view.h"

#include <stddef.h>
#include <string>
#include <vector>
#include <stdint.h>

#include <string>
#include <vector>

// Where the editor's camera is and which way it looks, in the game's own units: the place in the game's world units, a turn around
// the up axis and a look up or down, each in 4096ths of a full turn as the game counts angles. Only the low 12 bits of the two angles
// are used, so any whole number is a real angle.
struct FlyCamera
{
	int x;
	int y;
	int z;
	int turn;
	int look;
	int roll;   // 4096ths of a turn about the way the camera faces: inteng's keypad 7 and 9 ("Roll left/right", its Movement help); 0 is level (the last field, so every FlyCamera made with five numbers has 0)
};

// How far the camera may go, and how far it may look up or down. Every frame adds the keys' and the mouse's steps to the camera, so with
// no limit a key held long enough would carry a number past the largest an int holds, which C++ leaves undefined: anything may happen.
// So the place is held to 2^24 of the game's world units (millimetres: about 16.8 km) each way from the level's middle on each axis, far
// past any level's edge. The turn is kept within one whole turn, 0 to 4095. The look is held to a quarter turn either way: straight up
// or straight down, and never past them, onto its back. (Added 2026-11-14 by Melanikus (Hicks), from Frost's and Apone's review.)
int const kFlyCameraMostPlace = 1 << 24;
int const kFlyCameraMostLook = 1024;
int const kFlyCameraWholeTurn = 4096;

// A number held between -most and most
inline int FlyCamera_HeldTo(long long value, int most)
{
	return value > most ? most : value < -most ? -most : (int)value;
}

// The camera held to those limits. Called after every move, and on the place a level starts the camera at.
inline void FlyCamera_Hold(FlyCamera *camera)
{
	camera->x = FlyCamera_HeldTo(camera->x, kFlyCameraMostPlace);
	camera->y = FlyCamera_HeldTo(camera->y, kFlyCameraMostPlace);
	camera->z = FlyCamera_HeldTo(camera->z, kFlyCameraMostPlace);
	camera->turn = ((camera->turn % kFlyCameraWholeTurn) + kFlyCameraWholeTurn) % kFlyCameraWholeTurn;
	camera->roll = ((camera->roll % kFlyCameraWholeTurn) + kFlyCameraWholeTurn) % kFlyCameraWholeTurn;
	camera->look = FlyCamera_HeldTo(camera->look, kFlyCameraMostLook);
}

// The game's own start, after the Direct3D device is made: its settings, its renderer's lists, its console and its words. Once only.
// Gives back false, with why, if it can't.
bool LevelView_Start(char *why, size_t whySize);

// Opens the level with this name (a file in the game's avp_rifs folder, without ".rif"), as the game starts a level, and gives back where
// the level puts the player, as a camera. The name is checked by the game's own rule for level file names first.
// Gives back false, with why, if the name is refused.
bool LevelView_Open(const char *levelName, FlyCamera *startHere, char *why, size_t whySize);

// Where the last level opened put things, in plain words for a run's report: the level's own start (its AVPSTART record), the player's
// body, the player's display block, and the room the camera starts in. Empty until a level is open. (Added 2026-11-14 by Melanikus
// (Dietrich), F3.)
const char *LevelView_StartWords();

// The path the game read the open level's file from, as the game builds it to load a level (system.cpp, LoadTheLevel): its folder's
// name, the level's name and its extension, with the game's own slash, from the folder the editor runs in. The editor opens the level to
// look things up in and to edit by this same path (level_objects.h), so that the picture and the objects are of one file. Gives back
// false, with path empty, when no level is open or the path doesn't fit in size. (Added 2026-11-15 by Melanikus (Dietrich).)
bool LevelView_FilePath(char *path, size_t size);

// Draws one frame of the open level, as seen from camera. With savePictureTo, the finished frame is also saved there as a PNG picture
// before it's shown: that's how a run proves what it drew, with nobody watching. Gives back false only if a picture was asked for and
// couldn't be saved. With addToFrame, the editor's panels are added to the frame after the level and before it is finished, as the game adds
// its HUD: they are over the level, in the picture, and in the frame shown, and their places are in pixels. (Added 2026-11-15 by Melanikus
// (Dietrich), L-1.)
bool LevelView_Draw(const FlyCamera &camera, const char *savePictureTo, void (*addToFrame)(void *context) = nullptr, void *context = nullptr);

// Which way the camera faced in the frame last drawn, exactly as the game's renderer used it (view.cpp's UpdateCamera, the look up or
// down included): its forward and its right, each a direction of length 65536 (the game's ONE_FIXED), in the game's world axes (x to the
// right, y down, z forward). The game keeps an object's forward in its matrix's third row and its right in the first (dynamics.cpp:1169);
// the frame transposes the camera's into the view's (view.cpp:806), so they're read from its columns (level_view.cpp says how). Gives
// back false if no frame has been drawn.
// One frame with no level open (inteng's start with no file: its message in its own window): the game's frame begun, addToFrame added to it (as LevelView_Draw), the frame finished, the picture saved to
// savePictureTo when it's given, and shown. (Added 2026-11-16 by Melanikus (Dietrich).)
bool LevelView_DrawNoLevel(const char *savePictureTo, void (*addToFrame)(void *context) = nullptr, void *context = nullptr);

bool LevelView_CameraAxes(int forward[3], int right[3]);

// Where the game put its eyes in the last frame (VDB_World): for a proof that the camera the editor sets is the one drawn from
bool LevelView_EyePlace(int32_t place[3]);

// The camera's own turn as nine whole numbers (16.16), row by row: its right, its down and its forward (the game's order for an object's matrix).
// Gives back false if no frame has been drawn. (Added 2026-11-15 by Melanikus (Frost), P3.)
bool LevelView_CameraMatrix(int32_t matrix[9]);

// The camera's heading as the game keeps a generator's: its Euler Y (0 to 4095) from MatrixToEuler on the camera's own turn (object_setup.cpp:2534 reads an AVPGENER's orientation as EulerY).
// Gives back false if no frame has been drawn. (Added 2026-11-15 by Melanikus (Vasquez), Object Placement.)
bool LevelView_CameraHeading(int32_t *heading);

// SPLIT SCREEN (Object Placement, Toggle Split Screen 0x52; inteng FUN_004d5980 / FUN_004d58e0): extra cameras whose worlds are drawn, each into its box of the window, before the main view in the same frame (the
// main view then has its own box, LevelView_MainBox). At most kLevelViewMostExtraViews; count 0 (or no main box) draws none. Each is a full perspective render through the game's own renderer, as the main view is.
// (Added 2026-11-15 by Melanikus (Vasquez); the flush is avpx's R_EditorFlushWorld.)
struct LevelViewExtra
{
	FlyCamera camera;
	int left, top, width, height;
};

size_t const kLevelViewMostExtraViews = 3;

void LevelView_SetExtraViews(const LevelViewExtra *views, size_t count);

// The camera's turn as a particle generator's PARGENDA holds it: a quaternion (x, y, z, w) that the game's loader negates (x, y, z) and turns back into this matrix with QuatToMat (object_setup.cpp:1232-1242).
// Found by trying the matrix's quaternion and its conjugate through the game's own QuatToMat until one gives the camera's turn back; false when no frame has been drawn or neither does.
bool LevelView_CameraQuaternion(float q[4]);

// The object with this 8-byte ID (an object's ID, selection.h) is drawn in green over the level, in every draw mode, until another ID or null
// is given (null: none). An ID no object in the level has draws nothing. (Added 2026-11-15 by Melanikus (Dietrich).)
void LevelView_SelectObject(const uint8_t *id);

// The three orthographic views of inteng's track and point screens (three_view.h): drawn this frame when on, laid over the window's box (its pixels)
// from boxLeft, boxTop, boxWidth by boxHeight as inteng's 640 by 480 screen, each with its crosshair at cursor. Call before LevelView_Draw each frame.
void LevelView_ThreeViews(bool on, const host::ThreeViews &views, const int32_t cursor[3], int boxLeft, int boxTop, int boxWidth, int boxHeight);

// The track's lines drawn in the three views (track_view.h); count 0 none
void LevelView_ThreeViewLines(const host::TrackLine *lines, size_t count);
// The same lines drawn in the 3D view in the level's world places (R_SetEditorWorldLines: depth-tested, about three pixels wide); count 0 takes them away
void LevelView_WorldLines(const host::TrackLine *lines, size_t count);

// The same lines drawn in the 3D view in the level's world places (R_SetEditorWorldLines: depth-tested, about three pixels wide); count 0 takes them away
void LevelView_WorldLines(const host::TrackLine *lines, size_t count);

// Flat FILLED triangles in the 3D view, in the game's world units (millimetres), each in its own ARGB colour (opaque): the avpx hook R_SetEditorWorldTriangles (the light markers' pyramids, Stability flags' faces). count 0 takes them away.
struct LevelViewTriangle
{
	int32_t a[3];
	int32_t b[3];
	int32_t c[3];
	uint32_t colour;
	int32_t group = -1;   // triangles that follow one another with the same group (0 or more) are drawn in an occlusion query of their own: LevelView_TriangleGroupPixels says whether they were seen
};
void LevelView_WorldTriangles(const LevelViewTriangle *triangles, size_t count);

// How many pixels of the group were seen in the last frame whose answer came back (one frame late): 0 hidden (by the level or off the screen), -1 not known yet
int LevelView_TriangleGroupPixels(int32_t group);

// The view's projection scale against the game's own, as inteng's Hierarchy Marine / Alien items set it (the view block's +0x34 / +0x38 = 0x140 or 0xa0 against 320): 1.0 is the marine's (the game's), 0.5 the alien's
// (the picture twice as wide). Done through the game's CameraZoomScale, which its projection matrix is built with every frame, so no hook is needed. (Added 2026-11-15 by Melanikus (Vasquez).)
void LevelView_SetProjectionScale(float scale);

// The view's focal length in pixels for a view viewWidth pixels wide, at the projection scale now set: (width / 2) x scale / tan(half the field of view). inteng's zoom turns the camera toward the place clicked by the angle whose tangent is
// the distance from the middle over this (FUN_00471cb0).
double LevelView_FocalLength(int viewWidth);

// Hierarchy mode's Adaptive Degradation (0x45): on, a model shows its lower-detail shapes by the camera's distance (the game's Get_Degraded_Shape, with the hierarchy's HIDEGDIS distances); off, always the
// full-detail shape. Done through the game's GlobalLevelOfDetail_Hierarchical (ONE_FIXED, or 0 for the nearest band always). (Added 2026-11-15 by Melanikus (Vasquez).)
void LevelView_SetAdaptiveDegradation(bool on);

// The current module of the module screens, drawn purple (inteng: the picked module purple, 0x7F007F, menu-reads-1-full.txt:5648, :7174, :7184, :8168, :8179), or -1
void LevelView_SetCurrentModule(int32_t moduleIndex);

// Modules drawn in a colour of their own (Module Linking: the current module, its linked modules): the game's module numbers (MODULE.m_index) and each one's
// colour as ARGB, at most kLevelViewMostSelected. They are drawn over what the level looks like, the selected objects' green with them. count 0 clears them.
void LevelView_SetModuleTints(const int32_t *moduleIndex, const uint32_t *argb, size_t count);

// Placed objects (not modules) drawn in a colour of their own: each ID is 8 bytes, in ids, and each one's colour as ARGB (Strategy Data: the objects that hold a strategy, green). Drawn like the module tints, after
// the selected ones. count 0 clears them. (inteng: FUN_00522680 sets 0x2000000 in the block's flags, FUN_004a7eca turns it into item flag 0x100, FUN_004980f0 into the colour 0x00ff00.)
void LevelView_SetObjectTints(const uint8_t *ids, const uint32_t *argb, size_t count);

// The colour the SELECTED objects are drawn in (green unless this says otherwise). inteng marks the selected block 0x40000000 (+0x28 of the placement object, FUN_00529470 of Strategy Data), which FUN_004a7eca makes item
// flag 0x80 and FUN_004980f0 makes 0xff0000: RED, in Object Placement and Strategy Data.
void LevelView_SetSelectedColour(uint32_t argb);

// The 3D view in a box of the window (the bottom right quarter of the three views), or the whole window for on = false. Call before LevelView_Draw each frame.
void LevelView_MainBox(bool on, int boxLeft, int boxTop, int boxWidth, int boxHeight);

// The extent in the world of what the three views draw (from the last frame that drew them); false when there is none yet
bool LevelView_ThreeViewBounds(int32_t mins[3], int32_t maxs[3]);

// WHICH SHAPE a polygon is in, for the two calls below: a module of the level (the game's module number, MODULE.m_index) or an object (its 8-byte ID)
struct LevelViewShapeOf
{
	bool isModule;
	int32_t moduleIndex;   // for a module
	uint8_t objectId[8];   // for an object
};

// One polygon of the game's own loaded shape, by its ITEM number (polygon_items.h maps the level file's polygon numbers to these)
struct LevelViewHighlight
{
	LevelViewShapeOf shape;
	int32_t item;
};

// These polygons drawn again over the level in yellow (the Texture Map mode's selection): up to 20000, none for count 0. A shape the game does not have in play, or an item number past
// its shape's, is left out. Call before LevelView_Draw each frame.
size_t const kLevelViewMostHighlighted = 20000;
void LevelView_SetHighlightedItems(const LevelViewHighlight *items, size_t count);

// How many corners each item of the shape the game has loaded for this module or object has (3 to 5), in the game's item order. False when the shape is not in play.
bool LevelView_ItemCorners(const LevelViewShapeOf &shape, std::vector<uint8_t> *corners, std::vector<int32_t> *points = nullptr);   // points (when given): 5 point numbers per item, -1 where it has none

// The game's modules: how many module numbers there are, the NAME of module m in the level file (the o_name of its object, which the game keeps in MODULE.name: MODULE.m_name is only a
// number; false when the module has none in play), and the module the world place holds (-1 when none)
// The size in texels of the LOWEST-NUMBERED picture any shape of the level is textured with (the game's own texture table; an item's colour word holds its texture number). inteng's Texture Map
// starts with its current texture set to the smallest texture number the level loaded (FUN_00555cd0 then FUN_00555dd0), and its window is the whole of that picture. False when no shape is in play.
bool LevelView_LowestPictureSize(int32_t *width, int32_t *height);

// THE TEXTURE CHOOSER's data: every picture a textured polygon of the level's shapes (modules and objects in play) uses, by the game's texture number, lowest first, each with its size in texels and the name
// the game's texture table gives it (inteng's chooser, FUN_005575a0, shows a grid of every texture with its size and name). At most `most` are given.
struct LevelViewPicture
{
	int32_t texture;
	int32_t width;
	int32_t height;
	std::string name;
};

bool LevelView_Pictures(std::vector<LevelViewPicture> *pictures, size_t most);

// The game's texture number of the picture polygon item `item` of the shape is textured with; false for a shape not in play or an item that is not textured
bool LevelView_ItemTexture(const LevelViewShapeOf &shape, int32_t item, int32_t *texture);

// The live preview of Set Texture: the game's item takes that texture number (its UV list number, the high half of the same word, is kept), so the view shows the new picture at once; false for a shape not in play or an item that is not textured
bool LevelView_SetItemTexture(const LevelViewShapeOf &shape, int32_t item, int32_t texture);

// The live preview of the texture coordinate edits: the game's item takes `count` (u, v) pairs (whole numbers) as the corners of its list; false when the item has no list or a different number of corners
bool LevelView_SetItemUv(const LevelViewShapeOf &shape, int32_t item, const int32_t *uv, int count);

// The game's item takes (on) or loses (not on) these bits of its flags (iflag_notvis 1, iflag_nolight 2, iflag_transparent 0x10, iflag_no_bfc 0x20, iflag_mirror 0x1000, ...) at once, so the view shows the flag now; the file's polygon is not touched here. (Added 2026-11-15 by Melanikus (Dietrich), Texture Map flags.)
bool LevelView_SetItemFlagBits(const LevelViewShapeOf &shape, int32_t item, int32_t mask, bool on);

// Objects that no module holds are drawn after the modules (the avpx hook R_SetEditorDrawObjectsOutsideModules): on for a file with no modules, whose objects the editor made display blocks of (hierarchy_view.h, ModelObjects_Build)
void LevelView_DrawObjectsOutsideModules(bool on);

// FLY THE REAL OBJECT (inteng FUN_004dd280 / the frame code 0x4dc9a6: the object itself takes the flying place and matrix every frame and is drawn there): the shared mechanism of Object Placement's F and
// Track Editing's end marker. Begin takes the display block of the object whose 8-byte ID this is out of the picture and puts a copy of it, drawn live, in its place (a copy no module holds, drawn through
// R_SetEditorDrawObjectsOutsideModules, so it shows wherever it is); Put moves the copy (the place in the game's units, the turn as the file has it, x y z w); End takes the copy away and shows the real one
// again, which the level's own bytes decide (the caller writes the place and turn into the level, then End). False from Begin when the object is not in play or is a hierarchy.
bool LevelView_FlyBegin(const uint8_t *id);
void LevelView_FlyPut(const int32_t world[3], const float quat[4]);
void LevelView_FlyEnd();
bool LevelView_FlyActive();

// Where the live copy is, as the game holds it (its ObWorld), its radius and whether the game has it among the blocks it is drawing this frame (for the scenario's state line); false when none flies
bool LevelView_FlyCopyState(int32_t world[3], int32_t *radius, bool *active);

int32_t LevelView_ModuleCount();
bool LevelView_ModuleName(int32_t moduleIndex, std::string *name);
int32_t LevelView_ModuleAt(const int32_t place[3]);
bool LevelView_ModuleRadius(int32_t moduleIndex, int32_t *radius);   // the module's shape radius (shaperadius), in the game's units (mm)
bool LevelView_ModuleWorld(int32_t moduleIndex, int32_t world[3]);   // where the game puts module m, in millimetres (MODULE.m_world)

// The 8-byte IDs of the objects the game has in play that have a strategy block (what the file's objects are found by), for a proof run
void LevelView_ActiveObjectIds(std::vector<std::vector<uint8_t>> *ids);

// HIDDEN OBJECTS: the objects with these 8-byte IDs are not drawn at all (up to kLevelViewMostHidden), and with everyObject true neither is any object the level file places (the modules still are).
// Every call REPLACES what was hidden before; count 0 and everyObject false shows everything again. An ID of no object in play is left out. Editor-only: avpx R_SetEditorHiddenBlocks.
size_t const kLevelViewMostHidden = 20000;
void LevelView_HideObjects(const uint8_t (*ids)[8], size_t count, bool everyObject);

// The same for MODULES of the level (the game's module numbers, MODULE.m_index): these are not drawn, and with everyModule none is. Replaces what modules were hidden before.
void LevelView_HideModules(const int32_t *moduleIndex, size_t count, bool everyModule);

// Everything (module or object) whose distance from the camera, less its radius, is at least `distance` millimetres is not drawn, as inteng's view-distance rule hides it
// (FUN_004c36f0: distance(camera place, record place) - radius < view distance is drawn). on false: off.
void LevelView_HideBeyond(bool on, int32_t distance);
// The object whose block the distance rule never hides (Object Placement's current object, FUN_004dd2f0); null: none
void LevelView_HideBeyondKeep(const uint8_t *objectId);

// THE VIEW RULE's own hiding, kept apart from the explicit Hide calls above so the two never undo each other (the hidden set drawn is the union): these modules (game numbers) and, with everyObject,
// every placed object. Replaces the rule's earlier set; (nullptr, 0, false) lets go of it.
void LevelView_HideByRule(const int32_t *moduleIndex, size_t count, bool everyObject);

// Every selected object in green: the 8-byte IDs of up to kLevelViewMostSelected of them (more are not drawn green), none for count 0
size_t const kLevelViewMostSelected = 1024;   // was 256: the stock levels have up to 608 modules, and Show All colours every one
void LevelView_SelectObjects(const uint8_t (*ids)[8], size_t count);

// For a run's report: whether the last frame found the selected object among the game's active display blocks, and how many list items the
// green pass drew for it.
bool LevelView_SelectedIsInPlay();
int LevelView_SelectedItemsDrawn();

// Closes the open level, as the game does at a level's end. Nothing happens if no level is open.
void LevelView_Close();

// One module's baked colours for the view (Light Sourcing: Save Intensities re-lights what the view draws): the module as the level file names it (its name and its place in the file's numbers) and a colour for
// each point, red << 16 | green << 8 | blue, as a SHPVTINT holds them. (Added 2026-11-15 by Melanikus (Drake).)
struct LevelViewBaked
{
	std::string name;
	int32_t location[3];
	std::vector<uint32_t> colours;
};

// Gives each game module whose name and place (the file's number x scale) are one of these the colours, as the game's loader makes them from a SHPVTINT (projload.cpp set_up_baked_colours), and builds the static
// level's list again so the next frame draws them. A module the game gave no baked colours to is left alone. Gives back how many modules were given colours.
size_t LevelView_SetBakedColours(const std::vector<LevelViewBaked> &modules, double scale);

// The name and the world place of a module of the game (its m_world, in the game's units), which tell the object of the level file it is (module_site.h). False for a module the game does not have or has no name for.
// (Added 2026-11-15 by Melanikus (Drake).)
bool LevelView_ModuleSite(int32_t moduleIndex, std::string *name, int32_t world[3]);

// The box round a module's shape in the game's world units (its place plus the shape's extremes: SHAPEHEADER shapeminx .. shapemaxz), for Stability flags' box (FUN_00447f10 makes it from those six numbers). False for a module with no shape.
bool LevelView_ModuleBox(int32_t moduleIndex, int32_t mins[3], int32_t maxs[3]);

// Doors > Close / Open (FUN_004a9f40 / FUN_004aa1b0): every module whose shape has morph frames has the points and vertex normals of its main shape exchanged with those of the other shape of its first frame (the game keeps both as
// shapes; closed is the exchanged state). Does it only when the state changes. The static level is built again so the next frame draws the new points. Gives back how many module shapes were exchanged. (Added 2026-11-15 by
// Melanikus (Drake).)
size_t LevelView_SetDoorsClosed(bool closed);
bool LevelView_DoorsAreClosed();
