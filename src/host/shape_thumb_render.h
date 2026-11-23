// shape_thumb_render.h: the placement picker's pictures drawn by Direct3D 9: a shape made ready by shape_thumb.h, its pictures' pixels as the host read them,
// drawn small, textured and lit, into 96 x 96 pixels read back into memory. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// AS AUTOLINK'S PICTURES ARE (autolink_render.h, Drake): a Direct3D 9 device of its OWN on a hidden window of its own, so it shares nothing with the editor's
// view (no texture of the level's is touched, none of the picker's is the level's), and the window never takes the front. One thread at a time uses one
// renderer (the editor's own, a few pictures a frame). Each picture is drawn twice as large (192 x 192) with a depth buffer, no culling (both sides drawn),
// the shade worked out already in the corners (shape_thumb.h), each picture's pixels a texture for that drawing only, a texel whose alpha is under half left
// out (a picture's see-through colour, as the game draws it); then read back and every 2 x 2 pixels made one, so its edges are smooth. A device that is lost
// is had back when it can be (Reset), as AutoLink's is; until then a picture says why and is tried again later.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "shape_thumb.h"

namespace host
{
	int32_t const kShapeThumbSide = 96;              // the picture's side, in pixels (shape_picker.h kShapePickerPicture)
	int32_t const kShapeThumbDrawSide = 192;         // drawn at twice that, then made smooth
	uint32_t const kShapeThumbMostTextureSide = 4096;   // a picture larger than this either way is drawn without it (RimLoader's own most is 2048 x 2048 pixels)
	float const kShapeThumbMostFieldOfView = 1.0f;      // the widest lens Draw takes, in radians (about 57 degrees): its near plane, 0.3 of the distance, holds a framed
	                                                    // shape up to about 89 degrees (Drake's SHOULD 2 on 66a5284e); the picker's own is 0.7

	// A picture's pixels as the host read it: 4 bytes a pixel, blue green red alpha, the top row first, rows packed (width x 4 bytes)
	struct ShapeThumbTexture
	{
		uint32_t width = 0, height = 0;
		std::vector<uint8_t> bgra;
	};

	class ShapeThumbRenderer
	{
	public:
		ShapeThumbRenderer();
		~ShapeThumbRenderer();

		// Makes the device on a hidden window of its own (call from the thread that will draw). False, with *why, when Direct3D 9 or the card can't give one.
		bool Open(std::string *why);
		void Close();
		bool IsOpen() const;

		// One picture: the batches (shape_thumb.h ShapeThumb_Triangles) drawn from eye toward at (the world's mm, y down) in a view fieldOfView radians
		// across, each batch with textures[its picture] (one that is empty, or past kShapeThumbMostTextureSide, draws its batch shaded), on the background
		// colour (0xRRGGBB), into *bgra: kShapeThumbSide x kShapeThumbSide pixels, 4 bytes each, blue green red alpha 255, the top row first. False with
		// *why when the device is lost and can't be had back yet (try again later), a lens wider than kShapeThumbMostFieldOfView, or a step of the drawing fails.
		// *tryLater (when given) is true only for the first: the card is busy, so the caller tries again in a later frame instead of failing the picture (Drake's
		// SHOULD 1 on df479439: the caller read it from the words before). Every allocation is made before the scene begins, so nothing inside it can throw
		// (Drake's SHOULD 1 on 66a5284e).
		bool Draw(std::vector<ShapeThumbBatch> const &batches, std::vector<ShapeThumbTexture> const &textures, float const eye[3], float const at[3], float fieldOfView,
			uint32_t background, std::vector<uint8_t> *bgra, std::string *why, bool *tryLater = nullptr);

	private:
		struct Parts;
		Parts *parts;

		ShapeThumbRenderer(ShapeThumbRenderer const &) = delete;
		ShapeThumbRenderer &operator=(ShapeThumbRenderer const &) = delete;
	};

	// THE CAMERA'S TWO MATRICES (pure, so the test checks them without a card), row-vector form as Direct3D 9 takes them (4 x 4, m[row][column]):
	// the view from eye toward at (the world's y is down; Direct3D's is up, so the camera's down row is turned over), and the lens: fieldOfView across,
	// square, depth from nearPlane to farPlane. False for an eye on its target, or planes that aren't nearPlane > 0 < farPlane > nearPlane.
	bool ShapeThumb_ViewMatrix(float const eye[3], float const at[3], float view[4][4]);
	bool ShapeThumb_LensMatrix(float fieldOfView, float nearPlane, float farPlane, float lens[4][4]);

	// Every 2 x 2 pixels of a picture side x side (4 bytes a pixel, BGRA or BGRX) made one, the four averaged, into (side / 2) x (side / 2) with alpha 255
	void ShapeThumb_HalveSmooth(uint8_t const *pixels, int32_t side, int32_t pitch, std::vector<uint8_t> *out);
}
