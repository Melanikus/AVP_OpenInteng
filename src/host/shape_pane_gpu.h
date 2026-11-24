// shape_pane_gpu.h: THE PLACEMENT PICKER'S LIVE PANES, their GPU side: each listed shape put on the card once a sitting and drawn LIVE in its pane, turning,
// over the level, in the editor's own frame (the owner, 2026-11-23: "The shapes don't change either (maps or shapes) they should live in the GPU: Goal, I want
// sub milisecond draw on each of the panes and I want them real-time renderered rotating in 3d"; R-INT-16: the renderer and avpx may be refactored for it,
// shapes first). THE INTERFACE Frost and Vasquez agreed, 2026-11-23: Frost writes the GPU side behind it, Vasquez the screen side that calls it
// (shape_picker_screen.cpp: the boxes, the turning, the paced uploads, the names, the frame and the bar).
// Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng, as the STAND-IN the screen side builds against until Frost's own lands: every upload is
// refused in words ("not in yet"), so the picker keeps its still pictures (nothing is drawn live). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHEN, IN THE FRAME: the picker's callback (level_view.h LevelView_SetPanes) runs inside LevelView_Draw, after the level and its inset are drawn and before the
// panels (addToFrame): ShapePaneGpu_Draw for each pane on the screen, then ShapePaneGpu_FrameEnd once. Draw may only queue; FrameEnd draws what was queued.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "shape_thumb.h"   // ShapeThumbBatch, ShapeThumbPicture: what the screen side reads of a shape

namespace host
{
	// What the screen side gives for one drawing: the triangles (ShapeThumb_Triangles at no turn: their corner shades are for the still light, and
	// ShapeThumb_LightTurned gives the light at any turn), the pictures by their judged paths (ShapeThumbPicture: read by the GPU side's own cache under the
	// visit's one allowance, never by the screen side), and the still camera (ShapeThumb_Camera's eye and at; turned round `at` about the world's y axis by
	// ShapePaneGpu_Draw's turn, as ShapeThumb_CameraTurned turns it)
	struct ShapePaneModel
	{
		std::vector<ShapeThumbBatch> batches;
		std::vector<ShapeThumbPicture> pictures;
		float eye[3] = { 0.0f, 0.0f, 0.0f };
		float at[3] = { 0.0f, 0.0f, 0.0f };
	};

	// The drawing put on the card for the sitting: its handle, or -1 with *why when it can't be (then the screen side keeps its still picture)
	int ShapePaneGpu_Upload(ShapePaneModel const &model, std::string *why);

	// One pane this frame: the drawing `handle` in windowBox (x, y, w, h, the window's pixels: pickerboxes'), its camera turned `turn` radians; queued only
	void ShapePaneGpu_Draw(int handle, int32_t const windowBox[4], float turn);

	// Once, at the end of the picker's callback: what was queued this frame drawn (into the GPU side's own render-target texture: Frost and Vasquez, 2026-11-23,
	// so the object card can lie under the panes: the panels' pass only queues what it draws, so a pane drawn straight onto the frame would be under the card)
	void ShapePaneGpu_FrameEnd();

	// In the panels' pass (ShapePickerScreen_Paint, at Dietrich's overPanels hook: over the object card, under the bar): one see-through quad queued per pane drawn
	// this frame, its picture into its windowBox (TRANSLUCENCY_NORMAL), before the picker's names, frame and bar
	void ShapePaneGpu_Composite();

	// One drawing let go: the picker keeps at most kShapePickerMostKept drawings and lets the least recently shown go past that (Frost, 2026-11-23: on the owner's
	// 352 tiles, paging to the end and back lets drawings go, and each is uploaded again when it is shown again), so its card memory goes with it, not only at the
	// close. A handle of -1, or one already let go, does nothing
	void ShapePaneGpu_Forget(int handle);

	// Every drawing let go (the picker's close)
	void ShapePaneGpu_ForgetAll();

	// THE DRAW PATH'S FIELDS on the state line (StateLine_Add; Frost, Vasquez and Dietrich, 2026-11-23), called from the picker's own StateFields:
	// pickerpanetime "cpu <us> gpu <us|unknown> most <us>": the last frame's panes on the processor and on the card (unknown when the card's clock can't be
	// asked, or changed speed), and the most on the processor this sitting; pickerpanedraw "live <0|1> drawn <n> frame <k> atlas <id> valid <0|1>[ why <words>]":
	// drawn live or not, the panes drawn last frame, a count of the frames they were drawn in, the atlas's number (-1 when none) and whether that number still
	// means the atlas (its slot's own name and usage)
	void ShapePaneGpu_StateFields();

	// THE TESTS' OWN (the editor calls it only in a hidden test run whose ini says PickerPaneQueriesRefused=1 under [Editor]): the card's clock taken as refused,
	// so a test can see the panes still drawn with "gpu unknown"
	void ShapePaneGpu_RefuseQueries();
}
