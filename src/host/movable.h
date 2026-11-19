// movable.h: ONE way to read and write the place and turn of anything the mouse can move (the owner's mouse gizmo, surface placement and Waypoint by mouse:
// Hicks's plan .claude/plans/streamed-twirling-spindle.md, section 1). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// The gizmo, placement and the waypoint tools never touch a kind's chunks: they ask Movable_Get for a pose and give one back to Movable_Set, which writes it through
// that kind's OWN writer (the one its keys already use). One adapter per kind, each in a .cpp of its own (movable_<kind>.cpp), so the devs who own the kinds never edit
// the same file:
//
//   Kind                       Turn    Writer                                                  Adapter file                    Owner (11-18)
//   PlacedObject (shape)       yes     TrackSetObjectPose (track_edit.cpp)                     movable_placed_object.cpp       Drake (P1)
//   PlacedHierarchy            yes     PlacedHierarchy_WritePlace (it writes the turn too)     movable_placed_hierarchy.cpp    Frost (P2)
//   PlayerStart                yes     SpecialObject_Write (special_objects_edit.cpp)          movable_special.cpp             Frost (P2)
//   ParticleGenerator          yes     SpecialObject_Write                                     movable_special.cpp             Frost (P2)
//   Sound, AvpGenerator, BadGuy  no    SpecialMode_MoveCurrent (special_mode.cpp)              movable_special.cpp             Frost (P2)
//   VirtualObject (VOBJPROP)   no      VirtualObject_SetPlace (new)                            movable_virtual_object.cpp      Frost (P2)
//   Light (STDLIGHT)           yes     LightPlace_Set (light_keys.cpp) + its turn (new)        movable_light.cpp               Frost (P2)
//   EntryPoint (Waypoint 0)    no      the fly's stop (construction_modes_screen.cpp)          movable_entry_point.cpp         Frost (P2)
//   WaypointBox                no      Waypoints_Translate (new), WaypointMode::Write          movable_waypoint.cpp            Dietrich (P4)
//   TrackSectionEnd            yes     TrackData_FinishEnd / TrackData_SetEndTurn              movable_track_end.cpp           Frost (P2)
//
// An adapter not built yet answers MovableResult::NotBuilt, so the gizmo shows nothing for that kind.
//
// THE NUMBERS: a place is the game's world in millimetres, y DOWN (as every chunk holds it). A turn is a quaternion x y z w, as OBJHEAD1 holds it (track_edit.h); a kind
// that keeps a matrix or a heading turns it to and from one in its adapter. A kind with no turn (Movable_CanRotate false) gives the identity and takes only the place.
//
// HOSTILE NUMBERS: a pose comes from mouse sums over a level anyone can make. Movable_Set refuses, before any adapter sees it: a kind out of range, a place outside
// -kMovableMostPlace .. kMovableMostPlace on any axis, and a turn whose numbers are not real (judged by their bits: the editor is built with /fp:fast) or whose length is
// near 0. A usable turn is made exactly 1 long first, so no adapter ever writes a stretched one. Movable_Get's answers are checked the same way.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "selection.h"

class OpenLevelFile;

namespace host
{
	enum class MovableKind : uint8_t
	{
		None,
		PlacedObject,
		PlacedHierarchy,
		PlayerStart,
		ParticleGenerator,
		Sound,
		AvpGenerator,
		BadGuy,
		VirtualObject,
		Light,
		EntryPoint,
		WaypointBox,
		TrackSectionEnd,
		Count
	};

	// The farthest a place may be from the world's middle on any axis, in millimetres: 16,777,216 (as pick_ray.h's kPickMostPlace), far past any level
	int32_t const kMovableMostPlace = 1 << 24;

	// WHICH thing: what the screen has selected, as its kind's adapter finds it again
	struct MovableRef
	{
		MovableKind kind = MovableKind::None;
		uint8_t id[kSelectionIdBytes] = {};   // a placed object's or placed hierarchy's ID
		int32_t number = -1;                  // the thing's number in its own list (a special object's, a light's, a waypoint box's, a track's section end)
		int32_t module = -1;                  // the module it belongs to, for the kinds kept per module (a waypoint box)
	};

	// WHERE it is
	struct MovablePose
	{
		int32_t place[3] = { 0, 0, 0 };          // millimetres, y down
		float turn[4] = { 0.0f, 0.0f, 0.0f, 1.0f };   // x y z w; the identity for a kind with no turn
	};

	enum class MovableResult
	{
		Done,
		NothingOpen,     // no level is open
		NoSuchThing,     // the ref names nothing in the level (an ID no object has, a number past the list)
		NotBuilt,        // that kind's adapter isn't written yet
		PlaceRefused,    // a place outside -kMovableMostPlace .. kMovableMostPlace
		TurnRefused,     // a turn that is not real numbers, or about 0 long
		KindRefused,     // MovableKind::None, Count or past it
		WriterRefused,   // the kind's own writer said no (the game's loader refused the changed level): nothing was changed
	};

	// What an answer means, for the status line: static text, never null
	char const *Movable_ResultWords(MovableResult result);

	// Whether the kind holds a turn (the gizmo shows its rings): the table above
	bool Movable_CanRotate(MovableKind kind);

	// Whether the kind's pose lives in the level's bytes, so it changes only with the level (OpenLevelFile's generation): false for the kinds the editor holds in its own
	// memory while they are edited (a track's section end: Track Editing keeps the track until Stop, Frost 11-18; a waypoint box: Waypoint mode keeps its boxes until it
	// writes them), which a reader must read again each time instead of keeping
	bool Movable_PoseInLevel(MovableKind kind);

	// The kind's name, for the status line and the state line ("placed object", "sound", ...); "nothing" for None and out of range
	char const *Movable_KindName(MovableKind kind);

	// The ref of what the editor's selection holds, for the kinds the selection itself names (a placed object). The other kinds' screens make their refs from their own
	// state (the special object being edited, the light, the waypoint box): kind None when the selection names nothing movable.
	MovableRef Movable_FromSelection(Selected const &selection);

	// The thing's place and turn now. *pose is written only on Done.
	MovableResult Movable_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);

	// The pose of the FIRST of two refs that reads (Done), else of the second: the gizmo asks the selection's ref first and the screen's own (gizmo_screen.h) when that one
	// does not read (Strategy Data's virtual object is a selected Object that is no placed object, Frost 11-18). *used and *pose are written only on Done; the answer is the
	// second's when neither reads (the first's when the second is kind None).
	MovableResult Movable_GetFirst(OpenLevelFile *file, MovableRef const &first, MovableRef const &second, MovableRef *used, MovablePose *pose);

	// The place (and the turn, for a kind that holds one) written through the kind's own writer, after the checks above. Nothing is written unless it says Done.
	MovableResult Movable_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);

	// The checks Movable_Set makes, on their own: Done, PlaceRefused or TurnRefused; on Done *usable is the pose with its turn made 1 long
	MovableResult Movable_CheckPose(MovablePose const &pose, MovablePose *usable);

	// THE ADAPTERS: one pair per kind, each defined in its own movable_<kind>.cpp (the table above). Movable_Get / Movable_Set call them with the ref's kind already
	// checked and, for Set, the pose already checked and made usable; nothing else should call them.
	MovableResult MovablePlacedObject_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovablePlacedObject_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovablePlacedHierarchy_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovablePlacedHierarchy_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovableSpecial_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);   // PlayerStart, ParticleGenerator, Sound, AvpGenerator, BadGuy
	MovableResult MovableSpecial_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovableVirtualObject_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovableVirtualObject_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovableLight_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovableLight_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovableEntryPoint_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovableEntryPoint_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovableWaypoint_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovableWaypoint_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
	MovableResult MovableTrackEnd_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose);
	MovableResult MovableTrackEnd_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose);
}
