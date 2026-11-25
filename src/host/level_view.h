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
// THE AXES, MEASURED (11-18: Bishop's two camera trials against the editor's matrix, a701208a; Drake's orbit camera on Genshd1's lift shaft): x across, y points DOWN, z forward; turn 0
// looks along +z and turn 1024 along -x; a POSITIVE look looks down (held to a quarter turn, 1024, either way). Every other note about the camera (the harness's camera verbs,
// harness-verbs.md, camera_orbit.h) points here.
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

// The last level opened's MISSING PICTURES (the owner, 2026-11-17: users are told everything): how many of its pictures could not be loaded, or that it has no list
// of pictures at all, and that their polygons are drawn as white lines, as inteng draws them, said in the log and given back for the status line. Gives back otherwise
// when none is missing.
const char *LevelView_SayMissingPictures(const char *otherwise);

// Where the last level opened put things, in plain words for a run's report: the level's own start (its AVPSTART record), the player's
// body, the player's display block, and the room the camera starts in. Empty until a level is open. (Added 2026-11-14 by Melanikus
// (Dietrich), F3.)
const char *LevelView_StartWords();

// The path the game read the open level's file from, as the game builds it to load a level (system.cpp, LoadTheLevel): its folder's
// name, the level's name and its extension, with the game's own slash, from the folder the editor runs in. The editor opens the level to
// look things up in and to edit by this same path (level_objects.h), so that the picture and the objects are of one file. Gives back
// false, with path empty, when no level is open or the path doesn't fit in size. (Added 2026-11-15 by Melanikus (Dietrich).)
// It is always the file the level was OPENED from (LevelView_Open), also after LevelView_ShowFile has loaded the editor's working bytes from another.
bool LevelView_FilePath(char *path, size_t size);

// THE VIEW GAP (Bishop 2026-11-16: "on every screen entry the game view must load OUR working bytes, never the opened file"): loads the level with this name (a file in the game's
// avp_rifs folder, without ".rif": the editor's own copy of its working bytes) in place of the one open, the camera left to the caller. LevelView_FilePath goes on naming the file the level
// was opened from. When the game's loader refuses it, the opened file is loaded again (so a level is always open) and false is given back with why.
bool LevelView_ShowFile(const char *levelName, char *why, size_t whySize);

// Draws one frame of the open level, as seen from camera. With savePictureTo, the finished frame is also saved there as a PNG picture
// before it's shown: that's how a run proves what it drew, with nobody watching. Gives back false only if a picture was asked for and
// couldn't be saved. With addToFrame, the editor's panels are added to the frame after the level and before it is finished, as the game adds
// its HUD: they are over the level, in the picture, and in the frame shown, and their places are in pixels. (Added 2026-11-15 by Melanikus
// (Dietrich), L-1.)
bool LevelView_Draw(const FlyCamera &camera, const char *savePictureTo, void (*addToFrame)(void *context) = nullptr, void *context = nullptr);

// LIVE PANES IN THE FRAME (Vasquez 2026-11-23, the placement picker's turning shapes: shape_pane_gpu.h, the interface agreed with Frost): draw, when given, is
// called inside LevelView_Draw AFTER the level and its inset are drawn and BEFORE addToFrame's panels, so what it draws is over the level and under the
// panels (the names, the selection's frame, the scroll bar), in the picture and the frame shown. nullptr takes it away. One at a time: a second replaces the first.
void LevelView_SetPanes(void (*draw)(void *context), void *context);

// AFTER THE EYES (Drake 2026-11-24, the owner: the gizmo "flickers alot/doesn't follow rest of map" while the camera moves): eyes, when given, is called inside LevelView_Draw
// right after the main view's camera is this frame's (AvpShowViews: Global_VDB_Ptr) and BEFORE the frame's world lines are drawn, so lines made from the camera (the
// gizmo's, sized to the screen) are made from the camera they are drawn with, not the last frame's. nullptr takes it away. One at a time.
void LevelView_SetAfterEyes(void (*eyes)(void *context), void *context);

// Which way the camera faced in the frame last drawn, exactly as the game's renderer used it (view.cpp's UpdateCamera, the look up or
// down included): its forward and its right, each a direction of length 65536 (the game's ONE_FIXED), in the game's world axes (x to the
// right, y down, z forward). The game keeps an object's forward in its matrix's third row and its right in the first (dynamics.cpp:1169);
// the frame transposes the camera's into the view's (view.cpp:806), so they're read from its columns (level_view.cpp says how). Gives
// back false if no frame has been drawn.
// One frame with no level open (inteng's start with no file: its message in its own window): the game's frame begun, addToFrame added to it (as LevelView_Draw), the frame finished, the picture saved to
// savePictureTo when it's given, and shown. (Added 2026-11-16 by Melanikus (Dietrich).)
bool LevelView_DrawNoLevel(const char *savePictureTo, void (*addToFrame)(void *context) = nullptr, void *context = nullptr);

bool LevelView_CameraAxes(int forward[3], int right[3], int *down = nullptr);   // down (when given, 3 numbers): the camera's own down axis, the view's (0, 1, 0)

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

// The most world lines LevelView_WorldLines hands the engine (Vasquez's waypoint display 2026-11-19: every AI module's boxes and links at once, ~235k lines on v2d;
// it was 4096). Frost lifts avpx's R_SetEditorWorldLines to the same number (renderer.h); until that lands the engine itself still keeps only its first 4096.
size_t const kLevelViewWorldLinesMax = 524288;

// The engine's lines from the editor's (what LevelView_WorldLines gives R_SetEditorWorldLines): each line's from, to and colour as they are, at most `most` of them,
// none for null or 0, into `out` (kept from call to call, so a big list is not allocated again each time). EngineLine has the fields x0 y0 z0 x1 y1 z1 colour
// (avpx renderer.h EditorWorldLine). Gives how many.
template <class EngineLine>
size_t LevelView_ConvertWorldLines(const host::TrackLine *lines, size_t count, size_t most, std::vector<EngineLine> *out)
{
	size_t const used = lines ? (count < most ? count : most) : 0;

	out->resize(used);

	for (size_t i = 0; i < used; i++)
	{
		EngineLine &line = (*out)[i];

		line.x0 = lines[i].from[0]; line.y0 = lines[i].from[1]; line.z0 = lines[i].from[2];
		line.x1 = lines[i].to[0];   line.y1 = lines[i].to[1];   line.z1 = lines[i].to[2];
		line.colour = lines[i].colour;
	}

	return used;
}

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
// The current module's colour (0xAARRGGBB): purple 0x7f007f unless a screen gives another (the Construction hub: its selection ORANGE 0xff7f00, view-state-table.md H-2a)
void LevelView_SetCurrentModuleColour(uint32_t argb);

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

// PICTURES IMPORTED INTO THE OPEN LEVEL (the RIM import page, texture_import_screen.h; Dietrich 2026-11-23): a picture no polygon uses yet is in no shape's items,
// so LevelView_Pictures, which reads them, wouldn't offer it. Each one the page imports is given here, the game's texture number (Tex_CreateFromRIM's) with its
// index in the level's list, and LevelView_Pictures offers it too. Forgotten when a level is opened (LevelView_Open); kept across an edit that rebuilds the
// level's bytes (the view's textures are the same). False for a number given already, or past kLevelViewMostImported
size_t const kLevelViewMostImported = 256;
bool LevelView_AddImportedPicture(int32_t texture, int32_t listIndex);
std::vector<std::pair<int32_t, int32_t> > const &LevelView_ImportedPictures();   // (texture, list index), in the order imported

// The game's texture number of the picture polygon item `item` of the shape is textured with; false for a shape not in play or an item that is not textured
bool LevelView_ItemTexture(const LevelViewShapeOf &shape, int32_t item, int32_t *texture);

// The live preview of Set Texture: the game's item takes that texture number (its UV list number, the high half of the same word, is kept), so the view shows the new picture at once; false for a shape not in play or an item that is not textured
bool LevelView_SetItemTexture(const LevelViewShapeOf &shape, int32_t item, int32_t texture);

// The live preview of the texture coordinate edits: the game's item takes `count` (u, v) pairs (whole numbers) as the corners of its list; false when the item has no list or a different number of corners
bool LevelView_SetItemUv(const LevelViewShapeOf &shape, int32_t item, const int32_t *uv, int count);

// The game's item takes (on) or loses (not on) these bits of its flags (iflag_notvis 1, iflag_nolight 2, iflag_transparent 0x10, iflag_no_bfc 0x20, iflag_mirror 0x1000, ...) at once, so the view shows the flag now; the file's polygon is not touched here. (Added 2026-11-15 by Melanikus (Dietrich), Texture Map flags.)
bool LevelView_SetItemFlagBits(const LevelViewShapeOf &shape, int32_t item, int32_t mask, bool on);

// OBJECT PLACEMENT'S LIGHTING (OP-3, OP-44; added 2026-11-17 by Drake). on: the mode's look. Every item of every shape a module draws gets iflag_nolight, so the modules are drawn unlit (inteng's
// entry FUN_004d68e0 0x4d84e6-0x4d8526), and the level's other shapes that hold baked colours (ShapeFlag_PreLit; FUN_004d67f0) are lit (objectsLit: the bit cleared) or unlit (set). The
// other shapes WITHOUT baked colours are unlit too while objectsLit is false, and have their own bit back (lit by the level's lights, as inteng draws them) while it is true: a NAMED
// DIFFERENCE from inteng, which leaves them alone, by the owner's ruling of 2026-11-23 ("unlit like the level"; Dietrich). Not on: every
// item the look changed has its own bit back (inteng loads the level again when the mode ends). Each call does only what changed since the last, so it can be called every frame; a level opened in
// between comes in with its own bits and is taken as new. inteng also swaps those items' types (5 <-> 6, 7 <-> 0x18); the game's loader has made every textured item of ours
// I_ZB_Gouraud3dTexturedPolygon (post_process_shape, projload.cpp:3027-3080), so none holds those types and the bit is the whole of it for our renderer. NAMED: inteng's objects are its shapes from
// DAT_008f588c on; ours are the level's shapes no module draws. Gives back how many items' bits it set or put back.
int LevelView_PlacementLighting(bool on, bool objectsLit);

// The look for a scenario's state line: "look <0|1> lit <0|1> modules <items> objects <items> unbaked <items>" (objects: the baked-colour objects' items Lighting sets;
// unbaked: the other objects' items held plain, 0 while Lighting is on), then " at x y z|..." for up to four blocks in play whose shape is an object's with baked
// colours (the world's places: what a picture of the look can be aimed at). Empty when no level is open.
std::string LevelView_PlacementLightingWords();

// What the open level took of its 40 blocks of memory once it had opened (avpx mempool.h, LevelMemoryPool_Use; Frost 2026-11-21, the owner's track import), for the
// state line: "taken N of 40 left X handed Y outside Z", in MB (left: in the block in use; outside: given outside the 40 blocks, what the 1999 game writes past them).
// Empty when no level is open.
std::string LevelView_MemoryWords();

// The same for the user, on the status line when a level opens (Bishop 2026-11-21, the owner's rule of 11-18: tell him on the status line, never by changing the look): one
// short line, "Level memory: N of 40 blocks taken, F MB free", F being the whole free blocks and what is left in the last (what track-import's --pool-free-mib takes), after
// otherwise when the status line already says something, with "; " between. A level given memory outside the 40 blocks (past the 40th, or one request over a block) is
// told so instead, as Rebellion's game would write past its memory there. Gives back otherwise when no level is open.
const char *LevelView_SayMemory(const char *otherwise);

// THE LIVE SCALE of Object Placement's '=' and '-' keys (OP-14; inteng's FUN_004e1340 scales the game's own shape of the block each frame the key is held): every point of the
// shape the game has loaded for this object, its radius and its six extents are multiplied by factor, each cut toward zero as inteng's __ftol; the block takes its extents
// from the shape again (as map.cpp makes a block). Nothing is changed when the shape is not in play, a number would leave the 32-bit range, or factor is not above 0 and at
// most 1,000,000. The file's bytes are the caller's (shape_rescale_edit.h); this is only what the view draws.
bool LevelView_ScaleObjectShape(const LevelViewShapeOf &shape, double factor);

// THE LIVE QUARTER TURN of Construction's R (TB-C5; inteng FUN_004452b0 0x447474-0x447505 turns the game's own shape of the selected object as the file's): every point of the shape the
// game has loaded for this module or object, every polygon normal and point normal, (x, y, z) -> (-z, y, x); the extents turned whole (max x = -min z, min x = -max z, max z = max x,
// min z = min x: inteng's own turn of the loaded extents is wrong, the file's bug fixed the same way) and the block takes them. Nothing is changed when the shape is not in play or a
// number has no negative (INT_MIN). The file's bytes are the caller's (ShapeEdit_TurnQuarter); this is only what the view draws.
bool LevelView_TurnObjectShape(const LevelViewShapeOf &shape);

// Objects that no module holds are drawn after the modules (the avpx hook R_SetEditorDrawObjectsOutsideModules): on for a file with no modules, whose objects the editor made display blocks of (hierarchy_view.h, ModelObjects_Build)
void LevelView_DrawObjectsOutsideModules(bool on);

// FLY THE REAL OBJECT (inteng FUN_004dd280 / the frame code 0x4dc9a6: the object itself takes the flying place and matrix every frame and is drawn there): the shared mechanism of Object Placement's F and
// Track Editing's end marker. Begin takes the display block of the object whose 8-byte ID this is out of the picture and puts a copy of it, drawn live, in its place (a copy no module holds, drawn through
// R_SetEditorDrawObjectsOutsideModules, so it shows wherever it is); Put moves the copy (the place in the game's units, the turn as the file has it, x y z w); End takes the copy away and shows the real one
// again, which the level's own bytes decide (the caller writes the place and turn into the level, then End). False from Begin when the object is not in play or is a hierarchy.
bool LevelView_FlyBegin(const uint8_t *id);
char const *LevelView_FlyRefused();   // why the last LevelView_FlyBegin gave false, in words ("" after one that gave true): the state line says it while the object can't be drawn moving
void LevelView_FlyPut(const int32_t world[3], const float quat[4]);
void LevelView_FlyEnd();
// THE REAL OBJECT AT A NEW POSE: its display block's place (the game's units) and turn (the file's, x y z w) set as the copy's are (PutCopyAt), for after the level was written
// with that pose: the level's bytes alone never move a block already made (Vasquez 11-18: FlyEnd shows the real one where the level was loaded). inteng moves the object itself
// (FUN_004dd280 / the frame code 0x4dc9a6). False when it has no plain block (a hierarchy, or none in play). Drake 2026-11-18 (the mouse gizmo's release).
bool LevelView_PutObjectAt(const uint8_t *id, const int32_t world[3], const float quat[4]);
// Where the object's real display block is now (the game's units); false when it has none (for the harness's state line)
bool LevelView_ObjectBlockAt(const uint8_t *id, int32_t world[3]);
// The flying copy's display block, for a pick that leaves it out (PickGame_AtClickLeavingOut); null when nothing flies
void const *LevelView_FlyCopyBlock();
// The REAL display block of the object whose copy flies (not drawn while it flies, but still on the frame's on-screen list), for a pick that leaves both out; null when nothing
// flies. (Added 2026-11-18 by Vasquez, P3 placement mode.)
void const *LevelView_FlyOriginalBlock();
bool LevelView_FlyActive();

// A SECOND LIVE COPY of an object, drawn where it is put while the object itself stays drawn (or flies as the copy above): Track Editing's start marker, inteng's copy of the object at the
// current section's start (FUN_004b2590 at the track's load, placed by FUN_004f0da0; T-3c). Begin false when the object is not in play, is a hierarchy, or a second copy is already shown.
bool LevelView_GhostBegin(const uint8_t *id);
void LevelView_GhostPut(const int32_t world[3], const float quat[4]);
void LevelView_GhostEnd();
bool LevelView_GhostActive();
// Where the second copy is, as the game holds it (its ObWorld), for the scenario's state line; false when none is shown
bool LevelView_GhostWorld(int32_t world[3]);

// Where the live copy is, as the game holds it (its ObWorld), its radius and whether the game has it among the blocks it is drawing this frame (for the scenario's state line); false when none flies
bool LevelView_FlyCopyState(int32_t world[3], int32_t *radius, bool *active);

// The live copy's matrix as the game holds it (its ObMat: mat11 .. mat33, 16.16), false when none flies
bool LevelView_FlyMatrix(int32_t matrix[9]);

// THE ORBIT (V-17): inteng's camera object of kind 1 (FUN_00580a40, the kind R's link gives it), from the block it follows: C = the block's matrix turned by the orbit's yaw and then its pitch
// (CreateEulerMatrix of (0, yaw, 0) and (pitch, 0, 0), MatrixMultiply: the game's own, as inteng's FUN_004b3370 and FUN_004b34b0 are), and the camera at the block's place plus C applied to
// (0, 0, -distance) (RotateVector, AddVector: FUN_004b3860, FUN_004b3800), so it is that far back along C's forward and faces the way C does. The angles in 4096ths, the places in the world's units.
void LevelView_OrbitOf(const int32_t blockPlace[3], const int32_t blockMatrix[9], int pitch, int yaw, int32_t distance, int32_t place[3], int32_t matrix[9]);

// THE ENGINE'S OWN DYNAMICS ON CHOSEN BLOCKS (Object Placement's Gravity OP-47 and a flight's collisions OP-117 / OP-118; Bishop's ruling 11-17: the game's code, no physics of ours). inteng ran
// its engine on its blocks; here avpx's ObjectDynamics (dynamics.cpp) runs once a frame with every block NOT chosen held static for that call (its IsStatic bit set and put back), so only the
// chosen ones move, by the game's own gravity, collisions and relocation.
// Gravity on: every placed inanimate object of type 1 (furniture), 2 (weapon), 3 (ammo), 4 (health), 5 (armour) or 15 (field charge) (inteng's FUN_004e3e30 by the object's +0x44) is chosen:
// its GravityOn set, its IsStatic cleared; off: both put back, the objects left where the engine took them. False when no level is open or the game has no player block to run with.
bool LevelView_SetGravity(bool on);
// One frame of it (Object Placement's frame, while Gravity is on)
void LevelView_GravityStep();
// The chosen objects that the engine has moved since Gravity went on: each one's 8-byte ID, its place now in the game's units and its matrix (16.16, by rows), for the save that writes them
struct LevelViewMovedObject { uint8_t id[8]; int32_t world[3]; int32_t matrix[9]; };
void LevelView_GravityMoved(std::vector<LevelViewMovedObject> *moved);
// The state line's gravity words: "game <1|0> objects <n>": whether the game's engine runs Gravity now, and how many objects it was given
std::string LevelView_GravityWords();
// A FLIGHT'S COLLISIONS (L, OP-118): the object whose 8-byte ID this is moved by the engine from `from` to `to` (the game's units) through its own strategy block's dynamics, gravity off,
// colliding with the level as the game's objects do; *reached is where it got to. False (reached = to) when the object has no dynamics block or the engine can't run.
bool LevelView_FlyCollideMove(const uint8_t *id, const int32_t from[3], const int32_t to[3], int32_t reached[3]);
// B (OP-117, FUN_004df1e0 / FUN_004df2d0): the object's collision box (its display block's ObMaxX..ObMinZ, which the engine's box is made from) grown by 2000 on each side, or put back
bool LevelView_FlyBoxGrow(const uint8_t *id, bool grow);

// A view matrix in place of the camera's turn, look and roll for the draws that follow (the orbit's C: inteng's camera block takes it whole), or none again (nullptr). The camera's x y z stay its place.
// form: the game's (the player's ObMat, as PutTheEyes puts it; 16.16, mat11 mat12 mat13 mat21 ...).
void LevelView_SetViewMatrix(const int32_t *matrix);

// The camera's turn, look and roll whose view (the look about x after the turn and roll, as the game's UpdateCamera makes it) is this matrix, the look kept within a quarter turn either way; in 4096ths
// form: the game's VDB_Mat (local to world: the way the camera faces is column 3, mat13 mat23 mat33), NOT inteng's view block +0x38, which is it turned over (FUN_004c19a0).
void LevelView_AnglesOfMatrix(const int32_t matrix[9], int *turn, int *look, int *roll);

// The view's matrix of the camera's turn, look and roll, as the game's UpdateCamera makes it from PutTheEyes's player: pan(look) x CreateEulerMatrix(0, turn, roll) (MatrixMultiply(ObMat, pan))
// form: the game's VDB_Mat, as LevelView_AnglesOfMatrix takes it. THE GAME'S OWN (avpx maths.cpp, not ours to comment there): form: CreateEulerMatrix(e) gives the game's (PutTheEyes's ObMat, VDB_Mat for no look);
// form: MatrixToEuler(m) takes CreateEulerMatrix's TURNED OVER (editor_main.cpp CameraOfFileSlot and LevelView_CameraHeading turn it over first).
void LevelView_MatrixOfAngles(int turn, int look, int roll, int32_t matrix[9]);

// A-7 (OP-113), THE TURN KEYS' SPEEDS as inteng's mover FUN_0053db40 makes them (0x53dc1a-0x53dd3c): a matrix that starts as the identity is multiplied by a 10 degree turn for each key held, in the order
// numpad 4, 6, 7, 9, 8, 2 (the exe's matrices at 0x5f0568, 0x5f0540, 0x5f0608, 0x5f05e0, 0x5f0590, 0x5f05b8, read: cos 64540 and sin 11380 of 65536; 4 / 6 about y, 7 / 9 about z, 8 / 2 about x), and its
// Euler angles (MatrixToEuler, FUN_004b3d90) are the speeds x, y, z, each past half a turn taken as the negative; in 4096ths of a turn a second
void LevelView_TurnSpeedsOfKeys(bool numpad4, bool numpad6, bool numpad7, bool numpad9, bool numpad8, bool numpad2, int speeds[3]);

// A matrix (the game's ObMat) turned by a frame's angles x, y, z about its own axes (0x53dea7-0x53df8d): M = CreateEulerMatrix of the angles, the matrix multiplied by M (MatrixMultiply(matrix, M, matrix),
// FUN_004b34b0), then squared again (MNormalise, FUN_004b4030)
void LevelView_TurnMatrix(int32_t matrix[9], const int angles[3]);

// THE ORBIT (camera-closeout-drake.md CAM-3; FUN_0053db40 0x53dec0-0x53e071, '/' or numpad / held): the frame's angles as a matrix (CreateEulerMatrix, FUN_004b3370), turned round (TransposeMatrixCH,
// FUN_004b3710); about the CAMERA's own axes when cameraAxes (numpad /: the camera's matrix C, the turn made C^T x turn x C through MatrixMultiply(C, R, R), TransposeMatrixCH(C), MatrixMultiply(R, C^T, R)),
// else about the world's ('/'); the place swung round the centre by it (place - centre, RotateVector (FUN_004b3860), + centre) and the matrix turned by it (MatrixMultiply(R, matrix, matrix), FUN_004b34b0, then
// MNormalise, FUN_004b4030). The same calls in the same order as inteng's, the engine's own: so the centre keeps its place in the view while the camera swings round it.
void LevelView_OrbitTurn(int32_t matrix[9], int32_t place[3], const int32_t centre[3], const int angles[3], bool cameraAxes);

// The live copy put at a place with a matrix as its ObMat (the flown object's own, A-7), its Euler from that matrix as the mover writes it (FUN_004b3d90 into +0x1c)
void LevelView_FlyPutMatrix(const int32_t world[3], const int32_t matrix[9]);

// The file's turn (x y z w, as LevelView_FlyPut takes it) whose ObMat this matrix is: FlyPut's the other way (MatToQuat, then the file's signs)
void LevelView_TurnOfMatrix(const int32_t matrix[9], float quat[4]);

// The ObMat LevelView_FlyPut gives the live copy for a file's turn (x y z w), the same steps (QuatToMat, MatrixToEuler, CreateEulerMatrix, TransposeMatrixCH): for a pose put on the flown object (Move rel.
// to poly) while its own matrix is its state (A-7)
void LevelView_MatrixOfTurn(const float quat[4], int32_t matrix[9]);

// A-7b, numpad '*' (the mover 0x53e41b-0x53e51a): the matrix LEVELLED: row 2 the forward with no up or down (its y 0, Normalise: FUN_004b3280), row 1 straight down (0, 65536, 0), row 0 their normal
// (MakeNormal(0, row 1, row 2): FUN_004fd620); a forward straight up or down (x and z both 0) gives the identity. NAMED: avpx's Normalise is our own rewrite (normalise.h), inteng's may round a unit apart.
void LevelView_LevelMatrix(int32_t matrix[9]);

// A-7b, Shift + numpad '*' (FUN_0053d1e0, at 0x53e532): the matrix SNAPPED to right angles: row 0's largest component (by size; the first of two the same) becomes +-65536 and its others 0, row 1 the
// same among the two axes row 0 did not take, row 2 their normal (MakeNormal(0, row 0, row 1))
void LevelView_SquareMatrix(int32_t matrix[9]);

int32_t LevelView_ModuleCount();
bool LevelView_ModuleName(int32_t moduleIndex, std::string *name);
// Module m's own name, whether or not the game has it in play. LevelView_ModuleName answers only for a module with a display block, and the game gives one only to the modules
// seen from where the level starts, until the view's first draw shows every module. Vasquez 11-20, the 13-module paint (path_screen.cpp GameModuleOfNumber)
bool LevelView_ModuleOwnName(int32_t moduleIndex, std::string *name);
int32_t LevelView_ModuleAt(const int32_t place[3]);
bool LevelView_ModuleRadius(int32_t moduleIndex, int32_t *radius);   // the module's shape radius (shaperadius), in the game's units (mm)
bool LevelView_ModuleWorld(int32_t moduleIndex, int32_t world[3]);   // where the game puts module m, in millimetres (MODULE.m_world)

// The 8-byte IDs of the objects the game has in play that have a strategy block (what the file's objects are found by), for a proof run
void LevelView_ActiveObjectIds(std::vector<std::vector<uint8_t>> *ids);

// The modules the last frame drew (#78; added 2026-11-17 by Drake): the game's own list of what was on screen (OnScreenBlockList, which AVPGetInViewVolumeList fills as the frame is
// made), each module's index once, in that list's order. A module's own block only (its m_dptr), not the objects inside it.
void LevelView_DrawnModules(std::vector<int32_t> *moduleIndex);

// HIDDEN OBJECTS: the objects with these 8-byte IDs are not drawn at all (up to kLevelViewMostHidden), and with everyObject true neither is any object the level file places (the modules still are).
// Every call REPLACES what was hidden before; count 0 and everyObject false shows everything again. An ID of no object in play is left out. Editor-only: avpx R_SetEditorHiddenBlocks.
size_t const kLevelViewMostHidden = 20000;
void LevelView_HideObjects(const uint8_t (*ids)[8], size_t count, bool everyObject);

// The same for MODULES of the level (the game's module numbers, MODULE.m_index): these are not drawn, and with everyModule none is. Replaces what modules were hidden before.
void LevelView_HideModules(const int32_t *moduleIndex, size_t count, bool everyModule);

// And these display blocks themselves (up to kLevelViewMostHidden), for blocks no strategy block or module names: the ones the editor made for the objects of a file with no modules
// (hierarchy_view.h ModelObjects). Replaces the earlier list; count 0 hides none.
void LevelView_HideBlocks(const struct displayblock *const *blocks, size_t count);

// Everything (module or object) whose distance from the camera, less its radius, is at least `distance` millimetres is not drawn, as inteng's view-distance rule hides it
// (FUN_004c36f0: distance(camera place, record place) - radius < view distance is drawn). on false: off.
void LevelView_HideBeyond(bool on, int32_t distance);
// The object whose block the distance rule never hides (Object Placement's current object, FUN_004dd2f0); null: none
void LevelView_HideBeyondKeep(const uint8_t *objectId);
// The same for several objects or modules (up to kLevelViewMostHidden): Texture Map keeps every one with a selected polygon (FUN_00544070 0x5440d3). Replaces the earlier keep; count 0 keeps none.
void LevelView_HideBeyondKeepShapes(const LevelViewShapeOf *shapes, size_t count);

// THE VIEW RULE's own hiding, kept apart from the explicit Hide calls above so the two never undo each other (the hidden set drawn is the union): these modules (game numbers) and, with everyObject,
// every placed object. Replaces the rule's earlier set; (nullptr, 0, false) lets go of it.
void LevelView_HideByRule(const int32_t *moduleIndex, size_t count, bool everyObject);

// THE PLOT'S PREVIEW's own hiding (plot_preview_view.h), kept apart from the rule's and the explicit sets so none undoes another (the union is drawn): every module
// but these (game numbers) and every placed object is not drawn. Replaces the preview's earlier set; (nullptr, 0) lets go of it.
void LevelView_HideForPreview(const int32_t *shownModules, size_t count);

// THE PLOT'S PREVIEW's INSET (the owner 11-21, 05:4x: "Full map + inset on top"): a second view, drawn AFTER the main view and OVER it, in a box of the window
// (pixels). The main view is drawn whole first, as on any frame, WITH the frame's world lines (LevelView_WorldLines); then the inset's box alone is cleared
// (black, and its depth) and this camera's view is drawn into it, with the preview's own hidden set (LevelView_HideForPreview counts for this view only) and the
// inset's own lines (LevelView_InsetLines). After the frame the eyes are the main camera's again, so what reads the last frame's eyes (the fly keys'
// LevelView_CameraAxes, LevelView_EyePlace, picks) reads the main view. Set it each frame before LevelView_Draw: on, with the camera and the box; off (false),
// none. A box under 16 x 12 is none. Drawn with the live plot's renderer calls (renderer.h R_EditorFlushWorldWithLines, R_EditorClearBox,
// R_SetEditorFinalLinesSlot, R_SetEditorMainViewport; Frost 11-21); the game's own drawing is unchanged by them.
void LevelView_Inset(bool on, const FlyCamera &camera, int boxLeft, int boxTop, int boxWidth, int boxHeight);

// THE INSET'S OWN LINES (the live plot: the module being planned), kept apart from the main view's (LevelView_WorldLines) and drawn only in the inset; count 0
// takes them away. Up to kLevelViewWorldLinesMax
void LevelView_InsetLines(const host::TrackLine *lines, size_t count);

// whether the last frame drew an inset over its main view (LevelView_Inset): the preview's state line
bool LevelView_InsetDrawn();

// how long the last frame's inset took to make, on the processor (ms): its view made, from the depth's clearing to its lists filled. The card's part (its lists
// drawn, in the frame's last drawing) is not in it: that shows only in the whole plot's time. 0 when the last frame drew none
double LevelView_InsetMs();

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

// An object's box in the world (its place plus its shape's extents), found among the active blocks by its ID; false when it is not in play
bool LevelView_ObjectBox(const uint8_t id[8], int32_t mins[3], int32_t maxs[3]);

// Doors > Close / Open (FUN_004a9f40 / FUN_004aa1b0): every module whose shape has morph frames has the points and vertex normals of its main shape exchanged with those of the other shape of its first frame (the game keeps both as
// shapes; closed is the exchanged state). Does it only when the state changes. The static level is built again so the next frame draws the new points. Gives back how many module shapes were exchanged. (Added 2026-11-15 by
// Melanikus (Drake).)
size_t LevelView_SetDoorsClosed(bool closed);
bool LevelView_DoorsAreClosed();
