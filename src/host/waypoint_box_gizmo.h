// waypoint_box_gizmo.h: THE WAYPOINT-BOX GIZMO's sums, with no window (Bishop's P4 item 4, 11-18, for the release after alpha.2; the owner's addition: inteng sizes a box only in Edit
// Extents' three views, FUN_00585750). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// On Waypoint mode 1 (the alien list), the current waypoint's box gets Drake's gizmo (gizmo.h: its arrows and its centre ball, no rings: a box holds no turn) and six FACE HANDLES, one
// at the middle of each face, a diamond square to the line of sight in its axis's colour (the gizmo's own lines; nothing else is drawn):
//   - the ARROWS move the whole box, its aim point with it (the aim point keeps its place in the box: one that was outside stays so, and the checker reports it);
//   - a FACE HANDLE moves that face along its axis: the box's lowest or highest on that axis. The face never passes the other one, and the axis always keeps the room the stock game
//     needs (avpx waypoint_room.h WaypointRoom 1 or more: it divides by it). The aim point then does as inteng's own Edit Extents has it (FUN_00586e90): the middle of the box, unless
//     it is the user's (flag 0x80000000), which is held inside the box;
//   - the CENTRE BALL, clicked, switches what the arrows move: the box, or its AIM POINT on its own (as inteng's Centre Edit, command 0x1f, switches Edit Extents' drags to the aim
//     point). The aim point is then the user's (the flag set, as inteng's Centre Edit and "aim x y z" set it) and always held INSIDE its own box, by the checker's own test
//     (waypoint_net_check.h Waypoint_Holds: the game's WaypointContainsPoint).
// THE NUMBERS: a box is in the file's units; the world's millimetres are the file's times the level's scale, toward zero (waypoint_pick.h WaypointPick_WorldBox), as the 3D view draws
// it. A drag is measured in the world and turned into the file's units (WaypointBox_FileStep).
// HOSTILE NUMBERS: a box comes from a level anyone can make. No sum here overflows (64 bits, and every place held within kWaypointBoxMostPlace of the middle); an edit whose box would
// have no room, or a number past that, is refused (false), and the drag shows the last box that was usable.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "gizmo.h"
#include "pick_ray.h"
#include "track_view.h"
#include "waypoint_model.h"

namespace host
{
	enum class WaypointBoxHandle : uint8_t
	{
		None,
		MoveX,
		MoveY,
		MoveZ,
		Ball,    // the centre ball: a click switches between the box and its aim point
		MinX,    // the faces: the box's lowest x, its highest x, then y, then z
		MaxX,
		MinY,
		MaxY,
		MinZ,
		MaxZ,
	};

	double const kWaypointFacePixels = 7.0;            // a face handle's diamond on the screen, and how near the pointer must be to its middle
	int32_t const kWaypointBoxMostPlace = 1 << 24;     // the farthest a box's number may be from the middle (Waypoints_Parse refuses past it)

	// "none", "move x", "move y", "move z", "centre ball", "min x", "max x", "min y", "max y", "min z", "max z": for the state line and the harness's drag-gizmo
	char const *WaypointBoxGizmo_HandleName(WaypointBoxHandle handle);

	// The face of a handle, 0 to 5 (min x, max x, min y, max y, min z, max z: its axis is face / 2, its highest side face % 2), -1 for the others; the axis of an arrow, 0 to 2, else -1
	int WaypointBoxGizmo_FaceOf(WaypointBoxHandle handle);
	int WaypointBoxGizmo_ArrowAxis(WaypointBoxHandle handle);

	// In the world (millimetres, y down): the middle of the box, its aim point, and the middle of one face (0 to 5), as the 3D view draws the box
	void WaypointBoxGizmo_Middle(WaypointRec const &box, double scale, double out[3]);
	void WaypointBoxGizmo_Aim(WaypointRec const &box, double scale, double out[3]);
	void WaypointBoxGizmo_FacePoint(WaypointRec const &box, double scale, int face, double out[3]);

	// The handle under the pointer (the view's pixels): the centre ball first (Gizmo_Hit's), then the face whose middle is nearest within kWaypointFacePixels, then an arrow (Gizmo_Hit's).
	// frame is Drake's (Gizmo_Frame at the box's middle or at its aim point, the world's axes).
	WaypointBoxHandle WaypointBoxGizmo_Hit(PickCamera const &camera, GizmoFrame const &frame, WaypointRec const &box, double scale, double x, double y);

	// The lines: the gizmo's arrows and centre ball (Gizmo_Lines, no rings), then the six face diamonds (each pulled toward the camera along its own line of sight to kGizmoNear ahead,
	// as Gizmo_Lines pulls its own: nearer than any wall), the hot handle yellow
	void WaypointBoxGizmo_Lines(PickCamera const &camera, GizmoFrame const &frame, WaypointRec const &box, double scale, WaypointBoxHandle hot, std::vector<TrackLine> *lines);

	// THE EDITS, each from the box as it was when the drag began, by `by` of the file's units: false (and *out untouched) when the box made would not be usable
	// The room the stock game needs on every axis (min <= max, WaypointRoom 1 or more), every number within kWaypointBoxMostPlace
	bool WaypointBox_HasRoom(WaypointRec const &box, double scale);

	// The whole box along an axis, with its aim point
	bool WaypointBox_Moved(WaypointRec const &start, int axis, int32_t by, double scale, WaypointRec *out);

	// One face (0 to 5) along its axis: held so it never passes the other face and the axis keeps its room; then the aim point as the top says
	bool WaypointBox_FaceMoved(WaypointRec const &start, int face, int32_t by, double scale, WaypointRec *out);

	// The aim point along an axis, the user's from then on (flag 0x80000000), held inside the box (all three axes)
	bool WaypointBox_AimMoved(WaypointRec const &start, int axis, int32_t by, double scale, WaypointRec *out);

	// The aim point held inside the box, as the checker tests it: each axis put within min .. max - 1, then, where the level's scale makes that not enough, moved toward the box's
	// middle until it is; false when no place inside is found (a box with no room)
	bool WaypointBox_HoldAimInside(WaypointRec *box, double scale);

	// A drag of worldMm millimetres as the file's units (over the scale, to the nearest), held within kWaypointBoxMostPlace; 0 for a scale or a number that is no use
	int32_t WaypointBox_FileStep(double worldMm, double scale);
}
