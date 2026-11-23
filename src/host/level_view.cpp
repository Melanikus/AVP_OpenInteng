// level_view.cpp: opening a level and drawing it, the game's own way (level_view.h says why each part is as it is). Written 2026-11-14
// by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every call below is the game's own, in the game's own order, from avpx/3dc/avp/win95/winmain.cpp at the pin (85704f7a). Each step
// names the line it comes from, so that at a pin move a reader can hold the two side by side.

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "gameplat.h"
#include "bh_types.h"
#include "compiled_shapes.h"
#include "gammacontrol.h"
#include "vision.h"
#include "static_level.h"
#include "d3d_render.h"
#include "view.h"
#include "FastFile.h"
#include "console.h"
#include "language.h"
#include "level_file_name.h"
#include "logstring.h"
#include "TextureManager.h"
#include "renderer.h"
#include "pvisible.h"
#include "kzsort.h"
#include "extents.h"

#include "camera_start.h"

#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "level_view.h"
#include "mempool.h"   // the level memory's use: what an opened level takes of its 40 blocks (LevelMemoryPool_Use; Frost 2026-11-21)

// Declared in no header: the game declares them itself where it calls them, as here
extern void ConstructOneOverSinTable(void);       // maths.cpp:69; io.cpp:55 declares it the same way
extern char LevelName[MAX_PATH];                   // winmain.cpp:136; the editor's copy is src/host/edge/data_winmain.cpp
extern void ReleaseAllFMVTextures(void);          // winmain.cpp's own declaration
extern void DeInitialisePlayer(void);             // winmain.cpp:161
extern void EndNPCs(void);                         // winmain.cpp's own declaration
extern void ExitGame(void);                        // winmain.cpp's own declaration
extern void ClearMemoryPool(void);                 // winmain.cpp's own declaration
extern void ResetFrameCounter(void);              // winmain.cpp's own declaration
extern void FrameCounterHandler(void);            // winmain.cpp's own declaration
extern bool IsDemoVersion();                       // winmain.cpp:177
extern texID_t AAFontImageNumber;                  // winmain.cpp:178; d3d_hud.cpp:86
extern texID_t AVPMENUGFX_SLIDERBAR;               // menu_gfx.cpp:45
extern texID_t AVPMENUGFX_SLIDER;                  // menu_gfx.cpp:46
extern void CalculateWidthsOfAAFont();             // winmain.cpp:923
extern const char *GameDataDirName;                // system.cpp:435: the game's folder of levels, "avp_rifs"
extern const char *FileNameExtension;              // system.cpp:436: ".rif"
extern void SetBriefingTextToBlank(void);          // menus.cpp:7406; menus.cpp:140 declares it the same way

static bool levelIsOpen = false;

// What the level took of its 40 blocks of memory once it had opened (avpx mempool.h), for the state line's "levelmemory" and the track import's cap (Frost
// 2026-11-21, the owner: "we had 40mb of space in the RIF (before hitting engine crash limit) to put in anim"). The game's own loader filled it, in the game's order.
static LevelMemoryPoolUse memoryAtOpen = {};
static bool doorsClosedInView = false;   // Doors > Close has exchanged the doors' shapes (LevelView_SetDoorsClosed)

// why, written so it always ends, whatever its size
static void Say(char *why, size_t whySize, const char *what)
{
	if (why && whySize)
	{
		_snprintf_s(why, whySize, _TRUNCATE, "%s", what);
	}
}

bool LevelView_Start(char *why, size_t whySize)
{
	// The editor shows a whole level at once, so it needs room for more display blocks than the game's 750 (the biggest stock level has 608
	// modules alone). Asked before any level is loaded, because the lists are made when it is (object.cpp).
	Objects_AllowManyDisplayBlocks(true);

	// winmain.cpp:877-878: the fast files, then the game's settings (InitGame: no network, a marine, the console's words)
	FF_Init();
	InitGame();

	// winmain.cpp:891's InitialiseSystem, but only its last step, the table the renderer's maths needs (io.cpp:191). The rest of it
	// starts the game's own keyboard, mouse and joystick input (io.cpp:167-185), which can take the mouse: the editor has its own.
	ConstructOneOverSinTable();

	// winmain.cpp:892: the renderer's lists of objects, strategies, lights and views
	InitialiseRenderer();

	// winmain.cpp:916-934: the font the game writes all its words in, and the menus' two slider pictures. The game's loading screen
	// draws a level's briefing in that font while the level loads (menus.cpp's RenderBriefingText), so it has to be in first.
	AAFontImageNumber = IsDemoVersion() ? Tex_CreateFromRIM("graphics/Menus/smallfont.RIM") : Tex_CreateFromRIM("graphics/Common/aa_font.RIM");
	CalculateWidthsOfAAFont();
	if (MISSING_TEXTURE != AAFontImageNumber)
	{
		DeRedTexture((Texture)Tex_GetTextureDetails(AAFontImageNumber));
	}
	AVPMENUGFX_SLIDERBAR = Tex_CreateFromRIM("graphics/Menus/SliderBar.RIM");
	AVPMENUGFX_SLIDER = Tex_CreateFromRIM("graphics/Menus/Slider.RIM");

	// winmain.cpp:933 and :959: the console, and the game's words (the level's names and messages)
	Con_Init();
	InitTextStrings();

	Say(why, whySize, "");
	return true;
}

extern VECTORCH PlayerStartLocation;   // player.cpp:64: the level's own start, from its AVPSTART record (projload.cpp:149-181)

static char startWords[400] = "";

const char *LevelView_StartWords()
{
	return startWords;
}

// The name LevelView_Open was given: the file the level was OPENED from. LevelView_ShowFile loads the working bytes' copy under another LevelName and keeps this one
static char openedName[MAX_PATH] = "";
static bool showingAFile = false;   // LevelView_ShowFile is opening: openedName stays

bool LevelView_FilePath(char *path, size_t size)
{
	if (!path || !size)
	{
		return false;
	}

	path[0] = '\0';

	if (!levelIsOpen)
	{
		return false;
	}

	// system.cpp's LoadTheLevel builds the level's path just so: the folder, a slash, the level's name (the one LevelView_Open was given: openedName, which LevelView_ShowFile keeps
	// while the game's LevelName is the working bytes' copy) and the extension. A path that doesn't fit is given back as none, never as a name cut short, which could name another file.
	if (_snprintf_s(path, size, _TRUNCATE, "%s/%s%s", GameDataDirName, openedName, FileNameExtension) < 0)
	{
		path[0] = '\0';
		return false;
	}

	return true;
}

// the pictures the RIM import page gave the open level (LevelView_AddImportedPicture), forgotten when a level opens
static std::vector<std::pair<int32_t, int32_t> > importedPictures;

bool LevelView_Open(const char *levelName, FlyCamera *startHere, char *why, size_t whySize)
{
	importedPictures.clear();   // another level: the RIM import page's pictures were the last one's

	if (!levelName || !startHere)
	{
		Say(why, whySize, "no level was named");
		return false;
	}

	// each picture of the level that can't be loaded is named in the log and counted (LevelView_SayMissingPictures), once: the edited level shown again (LevelView_ShowFile)
	// keeps the first open's count, and says nothing more
	R_SetEditorCountMissingPictures(!showingAFile);

	// The game's own rule for a level file's name (level_file_name.h), the one its -n switch is held to
	const char *refused = LevelFileName_WhyRefused(levelName);
	if (refused)
	{
		Say(why, whySize, refused);
		return false;
	}
	if (strlen(levelName) >= sizeof(LevelName))
	{
		Say(why, whySize, "the level's name is longer than the game can hold");
		return false;
	}

	LevelView_Close();

	// winmain.cpp:879-880 and :1005: the level to load is LevelName, as the first and only environment
	strcpy_s(LevelName, sizeof(LevelName), levelName);
	Env_List[0]->main = LevelName;
	AvP.CurrentEnv = AvP.StartingEnv = I_Gen1;
	AvP.Network = I_No_Network;

	// The game's loading screen writes the level's briefing while it loads (menus.cpp:7413), and the game's menus choose those words
	// first (menus.cpp:7183-7211). The editor has no menus and no briefing, so its loading screen has five blank lines, as the game's
	// own menus give a level with none (menus.cpp:3106, :7309).
	SetBriefingTextToBlank();

	// winmain.cpp:1008-1021, in order. The brightness (gamma) is the one the game gives a new player's profile, 128 of 255
	// (user_profile.cpp:435): the game takes it from the player's profile, which the editor never reads, and left at 0 the level is black.
	d3d_light_ctrl.ctrl = LCCM_NORMAL;
	RequestedGammaSetting = 128;
	InitialiseGammaSettings(RequestedGammaSetting);
	start_of_loaded_shapes = load_precompiled_shapes();
	InitCharacter();

	// The level itself, through the game's own loader. TryLoadRifFile (system.cpp) is the game's LoadRifFile without its ending: where
	// the game stops on a refused level, it gives back false, after writing the level's own log as the game does. So a refused level is
	// reported here, and the editor carries on. Nothing of the level is in. What was set up above is set up again by the next open: the
	// precompiled shapes are loaded only once (compiled_shapes.cpp), and InitCharacter lets go of the character it loaded before
	// (system.cpp's InitCharacter). The memory pool keeps that character until the next level that opens is closed.
	if (!TryLoadRifFile())
	{
		Say(why, whySize, "the game's loader refused the level's file; the level's own log says why");
		return false;
	}
	AssignAllSBNames();
	StartGame();
	StaticLevel_BuildThisLevel();

	// what the level takes of its 40 blocks, as the game's loader left it, and the game's own log line for it (winmain.cpp says the same once a level has loaded)
	LevelMemoryPool_Use(&memoryAtOpen);
	LevelMemoryPool_Say();
	ResetFrameCounter();
	levelIsOpen = true;
	doorsClosedInView = false;   // a level starts with its doors as the file holds them (open)

	if (!showingAFile)
	{
		strcpy_s(openedName, sizeof(openedName), levelName);   // the file the level was opened from (LevelView_FilePath)
	}

	// Where the level put the player, looking straight ahead (camera_start.h): the player's body, its dynamics block's Position, which
	// InitPlayer takes from the level's own start (player.cpp:340). The player's display block, Player->ObWorld, only follows the body once
	// the game's own frame moves the player, which the editor never runs, so it may be anywhere: on 2026-11-14 it was still at 0,0,0, so
	// Genshd1's view was black and Invasion's drew only because its origin is inside a room. A missing piece is never read: the camera
	// then starts at the level's own start, and the run's report says which piece. (Modified 2026-11-14 by Melanikus (Dietrich), F3: the
	// camera used to start at the display block.)
	bool const hasPlayer = Player != 0;
	bool const hasStrategy = hasPlayer && Player->ObStrategyBlock != 0;
	bool const hasDynamics = hasStrategy && Player->ObStrategyBlock->DynPtr != 0;
	CameraPlace body = { 0, 0, 0 };
	CameraPlace display = { 0, 0, 0 };
	CameraPlace const levelStart = { PlayerStartLocation.vx, PlayerStartLocation.vy, PlayerStartLocation.vz };
	const char *missing = 0;

	if (hasDynamics)
	{
		VECTORCH const &position = Player->ObStrategyBlock->DynPtr->Position;

		body.x = position.vx;
		body.y = position.vy;
		body.z = position.vz;
	}

	if (hasPlayer)
	{
		display.x = Player->ObWorld.vx;
		display.y = Player->ObWorld.vy;
		display.z = Player->ObWorld.vz;
	}

	CameraPlace const start = CameraStart_Choose(hasPlayer, hasStrategy, hasDynamics, body, levelStart, &missing);

	startHere->x = start.x;
	startHere->y = start.y;
	startHere->z = start.z;
	startHere->turn = 0;
	startHere->look = 0;
	FlyCamera_Hold(startHere);   // a level can put its player anywhere: the camera starts inside its limits all the same (level_view.h)

	// For a run's report: each place, and the room the camera starts in (by the game's own search, pvisible.cpp:857), by its number, or
	// none when the place is outside every room, where the game draws nothing
	VECTORCH cameraPlace = { startHere->x, startHere->y, startHere->z };
	MODULE *const room = ModuleFromPosition(&cameraPlace, (MODULE *)0);
	char roomWords[16] = "none";

	if (room)
	{
		_snprintf_s(roomWords, sizeof(roomWords), _TRUNCATE, "%d", room->m_index);
	}

	_snprintf_s(startWords, sizeof(startWords), _TRUNCATE,
		"the level's start %d %d %d; the player's body %d %d %d; its display block %d %d %d; the camera starts at %d %d %d, in room %s%s%s",
		levelStart.x, levelStart.y, levelStart.z, body.x, body.y, body.z, display.x, display.y, display.z, startHere->x, startHere->y,
		startHere->z, roomWords, missing ? "; " : "", missing ? missing : "");

	Say(why, whySize, "");
	return true;
}

// The frame just drawn, saved as a PNG picture, the way the game's own test player saves one (hudson.cpp:1432-1446): the back buffer is
// copied into memory the processor can read, and written out from there.
// A picture's file kind from its name: a BMP for ".bmp" (the Screenshot item's pictures), a PNG for anything else
static D3DXIMAGE_FILEFORMAT PictureKind(const char *path)
{
	size_t const length = strlen(path);

	return length >= 4 && _stricmp(path + length - 4, ".bmp") == 0 ? D3DXIFF_BMP : D3DXIFF_PNG;
}

static bool SaveThePicture(const char *path)
{
	LPDIRECT3DDEVICE9 const device = d3d.lpD3DDevice;
	IDirect3DSurface9 *screen = 0;
	IDirect3DSurface9 *copy = 0;
	D3DSURFACE_DESC size;
	bool saved = false;

	if (device && SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &screen)))
	{
		if (SUCCEEDED(screen->GetDesc(&size)) &&
			SUCCEEDED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL)))
		{
			saved = SUCCEEDED(device->GetRenderTargetData(screen, copy)) && SUCCEEDED(D3DXSaveSurfaceToFileA(path, PictureKind(path), copy, NULL, NULL));
			copy->Release();
		}
		screen->Release();
	}
	return saved;
}

extern DISPLAYBLOCK *ActiveBlockList[];             // the game's display blocks in play (d3d_render.cpp declares it the same way)
extern int NumActiveBlocks;                        // the game's count of display blocks in play (bh_death_volume.cpp:16 declares it the same way)

// The object with this ID drawn in green (L-1 and Bishop's 01:5x: "selecting a thing shouldn't change its color to green?", as inteng drew it): its
// display block is the one whose strategy has this ID as its name, found among the active blocks; none for no ID or no such object.
static bool selectedBlockInPlay = false;   // whether the last frame found the selected object among the active blocks
static uint8_t selectedIds[kLevelViewMostSelected][8];
static size_t selectedIdCount = 0;

bool LevelView_SelectedIsInPlay()
{
	return selectedBlockInPlay;
}

int LevelView_SelectedItemsDrawn()
{
	return R_EditorSelectedItemsDrawn();
}

void LevelView_ThreeViews(bool on, const host::ThreeViews &views, const int32_t cursor[3], int boxLeft, int boxTop, int boxWidth, int boxHeight)
{
	EditorOrtho settings;

	memset(&settings, 0, sizeof(settings));
	settings.on = on;
	settings.showLevel = false;   // inteng's 3-view screen shows only the track (run 18)
	settings.boxLeft = boxLeft;
	settings.boxTop = boxTop;
	settings.boxWidth = boxWidth;
	settings.boxHeight = boxHeight;

	host::OrthoView const *const mine[3] = { &views.top, &views.side, &views.front };

	for (int v = 0; v < 3; v++)
	{
		settings.views[v].originX = mine[v]->originX;
		settings.views[v].originY = mine[v]->originY;
		settings.views[v].scaleX = mine[v]->scaleX;
		settings.views[v].scaleY = mine[v]->scaleY;
		settings.views[v].worldX = mine[v]->worldX;
		settings.views[v].worldY = mine[v]->worldY;
		settings.views[v].worldZ = mine[v]->worldZ;
	}

	int32_t x = 0;
	int32_t y = 0;

	settings.crossShown[0] = on && host::ThreeView_CrosshairTop(views, cursor, &x, &y);
	settings.crossX[0] = x;
	settings.crossY[0] = y;
	settings.crossShown[1] = on && host::ThreeView_CrosshairSide(views, cursor, &x, &y);
	settings.crossX[1] = x;
	settings.crossY[1] = y;
	settings.crossShown[2] = on && host::ThreeView_CrosshairFront(views, cursor, &x, &y);
	settings.crossX[2] = x;
	settings.crossY[2] = y;
	R_SetEditorOrtho(&settings);
}

extern SCENE Global_Scene;   // the game's own declaration (bh_far.cpp:55): which scene of the modules is in use
static int32_t moduleTintIndex[kLevelViewMostSelected];
static uint32_t moduleTintColour[kLevelViewMostSelected];
static size_t moduleTintCount = 0;
static std::vector<uint8_t> objectTintIds;       // 8 bytes each
static std::vector<uint32_t> objectTintColours;
static uint32_t selectedTintColour = 0xFF00FF00u;

void LevelView_SetObjectTints(const uint8_t *ids, const uint32_t *argb, size_t count)
{
	objectTintIds.clear();
	objectTintColours.clear();

	if (!ids || !argb)
	{
		return;
	}

	count = count < kLevelViewMostSelected ? count : kLevelViewMostSelected;
	objectTintIds.assign(ids, ids + count * 8);
	objectTintColours.assign(argb, argb + count);
}

void LevelView_SetSelectedColour(uint32_t argb)
{
	selectedTintColour = argb;
}

void LevelView_SetModuleTints(const int32_t *moduleIndex, const uint32_t *argb, size_t count)
{
	moduleTintCount = moduleIndex && argb && count ? (count < kLevelViewMostSelected ? count : kLevelViewMostSelected) : 0;

	for (size_t i = 0; i < moduleTintCount; i++)
	{
		moduleTintIndex[i] = moduleIndex[i];
		moduleTintColour[i] = argb[i];
	}
}

static bool mainBoxOn = false;
static int mainBox[4] = { 0, 0, 0, 0 };
static LevelViewExtra extraViews[kLevelViewMostExtraViews];
static size_t extraViewCount = 0;

void LevelView_MainBox(bool on, int boxLeft, int boxTop, int boxWidth, int boxHeight)
{
	mainBoxOn = on && boxWidth >= 16 && boxHeight >= 12;
	mainBox[0] = boxLeft;
	mainBox[1] = boxTop;
	mainBox[2] = boxWidth;
	mainBox[3] = boxHeight;
	R_SetEditorMainViewport(on, boxLeft, boxTop, boxWidth, boxHeight);
}

void LevelView_SetExtraViews(const LevelViewExtra *views, size_t count)
{
	extraViewCount = views && count ? (count < kLevelViewMostExtraViews ? count : kLevelViewMostExtraViews) : 0;

	for (size_t i = 0; i < extraViewCount; i++)
	{
		extraViews[i] = views[i];
	}
}

void LevelView_ThreeViewLines(const host::TrackLine *lines, size_t count)
{
	static EditorOrthoLine converted[4096];
	size_t const used = lines ? (count < 4096 ? count : 4096) : 0;

	for (size_t i = 0; i < used; i++)
	{
		converted[i].x0 = lines[i].from[0]; converted[i].y0 = lines[i].from[1]; converted[i].z0 = lines[i].from[2];
		converted[i].x1 = lines[i].to[0];   converted[i].y1 = lines[i].to[1];   converted[i].z1 = lines[i].to[2];
		converted[i].colour = lines[i].colour;
	}

	R_SetEditorOrthoLines(converted, (int)used);
}

void LevelView_WorldLines(const host::TrackLine *lines, size_t count)
{
	// up to kLevelViewWorldLinesMax (the waypoint display's ~235k lines; was a fixed 4096), in one vector kept from call to call; WorldOverlay sends only a changed list
	static std::vector<EditorWorldLine> converted;
	size_t const used = LevelView_ConvertWorldLines(lines, count, kLevelViewWorldLinesMax, &converted);

	R_SetEditorWorldLines(used ? converted.data() : nullptr, (int)used);
}

void LevelView_InsetLines(const host::TrackLine *lines, size_t count)
{
	// the inset's own list (the renderer's slot 1), kept apart from the main view's (slot 0) so neither is sent again when the other changes
	static std::vector<EditorWorldLine> converted;
	size_t const used = LevelView_ConvertWorldLines(lines, count, kLevelViewWorldLinesMax, &converted);

	R_SetEditorWorldLinesSlot(1, used ? converted.data() : nullptr, (int)used);
}

void LevelView_WorldTriangles(const LevelViewTriangle *triangles, size_t count)
{
	// the hook takes 65536 (avpx editor-world-triangles); the numbers are kept in one vector that grows as needed
	static std::vector<EditorWorldTriangle> converted;
	size_t const used = triangles ? (count < 65536 ? count : 65536) : 0;

	converted.resize(used);

	for (size_t i = 0; i < used; i++)
	{
		converted[i].x0 = triangles[i].a[0]; converted[i].y0 = triangles[i].a[1]; converted[i].z0 = triangles[i].a[2];
		converted[i].x1 = triangles[i].b[0]; converted[i].y1 = triangles[i].b[1]; converted[i].z1 = triangles[i].b[2];
		converted[i].x2 = triangles[i].c[0]; converted[i].y2 = triangles[i].c[1]; converted[i].z2 = triangles[i].c[2];
		converted[i].colour = triangles[i].colour;
		converted[i].group = triangles[i].group;
	}

	R_SetEditorWorldTriangles(converted.data(), (int)used);
}

int LevelView_TriangleGroupPixels(int32_t group)
{
	return R_EditorTriangleGroupPixels(group);
}

static float projectionScaleNow = 1.0f;   // what LevelView_SetProjectionScale last set (the zoom's turn toward a place uses it)
extern float CameraZoomScale;   // the game's own (view.cpp): d3d.fieldOfView is multiplied by it when the projection is built (d3d_render.cpp UpdateProjectionMatrix)

void LevelView_SetProjectionScale(float scale)
{
	float const fov = (float)d3d.fieldOfView;

	projectionScaleNow = (scale > 0.1f && scale <= 10.0f) ? scale : 1.0f;

	if (scale == 1.0f)
	{
		CameraZoomScale = 1.0f;     // exactly the game's own
		return;
	}

	if (!(scale > 0.1f) || !(scale <= 10.0f) || !(fov > 1.0f) || !(fov < 170.0f))
	{
		CameraZoomScale = 1.0f;
		return;
	}

	// the half-width's tangent over the scale: ProjX 160 against 320 is a tangent twice as long
	float const wide = 2.0f * atanf(tanf(fov * 3.14159265f / 360.0f) / scale) * 180.0f / 3.14159265f;

	CameraZoomScale = wide < 175.0f ? wide / fov : 175.0f / fov;
}

double LevelView_FocalLength(int viewWidth)
{
	double const fov = (double)d3d.fieldOfView;

	if (viewWidth < 1 || !(fov > 1.0) || !(fov < 170.0))
	{
		return (double)viewWidth / 2.0;
	}

	return ((double)viewWidth / 2.0) * (double)projectionScaleNow / tan(fov * 3.14159265358979 / 360.0);
}

extern int GlobalLevelOfDetail_Hierarchical;   // the game's (gamevars.cpp): 65536 is its own level of detail

void LevelView_SetAdaptiveDegradation(bool on)
{
	GlobalLevelOfDetail_Hierarchical = on ? 65536 : 0;
}

static int32_t currentModule = -1;
static uint32_t currentModuleColour = 0xFF7F007Fu;

void LevelView_SetCurrentModule(int32_t moduleIndex)
{
	currentModule = moduleIndex;
}

void LevelView_SetCurrentModuleColour(uint32_t argb)
{
	currentModuleColour = argb;
}

bool LevelView_ThreeViewBounds(int32_t mins[3], int32_t maxs[3])
{
	int low[3];
	int high[3];

	if (!R_EditorOrthoBounds(low, high))
	{
		return false;
	}

	for (int axis = 0; axis < 3; axis++)
	{
		mins[axis] = low[axis];
		maxs[axis] = high[axis];
	}

	return true;
}

void LevelView_SelectObjects(const uint8_t (*ids)[8], size_t count)
{
	selectedIdCount = ids && count ? (count < kLevelViewMostSelected ? count : kLevelViewMostSelected) : 0;

	for (size_t i = 0; i < selectedIdCount; i++)
	{
		memcpy(selectedIds[i], ids[i], 8);
	}
}

void LevelView_SelectObject(const uint8_t *id)
{
	uint8_t one[1][8];

	if (id)
	{
		memcpy(one[0], id, 8);
	}

	LevelView_SelectObjects(id ? one : nullptr, id ? 1 : 0);
}

// The display blocks in play of the selected objects, found among the active blocks by their strategies' names; at most kLevelViewMostSelected
static size_t SelectedBlocks(const DISPLAYBLOCK **blocks)
{
	static_assert(sizeof(((STRATEGYBLOCK *)0)->SBname) == 8, "an object's ID is the 8 bytes of its strategy's name");

	size_t found = 0;

	if (selectedIdCount == 0)
	{
		return 0;
	}

	for (int i = 0; i < NumActiveBlocks && found < kLevelViewMostSelected; i++)
	{
		const DISPLAYBLOCK *const block = ActiveBlockList[i];

		if (!block || !block->ObStrategyBlock)
		{
			continue;
		}

		for (size_t s = 0; s < selectedIdCount; s++)
		{
			if (memcmp(block->ObStrategyBlock->SBname, selectedIds[s], 8) == 0)
			{
				blocks[found++] = block;
				break;
			}
		}
	}

	return found;
}

struct displayblock *ModelObjects_BlockOfId(const uint8_t id[8]);   // hierarchy_view.cpp: the block the editor made for an object of a file with no modules

// The shape a LevelViewShapeOf names: the display block of the module, or of the object found among the active blocks by its strategy's name, or (a model file, which has no
// modules: its objects have no strategy) the block the editor made for that object (ModelObjects_Build); null when it is not in play. (The model file's case added 2026-11-18 by
// Dietrich: the Texture Map highlight of a picked part of MelPyth_MarWep / marwep.rif was "not drawn", as no block was found for it.)
static const DISPLAYBLOCK *BlockOfShape(const LevelViewShapeOf &shape)
{
	if (shape.isModule)
	{
		SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

		if (scene && shape.moduleIndex >= 0 && shape.moduleIndex < ModuleArraySize && scene->sm_marray[shape.moduleIndex])
		{
			return scene->sm_marray[shape.moduleIndex]->m_dptr;
		}

		return nullptr;
	}

	for (int i = 0; i < NumActiveBlocks; i++)
	{
		const DISPLAYBLOCK *const block = ActiveBlockList[i];

		if (block && block->ObStrategyBlock && memcmp(block->ObStrategyBlock->SBname, shape.objectId, 8) == 0)
		{
			return block;
		}
	}

	return ModelObjects_BlockOfId(shape.objectId);
}

bool LevelView_LowestPictureSize(int32_t *width, int32_t *height)
{
	if (!width || !height)
	{
		return false;
	}

	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;
	int32_t lowest = -1;

	auto look = [&](const DISPLAYBLOCK *block)
	{
		SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

		if (!header || !header->items || header->numitems <= 0 || header->numitems > (1 << 20))
		{
			return;
		}

		for (int j = 0; j < header->numitems; j++)
		{
			int const type = header->items[j][0];

			// the eight textured polygon types of the file (chunk_type.cpp is_textured: 5, 6, 7, 21 to 25), the only ones whose colour word is a texture number
			if (!(type == 5 || type == 6 || type == 7 || (type >= 21 && type <= 25)))
			{
				continue;
			}

			int32_t const texture = header->items[j][3] & 0xffff;
			uint32_t w = 0, h = 0;

			if ((lowest < 0 || texture < lowest) && Tex_GetRealSize((texID_t)texture, &w, &h) && w > 0 && h > 0 && w <= 65536 && h <= 65536)
			{
				lowest = texture;
				*width = (int32_t)w;
				*height = (int32_t)h;
			}
		}
	};

	for (int m = 0; scene && m < ModuleArraySize; m++)
	{
		if (scene->sm_marray[m])
		{
			look(scene->sm_marray[m]->m_dptr);
		}
	}

	for (int i = 0; i < NumActiveBlocks; i++)
	{
		look(ActiveBlockList[i]);
	}

	return lowest >= 0;
}

bool LevelView_AddImportedPicture(int32_t texture, int32_t listIndex)
{
	if (texture < 0 || listIndex < 0 || importedPictures.size() >= kLevelViewMostImported)
	{
		return false;
	}

	for (std::pair<int32_t, int32_t> const &p : importedPictures)
	{
		if (p.first == texture)
		{
			return false;
		}
	}

	importedPictures.push_back(std::make_pair(texture, listIndex));
	return true;
}

std::vector<std::pair<int32_t, int32_t> > const &LevelView_ImportedPictures()
{
	return importedPictures;
}

bool LevelView_Pictures(std::vector<LevelViewPicture> *pictures, size_t most)
{
	if (!pictures)
	{
		return false;
	}

	pictures->clear();

	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;
	std::vector<int32_t> used;

	auto look = [&](const DISPLAYBLOCK *block)
	{
		SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

		if (!header || !header->items || header->numitems <= 0 || header->numitems > (1 << 20))
		{
			return;
		}

		for (int j = 0; j < header->numitems; j++)
		{
			int const type = header->items[j][0];

			if (type == 5 || type == 6 || type == 7 || (type >= 21 && type <= 25))
			{
				used.push_back(header->items[j][3] & 0xffff);
			}
		}
	};

	for (int m = 0; scene && m < ModuleArraySize; m++)
	{
		if (scene->sm_marray[m])
		{
			look(scene->sm_marray[m]->m_dptr);
		}
	}

	for (int i = 0; i < NumActiveBlocks; i++)
	{
		look(ActiveBlockList[i]);
	}

	// and the pictures imported into the level since it opened, which no polygon may use yet
	for (std::pair<int32_t, int32_t> const &p : importedPictures)
	{
		used.push_back(p.first);
	}

	std::sort(used.begin(), used.end());
	used.erase(std::unique(used.begin(), used.end()), used.end());

	for (int32_t const texture : used)
	{
		uint32_t w = 0, h = 0;

		if (pictures->size() >= most || !Tex_GetRealSize((texID_t)texture, &w, &h) || w == 0 || h == 0)
		{
			continue;
		}

		LevelViewPicture one;

		one.texture = texture;
		one.width = (int32_t)w;
		one.height = (int32_t)h;
		one.name = Tex_GetName((texID_t)texture);
		pictures->push_back(one);
	}

	return !pictures->empty();
}

bool LevelView_ItemTexture(const LevelViewShapeOf &shape, int32_t item, int32_t *texture)
{
	const DISPLAYBLOCK *const block = BlockOfShape(shape);
	SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

	if (!texture || !header || !header->items || item < 0 || item >= header->numitems)
	{
		return false;
	}

	int const type = header->items[item][0];

	if (!(type == 5 || type == 6 || type == 7 || (type >= 21 && type <= 25)))
	{
		return false;
	}

	*texture = header->items[item][3] & 0xffff;
	return true;
}

bool LevelView_SetItemTexture(const LevelViewShapeOf &shape, int32_t item, int32_t texture)
{
	int32_t current = 0;

	if (texture < 0 || texture > 0x7fff || !LevelView_ItemTexture(shape, item, &current))
	{
		return false;
	}

	const DISPLAYBLOCK *const block = BlockOfShape(shape);
	SHAPEHEADER *const header = GetShapeData(block->ObShape);

	header->items[item][3] = (header->items[item][3] & (int)0xffff0000) | texture;
	return true;
}

bool LevelView_SetItemFlagBits(const LevelViewShapeOf &shape, int32_t item, int32_t mask, bool on)
{
	int32_t texture = 0;

	if (!LevelView_ItemTexture(shape, item, &texture))
	{
		return false;
	}

	const DISPLAYBLOCK *const block = BlockOfShape(shape);
	SHAPEHEADER *const header = GetShapeData(block->ObShape);

	header->items[item][2] = on ? (header->items[item][2] | mask) : (header->items[item][2] & ~mask);
	return true;
}

// ---- OBJECT PLACEMENT'S LIGHTING (level_view.h: OP-3, OP-44; Drake 2026-11-17) ----
extern int maxshapes;   // compiled_shapes.cpp:65: how many places mainshapelist has (system.h:98 declares it the same way)

static bool placementLightOn = false;           // the mode's look is on the open level
static bool placementLightObjectsLit = false;   // and the objects' part of it: lit or not
static std::unordered_map<int *, bool> placementLightKept;   // every item flags word the look changed, and whether it held iflag_nolight before the look
static int placementLightModuleItems = 0, placementLightObjectItems = 0;   // how many items the look made unlit (the modules') and set by Lighting (the objects'), at its last change
static int placementLightUnbakedItems = 0;   // and how many items of the objects WITHOUT baked colours it holds plain (Lighting off; 0 while it is on): the owner's ruling, below

// One item's iflag_nolight (2) set (drawn unlit) or clear; its own bit kept the first time the look touches it (a later emplace keeps the first)
static void PlacementLightItem(int *flags, bool unlit)
{
	placementLightKept.emplace(flags, (*flags & iflag_nolight) != 0);
	*flags = unlit ? (*flags | iflag_nolight) : (*flags & ~iflag_nolight);
}

// Which of the game's shapes a module draws (inteng: the shapes before DAT_008f588c, which its loader fills with the modules'): the module's display block's shape, else its map's
static std::vector<bool> ShapesOfModules()
{
	std::vector<bool> isModule(maxshapes > 0 ? (size_t)maxshapes : 0, false);
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	for (int m = 0; scene && m < ModuleArraySize; m++)
	{
		MODULE *const room = scene->sm_marray[m];
		int const shape = !room ? -1 : room->m_dptr ? room->m_dptr->ObShape : (room->m_mapptr ? room->m_mapptr->MapShape : -1);

		if (shape >= 0 && shape < maxshapes)
		{
			isModule[(size_t)shape] = true;
		}
	}

	return isModule;
}

int LevelView_PlacementLighting(bool on, bool objectsLit)
{
	if (!levelIsOpen || !mainshapelist || (on == placementLightOn && (!on || objectsLit == placementLightObjectsLit)))
	{
		return 0;
	}

	bool const modulesToo = on != placementLightOn;   // the modules change only as the look comes and goes; Lighting changes the objects alone
	int changed = 0;

	if (!on)
	{
		for (auto const &kept : placementLightKept)
		{
			*kept.first = kept.second ? (*kept.first | iflag_nolight) : (*kept.first & ~iflag_nolight);
			changed++;
		}

		placementLightKept.clear();
	}
	else
	{
		std::vector<bool> const isModule = ShapesOfModules();
		int moduleItems = 0, objectItems = 0, unbakedItems = 0;

		// the level's shapes (the game's precompiled ones, before start_of_loaded_shapes, are none of the level's)
		for (int s = start_of_loaded_shapes > 0 ? start_of_loaded_shapes : 0; s < maxshapes; s++)
		{
			SHAPEHEADER *const shape = mainshapelist[s];

			if (!shape || !shape->items || shape->numitems <= 0)
			{
				continue;
			}

			bool const module = isModule[(size_t)s];
			// baked colours: the game's loader sets ShapeFlag_PreLit (0x1000) when the object has a SHPVTINT chunk for the level's light set (projload.cpp
			// set_up_baked_colours); inteng's FUN_004d67f0 tests the same bit (0x4d681b / 0x4d687e)
			bool const baked = (shape->shapeflags & ShapeFlag_PreLit) != 0;

			// a module: every item unlit, on the look's start only (0x4d84e6-0x4d8526)
			if (module && !modulesToo)
			{
				continue;
			}

			for (int i = 0; i < shape->numitems; i++)
			{
				if (!shape->items[i])
				{
					continue;
				}

				int *const flags = &shape->items[i][2];

				if (module)
				{
					PlacementLightItem(flags, true);
					moduleItems++;
				}
				else if (baked)
				{
					// an object with baked colours: plain, or lit by its colours, as Lighting says (FUN_004d67f0)
					PlacementLightItem(flags, !objectsLit);
					objectItems++;
				}
				else if (!objectsLit)
				{
					// AN OBJECT WITHOUT BAKED COLOURS, Lighting off: PLAIN, like the level. A NAMED DIFFERENCE from inteng, by the owner's ruling (2026-11-23, through
					// Bishop 2: "Yes, unlit like the level"): FUN_004d67f0 leaves these alone, so inteng draws them lit by the level's lights, dark beside the plain
					// modules ("all placed objects are taking lights in account ... env is lit and assets are dark"; stock Genshd1: none of its 174 placed objects has
					// baked colours, so Lighting changed none of them)
					PlacementLightItem(flags, true);
					unbakedItems++;
				}
				else
				{
					// the same, Lighting on: its own bit back, so it is drawn as inteng draws it, lit by the level's lights (an item the look never touched stays)
					auto const kept = placementLightKept.find(flags);

					if (kept == placementLightKept.end())
					{
						continue;
					}

					*flags = kept->second ? (*flags | iflag_nolight) : (*flags & ~iflag_nolight);
					placementLightKept.erase(kept);
				}

				changed++;
			}
		}

		if (modulesToo)
		{
			placementLightModuleItems = moduleItems;
		}

		placementLightObjectItems = objectItems;
		placementLightUnbakedItems = unbakedItems;
	}

	if (!on)
	{
		placementLightModuleItems = placementLightObjectItems = placementLightUnbakedItems = 0;
	}

	placementLightOn = on;
	placementLightObjectsLit = objectsLit;

	// the modules' baked colours read the bit when the list is built (static_level.cpp BakedColour: an unlit textured polygon is 0x00FFFFFF); objects are drawn each frame from their items (kshape.cpp)
	if (modulesToo)
	{
		StaticLevel_BuildThisLevel();
	}

	return changed;
}

std::string LevelView_PlacementLightingWords()
{
	if (!levelIsOpen || !mainshapelist)
	{
		return std::string();
	}

	std::string words = std::string("look ") + (placementLightOn ? "1" : "0") + " lit " + (placementLightObjectsLit ? "1" : "0") + " modules " + std::to_string(placementLightModuleItems) +
		" objects " + std::to_string(placementLightObjectItems) + " unbaked " + std::to_string(placementLightUnbakedItems);

	// the objects' baked colours, their average per channel over every point (0..255): lit by them, an object differs from its plain (full) look only where they are under 255
	{
		std::vector<bool> const modulesHere = ShapesOfModules();
		long long sum[3] = { 0, 0, 0 }, points = 0;

		for (int s = start_of_loaded_shapes > 0 ? start_of_loaded_shapes : 0; s < maxshapes; s++)
		{
			SHAPEHEADER const *const shape = mainshapelist[s];

			if (!shape || modulesHere[(size_t)s] || !(shape->shapeflags & ShapeFlag_PreLit) || !shape->sh_extraitemdata)
			{
				continue;
			}

			for (int p = 0; p < shape->sh_numextraitems && p < shape->numpoints; p++)
			{
				uint32_t const packed = (uint32_t)shape->sh_extraitemdata[p].EID_VertexI;

				sum[0] += (packed >> 16) & 255;
				sum[1] += (packed >> 8) & 255;
				sum[2] += packed & 255;
				points++;
			}
		}

		words += " baked " + (points ? std::to_string(sum[0] / points) + " " + std::to_string(sum[1] / points) + " " + std::to_string(sum[2] / points) : std::string("none"));
	}

	// the blocks DRAWN this frame (the game's OnScreenBlockList, made by AVPGetInViewVolumeList, view.cpp:657-692: not every block in play is drawn, kzsort.cpp:369-396) whose shape is an
	// object's with baked colours: how many, and up to four of their places in the world: what a picture of the look can be aimed at (the state line is printed after the frame's draw)
	extern DISPLAYBLOCK *OnScreenBlockList[];   // view.cpp:45
	extern int NumOnScreenBlocks;
	std::vector<bool> const isModule = ShapesOfModules();
	int const drawnCount = NumOnScreenBlocks < 0 ? 0 : (NumOnScreenBlocks < 2048 ? NumOnScreenBlocks : 2048);   // maxdisplayblocks (system.h)
	int drawn = 0;
	std::string places;

	for (int i = 0; i < drawnCount; i++)
	{
		DISPLAYBLOCK const *const block = OnScreenBlockList[i];
		int const index = block ? block->ObShape : -1;

		if (!block || block->ObMyModule || block->HModelControlBlock || index < start_of_loaded_shapes || index >= maxshapes || isModule[(size_t)index] || !mainshapelist[index] ||
			!(mainshapelist[index]->shapeflags & ShapeFlag_PreLit))
		{
			continue;
		}

		if (drawn < 4)
		{
			places += (drawn == 0 ? " at " : "|") + std::to_string(block->ObWorld.vx) + " " + std::to_string(block->ObWorld.vy) + " " + std::to_string(block->ObWorld.vz);
		}

		drawn++;
	}

	return words + " drawn " + std::to_string(drawn) + places;
}

bool LevelView_ScaleObjectShape(const LevelViewShapeOf &shape, double factor)
{
	DISPLAYBLOCK *const block = const_cast<DISPLAYBLOCK *>(BlockOfShape(shape));
	SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

	// plain compares (the build's /fp:fast does not keep isfinite): a factor above 0 and at most 1,000,000; a shape with points the game holds
	if (!header || !header->points || !header->points[0] || header->numpoints <= 0 || header->numpoints > (1 << 20) || !(factor > 0.0 && factor <= 1000000.0))
	{
		return false;
	}

	int *const numbers = header->points[0];   // 3 ints a point
	size_t const count = (size_t)header->numpoints * 3;
	int *const extents[] = { &header->shaperadius, &header->shapemaxx, &header->shapeminx, &header->shapemaxy, &header->shapeminy, &header->shapemaxz, &header->shapeminz };
	auto const fits = [factor](int v) { double const product = (double)v * factor; return product < 2147483648.0 && product > -2147483649.0; };

	// every product checked before anything is changed
	for (size_t i = 0; i < count; i++)
	{
		if (!fits(numbers[i]))
		{
			return false;
		}
	}

	for (int *const one : extents)
	{
		if (!fits(*one))
		{
			return false;
		}
	}

	for (size_t i = 0; i < count; i++)
	{
		numbers[i] = (int)((double)numbers[i] * factor);
	}

	for (int *const one : extents)
	{
		*one = (int)((double)*one * factor);
	}

	block->ObRadius = header->shaperadius;
	block->ObMaxX = header->shapemaxx;
	block->ObMinX = header->shapeminx;
	block->ObMaxY = header->shapemaxy;
	block->ObMinY = header->shapeminy;
	block->ObMaxZ = header->shapemaxz;
	block->ObMinZ = header->shapeminz;
	return true;
}

bool LevelView_TurnObjectShape(const LevelViewShapeOf &shape)
{
	DISPLAYBLOCK *const block = const_cast<DISPLAYBLOCK *>(BlockOfShape(shape));
	SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

	if (!header || !header->points || !header->points[0] || header->numpoints <= 0 || header->numpoints > (1 << 20) || header->numitems < 0 || header->numitems > (1 << 20))
	{
		return false;
	}

	int *const points = header->points[0];   // 3 ints a point
	int *const itemNormals = header->sh_normals && header->sh_normals[0] ? header->sh_normals[0] : nullptr;     // 3 ints an item
	int *const pointNormals = header->sh_vnormals && header->sh_vnormals[0] ? header->sh_vnormals[0] : nullptr; // 3 ints a point
	int const minInt = -2147483647 - 1;

	// every number that is negated must have a negative: checked before anything is changed
	for (int i = 0; i < header->numpoints; i++)
	{
		if (points[3 * i + 2] == minInt || (pointNormals && pointNormals[3 * i + 2] == minInt))
		{
			return false;
		}
	}

	for (int i = 0; itemNormals && i < header->numitems; i++)
	{
		if (itemNormals[3 * i + 2] == minInt)
		{
			return false;
		}
	}

	if (header->shapeminz == minInt || header->shapemaxz == minInt)
	{
		return false;
	}

	auto const turn = [](int *xyz)
	{
		int const x = xyz[0];

		xyz[0] = -xyz[2];
		xyz[2] = x;
	};

	for (int i = 0; i < header->numpoints; i++)
	{
		turn(points + 3 * i);

		if (pointNormals)
		{
			turn(pointNormals + 3 * i);
		}
	}

	for (int i = 0; itemNormals && i < header->numitems; i++)
	{
		turn(itemNormals + 3 * i);
	}

	int const maxX = header->shapemaxx, minX = header->shapeminx, maxZ = header->shapemaxz, minZ = header->shapeminz;

	header->shapemaxx = -minZ;
	header->shapeminx = -maxZ;
	header->shapemaxz = maxX;
	header->shapeminz = minX;
	block->ObMaxX = header->shapemaxx;
	block->ObMinX = header->shapeminx;
	block->ObMaxZ = header->shapemaxz;
	block->ObMinZ = header->shapeminz;
	return true;
}

bool LevelView_SetItemUv(const LevelViewShapeOf &shape, int32_t item, const int32_t *uv, int count)
{
	int32_t texture = 0;

	if (!uv || count < 3 || count > 4 || !LevelView_ItemTexture(shape, item, &texture))
	{
		return false;
	}

	const DISPLAYBLOCK *const block = BlockOfShape(shape);
	SHAPEHEADER *const header = GetShapeData(block->ObShape);
	int corners = 0;

	for (int k = 0; k < 5; k++)
	{
		corners += header->items[item][4 + k] != -1 ? 1 : 0;
	}

	const int list = (int)((unsigned)header->items[item][3] >> 16);

	if (corners != count || !header->sh_textures || !header->sh_textures[list] || (header->items[item][2] & iflag_txanim))
	{
		return false;
	}

	for (int k = 0; k < count * 2; k++)
	{
		header->sh_textures[list][k] = uv[k];
	}

	return true;
}

static bool outsideModulesWanted = false;   // what the editor asked of LevelView_DrawObjectsOutsideModules (a flying object draws a copy through it as well)
static MODULE flyModule;
static DISPLAYBLOCK *flyCopy = nullptr;
static uint8_t flyObjectId[8];
static bool flyOn = false;
static char flyRefused[160] = "";   // why the last LevelView_FlyBegin gave false
// THE SECOND COPY (Track Editing's start marker, T-3c): its own module and block, beside the flying one; the real object is not hidden for it
static MODULE ghostModule;
static DISPLAYBLOCK *ghostCopy = nullptr;
static bool ghostOn = false;

void LevelView_DrawObjectsOutsideModules(bool on)
{
	outsideModulesWanted = on;
	R_SetEditorDrawObjectsOutsideModules(on || flyOn || ghostOn);
}

extern MODULEMAPBLOCK VisibilityDefaultObjectMap;   // pvisible.cpp, as hierarchy_view.cpp has it

bool LevelView_FlyActive()
{
	return flyOn;
}

bool LevelView_FlyCopyState(int32_t world[3], int32_t *radius, bool *active)
{
	if (!flyOn || !flyCopy)
	{
		return false;
	}

	world[0] = flyCopy->ObWorld.vx;
	world[1] = flyCopy->ObWorld.vy;
	world[2] = flyCopy->ObWorld.vz;
	*radius = flyCopy->ObRadius;
	*active = false;

	for (int i = 0; i < NumActiveBlocks; i++)
	{
		if (ActiveBlockList[i] == flyCopy)
		{
			*active = true;
		}
	}

	return true;
}

char const *LevelView_FlyRefused()
{
	return flyRefused;
}

bool LevelView_FlyBegin(const uint8_t *id)
{
	flyRefused[0] = 0;

	if (flyOn || !id)
	{
		_snprintf_s(flyRefused, sizeof(flyRefused), _TRUNCATE, "%s", flyOn ? "another live copy is up" : "no object");
		return false;
	}

	LevelViewShapeOf shape = {};

	shape.isModule = false;
	memcpy(shape.objectId, id, 8);

	DISPLAYBLOCK const *const original = BlockOfShape(shape);

	if (!original || original->HModelControlBlock || original->ObShape < 0)
	{
		_snprintf_s(flyRefused, sizeof(flyRefused), _TRUNCATE, "%s", !original ? "the object has no display block in play" : (original->HModelControlBlock ? "the object is a hierarchy" : "the object has no shape"));
		return false;
	}

	VisibilityDefaultObjectMap.MapShape = original->ObShape;
	flyModule.m_mapptr = &VisibilityDefaultObjectMap;
	flyModule.m_sbptr = (STRATEGYBLOCK *)NULL;
	flyModule.m_numlights = 0;
	flyModule.m_lightarray = (struct lightblock *)0;
	flyModule.m_extraitemdata = (struct extraitemdata *)0;
	flyModule.m_dptr = NULL;
	flyModule.name = NULL;
	AllocateModuleObject(&flyModule);
	flyCopy = flyModule.m_dptr;

	if (!flyCopy)
	{
		_snprintf_s(flyRefused, sizeof(flyRefused), _TRUNCATE, "the game has no free display block (%d in play, %d at most)", NumActiveBlocks, (int)maxobjects);
		return false;
	}

	flyCopy->ObStrategyBlock = nullptr;
	flyCopy->ObMyModule = nullptr;
	flyCopy->ObWorld = original->ObWorld;
	flyCopy->ObEuler = original->ObEuler;
	flyCopy->ObMat = original->ObMat;
	flyCopy->ObFlags = original->ObFlags;
	flyCopy->ObFlags2 = original->ObFlags2;
	flyCopy->ObFlags3 = original->ObFlags3;
	memcpy(flyObjectId, id, 8);
	flyOn = true;
	R_SetEditorDrawObjectsOutsideModules(true);
	return true;   // (the real one is not drawn while its copy flies: LevelView_Draw leaves it out)
}

// A copy's place (the game's units) and turn (the file's, x y z w), as the game's add_default_object reads a turn (object_setup.cpp)
static void PutCopyAt(DISPLAYBLOCK *copy, const int32_t world[3], const float quat[4])
{
	QUAT q;
	MATRIXCH m;
	EULER euler;

	// the turn as the game's add_default_object reads it (object_setup.cpp), as the model objects take theirs
	q.quatx = (int)-(quat[0] * ONE_FIXED);
	q.quaty = (int)-(quat[1] * ONE_FIXED);
	q.quatz = (int)-(quat[2] * ONE_FIXED);
	q.quatw = (int)(quat[3] * ONE_FIXED);
	QuatToMat(&q, &m);
	MatrixToEuler(&m, &euler);
	copy->ObWorld.vx = world[0];
	copy->ObWorld.vy = world[1];
	copy->ObWorld.vz = world[2];
	copy->ObEuler = euler;
	CreateEulerMatrix(&copy->ObEuler, &copy->ObMat);
	TransposeMatrixCH(&copy->ObMat);
}

void LevelView_FlyPut(const int32_t world[3], const float quat[4])
{
	if (!flyOn || !flyCopy)
	{
		return;
	}

	PutCopyAt(flyCopy, world, quat);
}

bool LevelView_PutObjectAt(const uint8_t *id, const int32_t world[3], const float quat[4])
{
	if (!id || !world || !quat)
	{
		return false;
	}

	LevelViewShapeOf shape = {};

	shape.isModule = false;
	memcpy(shape.objectId, id, 8);

	// the block is the game's own (BlockOfShape gives it read-only for the picks): its place and turn are set here only, as the game's own mover sets them
	DISPLAYBLOCK *const block = const_cast<DISPLAYBLOCK *>(BlockOfShape(shape));

	if (!block || block->HModelControlBlock || block->ObShape < 0)
	{
		return false;
	}

	PutCopyAt(block, world, quat);
	return true;
}

bool LevelView_ObjectBlockAt(const uint8_t *id, int32_t world[3])
{
	if (!id || !world)
	{
		return false;
	}

	LevelViewShapeOf shape = {};

	shape.isModule = false;
	memcpy(shape.objectId, id, 8);

	DISPLAYBLOCK const *const block = BlockOfShape(shape);

	if (!block)
	{
		return false;
	}

	world[0] = block->ObWorld.vx;
	world[1] = block->ObWorld.vy;
	world[2] = block->ObWorld.vz;
	return true;
}

void const *LevelView_FlyCopyBlock()
{
	return flyOn ? flyCopy : nullptr;
}

void const *LevelView_FlyOriginalBlock()
{
	if (!flyOn || !flyCopy)
	{
		return nullptr;
	}

	LevelViewShapeOf flying = {};

	flying.isModule = false;
	memcpy(flying.objectId, flyObjectId, 8);
	return BlockOfShape(flying);
}

bool LevelView_FlyMatrix(int32_t matrix[9])
{
	if (!flyOn || !flyCopy)
	{
		return false;
	}

	MATRIXCH const &m = flyCopy->ObMat;
	int32_t const each[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

	memcpy(matrix, each, sizeof(each));
	return true;
}

void LevelView_OrbitOf(const int32_t blockPlace[3], const int32_t blockMatrix[9], int pitch, int yaw, int32_t distance, int32_t place[3], int32_t matrix[9])
{
	EULER aroundY = { 0, yaw & wrap360, 0 };
	EULER aroundX = { pitch & wrap360, 0, 0 };
	MATRIXCH turnY, turnX, c;

	CreateEulerMatrix(&aroundY, &turnY);
	CreateEulerMatrix(&aroundX, &turnX);
	c.mat11 = blockMatrix[0]; c.mat12 = blockMatrix[1]; c.mat13 = blockMatrix[2];
	c.mat21 = blockMatrix[3]; c.mat22 = blockMatrix[4]; c.mat23 = blockMatrix[5];
	c.mat31 = blockMatrix[6]; c.mat32 = blockMatrix[7]; c.mat33 = blockMatrix[8];

	// FUN_00580a40 0x580a9c-0x580ad2: the block's matrix copied, then multiplied by the yaw's, then by the pitch's (MatrixMultiply(m1, m2, m3) is m2 x m1)
	MatrixMultiply(&c, &turnY, &c);
	MatrixMultiply(&c, &turnX, &c);

	// 0x580ae0-0x580b13: the camera at the block's place, then (0, 0, -distance) turned by C added
	VECTORCH back = { 0, 0, -distance };

	RotateVector(&back, &c);
	place[0] = blockPlace[0] + back.vx;
	place[1] = blockPlace[1] + back.vy;
	place[2] = blockPlace[2] + back.vz;

	int32_t const each[9] = { c.mat11, c.mat12, c.mat13, c.mat21, c.mat22, c.mat23, c.mat31, c.mat32, c.mat33 };

	memcpy(matrix, each, sizeof(each));
}

static bool viewMatrixOn = false;   // LevelView_SetViewMatrix's: PutTheEyes takes viewMatrix whole
static MATRIXCH viewMatrix;

void LevelView_SetViewMatrix(const int32_t *matrix)
{
	viewMatrixOn = matrix != nullptr;

	if (matrix)
	{
		viewMatrix.mat11 = matrix[0]; viewMatrix.mat12 = matrix[1]; viewMatrix.mat13 = matrix[2];
		viewMatrix.mat21 = matrix[3]; viewMatrix.mat22 = matrix[4]; viewMatrix.mat23 = matrix[5];
		viewMatrix.mat31 = matrix[6]; viewMatrix.mat32 = matrix[7]; viewMatrix.mat33 = matrix[8];
	}
}

void LevelView_MatrixOfAngles(int turn, int look, int roll, int32_t matrix[9])
{
	EULER eyes = { 0, turn & wrap360, roll & wrap360 };
	EULER pan = { look & wrap360, 0, 0 };
	MATRIXCH m, panMatrix;

	CreateEulerMatrix(&eyes, &m);
	CreateEulerMatrix(&pan, &panMatrix);   // UpdateCamera's look matrix: (1 0 0; 0 cos -sin; 0 sin cos), the same as this Euler's
	MatrixMultiply(&m, &panMatrix, &m);

	int32_t const each[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

	memcpy(matrix, each, sizeof(each));
}

void LevelView_AnglesOfMatrix(const int32_t matrix[9], int *turn, int *look, int *roll)
{
	// The view is P(look) x E(0, turn, roll) (UpdateCamera: MatrixMultiply(ObMat, pan) = pan x ObMat; ObMat = CreateEulerMatrix(0, turn, roll), PutTheEyes). Written out, with y the turn, z the roll
	// and x the look: row 1 = (cy cz, -cy sz, sy); m23 = -sx cy; m33 = cx cy. The sign of cy is taken so that cx >= 0 (the look within a quarter turn).
	double const one = 65536.0;
	double const m11 = matrix[0] / one, m12 = matrix[1] / one, m13 = matrix[2] / one;
	double const m21 = matrix[3] / one, m22 = matrix[4] / one, m23 = matrix[5] / one, m33 = matrix[8] / one;
	double const sign = m33 < 0.0 ? -1.0 : 1.0;
	double const cy = sign * sqrt(m23 * m23 + m33 * m33);
	double const toUnits = 4096.0 / (2.0 * 3.14159265358979323846);
	double y = atan2(m13, cy), x = 0.0, z = 0.0;

	if (fabs(cy) > 1e-6)
	{
		x = atan2(-m23 * sign, m33 * sign);
		z = atan2(-m12 * sign, m11 * sign);
	}
	else
	{
		// facing straight along the y axis: the look and the roll turn about the same axis, so the look is 0 and the roll takes it all (row 2 = (sin z, cos z, 0))
		z = atan2(m21, m22);
	}

	// the look kept SIGNED, -1024 .. 1024, as the camera holds it (FlyCamera_Hold: a quarter turn either way); a look wrapped to 0 .. 4095 (3640 for -456) was held to 1024, straight
	// down, at the next frame the camera's own mover ran (after the orbit let it go)
	*turn = (int)floor(y * toUnits + 0.5) & wrap360;
	*look = (int)floor(x * toUnits + 0.5);
	*roll = (int)floor(z * toUnits + 0.5) & wrap360;
}

void MatToQuat(MATRIXCH *m, QUAT *quat);   // the game's (hmodel.cpp), declared there only

namespace
{
	MATRIXCH MatrixOf(const int32_t m[9])
	{
		MATRIXCH out;

		out.mat11 = m[0]; out.mat12 = m[1]; out.mat13 = m[2];
		out.mat21 = m[3]; out.mat22 = m[4]; out.mat23 = m[5];
		out.mat31 = m[6]; out.mat32 = m[7]; out.mat33 = m[8];
		return out;
	}

	void PutMatrix(const MATRIXCH &m, int32_t out[9])
	{
		int32_t const each[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

		memcpy(out, each, sizeof(each));
	}
}

void LevelView_TurnSpeedsOfKeys(bool numpad4, bool numpad6, bool numpad7, bool numpad9, bool numpad8, bool numpad2, int speeds[3])
{
	// 10 degrees as the exe holds it: 65536 x the cosine and the sine of 10 degrees, rounded (64540, 11380)
	int const c = 64540, s = 11380;
	int32_t const turns[6][9] = {
		{ c, 0, -s, 0, 65536, 0, s, 0, c },   // numpad 4, 0x5f0568
		{ c, 0, s, 0, 65536, 0, -s, 0, c },   // numpad 6, 0x5f0540
		{ c, s, 0, -s, c, 0, 0, 0, 65536 },   // numpad 7, 0x5f0608
		{ c, -s, 0, s, c, 0, 0, 0, 65536 },   // numpad 9, 0x5f05e0
		{ 65536, 0, 0, 0, c, s, 0, -s, c },   // numpad 8, 0x5f0590
		{ 65536, 0, 0, 0, c, -s, 0, s, c },   // numpad 2, 0x5f05b8
	};
	bool const held[6] = { numpad4, numpad6, numpad7, numpad9, numpad8, numpad2 };
	MATRIXCH m;

	m.mat11 = 65536; m.mat12 = 0; m.mat13 = 0;
	m.mat21 = 0; m.mat22 = 65536; m.mat23 = 0;
	m.mat31 = 0; m.mat32 = 0; m.mat33 = 65536;

	for (int k = 0; k < 6; k++)
	{
		if (held[k])
		{
			MATRIXCH turn = MatrixOf(turns[k]);

			MatrixMultiply(&m, &turn, &m);   // FUN_004b34b0(&local, &turn, &local)
		}
	}

	EULER e;

	MatrixToEuler(&m, &e);
	speeds[0] = e.EulerX > 0x800 ? e.EulerX - 0x1000 : e.EulerX;
	speeds[1] = e.EulerY > 0x800 ? e.EulerY - 0x1000 : e.EulerY;
	speeds[2] = e.EulerZ > 0x800 ? e.EulerZ - 0x1000 : e.EulerZ;
}

void LevelView_TurnMatrix(int32_t matrix[9], const int angles[3])
{
	EULER e = { angles[0] & wrap360, angles[1] & wrap360, angles[2] & wrap360 };
	MATRIXCH turn, m = MatrixOf(matrix);

	CreateEulerMatrix(&e, &turn);
	MatrixMultiply(&m, &turn, &m);
	MNormalise(&m);
	PutMatrix(m, matrix);
}

void LevelView_OrbitTurn(int32_t matrix[9], int32_t place[3], const int32_t centre[3], const int angles[3], bool cameraAxes)
{
	EULER e = { angles[0] & wrap360, angles[1] & wrap360, angles[2] & wrap360 };
	MATRIXCH turn, m = MatrixOf(matrix);

	CreateEulerMatrix(&e, &turn);   // 0x53debb
	TransposeMatrixCH(&turn);       // 0x53dfbc

	if (cameraAxes)
	{
		// 0x53dfcd-0x53e00d: the camera's own matrix copied, then turn = own x turn, own transposed, turn = turn x own
		MATRIXCH own = m;

		MatrixMultiply(&own, &turn, &turn);
		TransposeMatrixCH(&own);
		MatrixMultiply(&turn, &own, &turn);
	}

	// 0x53e010-0x53e059: the place less the centre, turned, the centre added back
	VECTORCH offset = { place[0] - centre[0], place[1] - centre[1], place[2] - centre[2] };

	RotateVector(&offset, &turn);
	place[0] = offset.vx + centre[0];
	place[1] = offset.vy + centre[1];
	place[2] = offset.vz + centre[2];

	// 0x53e05c-0x53e06c: the matrix turned by the same, squared again
	MatrixMultiply(&turn, &m, &m);
	MNormalise(&m);
	PutMatrix(m, matrix);
}

void LevelView_FlyPutMatrix(const int32_t world[3], const int32_t matrix[9])
{
	if (!flyOn || !flyCopy)
	{
		return;
	}

	flyCopy->ObWorld.vx = world[0];
	flyCopy->ObWorld.vy = world[1];
	flyCopy->ObWorld.vz = world[2];
	flyCopy->ObMat = MatrixOf(matrix);
	MatrixToEuler(&flyCopy->ObMat, &flyCopy->ObEuler);
}

void LevelView_LevelMatrix(int32_t matrix[9])
{
	if (matrix[6] == 0 && matrix[8] == 0)
	{
		int32_t const identity[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };

		memcpy(matrix, identity, sizeof(identity));
		return;
	}

	VECTORCH zero = { 0, 0, 0 };
	VECTORCH down = { 0, 65536, 0 };
	VECTORCH forward = { matrix[6], 0, matrix[8] };
	VECTORCH right;

	Normalise(&forward);
	MakeNormal(&zero, &down, &forward, &right);

	int32_t const levelled[9] = { right.vx, right.vy, right.vz, down.vx, down.vy, down.vz, forward.vx, forward.vy, forward.vz };

	memcpy(matrix, levelled, sizeof(levelled));
}

void LevelView_SquareMatrix(int32_t m[9])
{
	// +-65536 by the sign: ((v < 1) - 1 & 0x20000) - 0x10000, so 0 counts as negative (FUN_0053d1e0's own way)
	auto const one = [](int32_t v) { return v > 0 ? 65536 : -65536; };
	auto const size = [](int32_t v) { return v < 1 ? -v : v; };

	// row 0: x, unless y or z is larger (0x53d202-0x53d232)
	int rowZeroAxis;

	if (size(m[0]) < size(m[1]) || size(m[0]) < size(m[2]))
	{
		rowZeroAxis = size(m[1]) < size(m[2]) ? 2 : 1;
	}
	else
	{
		rowZeroAxis = 0;
	}

	int32_t const rowZero = one(m[rowZeroAxis]);

	m[0] = m[1] = m[2] = 0;
	m[rowZeroAxis] = rowZero;

	// row 1: the larger of the two axes row 0 did not take (the first of the two when they are the same size)
	int const first = rowZeroAxis == 0 ? 1 : 0;
	int const second = rowZeroAxis == 2 ? 1 : 2;
	int const rowOneAxis = size(m[3 + first]) < size(m[3 + second]) ? second : first;
	int32_t const rowOne = one(m[3 + rowOneAxis]);

	m[3] = m[4] = m[5] = 0;
	m[3 + rowOneAxis] = rowOne;

	// row 2: their normal (0x53d349)
	VECTORCH zero = { 0, 0, 0 };
	VECTORCH r0 = { m[0], m[1], m[2] };
	VECTORCH r1 = { m[3], m[4], m[5] };
	VECTORCH r2;

	MakeNormal(&zero, &r0, &r1, &r2);
	m[6] = r2.vx;
	m[7] = r2.vy;
	m[8] = r2.vz;
}

void LevelView_MatrixOfTurn(const float quat[4], int32_t matrix[9])
{
	QUAT q;
	MATRIXCH m;
	EULER euler;

	// as LevelView_FlyPut: the turn as the game's add_default_object reads it
	q.quatx = (int)-(quat[0] * ONE_FIXED);
	q.quaty = (int)-(quat[1] * ONE_FIXED);
	q.quatz = (int)-(quat[2] * ONE_FIXED);
	q.quatw = (int)(quat[3] * ONE_FIXED);
	QuatToMat(&q, &m);
	MatrixToEuler(&m, &euler);
	CreateEulerMatrix(&euler, &m);
	TransposeMatrixCH(&m);
	PutMatrix(m, matrix);
}

void LevelView_TurnOfMatrix(const int32_t matrix[9], float quat[4])
{
	MATRIXCH m = MatrixOf(matrix);
	QUAT q;

	MatToQuat(&m, &q);

	// FlyPut takes the file's turn with x y z negated (add_default_object's way): so back
	quat[0] = -(float)q.quatx / (float)ONE_FIXED;
	quat[1] = -(float)q.quaty / (float)ONE_FIXED;
	quat[2] = -(float)q.quatz / (float)ONE_FIXED;
	quat[3] = (float)q.quatw / (float)ONE_FIXED;
}

void LevelView_FlyEnd()
{
	if (!flyOn)
	{
		return;
	}

	flyOn = false;

	if (flyCopy)
	{
		DeallocateModuleObject(&flyModule);
		flyCopy = nullptr;
	}

	R_SetEditorDrawObjectsOutsideModules(outsideModulesWanted || ghostOn);
}

bool LevelView_GhostBegin(const uint8_t *id)
{
	if (ghostOn || !id)
	{
		return false;
	}

	LevelViewShapeOf shape = {};

	shape.isModule = false;
	memcpy(shape.objectId, id, 8);

	DISPLAYBLOCK const *const original = BlockOfShape(shape);

	if (!original || original->HModelControlBlock || original->ObShape < 0)
	{
		return false;
	}

	// as LevelView_FlyBegin's copy (AllocateModuleObject reads the map only while it makes the block), with a module of its own: DeallocateModuleObject frees the module's block
	VisibilityDefaultObjectMap.MapShape = original->ObShape;
	ghostModule.m_mapptr = &VisibilityDefaultObjectMap;
	ghostModule.m_sbptr = (STRATEGYBLOCK *)NULL;
	ghostModule.m_numlights = 0;
	ghostModule.m_lightarray = (struct lightblock *)0;
	ghostModule.m_extraitemdata = (struct extraitemdata *)0;
	ghostModule.m_dptr = NULL;
	ghostModule.name = NULL;
	AllocateModuleObject(&ghostModule);
	ghostCopy = ghostModule.m_dptr;

	if (!ghostCopy)
	{
		return false;
	}

	ghostCopy->ObStrategyBlock = nullptr;
	ghostCopy->ObMyModule = nullptr;   // drawn after the modules, wherever it is (kzsort.cpp)
	ghostCopy->ObWorld = original->ObWorld;
	ghostCopy->ObEuler = original->ObEuler;
	ghostCopy->ObMat = original->ObMat;
	ghostCopy->ObFlags = original->ObFlags;
	ghostCopy->ObFlags2 = original->ObFlags2;
	ghostCopy->ObFlags3 = original->ObFlags3;
	ghostOn = true;
	R_SetEditorDrawObjectsOutsideModules(true);
	return true;
}

void LevelView_GhostPut(const int32_t world[3], const float quat[4])
{
	if (ghostOn && ghostCopy)
	{
		PutCopyAt(ghostCopy, world, quat);
	}
}

void LevelView_GhostEnd()
{
	if (!ghostOn)
	{
		return;
	}

	ghostOn = false;

	if (ghostCopy)
	{
		DeallocateModuleObject(&ghostModule);
		ghostCopy = nullptr;
	}

	R_SetEditorDrawObjectsOutsideModules(outsideModulesWanted || flyOn);
}

bool LevelView_GhostActive()
{
	return ghostOn;
}

bool LevelView_GhostWorld(int32_t world[3])
{
	if (!ghostOn || !ghostCopy)
	{
		return false;
	}

	world[0] = ghostCopy->ObWorld.vx;
	world[1] = ghostCopy->ObWorld.vy;
	world[2] = ghostCopy->ObWorld.vz;
	return true;
}

int32_t LevelView_ModuleCount()
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	return scene ? ModuleArraySize : 0;
}

bool LevelView_ModuleName(int32_t moduleIndex, std::string *name)
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	if (!name || !scene || moduleIndex < 0 || moduleIndex >= ModuleArraySize || !scene->sm_marray[moduleIndex] || !scene->sm_marray[moduleIndex]->m_dptr || !scene->sm_marray[moduleIndex]->name)
	{
		return false;
	}

	*name = std::string(scene->sm_marray[moduleIndex]->name, strnlen(scene->sm_marray[moduleIndex]->name, 255));
	return !name->empty();
}

// the same name, with no display block asked for (level_view.h says why)
bool LevelView_ModuleOwnName(int32_t moduleIndex, std::string *name)
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	if (!name || !scene || moduleIndex < 0 || moduleIndex >= ModuleArraySize || !scene->sm_marray[moduleIndex] || !scene->sm_marray[moduleIndex]->name)
	{
		return false;
	}

	*name = std::string(scene->sm_marray[moduleIndex]->name, strnlen(scene->sm_marray[moduleIndex]->name, 255));
	return !name->empty();
}

bool LevelView_ModuleRadius(int32_t moduleIndex, int32_t *radius)
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	if (!radius || !scene || moduleIndex < 0 || moduleIndex >= ModuleArraySize || !scene->sm_marray[moduleIndex])
	{
		return false;
	}

	MODULE *const room = scene->sm_marray[moduleIndex];
	int const shapeIndex = room->m_dptr ? room->m_dptr->ObShape : (room->m_mapptr ? room->m_mapptr->MapShape : -1);
	SHAPEHEADER *const shape = shapeIndex >= 0 ? GetShapeData(shapeIndex) : nullptr;

	if (!shape)
	{
		return false;
	}

	*radius = shape->shaperadius;
	return true;
}

bool LevelView_ModuleWorld(int32_t moduleIndex, int32_t world[3])
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	if (!scene || moduleIndex < 0 || moduleIndex >= ModuleArraySize || !scene->sm_marray[moduleIndex])
	{
		return false;
	}

	world[0] = scene->sm_marray[moduleIndex]->m_world.vx;
	world[1] = scene->sm_marray[moduleIndex]->m_world.vy;
	world[2] = scene->sm_marray[moduleIndex]->m_world.vz;
	return true;
}

int32_t LevelView_ModuleAt(const int32_t place[3])
{
	VECTORCH at;

	at.vx = place[0];
	at.vy = place[1];
	at.vz = place[2];

	MODULE *const room = ModuleFromPosition(&at, (MODULE *)0);

	return room ? room->m_index : -1;
}

void LevelView_ActiveObjectIds(std::vector<std::vector<uint8_t>> *ids)
{
	if (!ids)
	{
		return;
	}

	ids->clear();

	for (int i = 0; i < NumActiveBlocks; i++)
	{
		const DISPLAYBLOCK *const block = ActiveBlockList[i];

		if (block && block->ObStrategyBlock)
		{
			ids->push_back(std::vector<uint8_t>(block->ObStrategyBlock->SBname, block->ObStrategyBlock->SBname + 8));
		}
	}
}

extern DISPLAYBLOCK *OnScreenBlockList[];   // view.cpp:45, maxdisplayblocks (system.h: 2048) long
extern int NumOnScreenBlocks;

void LevelView_DrawnModules(std::vector<int32_t> *moduleIndex)
{
	if (!moduleIndex)
	{
		return;
	}

	moduleIndex->clear();

	int const kOnScreenMost = 2048;   // the list's length (maxdisplayblocks)
	int const count = NumOnScreenBlocks < 0 ? 0 : (NumOnScreenBlocks < kOnScreenMost ? NumOnScreenBlocks : kOnScreenMost);

	for (int i = 0; i < count; i++)
	{
		DISPLAYBLOCK const *const block = OnScreenBlockList[i];
		MODULE const *const room = block ? block->ObMyModule : nullptr;

		if (room && room->m_dptr == block && room->m_index >= 0 && std::find(moduleIndex->begin(), moduleIndex->end(), (int32_t)room->m_index) == moduleIndex->end())
		{
			moduleIndex->push_back((int32_t)room->m_index);
		}
	}
}

static std::vector<std::vector<uint8_t>> hiddenIds;
static bool hideEveryObject = false;
static std::vector<int32_t> hiddenModules;
static bool hideEveryModule = false;
static int32_t hideBeyondDistance = 0;
static bool hideBeyondOn = false;
static std::vector<int32_t> ruleModules;
static bool ruleEveryObject = false;
static std::vector<int32_t> previewHidden;   // THE PLOT'S PREVIEW's own set (LevelView_HideForPreview)
static bool previewHiding = false;
static bool insetOn = false;                 // THE PLOT'S PREVIEW's INSET (LevelView_Inset): asked for this frame
static FlyCamera insetCamera = {};
static int insetBox[4] = { 0, 0, 0, 0 };
static bool insetDrawing = false;            // the inset's own view is being drawn: the preview's hidden set counts only then (PutTheHiddenBlocks)
static bool insetDrawn = false;              // the last frame drew it (LevelView_InsetDrawn)
static double insetMs = 0.0;                 // and how long it took to make (LevelView_InsetMs)

void LevelView_HideByRule(const int32_t *moduleIndex, size_t count, bool everyObject)
{
	ruleModules.clear();
	ruleEveryObject = everyObject;

	for (size_t i = 0; moduleIndex && i < count && i < kLevelViewMostHidden; i++)
	{
		ruleModules.push_back(moduleIndex[i]);
	}
}

void LevelView_HideForPreview(const int32_t *shownModules, size_t count)
{
	previewHidden.clear();
	previewHiding = shownModules && count > 0;

	for (int32_t m = 0; previewHiding && m < LevelView_ModuleCount() && previewHidden.size() < kLevelViewMostHidden; m++)
	{
		if (std::find(shownModules, shownModules + count, m) == shownModules + count)
		{
			previewHidden.push_back(m);
		}
	}
}

void LevelView_Inset(bool on, const FlyCamera &camera, int boxLeft, int boxTop, int boxWidth, int boxHeight)
{
	insetOn = on && boxWidth >= 16 && boxHeight >= 12;   // (R_SetEditorMainViewport's own least box)
	insetCamera = camera;
	insetBox[0] = boxLeft;
	insetBox[1] = boxTop;
	insetBox[2] = boxWidth;
	insetBox[3] = boxHeight;
}

bool LevelView_InsetDrawn()
{
	return insetDrawn;
}

double LevelView_InsetMs()
{
	return insetDrawn ? insetMs : 0.0;
}

void LevelView_HideModules(const int32_t *moduleIndex, size_t count, bool everyModule)
{
	hiddenModules.clear();
	hideEveryModule = everyModule;

	for (size_t i = 0; moduleIndex && i < count && i < kLevelViewMostHidden; i++)
	{
		hiddenModules.push_back(moduleIndex[i]);
	}
}

static std::vector<const DISPLAYBLOCK *> hiddenBlocksAsGiven;   // LevelView_HideBlocks

void LevelView_HideBlocks(const struct displayblock *const *blocks, size_t count)
{
	hiddenBlocksAsGiven.clear();

	for (size_t i = 0; blocks && i < count && i < kLevelViewMostHidden; i++)
	{
		if (blocks[i])
		{
			hiddenBlocksAsGiven.push_back(blocks[i]);
		}
	}
}

void LevelView_HideBeyond(bool on, int32_t distance)
{
	hideBeyondOn = on;
	hideBeyondDistance = distance > 0 ? distance : 0;
}

static std::vector<LevelViewShapeOf> hideBeyondKeeps;   // the objects and modules the distance rule never hides

void LevelView_HideBeyondKeep(const uint8_t *objectId)
{
	LevelViewShapeOf keep = {};

	if (objectId)
	{
		keep.isModule = false;
		memcpy(keep.objectId, objectId, 8);
	}

	LevelView_HideBeyondKeepShapes(&keep, objectId ? 1 : 0);
}

void LevelView_HideBeyondKeepShapes(const LevelViewShapeOf *shapes, size_t count)
{
	hideBeyondKeeps.assign(shapes, shapes + (shapes ? (count < kLevelViewMostHidden ? count : kLevelViewMostHidden) : 0));
}

void LevelView_HideObjects(const uint8_t (*ids)[8], size_t count, bool everyObject)
{
	hiddenIds.clear();
	hideEveryObject = everyObject;

	for (size_t i = 0; ids && i < count && i < kLevelViewMostHidden; i++)
	{
		hiddenIds.push_back(std::vector<uint8_t>(ids[i], ids[i] + 8));
	}
}

static std::vector<LevelViewHighlight> highlightedItems;

void LevelView_SetHighlightedItems(const LevelViewHighlight *items, size_t count)
{
	highlightedItems.clear();

	if (items && count)
	{
		highlightedItems.assign(items, items + (count < kLevelViewMostHighlighted ? count : kLevelViewMostHighlighted));
	}
}

bool LevelView_ItemCorners(const LevelViewShapeOf &shape, std::vector<uint8_t> *corners, std::vector<int32_t> *points)
{
	if (!corners)
	{
		return false;
	}

	corners->clear();

	if (points)
	{
		points->clear();
	}

	const DISPLAYBLOCK *const block = BlockOfShape(shape);
	SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

	if (!header || !header->items || header->numitems <= 0 || header->numitems > (1 << 20))
	{
		return false;
	}

	for (int j = 0; j < header->numitems; j++)
	{
		int count = 0;

		for (int k = 0; k < 5; k++)
		{
			if (header->items[j][4 + k] != -1)
			{
				count++;
			}

			if (points)
			{
				points->push_back(header->items[j][4 + k]);
			}
		}

		corners->push_back((uint8_t)count);
	}

	return true;
}

// The camera becomes the player's eyes. The game turns the player the same way, from an angle around the up axis
// (dynamics.cpp:980 copies the matrix CreateEulerMatrix makes), and keeps the look up or down apart, in ViewPanX, which
// view.cpp's UpdateCamera adds on.
static void PutTheEyes(const FlyCamera &camera)
{
	EULER turn = { 0, camera.turn & wrap360, camera.roll & wrap360 };
	CreateEulerMatrix(&turn, &Player->ObMat);

	// the orbit's matrix whole, when the editor gives one (LevelView_SetViewMatrix): the look is then 0, so the view is that matrix (UpdateCamera multiplies by the look's turn)
	if (viewMatrixOn)
	{
		Player->ObMat = viewMatrix;
	}

	PLAYER_STATUS *const playerStatus = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);

	// The game puts its eyes ABOVE the player: UpdateCamera/InteriorType_Body (view.cpp:448-501) adds the marine's eye height (CollisionExtents StandingTop, about 1750 mm up, plus 200 and the vertical speed over 16, which is 0 for
	// a player who is not moving) turned by the player's matrix to the player's place. inteng's camera IS the eye (its camera files, its Camera Root), so the player is put where that offset leaves the eyes AT the camera:
	// the view then starts from the camera's own x y z, as inteng's does, and the hierarchy's HUD view (the Camera Root) is seen from where inteng sees it. (Frost, 2026-11-16: found because the HUD gun view was 1750 mm too high.)
	VECTORCH eyeOffset;
	COLLISION_EXTENTS const *const extents = &CollisionExtents[AvP.PlayerType == I_Alien ? CE_ALIEN : (AvP.PlayerType == I_Predator ? CE_PREDATOR : CE_MARINE)];

	eyeOffset.vx = 0;
	eyeOffset.vy = (playerStatus->ShapeState == PMph_Standing ? extents->StandingTop : extents->CrouchingTop) + 200;
	eyeOffset.vz = 0;
	RotateVector(&eyeOffset, &Player->ObMat);

	Player->ObWorld.vx = camera.x - eyeOffset.vx;
	Player->ObWorld.vy = camera.y - eyeOffset.vy;
	Player->ObWorld.vz = camera.z - eyeOffset.vz;
	playerStatus->ViewPanX = viewMatrixOn ? 0 : camera.look & wrap360;
}

// The objects the editor hides, their blocks found now (an ID of no object in play is left out), and the view distance's rule measured from camera's place.
// The split screen calls it for each view with that view's own camera, as inteng draws each view from its own place (Drake 11-19: with the main camera 25 m
// away, the objects round the hierarchy the three views looked at were hidden in them).
static void PutTheHiddenBlocks(const FlyCamera &camera)
{
	std::vector<const struct displayblock *> hidden(hiddenBlocksAsGiven.begin(), hiddenBlocksAsGiven.end());

	for (std::vector<uint8_t> const &id : hiddenIds)
	{
		LevelViewShapeOf shape = {};

		shape.isModule = false;
		memcpy(shape.objectId, id.data(), 8);

		if (const DISPLAYBLOCK *const block = BlockOfShape(shape))
		{
			hidden.push_back(block);
		}
	}

	if (flyOn)
	{
		LevelViewShapeOf flying = {};

		flying.isModule = false;
		memcpy(flying.objectId, flyObjectId, 8);

		if (const DISPLAYBLOCK *const block = BlockOfShape(flying))
		{
			hidden.push_back(block);
		}
	}

	std::vector<int32_t> allHiddenModules = hiddenModules;

	allHiddenModules.insert(allHiddenModules.end(), ruleModules.begin(), ruleModules.end());

	if (insetDrawing)
	{
		allHiddenModules.insert(allHiddenModules.end(), previewHidden.begin(), previewHidden.end());   // (the preview's set: the inset's view only, LevelView_Inset)
	}

	for (int32_t const module : allHiddenModules)
	{
		LevelViewShapeOf shape = {};

		shape.isModule = true;
		shape.moduleIndex = module;

		if (const DISPLAYBLOCK *const block = BlockOfShape(shape))
		{
			hidden.push_back(block);
		}
	}

	if (hideBeyondOn)
	{
		std::vector<const DISPLAYBLOCK *> kept;

		for (LevelViewShapeOf const &keep : hideBeyondKeeps)
		{
			if (const DISPLAYBLOCK *const block = BlockOfShape(keep))
			{
				kept.push_back(block);
			}
		}

		auto beyond = [&](const DISPLAYBLOCK *block)
		{
			if (!block || std::find(kept.begin(), kept.end(), block) != kept.end())
			{
				return;
			}

			double const dx = (double)block->ObWorld.vx - camera.x, dy = (double)block->ObWorld.vy - camera.y, dz = (double)block->ObWorld.vz - camera.z;

			if (sqrt(dx * dx + dy * dy + dz * dz) - block->ObRadius >= hideBeyondDistance)
			{
				hidden.push_back(block);
			}
		};

		SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

		for (int m = 0; scene && m < ModuleArraySize; m++)
		{
			if (scene->sm_marray[m])
			{
				beyond(scene->sm_marray[m]->m_dptr);
			}
		}

		for (int i = 0; i < NumActiveBlocks; i++)
		{
			if (ActiveBlockList[i] && ActiveBlockList[i]->ObStrategyBlock && !ActiveBlockList[i]->ObMyModule)
			{
				beyond(ActiveBlockList[i]);
			}
		}
	}

	R_SetEditorHiddenBlocks(hidden.data(), (int)hidden.size(), hideEveryObject || ruleEveryObject || (insetDrawing && previewHiding), hideEveryModule);
}

bool LevelView_Draw(const FlyCamera &camera, const char *savePictureTo, void (*addToFrame)(void *context), void *context)
{
	if (!levelIsOpen)
	{
		return savePictureTo == nullptr;
	}

	// The camera becomes the player's eyes (PutTheEyes)
	PutTheEyes(camera);

	// winmain.cpp:1104-1172, without UpdateGame and the parts of a game (the HUD, the menus, the music): one view of the world
	const DISPLAYBLOCK *selectedBlocks[kLevelViewMostSelected];
	size_t const selectedBlockCount = SelectedBlocks(selectedBlocks);

	selectedBlockInPlay = selectedBlockCount != 0;
	// The selected objects in green, and the modules the editor gave colours to in theirs (a module not in play, or a number the game's list doesn't have, is left out)
	{
		const DISPLAYBLOCK *tintBlocks[kLevelViewMostSelected];
		unsigned int tintColours[kLevelViewMostSelected];
		size_t tinted = 0;

		// the object that flies is drawn as its live copy (the real block is left out), so the selected colour goes to the COPY, which moves; the real block stays where the file has it
		const DISPLAYBLOCK *flyOriginal = nullptr;

		if (flyOn && flyCopy)
		{
			LevelViewShapeOf flying = {};

			flying.isModule = false;
			memcpy(flying.objectId, flyObjectId, 8);
			flyOriginal = BlockOfShape(flying);
		}

		// a selection colour of 0: the selected objects are not tinted (Strategy Data while a kind's Edit view runs: FUN_005286b0 clears the current object's red mark first)
		for (size_t i = 0; selectedTintColour != 0 && i < selectedBlockCount && tinted < kLevelViewMostSelected; i++)
		{
			tintBlocks[tinted] = flyOriginal && selectedBlocks[i] == flyOriginal ? flyCopy : selectedBlocks[i];
			tintColours[tinted++] = selectedTintColour;
		}

		SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

		// the current module first, in its colour (purple 0x7F007F, or the screen's: LevelView_SetCurrentModuleColour), so it wins over any other colour given for it
		if (scene && currentModule >= 0 && currentModule < ModuleArraySize && scene->sm_marray[currentModule] && scene->sm_marray[currentModule]->m_dptr && tinted < kLevelViewMostSelected)
		{
			tintBlocks[tinted] = scene->sm_marray[currentModule]->m_dptr;
			tintColours[tinted++] = currentModuleColour;
		}

		for (size_t i = 0; scene && i < moduleTintCount && tinted < kLevelViewMostSelected; i++)
		{
			if (moduleTintIndex[i] >= 0 && moduleTintIndex[i] < ModuleArraySize && scene->sm_marray[moduleTintIndex[i]] && scene->sm_marray[moduleTintIndex[i]]->m_dptr)
			{
				tintBlocks[tinted] = scene->sm_marray[moduleTintIndex[i]]->m_dptr;
				tintColours[tinted++] = moduleTintColour[i];
			}
		}

		// the placed objects the editor gave colours to (found by ID; one not in play is left out)
		for (size_t i = 0; i < objectTintColours.size() && tinted < kLevelViewMostSelected; i++)
		{
			LevelViewShapeOf shape = {};

			shape.isModule = false;
			memcpy(shape.objectId, objectTintIds.data() + i * 8, 8);

			if (const DISPLAYBLOCK *const block = BlockOfShape(shape))
			{
				tintBlocks[tinted] = block;
				tintColours[tinted++] = objectTintColours[i];
			}
		}

		R_SetEditorTintedBlocks(tintBlocks, tintColours, (int)tinted);
	}
	// The objects the editor hides, and the view distance's rule from this camera
	PutTheHiddenBlocks(camera);

	// The polygons the editor highlights: their blocks found now, with the items that are really in their shapes
	{
		std::vector<const struct displayblock *> blocks;
		std::vector<int> items;

		for (const LevelViewHighlight &one : highlightedItems)
		{
			const DISPLAYBLOCK *const block = BlockOfShape(one.shape);
			SHAPEHEADER *const header = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

			if (header && one.item >= 0 && one.item < header->numitems)
			{
				blocks.push_back(block);
				items.push_back(one.item);
			}
		}

		R_SetEditorHighlightedPolygons(blocks.data(), items.data(), (int)blocks.size(), 0xFFFFFF00u);
	}
	R_SetEditorDrawPolylines(true);   // a polygon whose picture isn't known is drawn as a white line, as inteng draws it (the game leaves it out)
	R_SetEditorFillFollowsWire(true);   // a wire draw mode is wire for every drawing of the frame, near polygons too (d3d_render.cpp)

	// The editor shows the whole level from wherever the camera is, outside every room too (the game draws only what is visible from the
	// camera's own module, so outside all of them it would draw nothing). The game drops the backs of polygons by itself, so from outside
	// the rooms are seen into through their walls, as in Blender.
	Module_ShowEveryModule(true);
	KRender_AllowManyModules(true);   // the game draws 400 modules at most; the biggest stock level has 608 (kzsort.cpp)
	ThisFramesRenderingHasBegun();

	// SPLIT SCREEN: each extra camera's world drawn into its own box first, with the lists drawn at once (R_EditorFlushWorld), then the main camera and its box as the frame goes on
	if (extraViewCount != 0 && mainBoxOn)
	{
		for (size_t i = 0; i < extraViewCount; i++)
		{
			PutTheEyes(extraViews[i].camera);
			PutTheHiddenBlocks(extraViews[i].camera);   // the view distance from this view's own place
			R_SetEditorMainViewport(true, extraViews[i].left, extraViews[i].top, extraViews[i].width, extraViews[i].height);
			AvpShowViews();
			R_EditorFlushWorld();
		}

		PutTheEyes(camera);
		PutTheHiddenBlocks(camera);   // and the main view's from its own again
		R_SetEditorMainViewport(true, mainBox[0], mainBox[1], mainBox[2], mainBox[3]);
	}

	AvpShowViews();

	// THE PLOT'S PREVIEW's INSET (LevelView_Inset), on the live plot's renderer calls (Frost's, renderer.h 2026-11-21): the main view's lists drawn NOW WITH ITS
	// WORLD LINES (slot 0: the modules the plot has made so far, the owner's "modules updating on the map as they are build"), into its box or the whole window, and
	// its eyes kept for after the frame. Then the inset's box alone is cleared, its colour black (the inside of inteng's dialog windows, DL-2) and its depth, so the
	// main view keeps its own everywhere else; then the inset's camera, its hidden set and its box: its lists go in the frame's last drawing, with the INSET'S lines
	// (slot 1: the module being planned), over the main view (its backdrop fills only its own box: FillTheBackdrop, a later view of the frame). No inset: the frame's
	// last drawing draws slot 0, as before the live plot
	bool const inset = insetOn && Global_VDB_Ptr;
	VECTORCH mainWorld = {};
	MATRIXCH mainMat = {}, mainHorizon = {};
	EULER mainEuler = {};

	insetDrawn = false;

	if (inset)
	{
		R_EditorFlushWorldWithLines(0);
		mainWorld = Global_VDB_Ptr->VDB_World;
		mainMat = Global_VDB_Ptr->VDB_Mat;
		mainHorizon = Global_VDB_Ptr->VDB_HorizonMat;
		mainEuler = Global_VDB_Ptr->VDB_MatrixEuler;
		auto const insetFrom = std::chrono::steady_clock::now();

		R_EditorClearBox(insetBox[0], insetBox[1], insetBox[2], insetBox[3], 0xFF000000u);
		insetDrawing = true;
		PutTheEyes(insetCamera);
		PutTheHiddenBlocks(insetCamera);
		R_SetEditorMainViewport(true, insetBox[0], insetBox[1], insetBox[2], insetBox[3]);
		AvpShowViews();
		R_SetEditorFinalLinesSlot(1);
		insetDrawing = false;
		insetDrawn = true;
		insetMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - insetFrom).count();
	}
	else
	{
		R_SetEditorFinalLinesSlot(0);
	}

	// What the editor puts over the level, in the game's own frame, as the game puts its HUD: added to the screen list, which the game draws at
	// the end of this frame, over everything, and which the picture below and the shown frame hold. Its places are in pixels (renderer.h).
	if (addToFrame)
	{
		R_SetScreenPlacesInPixels(true);
		addToFrame(context);
		R_SetScreenPlacesInPixels(false);
	}

	ThisFramesRenderingHasFinished();

	// the inset drawn: the eyes, the hidden set and the box are the main view's again (the next frame starts from them, and what reads the last frame's eyes reads them)
	if (inset)
	{
		Global_VDB_Ptr->VDB_World = mainWorld;
		Global_VDB_Ptr->VDB_Mat = mainMat;
		Global_VDB_Ptr->VDB_HorizonMat = mainHorizon;
		Global_VDB_Ptr->VDB_MatrixEuler = mainEuler;
		PutTheEyes(camera);
		PutTheHiddenBlocks(camera);
		R_SetEditorMainViewport(mainBoxOn, mainBox[0], mainBox[1], mainBox[2], mainBox[3]);
	}

	bool const saved = savePictureTo == nullptr || SaveThePicture(savePictureTo);
	FlipBuffers();
	FrameCounterHandler();
	return saved;
}

bool LevelView_DrawNoLevel(const char *savePictureTo, void (*addToFrame)(void *context), void *context)
{
	ThisFramesRenderingHasBegun();

	if (addToFrame)
	{
		R_SetScreenPlacesInPixels(true);
		addToFrame(context);
		R_SetScreenPlacesInPixels(false);
	}

	ThisFramesRenderingHasFinished();
	bool const saved = savePictureTo == nullptr || SaveThePicture(savePictureTo);
	FlipBuffers();
	FrameCounterHandler();
	return saved;
}

bool LevelView_EyePlace(int32_t place[3])
{
	if (!levelIsOpen || !Global_VDB_Ptr)
	{
		return false;
	}

	place[0] = Global_VDB_Ptr->VDB_World.vx;
	place[1] = Global_VDB_Ptr->VDB_World.vy;
	place[2] = Global_VDB_Ptr->VDB_World.vz;
	return true;
}

bool LevelView_CameraAxes(int forward[3], int right[3], int *down)
{
	if (!levelIsOpen || !Global_VDB_Ptr)
	{
		return false;
	}
	// After a frame, VDB_Mat is the matrix that turns the world into the view, not the camera's own: the frame sets it to the camera's
	// turn (view.cpp:605, 638) and then transposes it (PrepareVDBForShowView, view.cpp:806), and the game turns each object's place into
	// the view with it (view.cpp:681, RotateVector, a row times the matrix: maths.cpp:730-749). So the direction the camera looks along,
	// the one that becomes the view's (0, 0, 1), is the matrix's third COLUMN, and its right, the view's (1, 0, 0), the first. (The first
	// try build of 18:47 read the rows: those are the camera's turn the other way round, so W went the mirror way once the camera turned.
	// Found by reading Frost's click-picking, which uses the same matrix the right way.)
	MATRIXCH const &view = Global_VDB_Ptr->VDB_Mat;
	forward[0] = view.mat13;
	forward[1] = view.mat23;
	forward[2] = view.mat33;
	right[0] = view.mat11;
	right[1] = view.mat21;
	right[2] = view.mat31;

	// the camera's own down, the view's (0, 1, 0): the second column (the game's y points down)
	if (down)
	{
		down[0] = view.mat12;
		down[1] = view.mat22;
		down[2] = view.mat32;
	}

	return true;
}

bool LevelView_CameraMatrix(int32_t matrix[9])
{
	if (!levelIsOpen || !Global_VDB_Ptr)
	{
		return false;
	}

	// The camera's own turn is the view's matrix turned the other way round (level_view.cpp, LevelView_CameraAxes: the frame transposes it): its rows
	// are the camera's right, its down and its forward, as the game keeps an object's matrix (dynamics.cpp:1169)
	MATRIXCH const &view = Global_VDB_Ptr->VDB_Mat;
	matrix[0] = view.mat11; matrix[1] = view.mat21; matrix[2] = view.mat31;
	matrix[3] = view.mat12; matrix[4] = view.mat22; matrix[5] = view.mat32;
	matrix[6] = view.mat13; matrix[7] = view.mat23; matrix[8] = view.mat33;
	return true;
}

bool LevelView_CameraHeading(int32_t *heading)
{
	int32_t m[9];

	if (!LevelView_CameraMatrix(m))
	{
		return false;
	}

	MATRIXCH turn;
	EULER euler;

	turn.mat11 = m[0]; turn.mat12 = m[1]; turn.mat13 = m[2];
	turn.mat21 = m[3]; turn.mat22 = m[4]; turn.mat23 = m[5];
	turn.mat31 = m[6]; turn.mat32 = m[7]; turn.mat33 = m[8];
	MatrixToEuler(&turn, &euler);
	*heading = euler.EulerY & 4095;
	return true;
}

bool LevelView_CameraQuaternion(float q[4])
{
	int32_t m[9];

	if (!LevelView_CameraMatrix(m))
	{
		return false;
	}

	for (int variant = 0; variant < 2; variant++)
	{
		double r[3][3];

		for (int a = 0; a < 3; a++)
		{
			for (int b = 0; b < 3; b++)
			{
				r[a][b] = (variant == 0 ? m[a * 3 + b] : m[b * 3 + a]) / 65536.0;
			}
		}

		double w, x, y, z;
		double const trace = r[0][0] + r[1][1] + r[2][2];

		if (trace > 0.0)
		{
			double const s = sqrt(trace + 1.0) * 2.0;

			w = 0.25 * s;
			x = (r[2][1] - r[1][2]) / s;
			y = (r[0][2] - r[2][0]) / s;
			z = (r[1][0] - r[0][1]) / s;
		}
		else if (r[0][0] > r[1][1] && r[0][0] > r[2][2])
		{
			double const s = sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2.0;

			w = (r[2][1] - r[1][2]) / s;
			x = 0.25 * s;
			y = (r[0][1] + r[1][0]) / s;
			z = (r[0][2] + r[2][0]) / s;
		}
		else if (r[1][1] > r[2][2])
		{
			double const s = sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2.0;

			w = (r[0][2] - r[2][0]) / s;
			x = (r[0][1] + r[1][0]) / s;
			y = 0.25 * s;
			z = (r[1][2] + r[2][1]) / s;
		}
		else
		{
			double const s = sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2.0;

			w = (r[1][0] - r[0][1]) / s;
			x = (r[0][2] + r[2][0]) / s;
			y = (r[1][2] + r[2][1]) / s;
			z = 0.25 * s;
		}

		QUAT game;
		MATRIXCH back;

		game.quatx = (int)(x * 65536.0);
		game.quaty = (int)(y * 65536.0);
		game.quatz = (int)(z * 65536.0);
		game.quatw = (int)(w * 65536.0);
		QuatToMat(&game, &back);

		int32_t const got[9] = { back.mat11, back.mat12, back.mat13, back.mat21, back.mat22, back.mat23, back.mat31, back.mat32, back.mat33 };
		bool same = true;

		for (int i = 0; i < 9; i++)
		{
			same = same && abs(got[i] - m[i]) < 256;
		}

		if (same)
		{
			q[0] = (float)(-game.quatx / 65536.0);
			q[1] = (float)(-game.quaty / 65536.0);
			q[2] = (float)(-game.quatz / 65536.0);
			q[3] = (float)(game.quatw / 65536.0);
			return true;
		}
	}

	return false;
}

void LevelView_Close()
{
	if (!levelIsOpen)
	{
		return;
	}

	// what this file keeps of the level's own memory goes first: a flying object's copy, the blocks given to hide (the level is loaded again by LevelView_ShowFile, its blocks new)
	LevelView_FlyEnd();
	LevelView_SetGravity(false);   // the blocks Gravity chose go with the level
	LevelView_GhostEnd();
	hiddenBlocksAsGiven.clear();
	placementLightKept.clear();   // Object Placement's lighting: its kept words go with the shapes (a level opened next comes in with its own bits)
	placementLightOn = false;

	// winmain.cpp:1213-1231, without the steps that write the player's key settings, stop sound and music, or end a network game. Nor
	// does it let go of the player's image in a mirror (winmain.cpp:1217): the editor never makes one (src/host/edge_own.cpp).
	ReleaseAllFMVTextures();
	DeInitialisePlayer();
	Destroy_CurrentEnvironment();
	EndNPCs();
	ExitGame();
	ClearMemoryPool();
	levelIsOpen = false;
}

bool LevelView_ShowFile(const char *levelName, char *why, size_t whySize)
{
	if (!levelIsOpen || !openedName[0])
	{
		Say(why, whySize, "no level is open");
		return false;
	}

	FlyCamera ignored;   // the camera stays the editor's own

	showingAFile = true;
	bool const shown = LevelView_Open(levelName, &ignored, why, whySize);

	if (!shown)
	{
		// the game's loader refused the working bytes' copy: the opened file again, so the view has a level
		char said[400] = "";

		snprintf(said, sizeof(said), "%s", why ? why : "");
		LevelView_Open(openedName, &ignored, nullptr, 0);
		Say(why, whySize, said);
	}

	showingAFile = false;
	return shown;
}

const char *LevelView_SayMissingPictures(const char *otherwise)
{
	static std::string words;   // the status line points at it
	int missing = 0, lines = 0, pastLimit = 0;
	bool noList = false;

	R_EditorMissingPictures(&missing, &lines, &noList, &pastLimit);

	if (!noList && missing == 0 && lines == 0 && pastLimit == 0)
	{
		return otherwise;
	}

	if (noList)
	{
		words = "This level has no list of pictures (BMPNAMES): its " + std::to_string(lines) + " textured polygons are drawn as white lines, as inteng draws them; the file keeps "
			"their picture numbers";
	}
	else if (missing == 0 && pastLimit == 0)
	{
		// every named picture loaded, but some polygons give a picture number the list does not have
		words = std::to_string(lines) + " textured polygons give a picture number this level's list does not have: they are drawn as white lines, as inteng draws them; the "
			"file keeps their numbers";
	}
	else if (missing > 0)
	{
		words = std::to_string(missing) + " of this level's pictures could not be loaded (each is named in the log): their " + std::to_string(lines) + " polygons are drawn as "
			"white lines, as inteng draws them; the file keeps the names";
	}
	else
	{
		words = "This level's " + std::to_string(lines) + " polygons whose pictures were not loaded are drawn as white lines, as inteng draws them; the file keeps the names";
	}

	if (pastLimit > 0)
	{
		// the names past the 4096 one file may ask for (chunk_load.cpp): not loaded, and not named one by one
		words += "; " + std::to_string(pastLimit) + " more names are past the 4096 pictures one level file may ask for, so they were not loaded (the log says so once)";
	}

	LogString(words);
	return words.c_str();
}

size_t LevelView_SetBakedColours(const std::vector<LevelViewBaked> &modules, double scale)
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;
	size_t given = 0;

	if (!levelIsOpen || !scene || !(scale > 0.0) || scale > 1.0e9)
	{
		return 0;
	}

	for (int m = 0; m < ModuleArraySize; m++)
	{
		MODULE *const room = scene->sm_marray[m];

		if (!room || !room->m_dptr || !room->name || room->m_dptr->ObShape < 0)
		{
			continue;
		}

		SHAPEHEADER *const shape = GetShapeData(room->m_dptr->ObShape);

		if (!shape || !shape->sh_extraitemdata)
		{
			continue;
		}

		for (const LevelViewBaked &one : modules)
		{
			if (_stricmp(one.name.c_str(), room->name) != 0 ||
				fabs((double)one.location[0] * scale - room->m_world.vx) > 2.0 || fabs((double)one.location[1] * scale - room->m_world.vy) > 2.0 || fabs((double)one.location[2] * scale - room->m_world.vz) > 2.0)
			{
				continue;
			}

			// the places the game made for this shape's colours are the most it can take
			size_t const places = (size_t)(shape->numpoints > shape->sh_numextraitems ? shape->numpoints : shape->sh_numextraitems);
			size_t const count = one.colours.size() < places ? one.colours.size() : places;

			for (size_t v = 0; v < count; v++)
			{
				int const ir = (int)(one.colours[v] >> 16 & 0xff), ig = (int)(one.colours[v] >> 8 & 0xff), ib = (int)(one.colours[v] & 0xff);
				int const mag = (int)sqrt((ir * ir + ig * ig + ib * ib) / 3.0);

				shape->sh_extraitemdata[v].EID_VertexI = (int)(one.colours[v] & 0xffffff) + (mag << 24);
			}

			given++;
			break;
		}
	}

	StaticLevel_BuildThisLevel();
	return given;
}

bool LevelView_ModuleSite(int32_t moduleIndex, std::string *name, int32_t world[3])
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	if (!name || !world || !scene || moduleIndex < 0 || moduleIndex >= ModuleArraySize || !scene->sm_marray[moduleIndex] || !scene->sm_marray[moduleIndex]->name)
	{
		return false;
	}

	MODULE *const room = scene->sm_marray[moduleIndex];

	*name = std::string(room->name, strnlen(room->name, 255));
	world[0] = room->m_world.vx;
	world[1] = room->m_world.vy;
	world[2] = room->m_world.vz;
	return !name->empty();
}

bool LevelView_ModuleBox(int32_t moduleIndex, int32_t mins[3], int32_t maxs[3])
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;

	if (!mins || !maxs || !scene || moduleIndex < 0 || moduleIndex >= ModuleArraySize || !scene->sm_marray[moduleIndex] || !scene->sm_marray[moduleIndex]->m_dptr || scene->sm_marray[moduleIndex]->m_dptr->ObShape < 0)
	{
		return false;
	}

	MODULE *const room = scene->sm_marray[moduleIndex];
	SHAPEHEADER *const shape = GetShapeData(room->m_dptr->ObShape);

	if (!shape)
	{
		return false;
	}

	mins[0] = room->m_world.vx + shape->shapeminx; mins[1] = room->m_world.vy + shape->shapeminy; mins[2] = room->m_world.vz + shape->shapeminz;
	maxs[0] = room->m_world.vx + shape->shapemaxx; maxs[1] = room->m_world.vy + shape->shapemaxy; maxs[2] = room->m_world.vz + shape->shapemaxz;
	return true;
}

bool LevelView_ObjectBox(const uint8_t id[8], int32_t mins[3], int32_t maxs[3])
{
	if (!id || !mins || !maxs)
	{
		return false;
	}

	LevelViewShapeOf shape = {};

	shape.isModule = false;
	memcpy(shape.objectId, id, sizeof(shape.objectId));

	const DISPLAYBLOCK *const block = BlockOfShape(shape);
	SHAPEHEADER *const data = block && block->ObShape >= 0 ? GetShapeData(block->ObShape) : nullptr;

	if (!data)
	{
		return false;
	}

	mins[0] = block->ObWorld.vx + data->shapeminx; mins[1] = block->ObWorld.vy + data->shapeminy; mins[2] = block->ObWorld.vz + data->shapeminz;
	maxs[0] = block->ObWorld.vx + data->shapemaxx; maxs[1] = block->ObWorld.vy + data->shapemaxy; maxs[2] = block->ObWorld.vz + data->shapemaxz;
	return true;
}

bool LevelView_DoorsAreClosed()
{
	return doorsClosedInView;
}

size_t LevelView_SetDoorsClosed(bool closed)
{
	SCENEMODULE *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : nullptr;
	size_t exchanged = 0;

	if (!levelIsOpen || !scene || closed == doorsClosedInView)
	{
		return 0;
	}

	std::vector<std::pair<int, int>> done;

	for (int m = 0; m < ModuleArraySize; m++)
	{
		MODULE *const room = scene->sm_marray[m];

		if (!room || !room->m_dptr || !room->m_dptr->ObMorphCtrl || !room->m_dptr->ObMorphCtrl->ObMorphHeader)
		{
			continue;
		}

		MORPHHEADER *const header = room->m_dptr->ObMorphCtrl->ObMorphHeader;

		if (header->mph_numframes < 1 || !header->mph_frames)
		{
			continue;
		}

		int const first = header->mph_frames[0].mf_shape1;
		int const second = header->mph_frames[0].mf_shape2;

		if (first == second || first < 0 || second < 0)
		{
			continue;
		}

		bool seen = false;

		for (std::pair<int, int> const &one : done)
		{
			seen = seen || (one.first == first && one.second == second);
		}

		if (seen)
		{
			continue;
		}

		done.push_back(std::make_pair(first, second));

		SHAPEHEADER *const a = GetShapeData(first);
		SHAPEHEADER *const b = GetShapeData(second);

		if (!a || !b || a->numpoints != b->numpoints)
		{
			continue;
		}

		// the points and the vertex normals, and any of the six shape instructions that point at them
		int **const aPoints = a->points, **const bPoints = b->points, **const aNormals = a->sh_vnormals, **const bNormals = b->sh_vnormals;

		a->points = bPoints;
		b->points = aPoints;
		a->sh_vnormals = bNormals;
		b->sh_vnormals = aNormals;

		for (int i = 0; i < 6; i++)
		{
			if (a->sh_instruction)
			{
				a->sh_instruction[i].sh_instr_data = a->sh_instruction[i].sh_instr_data == aPoints ? bPoints : a->sh_instruction[i].sh_instr_data == aNormals ? bNormals : a->sh_instruction[i].sh_instr_data;
			}

			if (b->sh_instruction)
			{
				b->sh_instruction[i].sh_instr_data = b->sh_instruction[i].sh_instr_data == bPoints ? aPoints : b->sh_instruction[i].sh_instr_data == bNormals ? aNormals : b->sh_instruction[i].sh_instr_data;
			}
		}

		exchanged++;
	}

	doorsClosedInView = closed;
	StaticLevel_BuildThisLevel();
	return exchanged;
}

// ---- THE ENGINE'S OWN DYNAMICS ON CHOSEN BLOCKS (level_view.h: Gravity OP-47, a flight's collisions OP-117 / OP-118; Drake 2026-11-17, Bishop's ruling: the game's code, no physics of ours) ----
extern int NormalFrameTime;          // io.cpp:74: how long the last frame took, 65536 = one second (FrameCounterHandler sets it each frame)
extern void ObjectDynamics(void);   // dynamics.cpp:293: every strategy block with a dynamics block that is not static is moved by gravity and collided with the level

namespace
{
	struct ChosenBlock
	{
		STRATEGYBLOCK *sb;
		unsigned isStatic;
		unsigned gravityOn;
		VECTORCH start;
	};

	std::vector<ChosenBlock> gravityChosen;   // the blocks Gravity gave the engine (their flags as they were), empty when it is off
	bool gravityOnNow = false;

	// inteng's FUN_004e3e30 (a placed shape's +0x50): its engine moves the placed shapes of type 1, 2, 3, 4, 5 and 15 (+0x44, the SimpleObject's type), which the game makes inanimate objects of
	bool FallsWithGravity(STRATEGYBLOCK const *sb)
	{
		if (!sb || !sb->DynPtr || !sb->SBdptr || sb->I_SBtype != I_BehaviourInanimateObject || !sb->SBdataptr)
		{
			return false;
		}

		INANIMATEOBJECT_TYPE const type = ((INANIMATEOBJECT_STATUSBLOCK const *)sb->SBdataptr)->typeId;

		return type == IOT_Furniture || type == IOT_Weapon || type == IOT_Ammo || type == IOT_Health || type == IOT_Armour || type == IOT_FieldCharge;
	}

	// ObjectDynamics reads the player's dynamics block on every call (its log line), and the level's lists must be the game's
	bool DynamicsCanRun()
	{
		return levelIsOpen && Player && Player->ObStrategyBlock && Player->ObStrategyBlock->DynPtr && NumActiveStBlocks >= 0;
	}

	STRATEGYBLOCK *StrategyBlockOf(const uint8_t *id)
	{
		for (int i = 0; id && i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const sb = ActiveStBlockList[i];

			if (sb && memcmp(sb->SBname, id, 8) == 0)
			{
				return sb;
			}
		}

		return nullptr;
	}

	// One call of the game's ObjectDynamics with every block that is not `moving` held static for it (its IsStatic set, then put back). The game copies a static block's dynamics place into
	// its display block (UpdateDisplayBlockData); the editor places blocks without their dynamics (the player's body, a block the view put somewhere), so each held block's display place
	// and matrix are put back as they were too: only the moving ones change
	struct HeldBlock
	{
		DYNAMICSBLOCK *dyn;
		unsigned isStatic;
		DISPLAYBLOCK *shown;
		VECTORCH world;
		MATRIXCH matrix;
	};

	void RunDynamicsOn(bool (*moving)(STRATEGYBLOCK const *sb, void const *context), void const *context)
	{
		std::vector<HeldBlock> held;

		for (int i = 0; i < NumActiveStBlocks; i++)
		{
			STRATEGYBLOCK *const sb = ActiveStBlockList[i];

			if (sb && sb->DynPtr && !moving(sb, context))
			{
				HeldBlock h = { sb->DynPtr, (unsigned)sb->DynPtr->IsStatic, sb->SBdptr, {}, {} };

				if (sb->SBdptr)
				{
					h.world = sb->SBdptr->ObWorld;
					h.matrix = sb->SBdptr->ObMat;
				}

				held.push_back(h);
				sb->DynPtr->IsStatic = 1;
			}
		}

		ObjectDynamics();

		for (HeldBlock const &h : held)
		{
			h.dyn->IsStatic = h.isStatic;

			if (h.shown)
			{
				h.shown->ObWorld = h.world;
				h.shown->ObMat = h.matrix;
			}
		}
	}

	bool IsGravityChosen(STRATEGYBLOCK const *sb, void const *)
	{
		for (ChosenBlock const &c : gravityChosen)
		{
			if (c.sb == sb)
			{
				return true;
			}
		}

		return false;
	}

	bool IsThisBlock(STRATEGYBLOCK const *sb, void const *context)
	{
		return sb == context;
	}
}

bool LevelView_SetGravity(bool on)
{
	if (!on)
	{
		for (ChosenBlock const &c : gravityChosen)
		{
			c.sb->DynPtr->IsStatic = c.isStatic;
			c.sb->DynPtr->GravityOn = c.gravityOn;
			c.sb->DynPtr->LinVelocity.vx = c.sb->DynPtr->LinVelocity.vy = c.sb->DynPtr->LinVelocity.vz = 0;
			c.sb->DynPtr->LinImpulse.vx = c.sb->DynPtr->LinImpulse.vy = c.sb->DynPtr->LinImpulse.vz = 0;
		}

		gravityChosen.clear();
		gravityOnNow = false;
		return true;
	}

	if (!DynamicsCanRun())
	{
		return false;
	}

	gravityChosen.clear();

	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK *const sb = ActiveStBlockList[i];

		if (FallsWithGravity(sb))
		{
			// the engine starts from where the view draws the object (an editor move, a flight's stop or Move relative, sets the display block; its dynamics block may still hold the load's place)
			sb->DynPtr->Position = sb->SBdptr->ObWorld;
			sb->DynPtr->PrevPosition = sb->SBdptr->ObWorld;
			sb->DynPtr->OrientMat = sb->SBdptr->ObMat;
			sb->DynPtr->PrevOrientMat = sb->SBdptr->ObMat;

			ChosenBlock c = { sb, (unsigned)sb->DynPtr->IsStatic, (unsigned)sb->DynPtr->GravityOn, sb->DynPtr->Position };

			gravityChosen.push_back(c);
			sb->DynPtr->IsStatic = 0;
			sb->DynPtr->GravityOn = 1;
		}
	}

	gravityOnNow = true;
	return true;
}

void LevelView_GravityStep()
{
	if (gravityOnNow && !gravityChosen.empty() && DynamicsCanRun())
	{
		RunDynamicsOn(IsGravityChosen, nullptr);
	}
}

void LevelView_GravityMoved(std::vector<LevelViewMovedObject> *moved)
{
	if (!moved)
	{
		return;
	}

	moved->clear();

	for (ChosenBlock const &c : gravityChosen)
	{
		VECTORCH const now = c.sb->DynPtr->Position;

		if (now.vx == c.start.vx && now.vy == c.start.vy && now.vz == c.start.vz)
		{
			continue;
		}

		LevelViewMovedObject one;
		MATRIXCH const &m = c.sb->DynPtr->OrientMat;
		int32_t const rows[9] = { m.mat11, m.mat12, m.mat13, m.mat21, m.mat22, m.mat23, m.mat31, m.mat32, m.mat33 };

		memcpy(one.id, c.sb->SBname, 8);
		one.world[0] = now.vx;
		one.world[1] = now.vy;
		one.world[2] = now.vz;
		memcpy(one.matrix, rows, sizeof(rows));
		moved->push_back(one);
	}
}

bool LevelView_FlyCollideMove(const uint8_t *id, const int32_t from[3], const int32_t to[3], int32_t reached[3])
{
	reached[0] = to[0];
	reached[1] = to[1];
	reached[2] = to[2];

	STRATEGYBLOCK *const sb = DynamicsCanRun() ? StrategyBlockOf(id) : nullptr;

	if (!sb || !sb->DynPtr || NormalFrameTime <= 0)
	{
		return false;
	}

	DYNAMICSBLOCK *const d = sb->DynPtr;
	unsigned const wasStatic = d->IsStatic, wasGravity = d->GravityOn, wasPickup = d->IsPickupObject;
	VECTORCH const wasVelocity = d->LinVelocity;

	// the move asked for this frame as the engine's impulse: ObjectDynamics moves the block by (velocity + impulse) x the frame's time (dynamics.cpp), so the impulse is the move per second
	d->Position.vx = from[0];
	d->Position.vy = from[1];
	d->Position.vz = from[2];
	d->PrevPosition = d->Position;
	d->LinVelocity.vx = d->LinVelocity.vy = d->LinVelocity.vz = 0;
	d->LinImpulse.vx = (int)(((long long)(to[0] - from[0]) << 16) / NormalFrameTime);
	d->LinImpulse.vy = (int)(((long long)(to[1] - from[1]) << 16) / NormalFrameTime);
	d->LinImpulse.vz = (int)(((long long)(to[2] - from[2]) << 16) / NormalFrameTime);
	d->IsStatic = 0;
	d->GravityOn = 0;
	d->IsPickupObject = 0;   // ObjectDynamics skips a pickup whose gravity is off (dynamics.cpp: "IsPickupObject && !GravityOn"): for this call it is an object like any other

	RunDynamicsOn(IsThisBlock, sb);

	reached[0] = d->Position.vx;
	reached[1] = d->Position.vy;
	reached[2] = d->Position.vz;
	d->IsStatic = wasStatic;
	d->GravityOn = wasGravity;
	d->IsPickupObject = wasPickup;
	d->LinVelocity = wasVelocity;
	d->LinImpulse.vx = d->LinImpulse.vy = d->LinImpulse.vz = 0;
	return true;
}

bool LevelView_FlyBoxGrow(const uint8_t *id, bool grow)
{
	STRATEGYBLOCK *const sb = levelIsOpen ? StrategyBlockOf(id) : nullptr;

	if (!sb || !sb->SBdptr)
	{
		return false;
	}

	int const by = grow ? 2000 : -2000;
	DISPLAYBLOCK *const b = sb->SBdptr;

	b->ObMaxX += by;
	b->ObMinX -= by;
	b->ObMaxY += by;
	b->ObMinY -= by;
	b->ObMaxZ += by;
	b->ObMinZ -= by;
	return true;
}

// The state line's "levelmemory" (level_view.h): what the open level took of its 40 blocks of memory once it had opened, in MB. Empty when no level is open.
std::string LevelView_MemoryWords()
{
	if (!levelIsOpen)
	{
		return std::string();
	}

	char words[160];
	double const mb = 1024.0 * 1024.0;

	_snprintf(words, sizeof(words) - 1, "taken %d of %d left %.2f handed %.2f outside %.2f", memoryAtOpen.blocksTaken, memoryAtOpen.blocksMost, memoryAtOpen.leftInBlock / mb,
		memoryAtOpen.bytesHandedOut / mb, memoryAtOpen.extraBytes / mb);
	words[sizeof(words) - 1] = '\0';
	return words;
}

// The status line's level memory at open (level_view.h): after what it already says, so a missing-pictures or video-screens line is never lost
const char *LevelView_SayMemory(const char *otherwise)
{
	static std::string said;   // the status line points at it

	if (!levelIsOpen)
	{
		return otherwise;
	}

	char words[200];
	double const mb = 1024.0 * 1024.0;

	// outside: past the 40th block, or one request larger than a block (mempool.cpp PoolAllocateMem); Rebellion's game writes past its memory in both
	if (memoryAtOpen.extraBytes > 0)
	{
		_snprintf(words, sizeof(words) - 1, "Level memory: %d of %d blocks taken and %.2f MB more outside them, where Rebellion's game would write past its memory",
			memoryAtOpen.blocksTaken, memoryAtOpen.blocksMost, memoryAtOpen.extraBytes / mb);
	}
	else
	{
		double const freeMb = (memoryAtOpen.blocksMost - memoryAtOpen.blocksTaken) + memoryAtOpen.leftInBlock / mb;
		_snprintf(words, sizeof(words) - 1, "Level memory: %d of %d blocks taken, %.2f MB free", memoryAtOpen.blocksTaken, memoryAtOpen.blocksMost, freeMb);
	}

	words[sizeof(words) - 1] = '\0';
	said = (otherwise && *otherwise) ? std::string(otherwise) + "; " + words : std::string(words);
	return said.c_str();
}
