// The level view's draw mode and switches, as plain data a test can drive: the draw mode, the Gouraud switch, back faces, the headlight
// and brightness. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code". (Modified 2026-11-14 by Melanikus (Frost): reshaped to the old inteng's own, AvP-docs\engine\inteng\view-state-table.md,
// from Vasquez's reads of inteng, vasquez-view-map.md. The old inteng is the spec: Melanikus, 21:44, "it just needs to work now as it
// did then". Where a row says "ours", it's his request or not read yet, and says which.)
//
// Each is the game's own switch, or a light of the game's own kind, set by the editor, never a new way of drawing (the editor never edits
// AvPx). The table's rows, with their sources:
// - THE DRAW MODE, one value, so choosing one replaces the other, and choosing the one already on changes nothing (table section 1):
//   - Polygon (inteng's 7, I_Polygon): the polygons solid, as the game draws them.
//   - Polyline (8, I_Polyline): every polygon, textured ones too, as a WHITE line. The game draws its own lines textured and lit, so these
//     are its lines (WireFrameMode 3, d3d_render.cpp:183) with R-DRAWMODES-1's outlines over the same edges in white (DrawFilledOutlines,
//     DrawFilledOutlinesColour), which covers them.
//   - Filled polyline (9, I_Wireframe): the polygons solid, with the same white outlines over them.
//   - Wireframe, textured (ours, his request, 21:30): the game's own lines, each in its texture's colour.
//   Polyline and Filled polyline need the game's outlines (R-DRAWMODES-1): while the editor's avpx can't show them, choosing either
//   changes nothing, and their menu lines are off with the reason.
// - THE GOURAUD SWITCH, kept across every mode change, because it isn't part of the mode (section 2, inteng's DAT_00914d6c): Off draws
//   textures unshaded at 100%, the game's DrawFullBright (kshape.cpp:147); On shades them in the level's own light. This is what lit and
//   unlit are for every mode, and inteng has no separate Fully lit. The white lines of Polyline and of Filled polyline are hard white with it
//   Off (inteng's own 8), and white in the level's own light with it On (ours, his request, 2026-11-15: DrawFilledOutlinesLit). inteng has a third value, 2, which shades only the level's own gouraud polygons: it isn't here, because
//   its command isn't found yet and the game has nothing to show it with.
// - BACK FACES: the polygons facing away, which the game drops before drawing (frustum.cpp:916). It needs the game's DrawBackFaces, which
//   comes with AvPx's R-BACKFACES-1; until the editor's avpx carries it, back faces can't be shown, so it stays off. (Ours, unproven:
//   inteng's only back-face control is the per-polygon no_bfc flag.)
// - THE HEADLIGHT (inteng's Ctrl+B, "requires gouraud polygons"; its code isn't read yet): a light that goes with the camera, the game's
//   own kind of light, the one its muzzle flash is (lighting.cpp:40), put on the player for each frame (view_toggles_game.cpp). With the
//   Gouraud switch Off there's nothing for it to light: everything is at 100% already.
// - BRIGHTNESS (ours, unproven): the game's gamma (InitialiseGammaSettings, gammacontrol.cpp), 0 to 255 as its own slider allows
//   (menu_data.cpp:337), 128 to start with as a new player's (user_profile.cpp:435), in steps of kBrightnessStep.
// Every value set is held to the range the game allows, whatever the data says, so nothing out of range ever reaches the game.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace host
{
	int32_t const kBrightnessLeast = 0;
	int32_t const kBrightnessMost = 255;
	int32_t const kBrightnessAsBuilt = 128;
	int32_t const kBrightnessStep = 16;

	// The game's WireFrameMode for everything drawn as lines, and for none
	int const kWireFrameEverything = 3;
	int const kWireFrameOff = 0;

	// THE HEADLIGHT'S NUMBERS, to be tuned on Melanikus's retest. How far it reaches, in the game's millimetres (the muzzle flash reaches
	// 10 metres); how bright it is, in the game's whole-number sums where 65536 is 1 (the muzzle flash is between a half and 1); and its
	// colour, each of red, green and blue in 256ths of 255, as the game's own lights give theirs (255 * 256 is all of it): white.
	int32_t const kHeadlightRange = 20000;
	int32_t const kHeadlightBrightness = 65536;
	int32_t const kHeadlightColour = 255 * 256;

	// The wire lines' colour, 0xRRGGBB: inteng's white (palette 0xf8; FUN_0055e240 and FUN_0055e110, vasquez-view-map 1; Melanikus, 21:46:
	// "White, as inteng"), its only line colour
	int32_t const kWireWhite = 0xFFFFFF;

	enum class DrawMode
	{
		Polygon,          // inteng's 7
		Polyline,         // inteng's 8: white lines
		FilledPolyline,   // inteng's 9: the polygons with white lines
		WireTextured,     // ours, his request: the game's own lines in their textures' colour
	};

	// inteng's DAT_00914d6c, its values as inteng numbers them (its 2 isn't here: see above)
	enum class Gouraud
	{
		Off = 0,          // textures at 100%, unshaded
		On = 1,           // shaded in the level's own light
	};

	enum class ViewToggle
	{
		Polygon,          // picks Polygon (7)
		Polyline,         // picks Polyline (8), once the game can show its lines
		FilledPolyline,   // picks Filled polyline (9), once the game can show its lines
		WireTextured,     // picks the textured wireframe
		GouraudSwitch,    // turns the Gouraud switch the other way (keys.txt's B; inteng's key for it isn't read yet)
		BackFaces,        // turns back faces on or off, once the game can show them
		Headlight,        // turns the headlight on or off (Ctrl+B)
		Brighter,
		Darker,
		Textures,         // turns the textures on or off for every draw mode (inteng's Textured / Untextured switch, DAT_005fd6b4: run 25)
	};

	struct ViewToggles
	{
		DrawMode drawMode;
		Gouraud gouraud;
		bool backFaces;
		bool headlight;
		int32_t brightness;
		bool textured;    // inteng's Textured (true, its start value: set to 1 at 0x54d3a4) or Untextured; works with every draw mode
	};

	// What the editor's copy of the game can show: what its avpx carries
	struct GameCanShow
	{
		bool backFaces;   // R-BACKFACES-1's DrawBackFaces
		bool outlines;    // R-DRAWMODES-1's outlines: Polyline's and Filled polyline's white lines
	};

	// What the game's own switches are to be set to: plain numbers, each in its range
	struct GameViewSwitches
	{
		int wireFrameMode;   // kWireFrameOff or kWireFrameEverything
		int drawFullBright;  // 0 or 1: the Gouraud switch Off
		int drawBackFaces;   // 0 or 1: always 0 while back faces can't be shown
		int filledOutlines;  // 0 or 1: R-DRAWMODES-1's switch, always 0 while it can't be shown
		int outlineColour;   // 0xRRGGBB: R-DRAWMODES-1's outline colour, always kWireWhite
		int outlinesLit;     // 0 or 1: R-DRAWMODES-1's outlines in the level's own light
		int headlight;       // 0 or 1
		int gamma;           // kBrightnessLeast to kBrightnessMost
		int untextured;      // 0 or 1: the polygons drawn in one plain colour, lit, with no texture
		float outlineDepthBias;      // the outlines' pull toward the camera, D3DRS_DEPTHBIAS: kOutlineDepthBias as the outlines always had, kHiddenLineDepthBias in the hidden-line look
		uint32_t untexturedColour;   // 0xAARRGGBB: that plain colour. kUntexturedColour (white) in every mode but one: Filled polyline with the textures off is BLACK (kHiddenLineFill)
	};

	// The plain colour of an untextured polygon, 0xAARRGGBB, before the level's light: inteng's WHITE, 0xFFFFFF (palette 0xf8), the colour FUN_0046da70 paints
	// every polygon with when a screen starts that way (Construction: FUN_00447550; Strategy Data with flag 0x80: FUN_00522ab0, out\menu-reads-24.txt:290-303;
	// p11-shape-handlers.md 35 and 37), lit by the level as the game's polygons are. What inteng's Untextured switch (DAT_005fd6b4) draws in each draw mode is
	// the texture view's own and not yet paired (Vasquez), so that switch uses this colour too.
	uint32_t const kUntexturedColour = 0xFFFFFFFFu;

	// Filled polyline with the textures off (owner, 2026-11-15, through Bishop, owner-intent item 16: "filled becomes filled black polygons with no shading and hard white
	// edges"): the polygons are flat black, so the light can do nothing to them (black times any light is black), and the white outlines over them are hard white, depth-tested,
	// so the black faces hide every edge behind them. Every other mode and combination keeps the plain white above.
	uint32_t const kHiddenLineFill = 0xFF000000u;

	// The outlines' pull toward the camera (avpx DrawFilledOutlinesDepthBias): the outlines' own, unchanged for every look but the hidden-line one, which pulls them harder so a
	// faint fight with the black faces under them (a dashed edge, Bishop 15:3x) can't cut them. Chosen on hidden pictures of Invasion and Genshd1: from -0.00002 on no edge is
	// dashed any more; a slope-scaled bias let hidden edges through, so only the constant one is used, and -0.0001 is the middle of the range that showed no leak.
	float const kOutlineDepthBias = -0.00001f;
	float const kHiddenLineDepthBias = -0.0001f;

	// The clean start: Polygon, with the Gouraud switch On (ours, unproven: inteng's own clean start isn't traced yet), no back faces, no
	// headlight, at the brightness a new player starts with
	ViewToggles ViewToggles_AsBuilt();

	// A brightness held to kBrightnessLeast..kBrightnessMost
	int32_t ViewToggles_HeldBrightness(int64_t asked);

	// The toggles after one item is used. A mode's item picks that mode and leaves everything else as it was, the Gouraud switch included;
	// picking the mode already on changes nothing. Polyline and Filled polyline do nothing while the game can't show their lines.
	// GouraudSwitch, BackFaces and Headlight turn their switch the other way; BackFaces does nothing while the game can't show back faces,
	// and then turns off, never on. Brighter and Darker move the brightness a step, held to its range.
	ViewToggles ViewToggles_Use(const ViewToggles &now, ViewToggle which, const GameCanShow &can);

	// The game's switches for these toggles, from where they are now, never from the way they got there. Every one is held to its range,
	// whatever the toggles hold: a draw mode that isn't one of the four is drawn as Polygon, and a Gouraud value that isn't Off is drawn On.
	GameViewSwitches ViewToggles_GameSwitches(const ViewToggles &toggles, const GameCanShow &can);

	// What the status line says after an item is used, the editor's own words: what's on or off now, or why nothing changed
	const char *ViewToggles_Words(const ViewToggles &after, ViewToggle which, const GameCanShow &can);
}

// THE EDITOR'S ONE SET OF TOGGLES. Kept here, with nothing of the game, so the View menu's items (editor_actions.cpp) and their test need
// no game to run. ViewToggles_Apply (view_toggles_game.h) gives them to the game once a frame.

// What the editor's copy of the game can show
host::GameCanShow ViewToggles_GameCanShow();

// Whether the editor's copy of the game can show back faces. The View menu's Back faces item is switched off while this is false.
bool ViewToggles_BackFacesCanBeShown();

// Whether it can show Polyline's and Filled polyline's white lines. Their View menu items are switched off while this is false.
bool ViewToggles_OutlinesCanBeShown();

// The editor's toggles now
host::ViewToggles ViewToggles_Now();

// Sets the textures on or off without a status line (the editor's start, from its settings file)
void ViewToggles_SetTextured(bool textured);

// Uses one item on the editor's toggles. Gives back what the status line says now, the editor's own words.
const char *ViewToggles_UseNow(host::ViewToggle which);

// THE PROOF RUNS' VIEW MODE. The command line's -viewmode word, for a -picture run that proves a mode draws: the same items the View menu
// uses, applied in order before the first frame, with no click. The command line is input from outside the code, so only these exact
// words are taken, nothing else:
// - "normal" (the clean start, no item), "polygon", "polyline", "filledpolyline", "wire-textured", "unlit" (the Gouraud switch Off) and
//   "headlight";
// - "polyline+unlit", "wire-textured+unlit" and "normal+headlight": a mode with the switch Off, or the headlight alone, for
//   Melanikus's four wireframes ({textured, white} x {lit, unlit}) and the headlight's pair.
// Writes the items to use, in order, into toggles (room for at least kCommandWordMostToggles) and their number into *count. Gives back
// false for any other word, or too little room, with nothing written.
size_t const kCommandWordMostToggles = 2;
bool ViewToggles_FromCommandWord(const wchar_t *word, host::ViewToggle *toggles, size_t room, size_t *count);
