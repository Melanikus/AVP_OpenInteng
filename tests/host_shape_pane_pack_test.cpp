// The test of the live panes' store, its pure part (src/host/shape_pane_pack.h). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code". Pure: no card, no file; the triangles are made here.
//   (1) PACKED: two batches and an empty one: the corners three a triangle in batch order, x y z u v copied, the indices 0 1 2 ..., each batch's run of indices and
//       corners, the empty batch left out, the triangles counted, the buffers' bytes (corners x 32 + indices x 4)
//   (2) THE FACE'S NORMAL: one long, the same for a face ten thousand times larger, on each of its three corners
//   (3) NO NORMAL MADE FROM NOTHING: a face with no area (three corners on a line), one too small for its area to be a number, one whose sides are so long the
//       normal's sums are past a float: 0, 0, 0, and never a number that isn't finite
//   (4) HOSTILE TRIANGLES REFUSED, in words, nothing half packed: no triangle at all, corners that aren't whole triangles, a place or a picture place that isn't
//       finite (NaN, infinity), a picture number below -1 or not below the pictures there are, more than kShapePaneMostTriangles triangles
//   (5) ROOM ON THE CARD: up to kShapePaneStoreMostBytes in all fits; one byte more doesn't, said with the most; sums near the top of 64 bits never wrap round
// The checks run with the editor's own /fp:fast, under which a compiler may take every number to be finite: the code reads a float's bits to know.
// RED FIRST: on the stand-in (nothing packed, no room), (1) to (5) fail but the refusals' own checks.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "shape_pane_pack.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static ShapeThumbCorner Corner(float x, float y, float z, float u = 0.0f, float v = 0.0f)
{
	ShapeThumbCorner c;
	c.x = x;
	c.y = y;
	c.z = z;
	c.shade = 0.5f;
	c.u = u;
	c.v = v;
	return c;
}

static ShapeThumbBatch Batch(int picture, std::vector<ShapeThumbCorner> const &corners)
{
	ShapeThumbBatch b;
	b.picture = picture;
	b.corners = corners;
	return b;
}

// a float's bits say whether it is a number (the exponent's bits not all ones): what /fp:fast can't take away
static bool IsNumber(float f)
{
	uint32_t bits = 0;
	memcpy(&bits, &f, sizeof(bits));
	return (bits & 0x7F800000u) != 0x7F800000u;
}

static float NotANumber()
{
	uint32_t const bits = 0x7FC00000u;
	float f = 0.0f;
	memcpy(&f, &bits, sizeof(f));
	return f;
}

static float Infinity()
{
	uint32_t const bits = 0x7F800000u;
	float f = 0.0f;
	memcpy(&f, &bits, sizeof(f));
	return f;
}

static bool Refused(std::vector<ShapeThumbBatch> const &batches, size_t pictures, std::string const &what)
{
	ShapePanePacked packed;
	std::string why;

	packed.triangles = 7;   // (anything there before is cleared)

	bool const done = ShapePane_Pack(batches, pictures, &packed, &why);
	bool const refused = !done && !why.empty() && packed.corners.empty() && packed.indices.empty() && packed.batches.empty() && packed.triangles == 0 &&
		packed.bufferBytes == 0;

	Check(refused, "(4) " + what + ": refused, nothing packed (" + why + ")");
	return refused;
}

int main()
{
	printf("host_shape_pane_pack_test: the live panes' store, its pure part\n");

	// ---- (1) PACKED
	{
		// batch A, picture 0: two triangles; an empty batch, picture 1; batch B, shaded: one triangle
		std::vector<ShapeThumbBatch> const batches = {
			Batch(0, { Corner(0, 0, 0, 0.0f, 0.0f), Corner(10, 0, 0, 1.0f, 0.0f), Corner(0, 0, 10, 0.0f, 1.0f),
				Corner(10, 0, 0, 1.0f, 0.0f), Corner(10, 0, 10, 1.0f, 1.0f), Corner(0, 0, 10, 0.0f, 1.0f) }),
			Batch(1, {}),
			Batch(-1, { Corner(0, -5, 0), Corner(5, -5, 0), Corner(0, -5, 5) }),
		};
		ShapePanePacked packed;
		std::string why;
		bool const done = ShapePane_Pack(batches, 2, &packed, &why);

		Check(done && packed.corners.size() == 9 && packed.indices.size() == 9 && packed.triangles == 3, "(1) packed: 9 corners, 9 indices, 3 triangles (" + why + ")");

		bool countUp = packed.indices.size() == 9;

		for (size_t i = 0; countUp && i < packed.indices.size(); i++)
		{
			countUp = packed.indices[i] == (uint32_t)i;
		}

		Check(countUp, "(1) the indices are 0, 1, 2 ... 8");
		Check(packed.corners.size() == 9 && packed.corners[1].x == 10 && packed.corners[1].u == 1.0f && packed.corners[2].z == 10 && packed.corners[2].v == 1.0f &&
			packed.corners[6].y == -5, "(1) each corner's x, y, z, u, v copied, in batch order");

		bool const twoBatches = packed.batches.size() == 2;
		Check(twoBatches && packed.batches[0].picture == 0 && packed.batches[0].firstIndex == 0 && packed.batches[0].triangles == 2 && packed.batches[0].minVertex == 0 &&
			packed.batches[0].vertexCount == 6, "(1) batch A: picture 0, indices from 0, 2 triangles, corners 0 to 5");
		Check(twoBatches && packed.batches[1].picture == -1 && packed.batches[1].firstIndex == 6 && packed.batches[1].triangles == 1 && packed.batches[1].minVertex == 6 &&
			packed.batches[1].vertexCount == 3, "(1) batch B: shaded, indices from 6, 1 triangle, corners 6 to 8; the empty batch left out");
		Check(packed.bufferBytes == 9 * 32 + 9 * 4, "(1) the buffers' bytes: 9 x 32 + 9 x 4 = 324 (" + std::to_string((unsigned long long)packed.bufferBytes) + ")");
		Check(sizeof(ShapePaneCorner) == 32, "(1) a corner is 32 bytes, as D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 reads it");
	}

	// ---- (2) THE FACE'S NORMAL: a, b, c in the floor (y = 0): (b - a) x (c - a) = (1, 0, 0) x (0, 0, 1) = (0, -1, 0)
	{
		for (float const scale : { 1.0f, 10000.0f })
		{
			ShapePanePacked packed;
			std::string why;
			bool const done = ShapePane_Pack({ Batch(-1, { Corner(0, 0, 0), Corner(scale, 0, 0), Corner(0, 0, scale) }) }, 0, &packed, &why);
			bool same = done && packed.corners.size() == 3;

			for (size_t i = 0; same && i < 3; i++)
			{
				ShapePaneCorner const &c = packed.corners[i];
				same = fabsf(c.nx) < 1e-6f && fabsf(c.ny + 1.0f) < 1e-6f && fabsf(c.nz) < 1e-6f;
			}

			Check(same, "(2) the floor's face, its sides " + std::to_string((int)scale) + " long: each corner's normal 0, -1, 0, one long (" + why + ")");
		}
	}

	// ---- (3) NO NORMAL MADE FROM NOTHING
	{
		struct Face
		{
			char const *what;
			ShapeThumbCorner a, b, c;
		};

		Face const faces[] = {
			{ "three corners on a line", Corner(0, 0, 0), Corner(1, 1, 1), Corner(2, 2, 2) },
			{ "a face too small for its area to be a number (sides 1e-30)", Corner(0, 0, 0), Corner(1e-30f, 0, 0), Corner(0, 0, 1e-30f) },
			{ "sides so long the normal's sums are past a float (3e38)", Corner(-3e38f, 0, 0), Corner(3e38f, 0, 0), Corner(0, 0, 3e38f) },
		};

		for (Face const &f : faces)
		{
			ShapePanePacked packed;
			std::string why;
			bool const done = ShapePane_Pack({ Batch(-1, { f.a, f.b, f.c }) }, 0, &packed, &why);
			bool zero = done && packed.corners.size() == 3;

			for (size_t i = 0; zero && i < 3; i++)
			{
				ShapePaneCorner const &c = packed.corners[i];
				zero = IsNumber(c.nx) && IsNumber(c.ny) && IsNumber(c.nz) && c.nx == 0.0f && c.ny == 0.0f && c.nz == 0.0f;
			}

			Check(zero, std::string("(3) ") + f.what + ": packed, its normal 0, 0, 0 (" + why + ")");
		}
	}

	// ---- (4) HOSTILE TRIANGLES REFUSED
	{
		std::vector<ShapeThumbCorner> const one = { Corner(0, 0, 0), Corner(1, 0, 0), Corner(0, 0, 1) };

		Refused({}, 0, "no batch at all");
		Refused({ Batch(-1, {}), Batch(0, {}) }, 1, "batches with no triangle");
		Refused({ Batch(-1, { Corner(0, 0, 0), Corner(1, 0, 0), Corner(0, 0, 1), Corner(1, 1, 1) }) }, 0, "four corners: not whole triangles");

		std::vector<ShapeThumbCorner> nan = one, inf = one, uvNan = one, uvInf = one;
		nan[1].y = NotANumber();
		inf[2].z = Infinity();
		uvNan[0].u = NotANumber();
		uvInf[1].v = -Infinity();
		Refused({ Batch(-1, nan) }, 0, "a place that is NaN");
		Refused({ Batch(-1, inf) }, 0, "a place that is infinite");
		Refused({ Batch(0, uvNan) }, 1, "a picture place that is NaN");
		Refused({ Batch(0, uvInf) }, 1, "a picture place that is minus infinity");
		Refused({ Batch(-2, one) }, 0, "picture -2");
		Refused({ Batch(3, one) }, 3, "picture 3 of 3 (0 to 2 there)");
		Refused({ Batch(0, one) }, 0, "picture 0 of none");

		ShapeThumbBatch many;
		many.picture = -1;
		many.corners.assign((size_t)(kShapePaneMostTriangles + 1) * 3, Corner(0, 0, 0));
		Refused({ many }, 0, "one triangle more than kShapePaneMostTriangles");

		// and the most is still taken
		ShapeThumbBatch most = many;
		most.corners.resize((size_t)kShapePaneMostTriangles * 3);
		ShapePanePacked packed;
		std::string why;
		bool const done = ShapePane_Pack({ most }, 0, &packed, &why);
		Check(done && packed.triangles == kShapePaneMostTriangles, "(4) exactly kShapePaneMostTriangles: packed (" + why + ")");
	}

	// ---- (5) ROOM ON THE CARD
	{
		std::string why;
		uint64_t const most = kShapePaneStoreMostBytes, top = ~0ull;

		bool const all = ShapePane_RoomFor(0, most, &why);
		Check(all, "(5) nothing held, the most added: fits");
		bool const oneMore = ShapePane_RoomFor(0, most + 1, &why);
		Check(!oneMore && why.find("96 MB") != std::string::npos, "(5) one byte past the most: no room, said with the most (" + why + ")");
		bool const fullNothing = ShapePane_RoomFor(most, 0, &why);
		Check(fullNothing, "(5) the most held, nothing added: fits");
		bool const fullOne = ShapePane_RoomFor(most, 1, &why);
		Check(!fullOne, "(5) the most held, one byte added: no room");
		bool const wrapA = ShapePane_RoomFor(top, 1, &why), wrapB = ShapePane_RoomFor(1, top, &why), wrapC = ShapePane_RoomFor(top, top, &why);
		Check(!wrapA && !wrapB && !wrapC, "(5) sums near the top of 64 bits: no room, never wrapped round to a small one");
		bool const over = ShapePane_RoomFor(most + 5, 0, &why);
		Check(!over, "(5) more than the most already held: no room");
	}

	printf(checksFailed ? "host_shape_pane_pack_test: %d FAILED\n" : "host_shape_pane_pack_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
