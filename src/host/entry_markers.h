// entry_markers.h: Waypoint mode's ENTRY POINT MARKERS and Entry points > Fly: the markers (FUN_00588260), and the write-back of a marker that was moved (FUN_0058a6b0). Written 2026-11-15 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". The facts: AvP-docs\engine\inteng\drake-waypoint-facts.md section 5 (dump-all-4.txt FUN_00588260 / FUN_0058a6b0 /
// FUN_0058b550 / FUN_0058b5e0).
//
// THE MARKERS (FUN_00588260, called when a module is picked in the AI modules mode and by Recalculate all): for each module of the picked AI module (its master and its slaves), each ADJMDLEP entry that leads
// to a module OUTSIDE the AI module and whose flags have neither bit 2 nor bit 4 gets a small orange cube (200 units each way: 20000 of inteng's memory numbers, which are the file's numbers times 100) at the
// entry's point.
//
// FLY (FUN_0058b550): the movement keys move the marker picked instead of the camera while it is on ("Moving Entry Point" on the status line); turned off, or when another marker is picked (FUN_0058b5e0), the
// marker's new place is written back (FUN_0058a6b0): the marker has two AI modules, its main module's and the entry's module's; for the entries of each AI module that lead into the other one, the module of
// the OTHER AI module whose box is nearest the new place (the biggest overshoot outside the box, over the three axes, the least) is found among those the entries lead to; its box is shrunk by 100 units each
// side (a box under 200 thick becomes its middle), the new place is held inside it, and every such entry gets that point and its flags' bit 1 (the hand-set bit: Recalculate Entry Points leaves its point alone).
// The same the other way round for the other AI module's entries.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class OpenLevelFile;

namespace host
{
	struct EntryMarker
	{
		int32_t main;         // the object index of the module whose ADJMDLEP holds the entry
		int32_t adjacent;     // the object index of the module the entry leads to
		int32_t point[3];     // the entry's point (the file's units)
		int32_t flags;        // the entry's flags
	};

	// The half side of a marker's cube, in the file's units
	int32_t const kEntryMarkerHalf = 200;

	// The markers of the AI module of the module `module` (an object index; a module with no AI chunks is an AI module of its own), in the order inteng makes them. false when the level can't be read.
	bool EntryMarkers_Of(OpenLevelFile *open, int32_t module, std::vector<EntryMarker> *out);

	enum class EntryMoveResult
	{
		Done,
		NothingMoved,    // the place is the marker's own, or no entry of the pair was found
		WriteFailed,     // the game's loader would not take the new chunks: nothing was changed
		NoLevel,
	};

	// FUN_0058a6b0: the marker (as EntryMarkers_Of gave it) is now at place (the file's units). *written is how many entries got the new point.
	EntryMoveResult EntryMarkers_Move(OpenLevelFile *open, EntryMarker const &marker, int32_t const place[3], size_t *written);
}
