// movable_live.h: what a gizmo drag is moving, and its pose NOW, so the drawing code draws the thing itself there while the drag goes on; and where the screen's current thing
// was drawn on the last frame, for the harness (the owner, 11-19, on his Tools build: "I see the gizmo, works perfectly, but the object doesn't move with it, you can only see it
// after you save/leave object mode"). Written 2026-11-19 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE KINDS (Frost's P2 kinds, movable.h): the special objects (player start, particle generator, sound, generator, bad guy), the placed hierarchies and the virtual objects are drawn
// as the editor's icons (editor_main.cpp SpecialIconTriangles), and a current placed hierarchy whose model is shown as its model (hierarchy_view.h). A placed object is Drake's: its
// live copy is LevelView_FlyPut's. Nothing here reads or writes a level: the gizmo writes through movable.h when the drag is let go, and the drawing reads the level again then.
// THE NUMBERS: a pose as movable.h has it (the file's units, turn x y z w); a drawn place in the world's (the file's times the level's scale, as the icons are drawn).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "movable.h"
#include "special_objects_edit.h"

namespace host
{
	// THE DRAG (gizmo_screen.cpp): the thing being dragged drawn at this pose; Clear when the drag ends (written or given up), from then on it is drawn as the level holds it
	void MovableLive_Show(MovableRef const &ref, MovablePose const &pose);
	void MovableLive_Clear();

	// What the drawing asks for each thing it draws: true, with the pose to draw it at, when it is the one being dragged
	bool MovableLive_PoseOfSpecial(SpecialKind kind, size_t number, MovablePose *pose);
	bool MovableLive_PoseOfHierarchy(uint8_t const id[8], size_t number, MovablePose *pose);
	bool MovableLive_PoseOfVirtual(uint8_t const id[8], MovablePose *pose);

	// THE HARNESS: the drawing notes where it drew the screen's CURRENT thing this frame (its kind's words and its place in the world); Forget at the start of each frame's drawing,
	// so a frame that drew none leaves the field empty
	void MovableLive_ForgetDrawn();
	// axes: the drawn icon's three axes (EditorIcon_Add's: rows of the game's matrix), or null when it is drawn unturned
	void MovableLive_NoteDrawn(char const *what, double const world[3], double const *axes = nullptr);

	// The state line's field: currenticon "<what> x y z" (the world's units, to the nearest), then " facing a b c" when it was drawn turned (its third axis, the way it faces, in
	// thousandths), empty when none was drawn
	void MovableLive_StateLine();
}
