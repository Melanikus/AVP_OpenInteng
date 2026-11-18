// See placement_lighting.h. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "placement_lighting.h"

#include <string.h>

#include "level_view.h"   // LevelView_PlacementLighting
#include "state_line.h"   // StateLine_Add: the look's own field (Bishop's D12)

extern int DrawFullBright;   // the game's own switch for drawing everything fully lit (kshape.cpp; ViewToggles_Apply sets it each frame from the Gouraud switch, before this)

static bool placementLighting = false;   // DAT_008f4f0c
static bool placementLook = false;       // the mode's look is wanted: Object Placement started, not yet left

// The mode's own screens: Object Placement, its sounds page, Track Editing and the Shape Chooser (inteng comes back from the chooser through its entry with DAT_008f4f38 = 1, which skips the
// look's start, 0x4d843e, and loads no level: the look stays)
static bool InTheMode(char const *screen)
{
	return screen && (strcmp(screen, "Object Placement") == 0 || strcmp(screen, "Placement mode 1 (sounds)") == 0 || strcmp(screen, "Track Editing") == 0 || strcmp(screen, "Shape Chooser") == 0);
}

void PlacementLighting_Start()
{
	placementLook = true;   // the modules unlit, the objects as Lighting says (OP-3)
}

bool PlacementLighting_Flip(char const **words)
{
	if (!placementLook)
	{
		return false;
	}

	placementLighting = !placementLighting;

	// inteng says nothing; the words are ours
	if (words)
	{
		*words = placementLighting ? "Lighting: on (the placed objects with baked colours are lit)" : "Lighting: off (the placed objects are drawn unlit)";
	}

	return true;
}

bool PlacementLightingOn()
{
	return placementLighting;
}

void PlacementLighting_Frame(char const *screen)
{
	// any way out of the mode ends the look (inteng loads the level again): its Exit and Tab set the screen themselves (LeaveTheMode -> ShowTheScreen), past the editor's screen change
	placementLook = placementLook && InTheMode(screen);
	LevelView_PlacementLighting(placementLook, placementLighting);

	// DECIDED OP-3: inteng's Object Placement draws by ITEM: the modules plain (flag 2), the objects with baked colours lit or plain by Lighting (FUN_004d67f0 swaps their types). The
	// editor's whole-view full bright (the Gouraud switch off, the hub's plain look: Frost 11-15) would draw every item fully lit and hide the objects' light, so while the look is on the game
	// lights by item: a plain item (iflag_nolight) is still drawn at its textures' full colours (kshape.cpp, static_level.cpp), so the modules look as in the hub
	if (placementLook)
	{
		DrawFullBright = 0;
	}

	// the state line's placementlight (LevelView_PlacementLightingWords: the look, the counts, the drawn baked-colour objects; the drawn ones are the last frame's)
	StateLine_Add("placementlight", LevelView_PlacementLightingWords());
}
