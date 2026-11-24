// shape_pane_store.h: THE LIVE PANES' STORE on the card: each listed shape's drawing, put on the card once a sitting, kept by a handle (the owner, 2026-11-23:
// "The shapes don't change either (maps or shapes) they should live in the GPU", "3d render viewports rotating are back along all bug fixes previously planned").
// Bishop's split of the GPU side, agreed by Frost and Vasquez, 2026-11-23: this store (Vasquez) implements shape_pane_gpu.h's ShapePaneGpu_Upload,
// ShapePaneGpu_Forget and ShapePaneGpu_ForgetAll; the draw path (Frost, shape_pane_gpu.cpp) implements ShapePaneGpu_Draw, _FrameEnd, _Composite and _FrameTime,
// and reads a drawing here by ShapePaneStore_Find. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng, the INTERFACE first, on its own, so both halves
// build against the same file from the start. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT THE STORE PROMISES: everything it makes is in Direct3D's MANAGED pool (the card's copy is Direct3D's to keep across a device reset); at most
// kShapePaneStoreMostBytes on the card in all, an upload past it refused in words; every corner number checked against the points before anything is made
// (a hostile shape's index past its points is refused, never read); each picture copied row by row by the texture's own pitch; every failure to make a buffer or a
// texture said in *why, nothing half made left behind; the pictures read as the picker reads them (shape_picker_screen.h ShapePickerScreen_ReadPicture). One
// thread: the editor's own (the main thread), as the rest of the picker.
#pragma once

#include <stdint.h>

#include <vector>

struct IDirect3DTexture9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;

namespace host
{
	uint64_t const kShapePaneStoreMostBytes = 96ull * 1024 * 1024;   // the card's memory the panes may take, in all (buffers and pictures)

	// One corner as the card draws it (32 bytes: D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1): its place (the world's mm, y down), its face's normal (one long;
	// 0, 0, 0 when the face has no area: a turning pane's shade changes with the turn, and both sides are lit alike, |n . l|), its place in its picture (0 to 1)
	struct ShapePaneCorner
	{
		float x, y, z;
		float nx, ny, nz;
		float u, v;
	};

	// The triangles of one picture (or none: shaded only), a run of the drawing's indices: firstIndex the first of them, triangles how many, and the corners they
	// name, minVertex to minVertex + vertexCount - 1 (DrawIndexedPrimitive's own arguments)
	struct ShapePaneBatch
	{
		IDirect3DTexture9 *picture;   // null: drawn shaded
		uint32_t firstIndex;
		uint32_t triangles;
		uint32_t minVertex;
		uint32_t vertexCount;
	};

	// One shape's drawing on the card: its corners and its 32-bit indices (MANAGED pool), its batches in drawing order, its triangles in all, and the still camera
	// it is framed from (shape_thumb.h ShapeThumb_Camera's eye and at; the draw path turns it)
	struct ShapePaneDrawing
	{
		IDirect3DVertexBuffer9 *corners;
		IDirect3DIndexBuffer9 *indices;
		std::vector<ShapePaneBatch> batches;
		uint32_t triangles;
		float eye[3];
		float at[3];
	};

	// The drawing of a handle ShapePaneGpu_Upload gave; null for -1, a handle never given, or one let go (ShapePaneGpu_Forget, _ForgetAll). The pointer is
	// good until that handle is let go. Main thread only.
	ShapePaneDrawing const *ShapePaneStore_Find(int handle);
}
