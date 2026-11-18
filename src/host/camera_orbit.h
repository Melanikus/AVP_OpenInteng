// camera_orbit.h: a test's camera ORBITING its subject (the owner, 11-18: "position these camera up/to side like 45 in all axis from subject (like aiming at the corner of a cube), that's how
// the click algorithms for humans works", "like orbiting the subject, not a straight shot"): the camera on a sphere round the centre of an object's box, looking at that centre. The maths only,
// no level: the harness's camera-orbit / camera-corner (orbit_command.h) put the editor's camera with it. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// THE GAME'S AXES (level_view.h FlyCamera): x across, y DOWN, z forward; the turn in 4096ths of a turn about the up axis, 0 looking along +z and 1024 along -x (measured against the editor's
// matrix, Bishop 11-18 a701208a); the look in 4096ths, held to a quarter turn either way, POSITIVE looking down (seen 11-18: a negative look from above showed Genshd1's lift shaft from below; the three views' extra cameras count
// their look the other way).

#pragma once

#include <stdint.h>

namespace host
{
	struct OrbitCamera
	{
		int32_t place[3];   // millimetres
		int32_t turn;       // 0 to 4095
		int32_t look;       // -1024 to 1024, positive down
	};

	double const kOrbitDefaultAround = 45.0;   // degrees round the up axis
	double const kOrbitDefaultUp = 45.0;       // degrees above the centre
	double const kOrbitDistancePerHalfDiagonal = 2.5;   // the default distance: this many times the box's half diagonal (the whole box in view)
	double const kOrbitLeastDistance = 1000.0;          // and at least a metre
	double const kOrbitMostDefaultDistance = 4000.0;     // and at most four: a human's reach for a click in a room (Genshd1's platform lift, 7.8 m across, put the camera 13.8 m off, behind the walls)
	double const kOrbitMostDistance = 1000000.0;        // a kilometre: a given distance past it is refused

	// The camera round the box (mins, maxs: the object's box in the world, millimetres; maxs not below mins): around degrees about the up axis (0: on the box's -z side looking along +z;
	// 90: on its +x side looking along -x, as turn 1024 looks), up degrees above the centre (-89 to 89), at distance millimetres from the centre (0: the default above, 1 to 4 metres), looking at the centre. False for a box
	// the wrong way round, a size past 2^25, an up outside -89..89, a distance below 0 or past kOrbitMostDistance, a number that is not finite, or a place past the camera's limit (2^24).
	// *usedDistance (when not null) gives the distance taken.
	bool CameraOrbit_Place(int32_t const mins[3], int32_t const maxs[3], double around, double up, double distance, OrbitCamera *out, double *usedDistance);
}
