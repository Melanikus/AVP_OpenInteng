// pick_ray.h: the line from the camera through a click on the level's view, as plain sums a test can drive. Written 2026-11-14 by
// Melanikus (Frost) for AVP_OpenInteng (click-picking). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A click picks what the game's own line of sight finds along this line (pick_game.h), so the line has to go through exactly the pixel
// that was clicked. It's the game's own drawing worked backwards:
// - Forwards, the game turns a place in the world into the camera's view by taking the camera's place off it and turning it by the
//   camera's matrix (RotateVector, maths.cpp:730: each of the view's three numbers is one column of the matrix times the place).
//   It hands the card the view's up-and-down number the other way round (d3d_render.cpp:1915, "y = -Y"), and the card's projection
//   (UpdateProjectionMatrix, d3d_render.cpp:1578) multiplies across by xScale and down by yScale and divides both by the distance
//   ahead. That gives a place on the screen from -1 to 1 each way, which the card spreads over the view's pixels.
// - Backwards, here: the click's pixel becomes that -1 to 1 place (Direct3D 9 puts a pixel's middle on its whole number), that
//   becomes a direction in the view (across / xScale, minus down / yScale, 1 ahead), and the matrix's rows turn it back into the
//   world, as the game's own gun sight does (targeting.cpp:83-86 turns the matrix over first: the same thing). Last, it's made 65536
//   long, as the game's line of sight wants it (los.h: "NORMALISED"; the game's Normalise makes 65536 long).
//
// Everything that comes in is checked before it's used, and a click that can't make a line makes none:
// - the view: each side 1 to kPickMostSide pixels, as R_DrawScreenToChain allows a picture; the click: a pixel inside it;
// - the projection's two numbers: real numbers above 0, checked by their bits (the editor is built with /fp:fast, which lets the
//   compiler assume a number is never "not a number", so a test like x > 0 can't be trusted to catch one);
// - the matrix: each number within -65536 to 65536, as any turn the game makes is (65536 is the game's 1);
// - the camera's place: each number within -kPickMostPlace to kPickMostPlace (see there);
// - and a direction that comes out with no length (a matrix of noughts) makes no line.

#pragma once

#include <stdint.h>

namespace host
{
	// The game's 1, in its whole-number sums: a matrix's numbers are in 65536ths, and a direction is 65536 long (ONE_FIXED)
	int32_t const kPickOne = 65536;

	// The largest side of a view, in pixels: the most R_DrawScreenToChain allows (draw_chain.h)
	int32_t const kPickMostSide = 16384;

	// How far from the middle of the world the camera may be, in the game's units (millimetres), each way: 16,777,216, about 16.8
	// kilometres. Far past any level. The game's line of sight starts its search 10,000,000 along the line (LOS_Lambda, los.cpp) and
	// works out places along it in whole numbers, so this keeps every one of those places well inside what a whole number can hold.
	int32_t const kPickMostPlace = 1 << 24;

	// The camera as the game drew the frame, and the view it was drawn into
	struct PickCamera
	{
		int32_t place[3];      // the camera's place: the game's VDB_World (x, y, z)
		int32_t turn[3][3];    // the camera's matrix: turn[row][column] is the game's VDB_Mat.mat<row+1><column+1>, in 65536ths
		float xScale;          // the card's projection across: d3d.matProjection._11
		float yScale;          // and down: d3d.matProjection._22
		int32_t viewWidth;     // the view's size in pixels, the same pixels the click is in
		int32_t viewHeight;
	};

	// A line from the camera: where it starts, and its direction, 65536 long
	struct PickRay
	{
		int32_t from[3];
		int32_t direction[3];
	};

	// Whether a line was made, and if not, which check said no
	enum class PickRayMade
	{
		Made,
		ViewSizeRefused,       // a side of the view below 1 or above kPickMostSide
		ClickOutsideTheView,   // the click isn't on one of the view's pixels
		ScaleRefused,          // a projection number that isn't a real number above 0
		TurnRefused,           // a matrix number outside -65536 to 65536
		PlaceRefused,          // a camera place outside -kPickMostPlace to kPickMostPlace
		NoDirection,           // the direction came out with no length
	};

	// The line from the camera through the middle of the pixel at (clickX, clickY), counted from the view's top left. *ray is written
	// only when the answer is Made.
	PickRayMade PickRay_FromClick(const PickCamera &camera, int32_t clickX, int32_t clickY, PickRay *ray);

	// The other way: where a place of the world (the game's millimetres) is on the view, in the view's pixels counted from its top left, the game's own drawing worked FORWARDS (the same sums PickRay_FromClick undoes). false when the place is
	// not ahead of the camera (its distance ahead, the view's third number, is not above 0) or the camera is refused; *ahead is that distance in the game's units.
	bool PickRay_Project(const PickCamera &camera, const int32_t world[3], double *pixelX, double *pixelY, double *ahead);

	// Whether a float is a real number above 0 and not too small to divide by (the smallest ones, below about 1.2e-38, are refused
	// too), judged by its bits alone, so it holds under /fp:fast
	bool PickRay_ScaleUsable(float scale);
}
