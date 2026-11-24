// See shape_pane_plan.h. Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_pane_plan.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "shape_thumb_render.h"   // ShapeThumb_ViewMatrix, ShapeThumb_LensMatrix: the still pictures' own view and lens

namespace
{
	// Whether a float is a number (not infinity, not NaN): read from its bits, because the editor builds with /fp:fast, under which a compiler may take every
	// number to be finite and drop a test a NaN fails (Drake's NIT on 0ae66bf9; shape_pane_pack.cpp reads them the same way)
	bool IsNumber(float f)
	{
		uint32_t bits = 0;
		memcpy(&bits, &f, sizeof(bits));
		return (bits & 0x7F800000u) != 0x7F800000u;
	}
}

namespace host
{
	std::vector<ShapePanePlace> ShapePane_Plan(std::vector<ShapePaneAsk> const &asks, uint32_t atlasSide, uint32_t mostTriangles)
	{
		std::vector<ShapePanePlace> places(asks.size());
		uint32_t x = 0, y = 0, shelf = 0;   // where the next pane goes on the shelf being filled, and that shelf's height so far
		uint32_t triangles = 0;             // the frame's, so far

		for (size_t i = 0; i < asks.size(); i++)
		{
			ShapePaneAsk const &ask = asks[i];
			ShapePanePlace &place = places[i];

			if (ask.box[2] <= 0 || ask.box[3] <= 0)
			{
				place.why = "its box has no size";
				continue;
			}

			uint32_t const w = (uint32_t)ask.box[2], h = (uint32_t)ask.box[3];

			if (w > atlasSide || h > atlasSide)
			{
				place.why = "its box (" + std::to_string(w) + " x " + std::to_string(h) + ") is larger than the panes' atlas (" + std::to_string(atlasSide) + " x " +
					std::to_string(atlasSide) + ")";
				continue;
			}

			// (by subtraction, so no count can wrap round and pass)
			if (ask.triangles > mostTriangles || triangles > mostTriangles - ask.triangles)
			{
				char most[32];   // the most, written as people write it: 200,000

				if (mostTriangles >= 1000)
				{
					snprintf(most, sizeof(most), "%u,%03u", mostTriangles / 1000, mostTriangles % 1000);
				}
				else
				{
					snprintf(most, sizeof(most), "%u", mostTriangles);
				}

				place.why = std::string("this frame's panes already draw as many triangles as a frame may (") + most + ")";
				continue;
			}

			// the next place on this shelf, else the start of a new shelf under it
			uint32_t px = x, py = y;

			if (w > atlasSide - x)
			{
				px = 0;
				py = y + shelf;

				if (py < y)
				{
					py = atlasSide;   // (can't happen with sides this small; never wrap)
				}
			}

			if (py > atlasSide || h > atlasSide - py)
			{
				place.why = "no room is left in the panes' atlas this frame";
				continue;
			}

			if (px == 0 && py != y)
			{
				y = py;
				shelf = 0;
			}

			place.drawn = true;
			place.region[0] = px;
			place.region[1] = py;
			place.region[2] = w;
			place.region[3] = h;
			x = px + w;
			shelf = h > shelf ? h : shelf;
			triangles += ask.triangles;
		}

		return places;
	}

	void ShapePane_TurnEye(float const eye[3], float const at[3], float turn, float turned[3])
	{
		// the way out from the middle to the eye, turned round the world's y axis, the way from +x toward +z (shape_thumb.cpp's own turn)
		float const out[3] = { eye[0] - at[0], eye[1] - at[1], eye[2] - at[2] };
		float const c = cosf(turn), s = sinf(turn);

		turned[0] = at[0] + out[0] * c - out[2] * s;
		turned[1] = at[1] + out[1];
		turned[2] = at[2] + out[0] * s + out[2] * c;
	}

	bool ShapePane_Matrix(float const eye[3], float const at[3], float fieldOfView, uint32_t width, uint32_t height, float matrix[4][4])
	{
		if (width == 0 || height == 0 || !IsNumber(fieldOfView))
		{
			return false;
		}

		for (int a = 0; a < 3; a++)
		{
			if (!IsNumber(eye[a]) || !IsNumber(at[a]))
			{
				return false;   // (before any arithmetic: a camera that isn't a number makes no matrix, whatever the compiler does with NaN)
			}
		}

		// the still pictures' own near and far (shape_thumb_render.cpp): from just in front of the shape's nearest possible point to past its farthest
		float const distance = sqrtf((at[0] - eye[0]) * (at[0] - eye[0]) + (at[1] - eye[1]) * (at[1] - eye[1]) + (at[2] - eye[2]) * (at[2] - eye[2]));
		float const nearPlane = distance * 0.3f > 1.0f ? distance * 0.3f : 1.0f;
		float view[4][4], lens[4][4];

		if (!(distance > 1e-3f) || !ShapeThumb_ViewMatrix(eye, at, view) || !ShapeThumb_LensMatrix(fieldOfView, nearPlane, distance * 2.0f + 1.0f, lens))
		{
			return false;
		}

		for (int r = 0; r < 4; r++)
		{
			for (int c = 0; c < 4; c++)
			{
				matrix[r][c] = view[r][0] * lens[0][c] + view[r][1] * lens[1][c] + view[r][2] * lens[2][c] + view[r][3] * lens[3][c];
			}
		}

		// fieldOfView across the shorter side: the longer one sees as much more as it is longer (its column of the glass drawn in by the ratio)
		float const across = width > height ? (float)height / (float)width : 1.0f;
		float const down = height > width ? (float)width / (float)height : 1.0f;

		for (int r = 0; r < 4; r++)
		{
			matrix[r][0] *= across;
			matrix[r][1] *= down;
		}

		return true;
	}
}
