// selection.h: what's selected in the editor, one thing for the whole editor, and the words the status line shows for a pick. Plain
// data a test can drive. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng (click-picking). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// Clicking on the level's view (pick_game.h), the object list and the field panel all use this one selection, so they always agree on
// what's selected. A thing is named the way both the game and the level file name it, never by its place in some list:
// - an object (a door, a lift, a switch, anything with a strategy) by its 8-byte ID: the ID the level file gives it, which the game
//   copies into its strategy block's SBname as it loads the level (for example bh_binary_switch.cpp:72);
// - a module (a room, a part of the level's shape) by the game's number for it: 0 up to one less than ModuleArraySize, given in the
//   order of the level's module list (module.cpp:705).
// A panel that shows the selection asks Selection_Changes() each frame and draws again only when the count has moved.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace host
{
	// An object's ID: SB_NAME_LENGTH bytes (strategy_def.h:188, "DO NOT CHANGE THIS!")
	size_t const kSelectionIdBytes = 8;

	// A module's own short name: the game's MODULE.m_name, 5 bytes, with no 0 at the end promised (module.h:205)
	size_t const kSelectionModuleNameBytes = 5;

	// The longest the status line's words for a pick can be, with their 0
	size_t const kSelectionMostWords = 96;

	// How far the game's line of sight looks: its search starts at 10,000,000 (LOS_Lambda, los.cpp), so no hit is further than that
	int32_t const kSelectionMostDistance = 10000000;

	enum class SelectedKind
	{
		Nothing,
		Module,
		Object,
		Light,    // one of the level's lights (a STDLIGHT of its light set), by its place in the set: Light Sourcing (light_edit.h)
	};

	// Only the field for its kind counts: moduleIndex for a Module, objectId for an Object, lightIndex for a Light
	struct Selected
	{
		SelectedKind kind;
		int32_t moduleIndex;
		uint8_t objectId[kSelectionIdBytes];
		int32_t lightIndex;   // last, so every Selected made with { kind, module, { id } } has 0 here
	};

	// Selects picked. Gives back false, with the selection as it was, for a Module numbered below 0, a Light numbered below 0, an Object whose 8 bytes are all 0
	// (the game's own "no name", NAME_ISNULL), or a kind that isn't one of the three. Selecting Nothing is the same as Selection_Clear.
	bool Selection_Set(const Selected &picked);

	// Selects nothing (and empties the set of objects below)
	void Selection_Clear();

	// THE SET OF SELECTED OBJECTS (P1, inteng: "Left click: Select object. Shift+LMB: Select multiple objects", "L: Deselect all objects"; its data
	// is a bit on each selected object's record, 0x40000000, set by select and cleared by deselect: FUN_004d64a0). Selection_Get stays the LAST
	// object picked, which the panel shows. Selection_Set of an object makes the set that one object; of a module or nothing, empty.
	size_t const kSelectionSetMost = 256;

	// Shift+click: an object not in the set is added, and becomes the one Selection_Get gives; one in the set is taken out, and Selection_Get then
	// gives the last one still in it, or nothing. Gives back false, changing nothing, for anything but an object with an ID that isn't all 0, or
	// when the set is full.
	bool Selection_Toggle(const Selected &picked);

	// How many objects are selected, and the ID of the one at index (0 first), into id (kSelectionIdBytes); false for an index past the end
	size_t Selection_SetCount();
	bool Selection_SetMember(size_t index, uint8_t *id);


	// What's selected now
	Selected Selection_Get();

	// Goes up by one each time what's selected changes, and not when it's set to what it already was. It counts round past 4 billion to
	// 0 again, so a panel only compares it with the count it last saw.
	uint32_t Selection_Changes();

	// What one click found, as the game told the editor (pick_game.cpp fills it in)
	enum class PickFound
	{
		NoLine,          // no line could be made from that click (pick_ray.h), or there's no frame to pick from
		Nothing,         // the line went through the view and hit nothing
		Module,          // it hit a module, with a number the game's module list has
		Object,          // it hit an object the level file names
		NotInTheFile,    // it hit something the game made itself (the player, a body, a spark), which the level file doesn't name
		NumberRefused,   // it hit a module whose number isn't one the game's module list has
	};

	struct PickReport
	{
		PickFound found;
		int32_t moduleIndex;                            // for Module
		uint8_t moduleName[kSelectionModuleNameBytes];  // for Module: its bytes as the game holds them
		uint8_t objectId[kSelectionIdBytes];            // for Object
		int32_t distance;                               // for Module and Object: how far along the line, in millimetres

		// What else the game's line of sight knows about the spot it hit (P1, the pick service: one click gives all of this). Left 0 / false
		// by a report that was not read from the game.
		bool hasHit;                  // true: hitPoint, hitNormal and polygonNumber below were read, for Module, Object and NotInTheFile
		int32_t hitPoint[3];          // where the line met the surface, in the world, in millimetres
		int32_t hitNormal[3];         // the normal of the face that was met, 65536 = length 1 (the game's own ONE_FIXED)
		int32_t polygonNumber;        // which polygon of the hit shape, counting from 0 in the order the game walks them; -1 if it isn't known
		bool hasCorner;               // true: nearestCorner below was found. Not for the parts of a hierarchical model (a body, a pickup's arm)
		int32_t nearestCorner[3];     // the corner of that polygon nearest the hit point, in the world, in millimetres
		void const *hierarchySection; // the part of a hierarchical model whose polygons the line met (the game's SECTION_DATA, LOS_HModel_Section), or null; only compared, never read
	};

	// What the status line says about report, written into words: at most size - 1 letters, always ending in 0, nothing at all if size
	// is 0. A module's name is shown with every byte outside the plain printable letters (space to ~) as "?", and stops at its first 0;
	// an object's ID as 16 hex digits; a distance as metres, held to 0 to kSelectionMostDistance.
	void Selection_Words(const PickReport &report, char *words, size_t size);

	// Selects what report found, if it found a module or an object: anything else leaves the selection as it was. Gives back the
	// words for the status line, which stay as they are until the next call. If Selection_Set refuses what was found (a module numbered
	// below 0, an object whose ID is all 0), nothing is selected and the words say it wasn't picked, never "Picked".
	const char *Selection_UsePick(const PickReport &report);

	// Like Selection_UsePick, for a Shift+click: an object found is added or taken out; anything else leaves the selection as it was. The words
	// say how many are selected.
	const char *Selection_UsePickToggling(const PickReport &report);
}
