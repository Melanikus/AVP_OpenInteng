// shape_pane_plan.h: THE LIVE PANES' FRAME, worked out before anything is drawn (shape_pane_gpu.cpp draws it). Written 2026-11-23 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The placement picker's live panes (the owner, 2026-11-23: "real-time renderered rotating in 3d") are drawn every frame into ONE texture on the card, their
// atlas, each pane into a region of it its box's size; then each region is shown in its box (AvP-docs tools/frost/pickertime/SAFETY-CASE-live-panes.md). What
// this works out, with no card at all (so a test can check every number):
//   - THE PLAN: which of the panes asked for this frame are drawn, and where in the atlas: shelf by shelf from its top left, in the order asked (the screen
//     side asks for the selected one first), while the frame's triangles stay within kShapePanesTrianglesAFrame (the safety case's section 3: a hostile folder
//     of the largest shapes would otherwise put 590,000 a frame on the card). One that isn't drawn says why, and its tile keeps its still picture.
//   - THE TURNED CAMERA: the drawing's still camera (eye and at, as the store keeps them) gone round its middle, as ShapeThumb_CameraTurned turns it.
//   - THE PANE'S MATRIX: world, view and lens in one, for its box's shape, the still pictures' own view and lens (shape_thumb_render.h).
#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "shape_pane_pack.h"   // kShapePaneMostTriangles: one number for a shape's most and a frame's

namespace host
{
	uint32_t const kShapePaneAtlasSide = 2048;                                    // the atlas, square: at most 2048 x 2048 (the safety case, section 1)
	uint32_t const kShapePanesTrianglesAFrame = kShapePaneMostTriangles;          // the triangles drawn in one frame, over all panes (section 3): 200,000

	// One pane asked for this frame: its drawing's handle (shape_pane_store.h), its box (x, y, width, height: the window's pixels), its turn (radians) and how
	// many triangles its drawing has
	struct ShapePaneAsk
	{
		int handle = -1;
		int32_t box[4] = { 0, 0, 0, 0 };
		float turn = 0.0f;
		uint32_t triangles = 0;
	};

	// What became of it: drawn into region (x, y, width, height: the atlas's texels, the box's own size), or not drawn, with why in plain words
	struct ShapePanePlace
	{
		bool drawn = false;
		uint32_t region[4] = { 0, 0, 0, 0 };
		std::string why;
	};

	// THE PLAN, one place for each ask, in the order asked. Not drawn: a box of no size; one wider or taller than the atlas; one whose triangles would take the
	// frame past mostTriangles (it takes no room, so a later, smaller one can still be drawn); one for which no room is left in the atlas this frame.
	std::vector<ShapePanePlace> ShapePane_Plan(std::vector<ShapePaneAsk> const &asks, uint32_t atlasSide, uint32_t mostTriangles);

	// THE TURNED CAMERA: eye gone `turn` radians round `at`, about the world's up axis, the way from +x toward +z, at the same distance and height (as
	// ShapeThumb_CameraTurned turns the still camera)
	void ShapePane_TurnEye(float const eye[3], float const at[3], float turn, float turned[3]);

	// THE PANE'S MATRIX: a corner times it (Direct3D's order: a row of x, y, z, 1 on the left) gives its place on the pane's glass. The view and lens of the
	// still pictures (ShapeThumb_ViewMatrix, ShapeThumb_LensMatrix, the same near and far), fieldOfView radians across the box's shorter side, the longer side
	// seeing that much more. False when it can't be made: the eye at the middle, a box of no size, a field of view the lens refuses.
	bool ShapePane_Matrix(float const eye[3], float const at[3], float fieldOfView, uint32_t width, uint32_t height, float matrix[4][4]);
}
