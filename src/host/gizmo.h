// gizmo.h: the mouse gizmo's maths, with no window: three arrows that move an object along an axis and three rings that turn it about one (the owner's addition,
// Hicks's plan .claude/plans/streamed-twirling-spindle.md, sections 2 and 3; P1). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// Everything is in the game's world: millimetres, y down, the camera as PickCamera holds it (pick_ray.h). The pointer is in the view's pixels from its top left.
// - THE FRAME: the gizmo's centre (the object's place) and its three axes, the world's x y z or the object's own (its matrix's rows: RotateVector turns the object's x into
//   row 0), with a size that stays the same on the screen (kGizmoArrowPixels long, whatever the distance).
// - THE HIT: each handle is projected onto the view (PickRay_Project); the pointer takes the nearest within kGizmoHitPixels (an arrow before a ring when both are as near).
// - THE DRAGS, from the mouse's line (PickRay_FromClick): an arrow moves by where the line passes closest to the axis; a ring turns by the angle, on the ring's own plane,
//   between where the line met it at the press and where it meets it now. Shift snaps the angle to 15 degrees (Gizmo_Snap).
// - A TURN is applied to the object's MATRIX (each row turned about the axis by the same angle, Rodrigues' sum), not to its quaternion: the editor turns the matrix back into
//   the file's quaternion with the game's own function (level_view.h LevelView_TurnOfMatrix), so the object turns the way the ring was dragged whatever the game's quaternion
//   convention is.
// - THE LINES: the world overlay is always depth-tested and avpx is not changed, so the gizmo is drawn as a copy SCALED TOWARD THE CAMERA: every point moved along its own
//   line of sight to kGizmoNear millimetres ahead. On the screen it is the same picture; in depth it is nearer than any wall.
//
// HOSTILE NUMBERS: the place and matrix come from a level, the pointer from the window. A camera the pick refuses makes no frame; a point not ahead of the camera is never a
// hit; a line nearly along an arrow, or nearly in a ring's plane, makes no drag (no division by about 0); a moved place is held within kMovableMostPlace (movable.h's checks
// refuse past it anyway).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "pick_ray.h"
#include "track_view.h"

namespace host
{
	enum class GizmoHandle : uint8_t
	{
		None,
		MoveX,
		MoveY,
		MoveZ,
		TurnX,
		TurnY,
		TurnZ,
		Centre,   // the centre ball: Vasquez's surface placement (P3) takes a press on it; hit FIRST, within kGizmoCentrePixels, for every kind
	};

	double const kGizmoArrowPixels = 90.0;    // an arrow's length on the screen
	double const kGizmoRingPixels = 70.0;     // a ring's radius on the screen
	double const kGizmoHitPixels = 8.0;       // how near the pointer must be to a handle
	double const kGizmoCentrePixels = 9.0;    // the centre ball's radius on the screen, and how near the pointer must be to its middle
	int const kGizmoRingSegments = 48;        // a ring is drawn and hit as this many straight pieces
	double const kGizmoNear = 600.0;          // the drawn copy's distance ahead of the camera, in millimetres
	double const kGizmoSnapRadians = 0.26179938779914943;   // Shift's step: 15 degrees

	uint32_t const kGizmoColourX = 0xFFFF3030u;     // ours: x red, y green, z blue, the handle under the pointer (or dragged) yellow
	uint32_t const kGizmoColourY = 0xFF30E030u;
	uint32_t const kGizmoColourZ = 0xFF3070FFu;
	uint32_t const kGizmoColourHot = 0xFFFFE000u;

	struct GizmoFrame
	{
		double centre[3] = { 0.0, 0.0, 0.0 };   // the object's place
		double axis[3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } };   // each 1 long, in the world
		double length = 0.0;                    // an arrow's length in the world at the centre's distance (kGizmoArrowPixels on the screen)
		double radius = 0.0;                    // a ring's radius in the world (kGizmoRingPixels on the screen)
		double ahead = 0.0;                     // the centre's distance ahead of the camera
	};

	// "none", "move x", "move y", "move z", "turn x", "turn y", "turn z", "centre ball": for the state line (gizmo=)
	char const *Gizmo_HandleName(GizmoHandle handle);
	bool Gizmo_IsTurn(GizmoHandle handle);
	int Gizmo_AxisOf(GizmoHandle handle);   // 0, 1 or 2; -1 for None

	// The frame for an object at place: with local, the axes are matrix's rows (16.16, the game's ObMat; refused when one is about 0 long); without, the world's. False
	// when the camera is refused or the place is not ahead of it.
	bool Gizmo_Frame(PickCamera const &camera, int32_t const place[3], int32_t const matrix[9], bool local, GizmoFrame *frame);

	// The handle under the pointer (x, y in the view's pixels); rings only when canRotate. The centre ball first: within kGizmoCentrePixels of the projected centre it is the
	// ball's, before the arrows that all start there
	GizmoHandle Gizmo_Hit(PickCamera const &camera, GizmoFrame const &frame, bool canRotate, double x, double y);

	// Where along the axis (millimetres from the centre) the mouse's line at (x, y) passes closest; false when there is no line or it is nearly along the axis
	bool Gizmo_AlongAxis(PickCamera const &camera, GizmoFrame const &frame, int axis, int32_t x, int32_t y, double *along);

	// Where the mouse's line at (x, y) meets the ring's plane, from the centre; false when there is no line, it is nearly in the plane, or it meets it behind the camera
	bool Gizmo_OnRingPlane(PickCamera const &camera, GizmoFrame const &frame, int axis, int32_t x, int32_t y, double out[3]);

	// The angle (radians, -pi .. pi) that turns from towards to about axis, as Rodrigues' sum turns: positive when axis . (from x to) is
	double Gizmo_AngleAbout(double const axis[3], double const from[3], double const to[3]);

	// The angle to the nearest whole step (Shift: kGizmoSnapRadians)
	double Gizmo_Snap(double radians, double step);

	// The place moved by mm along axis (held within the world's limit)
	void Gizmo_Moved(int32_t const place[3], double const axis[3], double mm, int32_t out[3]);

	// The matrix (16.16, rows the object's axes) with every row turned by radians about axis (1 long), each row made 1 long (65536) again
	void Gizmo_Turned(int32_t const matrix[9], double const axis[3], double radians, int32_t out[9]);

	// The lines that draw the gizmo (three arrows, the rings when canRotate, then the centre ball: two diamonds facing the camera), the hot handle yellow, scaled toward the
	// camera to kGizmoNear ahead (see the top)
	void Gizmo_Lines(PickCamera const &camera, GizmoFrame const &frame, bool canRotate, GizmoHandle hot, std::vector<TrackLine> *lines);
}
