// See view_toggles_game.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". (Modified 2026-11-14 by Melanikus (Frost): the headlight; R-DRAWMODES-1's three outline switches; the
// flat shade mode taken out again, as inteng's own modes have none (view-state-table.md).)

#include "3dc.h"
#include "inline.h"
#include "gamedef.h"        // Player
#include "gammacontrol.h"   // InitialiseGammaSettings

#include "view_toggles_game.h"

// The game's own switches and lists, declared as the game's files declare them for themselves: no header has them (DrawFullBright at
// kshape.cpp:147, WireFrameMode at d3d_render.cpp:183, and the list of every block in play at object.cpp, which bh_death_volume.cpp
// declares the same way)
extern int DrawFullBright;
extern int WireFrameMode;
extern int NumActiveBlocks;
extern DISPLAYBLOCK *ActiveBlockList[];

// R-DRAWMODES-1's three switches (d3d_render.cpp, declared in its d3d_render.h, which needs more of the game than this file includes):
// the outlines over the solid polygons, their colour (0xRRGGBB, only the low 24 bits read) and whether that colour is in the level's own
// light. Added 2026-11-14 by Melanikus (Frost), now the editor's avpx carries them.
extern int DrawFilledOutlines;
extern int DrawFilledOutlinesColour;
extern int DrawFilledOutlinesLit;
extern float DrawFilledOutlinesDepthBias;   // how far toward the camera the outlines are pulled (d3d_render.cpp)

namespace
{
	// The gamma last given to the game, or -1 for none yet, so the first frame sets it
	int appliedGamma = -1;

	// Whether the player's block is in play now: one of the game's active blocks. The game never clears Player when a level closes, but
	// closing takes the block out of the active list and its lights with it (DestroyActiveObject, object.cpp:172), so this is the game's
	// own record of whether Player is a live block.
	bool PlayerIsInPlay()
	{
		if (!Player)
		{
			return false;
		}

		for (int i = 0; i < NumActiveBlocks; i++)
		{
			if (ActiveBlockList[i] == Player)
			{
				return true;
			}
		}

		return false;
	}

	// The headlight for this frame: a light of the same kind as the game's own muzzle flash (LFX_MUZZLEFLASH, lighting.cpp:40), lighting
	// all round (Omni) and lighting each corner (PerVertex), put on the player, whose eyes the camera is. The game itself puts it where
	// the player is (UpdateObjectLights(Player), view.cpp:767) and takes it away when the frame is finished (LightBlockDeallocation,
	// d3d_render.cpp:3235, for every light marked Deallocate on an active block). So it's made again each frame, nothing here keeps hold
	// of it, and there's never more than the game's own limits allow: AddLightBlock gives nothing when the player's 50 lights
	// (MaxObjectLights) or the game's store of lights are full.
	void AddTheHeadlight()
	{
		if (!PlayerIsInPlay())
		{
			return;
		}

		LIGHTBLOCK *const light = AddLightBlock(Player, NULL);

		if (!light)
		{
			return;
		}

		// A new light comes cleared (AllocateLightBlock, object.cpp:368), so only these are set, as the muzzle flash sets them
		light->LightBright = host::kHeadlightBrightness;
		light->LightFlags = LFlag_Omni | LFlag_Deallocate;
		light->LightType = LightType_PerVertex;
		light->LightRange = host::kHeadlightRange;
		light->RedScale = host::kHeadlightColour;
		light->GreenScale = host::kHeadlightColour;
		light->BlueScale = host::kHeadlightColour;
	}
}

extern void R_SetEditorUntextured(bool on, unsigned int colour);   // avpx renderer.h (RenderList.cpp): the editor's Untextured

static bool paintWhite = false;
static bool mixed = false;
extern void R_SetEditorMixed(bool on);   // avpx renderer.h (d3d_render.cpp)

void ViewToggles_SetMixed(bool on)
{
	mixed = on;
}

void ViewToggles_SetPaintWhite(bool on)
{
	paintWhite = on;
}

static bool moduleWire = false;

void ViewToggles_SetModuleWire(bool on)
{
	moduleWire = on;
}

static bool screenNoTextures = false;

void ViewToggles_SetScreenNoTextures(bool on)
{
	screenNoTextures = on;
}

void ViewToggles_Apply()
{
	// A screen whose start loaded no textures (load flag 0x80: the hub, Module Linking, Adjacent Modules, Path, Waypoint) draws without them whatever the Textured switch says: its
	// Filled polyline is the hidden-line look (black faces, white edges: inteng's kind 0x10 in white, FUN_0046dae0), its Polygon is white and LIT (kind 2 with flag 2 cleared,
	// FUN_0046db20: Module Linking's View Mode > Filled 0x48).
	host::ViewToggles now = ViewToggles_Now();

	if (screenNoTextures)
	{
		now.textured = false;
	}

	host::GameViewSwitches switches = host::ViewToggles_GameSwitches(now, ViewToggles_GameCanShow());

	if (screenNoTextures && now.drawMode == host::DrawMode::Polygon)
	{
		switches.drawFullBright = 0;
	}

	// STRATEGY DATA's No Textures (0x3a: FUN_00524cc0 -> FUN_00522ab0 -> FUN_0046e700(0x93)) is a WHITE WIREFRAME, the Module Linking look: with load flag 0x80 no texture is loaded (FUN_0046e700 0x46ed57: the table is NULL), so the
	// shape converter makes every textured polygon kind 0xe, a see-through line, with flag 2, unlit (FUN_004ff010 0x4ffc53-0x4ffc62), and FUN_00522ab0 0x522bca paints every level shape 0xffffff; the strategy colours (green has a
	// strategy, red the current object) colour the lines. NAMED DIFFERENCE: inteng leaves the few untextured polygons (kinds 2 / 3) filled white; the game's wire switch is per category, so ours draws them as lines too.
	bool const wire = moduleWire || paintWhite;

	DrawFullBright = wire ? 1 : switches.drawFullBright;
	R_SetEditorUntextured(switches.untextured != 0 || wire, wire ? host::kUntexturedColour : switches.untexturedColour);
	WireFrameMode = wire ? host::kWireFrameEverything : switches.wireFrameMode;

	// R-DRAWMODES-1's outlines: Polyline's and Filled polyline's white lines. Set every frame, like the others, so leaving a mode takes its
	// outlines away on the very next frame.
	DrawFilledOutlines = wire ? 0 : switches.filledOutlines;   // the game's own lines carry the colours; white outlines over them would cover them
	DrawFilledOutlinesColour = mixed && switches.filledOutlines ? 0xFF7F00 : switches.outlineColour;
	R_SetEditorMixed(mixed);
	DrawFilledOutlinesLit = switches.outlinesLit;
	DrawFilledOutlinesDepthBias = switches.outlineDepthBias;

	if (switches.gamma != appliedGamma)
	{
		InitialiseGammaSettings(switches.gamma);
		appliedGamma = switches.gamma;
	}

	if (switches.headlight)
	{
		AddTheHeadlight();
	}

	// switches.drawBackFaces goes to the game's DrawBackFaces once the editor's avpx carries R-BACKFACES-1; until then it's always 0, and
	// there's nothing in the game to set
}

void ViewToggles_LevelOpenedAgain()
{
	appliedGamma = -1;   // LevelView_Open gave the game its own gamma (128): the next frame gives it the toggles' again
}
