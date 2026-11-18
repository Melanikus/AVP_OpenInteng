// special_alt_edit.h: a SPECIAL object's alternate locations (Object Placement's Alt Locations and , / ., reassess/object-placement.md OP-34..38, OP-91; V-28), the same rules as a placed
// object's (alt_locations.h) on the ALTLOCAT where the game reads a special's (special_objects_edit.h SpecialObject_ReadAlt / WriteAlt). Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng's Alt Locations take the object kinds 0, 1, 2, 3 and 7 (FUN_004df820): a placed shape, a generator, a bad guy, a sound, a particle generator; never a player start (BadCall here). A
// location is the object's place and its block's matrix (FUN_004df820: +0x10..0x18 and +0x38); the save (FUN_004dfc70, from each class's save) writes the list, and takes the ALTLOCAT out
// when the object has no locations (the generator's FUN_004e52a0: +0x20 == 0, V-28). Ours: a special's place, and a particle generator's turn (the only special with four turn floats); the
// generators' heading and the player start's matrix are not a location's (NAMED: inteng's matrix from FUN_004d5800 is not kept for a generator).

#pragma once

#include <stddef.h>

#include "alt_locations.h"
#include "special_objects_edit.h"

class OpenLevelFile;

namespace host
{
	// The object's list from its ALTLOCAT: an empty one (group 0) when it has none. BadValue when the ALTLOCAT is not one the game's reader takes.
	SpecialResult SpecialAlt_Load(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocations *state);

	// The list into its ALTLOCAT; an empty list takes the ALTLOCAT out (inteng's saves, V-28). BadValue when the list is longer than a chunk's size can say.
	SpecialResult SpecialAlt_Save(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocations const &state);

	// Where the object is now, as a location: its place (file units), a particle generator's turn, else no turn (0 0 0 1)
	SpecialResult SpecialAlt_Now(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocation *now);

	// The object put at a location: its place, and a particle generator's turn (FUN_004dfae0 puts the block there; the class's save writes it). BadValue for a place past +-(1 << 24) or a
	// turn number past +-2 (as AlternateLocations_PutOnObject refuses them).
	SpecialResult SpecialAlt_Put(OpenLevelFile *file, SpecialKind kind, size_t number, model::AltLocation const &location);
}
