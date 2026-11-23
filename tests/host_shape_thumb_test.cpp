// The test for a shape file made ready to draw small (src/host/shape_thumb.h), the placement picker's tiles. Written 2026-11-23 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: the files are made here, byte by byte.
//   (1) A FILE OF ONE SHAPE: its points, polygons, middle and radius; a textured quad's picture found by its list entry's own INDEX, its path "<RIFFNAME>/<name>",
//       its corners' texels; a plain triangle shaded; the triangles grouped by picture, the texels over the picture's size, both sides lit alike, a picture that
//       couldn't be read drawn shaded
//   (2) THE GAME'S RULES for the texture list: the IFF flag's path is the name alone; an entry not for the PC sets no picture; of two entries of one index the
//       LAST wins (avpx chunk_load.cpp setup_tex_conv_array)
//   (3) A FRAGMENT FILE: its parts counted, its first drawn
//   (4) HOSTILE FILES, each refused in words, never read past: cut short, no shape, a corner that is no point, past the limits; a broken texture list or texture
//       coordinates draw the shape shaded and never refuse it
//   (5) THE CAMERA: from the 45-degree corner, far enough to frame the radius
//   (6) A PICTURE'S PATH IS THE FILE'S OWN BYTES (Frost's review of 4f0db774, MUST 1): held to the level open's rule (avpx level_file_name.h): "..\..\x",
//       "C:\x", "a.rim:stream", "\\server\share\x", a device's name, and a RIFFNAME of ".." are refused, with why and no path (its polygons shaded);
//       a stock name is taken
// RED FIRST: on a stand-in that reads nothing, (1) to (5) fail; (6) on 4f0db774 (every path taken as the file gave it).
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "shape_import.h"   // shape_chunks: MakeChunk, AddInt
#include "shape_thumb.h"

using namespace host;
using namespace host::shape_chunks;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void AddFloat(Bytes *b, float f)
{
	int32_t i;
	memcpy(&i, &f, sizeof(i));
	AddInt(b, i);
}

static void AddText(Bytes *b, std::string const &text)
{
	b->insert(b->end(), text.begin(), text.end());
	b->push_back(0);
	PadToFour(b);
}

// A square of side 2000 at y = 0 (points 0 to 3) and one more point over it (4): a textured quad (engine type 5, picture `index`, list 0) and a plain
// triangle (type 0). Corners may be changed by the caller (a hostile one).
struct Made
{
	int32_t quadIndex = 3;
	int32_t triangleLast = 4;      // the triangle's third corner
	size_t points = 5;             // how many points SHPRAWVT holds (past the limit: "too big")
	bool uvBroken = false;         // the list's corner count past 4
	bool namesBroken = false;      // BMPNAMES cut inside its first name
	std::vector<std::vector<std::string> > entries = { { "0", "3", "Pikup.rim" } };   // flags, index, name
	int shapes = 1;
	std::string riff = "Pulrak";   // the file's RIFFNAME
};

static Bytes Shape(Made const &m)
{
	Bytes head, points, polygons, uv;

	AddInt(&head, 0);
	head.insert(head.end(), 16, 0);
	AddInt(&head, 7);                              // its number
	AddInt(&head, (int32_t)m.points);
	AddInt(&head, 2);
	AddFloat(&head, 1500.0f);

	for (int32_t v : { 1000, -1000, 0, -1000, 1000, -1000 })
	{
		AddInt(&head, v);
	}

	AddInt(&head, 0);
	AddInt(&head, 0);

	for (size_t i = 0; i < m.points; i++)
	{
		int32_t const xyz[5][3] = { { -1000, 0, -1000 }, { 1000, 0, -1000 }, { 1000, 0, 1000 }, { -1000, 0, 1000 }, { 0, -1000, 0 } };
		int32_t const *const p = xyz[i < 5 ? i : 4];

		AddInt(&points, p[0]);
		AddInt(&points, p[1]);
		AddInt(&points, p[2]);
	}

	// the quad: engine type 5 (textured), normal 0, flags 0, colour = picture | list 0 << 16, corners 0 1 2 3
	for (int32_t v : { 5, 0, 0, m.quadIndex, 0, 1, 2, 3, -1 })
	{
		AddInt(&polygons, v);
	}

	// the triangle: type 0, corners 0 1 and the given one
	for (int32_t v : { 0, 0, 0, 0, 0, 1, m.triangleLast, -1, -1 })
	{
		AddInt(&polygons, v);
	}

	AddInt(&uv, 1);
	AddInt(&uv, m.uvBroken ? 9 : 4);

	for (float t : { 0.0f, 0.0f, 32.0f, 0.0f, 32.0f, 64.0f, 0.0f, 64.0f })
	{
		AddFloat(&uv, t);
	}

	Bytes body;

	for (Bytes const &c : { MakeChunk("SHPHEAD1", head), MakeChunk("SHPRAWVT", points), MakeChunk("SHPPOLYS", polygons), MakeChunk("SHPUVCRD", uv) })
	{
		body.insert(body.end(), c.begin(), c.end());
	}

	return MakeChunk("REBSHAPE", body);
}

static Bytes File(Made const &m)
{
	Bytes names, rif, env, whole;

	AddInt(&names, (int32_t)m.entries.size());

	for (std::vector<std::string> const &e : m.entries)
	{
		AddInt(&names, (int32_t)strtoul(e[0].c_str(), nullptr, 0));
		AddInt(&names, atoi(e[1].c_str()));
		AddInt(&names, 0);
		AddInt(&names, 0);
		AddInt(&names, 0);
		AddText(&names, e[2]);
	}

	if (m.namesBroken)
	{
		names.resize(4 + 20 + 2);   // the first name cut with no end
	}

	AddText(&rif, m.riff);

	for (Bytes const &c : { MakeChunk("BMPNAMES", names), MakeChunk("RIFFNAME", rif) })
	{
		env.insert(env.end(), c.begin(), c.end());
	}

	for (int s = 0; s < m.shapes; s++)
	{
		Bytes const shape = Shape(m);
		whole.insert(whole.end(), shape.begin(), shape.end());
	}

	Bytes const e = MakeChunk("REBENVDT", env);
	whole.insert(whole.end(), e.begin(), e.end());
	return MakeChunk("REBINFF2", whole);
}

int main()
{
	printf("host_shape_thumb_test: a shape file made ready to draw small\n");

	// ---- (1) A FILE OF ONE SHAPE
	{
		ShapeThumbModel m;
		std::string why;
		bool const read = ShapeThumb_Read(File(Made()), &m, &why);

		Check(read && m.parts == 1 && m.points.size() == 15 && m.polygons.size() == 2, "read: 1 part, 5 points, 2 polygons (" + why + ")");
		Check(read && fabsf(m.centre[0]) < 0.01f && fabsf(m.centre[1] + 500.0f) < 0.01f && fabsf(m.centre[2]) < 0.01f && fabsf(m.radius - sqrtf(2000000.0f + 250000.0f)) < 0.5f,
			"its middle is its box's (0, -500, 0) and its radius the farthest point's (" + std::to_string(m.radius) + ")");
		Check(read && m.pictures.size() == 1 && m.pictures[0].index == 3 && m.pictures[0].path == "Pulrak/Pikup.rim" && m.polygons[0].picture == 0 &&
			m.polygons[0].texels[2][0] == 32.0f && m.polygons[0].texels[2][1] == 64.0f, "the quad's picture is entry index 3, Pulrak/Pikup.rim, its corner 2 at texel (32, 64)");
		Check(read && m.polygons[1].picture == -1, "the plain triangle has no picture");

		std::vector<ShapeThumbBatch> const drawn = ShapeThumb_Triangles(m, { { 32, 64 } });
		ShapeThumbBatch const *textured = nullptr, *shaded = nullptr;

		for (ShapeThumbBatch const &b : drawn)
		{
			(b.picture == 0 ? textured : shaded) = &b;
		}

		Check(drawn.size() == 2 && textured && shaded && textured->corners.size() == 6 && shaded->corners.size() == 3,
			"two batches: the quad as 2 triangles with its picture, the triangle shaded");
		Check(textured && textured->corners.size() == 6 && textured->corners[2].u == 1.0f && textured->corners[2].v == 1.0f && textured->corners[4].u == 1.0f &&
			textured->corners[5].u == 0.0f && textured->corners[5].v == 1.0f, "the quad's fan (0 1 2, 0 2 3), its texels over the picture's 32 x 64");
		Check(textured && textured->corners[0].shade > 0.35f && textured->corners[0].shade <= 1.0f, "lit: its shade over the light's least (" +
			(textured ? std::to_string(textured->corners[0].shade) : std::string("-")) + ")");

		// the same quad turned over (its corners the other way round) is lit alike
		ShapeThumbModel flipped = m;
		std::swap(flipped.polygons[0].corners[1], flipped.polygons[0].corners[3]);
		std::vector<ShapeThumbBatch> const other = ShapeThumb_Triangles(flipped, { { 32, 64 } });
		float shadeOther = -1.0f;

		for (ShapeThumbBatch const &b : other)
		{
			shadeOther = b.picture == 0 && !b.corners.empty() ? b.corners[0].shade : shadeOther;
		}

		Check(textured && fabsf(shadeOther - textured->corners[0].shade) < 1e-5f, "both sides of a face are lit alike");

		std::vector<ShapeThumbBatch> const unread = ShapeThumb_Triangles(m, { { 0, 0 } });
		Check(unread.size() == 1 && unread[0].picture == -1 && unread[0].corners.size() == 9, "a picture that couldn't be read: every polygon drawn shaded");
	}

	// ---- (2) THE GAME'S RULES FOR THE TEXTURE LIST
	{
		Made iff;
		iff.entries = { { "0x01000000", "3", "Common\\Pikup.rim" } };
		ShapeThumbModel m;
		std::string why;

		Check(ShapeThumb_Read(File(iff), &m, &why) && m.pictures.size() == 1 && m.pictures[0].path == "Common\\Pikup.rim", "the IFF flag: the path is the name alone (" +
			(m.pictures.empty() ? std::string("-") : m.pictures[0].path) + ")");

		Made notPc;
		notPc.entries = { { "0x800", "3", "Pikup.rim" } };
		Check(ShapeThumb_Read(File(notPc), &m, &why) && m.pictures.empty() && m.polygons[0].picture == -1, "an entry not for the PC sets no picture: the quad is shaded");

		Made twice;
		twice.entries = { { "0", "3", "First.rim" }, { "0", "3", "Last.rim" } };
		Check(ShapeThumb_Read(File(twice), &m, &why) && m.pictures.size() == 1 && m.pictures[0].name == "Last.rim", "two entries of one index: the last wins, as the game's loop leaves it");

		Made noEntry;
		noEntry.quadIndex = 9;
		Check(ShapeThumb_Read(File(noEntry), &m, &why) && m.pictures.empty() && m.polygons[0].picture == -1, "a picture number no entry has: shaded, not refused");
	}

	// ---- (3) A FRAGMENT FILE
	{
		Made two;
		two.shapes = 2;
		ShapeThumbModel m;
		std::string why;

		Check(ShapeThumb_Read(File(two), &m, &why) && m.parts == 2 && m.polygons.size() == 2, "two shapes: 2 parts, the first drawn (" + why + ")");
	}

	// ---- (4) HOSTILE FILES
	{
		ShapeThumbModel m;
		std::string why;
		Bytes const whole = File(Made());
		Bytes const half(whole.begin(), whole.begin() + (std::ptrdiff_t)(whole.size() / 2));

		Check(!ShapeThumb_Read(half, &m, &why) && why.find("not a rif file") == 0, "cut short: refused (" + why + ")");
		Check(!ShapeThumb_Read(Bytes(), &m, &why) && !ShapeThumb_Read(Bytes(8, 'x'), &m, &why), "empty, and 8 bytes of nothing: refused");

		Made none;
		none.shapes = 0;
		Check(!ShapeThumb_Read(File(none), &m, &why) && why == "the file holds no shape", "no shape: refused (" + why + ")");

		Made badCorner;
		badCorner.triangleLast = 9;
		Check(!ShapeThumb_Read(File(badCorner), &m, &why) && why.find("its shape is refused") == 0, "a corner that is no point: refused (" + why + ")");

		Made big;
		big.points = kShapeThumbMostPoints + 1;
		Check(!ShapeThumb_Read(File(big), &m, &why) && why.find("too big to draw small: 16385 points") != std::string::npos, "past the limit: refused with its counts (" + why + ")");

		Made uv;
		uv.uvBroken = true;
		Check(ShapeThumb_Read(File(uv), &m, &why) && m.polygons[0].picture == -1, "texture coordinates that don't fit: drawn shaded, not refused");

		Made names;
		names.namesBroken = true;
		Check(ShapeThumb_Read(File(names), &m, &why) && m.pictures.empty(), "a texture list cut inside a name: drawn shaded, not refused");
	}

	// ---- (6) A PICTURE'S PATH IS THE FILE'S OWN BYTES
	{
		struct Case
		{
			char const *flags, *name, *riff, *what;
		};

		Case const hostile[] = {
			{ "0x01000000", "..\\..\\..\\Users\\x\\y.rim", "Pulrak", "a path up out of the graphics folder" },
			{ "0x01000000", "C:\\x.rim", "Pulrak", "a drive" },
			{ "0x01000000", "a.rim:stream", "Pulrak", "a hidden stream" },
			{ "0x01000000", "\\\\server\\share\\x.rim", "Pulrak", "a network path (it could hand the player's sign-in to that server)" },
			{ "0x01000000", "nul.rim", "Pulrak", "a device's name" },
			{ "0", "Pikup.rim", "..", "a RIFFNAME of \"..\" in front of a plain name" },
		};

		for (Case const &c : hostile)
		{
			Made m;
			ShapeThumbModel model;
			std::string why;

			m.entries = { { c.flags, "3", c.name } };
			m.riff = c.riff;

			bool const read = ShapeThumb_Read(File(m), &model, &why);
			bool const refused = read && model.pictures.size() == 1 && model.pictures[0].path.empty() && !model.pictures[0].refused.empty() && model.polygons[0].picture == 0;
			std::vector<ShapeThumbBatch> const drawn = read ? ShapeThumb_Triangles(model, { { 0, 0 } }) : std::vector<ShapeThumbBatch>();

			Check(refused && drawn.size() == 1 && drawn[0].picture == -1, std::string("refused, never read, drawn shaded: ") + c.what + " (" +
				(model.pictures.empty() ? std::string("no picture") : model.pictures[0].refused) + ")");
		}

		Made stock;
		ShapeThumbModel model;
		std::string why;

		stock.entries = { { "0x01000000", "3", "Common\\Pikup.rim" } };
		Check(ShapeThumb_Read(File(stock), &model, &why) && model.pictures.size() == 1 && model.pictures[0].path == "Common\\Pikup.rim" && model.pictures[0].refused.empty(),
			"a stock name is taken: Common\\Pikup.rim");
	}

	// ---- (5) THE CAMERA
	{
		ShapeThumbModel m;
		std::string why;

		ShapeThumb_Read(File(Made()), &m, &why);

		float eye[3], at[3];
		float const fov = 0.7f;

		ShapeThumb_Camera(m, fov, eye, at);

		float const d = m.radius / sinf(fov / 2.0f) * 1.05f;
		Check(fabsf(at[1] - m.centre[1]) < 0.01f && fabsf(eye[0] - (m.centre[0] + 0.5f * d)) < 0.5f && fabsf(eye[1] - (m.centre[1] - 0.7071068f * d)) < 0.5f &&
			fabsf(eye[2] - (m.centre[2] + 0.5f * d)) < 0.5f, "the camera: 45 degrees round and 45 up (up is -y), far enough to frame the radius");

		float light[3];
		ShapeThumb_Light(light);
		Check(fabsf(light[0] * light[0] + light[1] * light[1] + light[2] * light[2] - 1.0f) < 1e-5f && light[1] < 0, "the light: one length, from above");
	}

	printf(checksFailed ? "host_shape_thumb_test: %d FAILED\n" : "host_shape_thumb_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
