// light_scene.h: the open level's lights and shapes as inteng's lighting calculation wants them, and the calculation for a whole level (Light Sourcing; light_maths.h has what one light gives one vertex). Written
// 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-lighting-facts.md.
//
// WHAT IT DOES: reads the level's lights (the STDLIGHT chunks of the light set named NORMALLT, as FUN_004ae5d0 turns them into engine lights: place and range = the file's number x 100 x the level's scale
// truncated, the direction = the third row of the light's matrix, the spread's cosine from the game's cosine table, the engine flags with 0x300 added for "No PreLight"), reads every module's shape (the points
// moved by the module's place, the vertex normals), and adds up for each vertex the lights that are on. What it gives is exactly the numbers Save Intensities writes (a vertex's red << 16 | green << 8 | blue),
// so they can be compared with the SHPVTINT chunks a stock level holds.
//
// NOT HERE YET: the placed lights (PLOBJLIT in an object: FUN_004ae5d0 0x4ae9ce), the shadows (FUN_004fdd40), a module whose matrix is not the identity (a module's own is).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "light_maths.h"
#include "light_shadows.h"

class OpenLevelFile;

namespace host
{
	struct LightSceneLights
	{
		LightSceneLights() : ambience(0), scale(1.0) {}

		std::vector<light::Light> lights;
		std::vector<light::Light> placedLights;   // the PLOBJLIT lights of the objects: place and range (and the flags), for the reach rule and the tools' save; the direction is not made (they are omni or No PreLight in every stock level)
		int32_t ambience;       // the light set's AMBIENCE
		double scale;           // the level's scale (ENVSDSCL, 1 when absent)
	};

	// A light's MARKER (FUN_004b2590 builds it from the descriptor at 0x5e1c28: shape index 2, the editor's own five-point shape at 0x5e1138): where it stands and how it is turned, and whether it is hidden
	// (the local flag LOFlag_Invisible 0x20 that Hide All and the I key set). place is the file's own numbers; matrix the light's 3 x 3 (16.16), row by row, its third row the direction.
	struct LightMarker
	{
		double place[3];
		int32_t matrix[9];
		bool hidden;
		bool changed = false;   // its local flags hold 4 (LOFlag_Changed): Light Sourcing turns its marker (FUN_004ab670, light-closeout-frost.md TB-L6)
	};

	// The markers of the STDLIGHTs of the set NORMALLT, in the set's order (so the number of a light is its marker's place). False when the level has no such set.
	bool LightScene_Markers(OpenLevelFile *open, std::vector<LightMarker> *out);

	// The level's lights (the set named NORMALLT). False when the level has no light set; *why says what was not found (null: not said).
	bool LightScene_ReadLights(OpenLevelFile *open, LightSceneLights *out, std::string *why);

	// One object's shape lit: the saved value of each vertex, and (when the object has a SHPVTINT of the set NORMALLT) the stored values to compare with
	struct LitObject
	{
		LitObject() : number(0), hasStored(false), reached(false) {}

		int32_t number;                    // the object number
		std::string name;
		int32_t location[3] = { 0, 0, 0 };   // the object's place as the file holds it
		std::vector<uint32_t> computed;    // per vertex: red << 16 | green << 8 | blue
		std::vector<int32_t> vertices;     // per vertex: x, y, z in memory units, moved by the module's place
		std::vector<int32_t> normals;      // per vertex: the vertex normal, 16.16 whole numbers
		std::vector<uint32_t> stored;      // per vertex, from the level's own SHPVTINT, when hasStored
		bool hasStored;
		bool reached;                      // some light that is on reaches the shape (FUN_004fe5e0: the distance from the shape's place to the light < the shape's radius + the light's range): only then does Save Intensities write it a chunk
	};

	// Lights every module of the level that has a shape and gives what Save Intensities for the game would write (the lights that are "No PreLight" are off; with toolsSave they are on). Changes nothing.
	// doorsClosed: the state of Doors > Close (FUN_004a9f40): a module whose shape has morph frames (SHPMORPH, FRMMORPH) is lit with the other shape of its first frame, whose points and vertex normals it swaps in.
	void LightScene_LightModules(OpenLevelFile *open, LightSceneLights const &lights, bool toolsSave, bool doorsClosed, std::vector<LitObject> *out, bool flatNormals = false);

	// The saved value of one vertex: the sum of the lights' parts. For the callers that light one vertex.
	uint32_t LightScene_LightVertex(LightSceneLights const &lights, bool toolsSave, int32_t const vertex[3], int32_t const normal[3], light::ShadowScene const *shadowScene);
}
