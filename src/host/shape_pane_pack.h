// shape_pane_pack.h: THE LIVE PANES' STORE, ITS PURE PART: a shape's triangles, as the picker made them ready (shape_thumb.h ShapeThumb_Triangles), packed into
// the corners and indices the card draws (shape_pane_store.h ShapePaneCorner), each triangle with its face's normal, every number checked first; and the card's
// memory counted against kShapePaneStoreMostBytes. No card here: the store's D3D part (Frost, shape_pane_store.cpp) calls these, then makes the buffers and the
// pictures. Bishop's split at Frost's seam, 2026-11-23; the interface as Frost proposed it. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// EVERY NUMBER IS HOSTILE (Tao 3): the triangles come from a stranger's shape file. Pack refuses, in words, and leaves nothing half packed: no triangle at all, a
// batch whose corners aren't whole triangles, a place or a picture place that isn't a finite number, a picture number below -1 or not below the pictures there
// are, more triangles than kShapePaneMostTriangles. A face with no area, or a normal that can't be made, is given 0, 0, 0: never a length of zero divided by.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "shape_pane_store.h"   // ShapePaneCorner, kShapePaneStoreMostBytes
#include "shape_thumb.h"        // ShapeThumbBatch

namespace host
{
	uint32_t const kShapePaneMostTriangles = 200000;   // a shape's triangles at most, and a frame's over every pane (Frost's shape_pane_plan.h takes it from here)

	// One batch packed: its picture (the model's own number, -1 drawn shaded), and its run of indices and corners (DrawIndexedPrimitive's own arguments)
	struct ShapePanePackedBatch
	{
		int picture = -1;
		uint32_t firstIndex = 0;
		uint32_t triangles = 0;
		uint32_t minVertex = 0;
		uint32_t vertexCount = 0;
	};

	// A shape packed: three corners a triangle, in batch order; the indices our own numbers 0, 1, 2 ... (each below corners.size()); the batches with a triangle
	// (one with none is left out); the triangles in all; and the bytes the two buffers take (corners x 32 + indices x 4)
	struct ShapePanePacked
	{
		std::vector<ShapePaneCorner> corners;
		std::vector<uint32_t> indices;
		std::vector<ShapePanePackedBatch> batches;
		uint32_t triangles = 0;
		uint64_t bufferBytes = 0;
	};

	// The batches packed (each corner's x, y, z, u, v copied; its shade dropped: the card lights it by the normal). pictureCount: how many pictures the model
	// names (a batch's picture must be -1, or 0 to pictureCount - 1). False, with *why in plain words and *packed left empty, for any refusal above.
	bool ShapePane_Pack(std::vector<ShapeThumbBatch> const &batches, size_t pictureCount, ShapePanePacked *packed, std::string *why);

	// Whether addBytes more fit on the card beside heldBytes, under kShapePaneStoreMostBytes. False, with *why ("the picker's card memory is full (96 MB)"),
	// when they would pass it. Worked out by subtraction, so no sum wraps round.
	bool ShapePane_RoomFor(uint64_t heldBytes, uint64_t addBytes, std::string *why);
}
