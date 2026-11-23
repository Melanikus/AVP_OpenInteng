// The test of the placement picker's pictures drawn on the card (src/host/shape_thumb_render.h). Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It reads no file; like host_autolink_render_test it needs a Direct3D 9 device (a
// hidden window of its own, never in front), so it runs only where a display is free.
//   (1) THE MATRICES, pure: the view puts the target straight ahead at its distance, the world's +x to the right and its +y (down) down the picture; the 45-degree
//       camera's target is straight ahead too; the lens; a camera on its own target, and planes the wrong way round, refused
//   (2) THE SMOOTHING, pure: every 2 x 2 pixels averaged into one, alpha 255
//   (3) ON THE CARD: nothing drawn is the background only; a shaded square in front of the camera covers the middle, grey, its corners left as the background;
//       a textured one shows its picture's left texel on the left and its right one on the right; a see-through texel (alpha 0) leaves the background; a
//       picture of no size, or past the most, draws its polygons shaded; the picture is 96 x 96, 4 bytes a pixel
//   (4) "TRY LATER" IN THE RESULT (Drake's SHOULD 1 on df479439): not said for a drawn picture or a refused lens (a lost device, the one case that says it, can't
//       be made here)
// RED FIRST: on a stand-in renderer that draws nothing (and matrices of zeros), (1), (2) and (3) fail; (4) by mutation (Draw saying "try later" always): both fail.
#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <windows.h>

#include "shape_thumb.h"
#include "shape_thumb_render.h"

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

// a point through a row-vector matrix (x y z 1) x M: its first three (before the divide)
static void Through(float const m[4][4], float x, float y, float z, float out[3])
{
	for (int c = 0; c < 3; c++)
	{
		out[c] = x * m[0][c] + y * m[1][c] + z * m[2][c] + m[3][c];
	}
}

// a square of side 600 at z = 0 (the camera at z = -1000 looks at it), u from 0 on the left to 1 on the right
static ShapeThumbBatch Square(int picture)
{
	ShapeThumbBatch b;
	float const s = 300.0f;
	ShapeThumbCorner const c[4] = { { -s, -s, 0, 1.0f, 0.0f, 0.0f }, { s, -s, 0, 1.0f, 1.0f, 0.0f }, { s, s, 0, 1.0f, 1.0f, 1.0f }, { -s, s, 0, 1.0f, 0.0f, 1.0f } };

	b.picture = picture;

	for (int k : { 0, 1, 2, 0, 2, 3 })
	{
		b.corners.push_back(c[k]);
	}

	return b;
}

static void Pixel(std::vector<uint8_t> const &p, int x, int y, int *r, int *g, int *b)
{
	size_t const at = ((size_t)y * kShapeThumbSide + (size_t)x) * 4;
	*b = p[at];
	*g = p[at + 1];
	*r = p[at + 2];
}

int main()
{
	printf("host_shape_thumb_render_test: the picker's pictures\n");

	// ---- (1) THE MATRICES
	{
		float const eye[3] = { 0, 0, -1000 }, at[3] = { 0, 0, 0 };
		float view[4][4], p[3];

		Check(ShapeThumb_ViewMatrix(eye, at, view), "a view from (0, 0, -1000) to the middle is made");
		Through(view, 0, 0, 0, p);
		Check(fabsf(p[0]) < 1e-3f && fabsf(p[1]) < 1e-3f && fabsf(p[2] - 1000.0f) < 1e-2f, "the target is straight ahead at 1000");
		Through(view, 100, 0, 0, p);
		Check(p[0] > 99.0f && fabsf(p[1]) < 1e-3f, "the world's +x is to the right");
		Through(view, 0, 100, 0, p);
		Check(p[1] < -99.0f, "the world's +y (down) is down the picture (Direct3D's y up, so negative)");

		ShapeThumbModel m;
		m.centre[0] = 10;
		m.centre[1] = -500;
		m.centre[2] = 20;
		m.radius = 1500;

		float e[3], a[3];
		ShapeThumb_Camera(m, 0.7f, e, a);
		Check(ShapeThumb_ViewMatrix(e, a, view), "the 45-degree camera's view is made");
		Through(view, a[0], a[1], a[2], p);

		float const d = sqrtf((a[0] - e[0]) * (a[0] - e[0]) + (a[1] - e[1]) * (a[1] - e[1]) + (a[2] - e[2]) * (a[2] - e[2]));
		Check(fabsf(p[0]) < 0.05f && fabsf(p[1]) < 0.05f && fabsf(p[2] - d) < 0.5f, "and its target is straight ahead at its distance");
		Through(view, a[0], a[1] - 100, a[2], p);
		Check(p[1] > 10.0f, "and up the world (-y) is up the picture");

		float lens[4][4];
		Check(ShapeThumb_LensMatrix(0.7f, 10.0f, 1000.0f, lens) && fabsf(lens[0][0] - 1.0f / tanf(0.35f)) < 1e-5f && lens[2][3] == 1.0f, "the lens");
		Check(!ShapeThumb_ViewMatrix(eye, eye, view) && !ShapeThumb_LensMatrix(0.7f, 10.0f, 5.0f, lens) && !ShapeThumb_LensMatrix(0.7f, 0.0f, 5.0f, lens) &&
			!ShapeThumb_LensMatrix(0.0f, 1.0f, 5.0f, lens), "a camera on its target, and planes the wrong way round, are refused");
	}

	// ---- (2) THE SMOOTHING
	{
		std::vector<uint8_t> in(4 * 4 * 4, 0), out;

		for (int y = 0; y < 4; y++)
		{
			for (int x = 0; x < 4; x++)
			{
				in[(y * 4 + x) * 4 + 2] = (uint8_t)(x < 2 ? (y == 0 ? 100 : 200) : 0);   // red, the left 2 x 2: 100 100 over 200 200
				in[(y * 4 + x) * 4 + 0] = (uint8_t)(x >= 2 ? 40 : 0);                    // blue on the right
			}
		}

		ShapeThumb_HalveSmooth(in.data(), 4, 16, &out);
		Check(out.size() == 2 * 2 * 4 && out[2] == 150 && out[0] == 0 && out[4 + 0] == 40 && out[4 + 2] == 0 && out[3] == 255,
			"2 x 2 into one: (100 + 100 + 200 + 200) / 4 = 150 red on the left, the right's blue 40, alpha 255 (" + std::to_string(out.size() >= 3 ? out[2] : -1) + ")");
	}

	// ---- (3) ON THE CARD
	ShapeThumbRenderer renderer;
	std::string why;

	if (!renderer.Open(&why))
	{
		printf("  FAILED  a Direct3D 9 device: %s\n", why.c_str());
		printf("host_shape_thumb_render_test: %d FAILED (no device)\n", checksFailed + 1);
		return 1;
	}

	float const eye[3] = { 0, 0, -1000 }, at[3] = { 0, 0, 0 };
	uint32_t const background = 0x203040;
	std::vector<uint8_t> picture;
	int r = 0, g = 0, b = 0;

	Check(renderer.Draw({}, {}, eye, at, 0.7f, background, &picture, &why) && picture.size() == (size_t)kShapeThumbSide * kShapeThumbSide * 4, "nothing drawn: a picture of 96 x 96 (" + why + ")");

	bool allBackground = picture.size() == (size_t)kShapeThumbSide * kShapeThumbSide * 4;

	for (size_t i = 0; allBackground && i < picture.size(); i += 4)
	{
		allBackground = picture[i] == 0x40 && picture[i + 1] == 0x30 && picture[i + 2] == 0x20 && picture[i + 3] == 255;
	}

	Check(allBackground, "and it is the background only");

	// a shaded square
	Check(renderer.Draw({ Square(-1) }, {}, eye, at, 0.7f, background, &picture, &why), "a shaded square is drawn (" + why + ")");
	Pixel(picture, 48, 48, &r, &g, &b);
	Check(r == g && g == b && r > 150, "its middle is grey (" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")");
	Pixel(picture, 1, 1, &r, &g, &b);
	Check(r == 0x20 && g == 0x30 && b == 0x40, "its corners are the background (the square's edge is 82% of the way out)");

	// a textured one: red on the left texel, blue on the right
	ShapeThumbTexture halves;
	halves.width = 2;
	halves.height = 1;
	halves.bgra = { 0, 0, 255, 255, 255, 0, 0, 255 };

	Check(renderer.Draw({ Square(0) }, { halves }, eye, at, 0.7f, background, &picture, &why), "a textured square is drawn (" + why + ")");
	Pixel(picture, 24, 48, &r, &g, &b);
	Check(r > b + 60, "its left shows the picture's left texel, red (" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")");
	Pixel(picture, 72, 48, &r, &g, &b);
	Check(b > r + 60, "its right the right one, blue (" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")");

	// see-through: alpha 0 on the left
	ShapeThumbTexture clear = halves;
	clear.bgra[3] = 0;
	Check(renderer.Draw({ Square(0) }, { clear }, eye, at, 0.7f, background, &picture, &why), "a square with a see-through texel is drawn");
	Pixel(picture, 10, 48, &r, &g, &b);
	Check(r == 0x20 && g == 0x30 && b == 0x40, "where the texel is see-through, the background shows (" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")");

	// no size, and past the most: shaded
	ShapeThumbTexture none, huge;
	huge.width = kShapeThumbMostTextureSide + 1;
	huge.height = 1;
	Check(renderer.Draw({ Square(0) }, { none }, eye, at, 0.7f, background, &picture, &why), "a picture of no size: drawn");
	Pixel(picture, 24, 48, &r, &g, &b);
	Check(r == g && g == b && r > 150, "shaded, grey");
	Check(renderer.Draw({ Square(0) }, { huge }, eye, at, 0.7f, background, &picture, &why), "a picture past the most (its pixels not even there): drawn");
	Pixel(picture, 24, 48, &r, &g, &b);
	Check(r == g && g == b, "shaded too");

	// a lens wider than the renderer frames is refused, not drawn wrong (Drake's SHOULD 2 on 66a5284e: its near plane holds only so wide); and it is the
	// picture's fault, not the card's, so the result doesn't say "try later" (Drake's SHOULD 1 on df479439: the caller reads that, not the words). (The call
	// is made before its Check: MSVC builds a check's words first.)
	bool tryLater = true;
	bool const wide = renderer.Draw({ Square(-1) }, {}, eye, at, kShapeThumbMostFieldOfView + 0.5f, background, &picture, &why, &tryLater);
	Check(!wide && why.find("wider") != std::string::npos && !tryLater, "(4) a lens wider than the most is refused, and not as \"try later\" (" + why + ")");

	tryLater = true;
	bool const drew = renderer.Draw({ Square(-1) }, {}, eye, at, 0.7f, background, &picture, &why, &tryLater);
	Check(drew && !tryLater, "(4) a picture drawn: \"try later\" not said (only a lost device says it, which a test here can't make)");

	renderer.Close();
	printf(checksFailed ? "host_shape_thumb_render_test: %d FAILED\n" : "host_shape_thumb_render_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
