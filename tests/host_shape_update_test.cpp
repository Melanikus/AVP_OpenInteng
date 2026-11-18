// The test of the outside-shape update (shape_update.h: inteng FUN_00512fc0, AvP-docs\engine\inteng\reassess\object-placement.md section 7, OP-137..OP-141, OP-138a..e).
// Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It builds small level shapes and shape files of its own, chunk by chunk, so each rule and each refusal is tried on bytes it knows: the scale rule (kept, forced, ENVSDSCL when no object
// uses the shape), the copy (points scaled and cut toward 0, the box and radius scaled, the centre worked out again, the data and extra chunks replaced, SHPPRPRO gone, the version and rescale
// written), a fragment's scaling, the file-name-only case, and every refusal of hostile bytes: NaN / 0 / negative / too big scales, a point pushed too far, a file that isn't a rif or whose
// sizes lie, a point count that doesn't fit, an object name not in the file, a name list that runs off its header, a shape that isn't an outside one, a SHPEXTFN with no end to its name.
#include "shape_update.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

using namespace host;

typedef std::vector<char> Bytes;

static int checksRun = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;

	if (!good)
	{
		checksFailed++;
		printf("  FAILED  %s\n", what.c_str());
	}
}

static void AddInt(Bytes *b, int32_t v) { b->insert(b->end(), (char const *)&v, (char const *)&v + 4); }
static void AddFloat(Bytes *b, float v) { b->insert(b->end(), (char const *)&v, (char const *)&v + 4); }
static void AddDouble(Bytes *b, double v) { b->insert(b->end(), (char const *)&v, (char const *)&v + 8); }
static void AddText(Bytes *b, std::string const &t) { b->insert(b->end(), t.begin(), t.end()); b->push_back(0); while (b->size() % 4) b->push_back(0); }
static int32_t IntAt(Bytes const &b, size_t at) { int32_t v; memcpy(&v, b.data() + at, 4); return v; }
static float FloatAt(Bytes const &b, size_t at) { float v; memcpy(&v, b.data() + at, 4); return v; }
static double DoubleAt(Bytes const &b, size_t at) { double v; memcpy(&v, b.data() + at, 8); return v; }

static Bytes Chunk(char const *name, Bytes const &payload)
{
	Bytes c(name, name + 8);

	AddInt(&c, (int32_t)(payload.size() + 12));
	c.insert(c.end(), payload.begin(), payload.end());
	return c;
}

static Bytes Join(std::vector<Bytes> const &parts)
{
	Bytes all;

	for (Bytes const &p : parts)
	{
		all.insert(all.end(), p.begin(), p.end());
	}

	return all;
}

// A SHPHEAD1 as the game's chunk library writes it (Shape_Header_Chunk::fill_data_block, chunk_shape.cpp:1663): flags, 16 letters of lock user, number, points, polygons, radius, the box
// (max/min x, y, z), version, the object count (+64) and the names. FIXED 11-18 (Vasquez, CI on PR #80): a stray word sat between the version and the count, so the count was read as 0 and the
// update took the file's ENVSDSCL where objects use the shape (TestKeptScale's eight failures)
static Bytes Header(int32_t points, int32_t polygons, float radius, int32_t const box[6], int32_t version, std::vector<std::string> const &objects, int32_t countGiven = -1)
{
	Bytes p;

	AddInt(&p, 0x1000);
	p.insert(p.end(), 16, 0);
	AddInt(&p, 7);
	AddInt(&p, points);
	AddInt(&p, polygons);
	AddFloat(&p, radius);

	for (int k = 0; k < 6; k++)
	{
		AddInt(&p, box[k]);
	}

	AddInt(&p, version);
	AddInt(&p, countGiven >= 0 ? countGiven : (int32_t)objects.size());

	for (std::string const &one : objects)
	{
		p.insert(p.end(), one.begin(), one.end());
		p.push_back(0);
	}

	while (p.size() % 4)
	{
		p.push_back(0);
	}

	return Chunk("SHPHEAD1", p);
}

static Bytes Points(std::vector<int32_t> const &xyz)
{
	Bytes p;

	for (int32_t v : xyz)
	{
		AddInt(&p, v);
	}

	return Chunk("SHPRAWVT", p);
}

static Bytes Blob(char const *name, int marker)
{
	Bytes p;

	AddInt(&p, marker);
	return Chunk(name, p);
}

// One whole SHPPOLYS record (36 bytes, the game's loader reads SHPPOLYS / 36) starting with a marker
static Bytes Polygons(int marker)
{
	Bytes p(36, 0);

	memcpy(p.data(), &marker, 4);
	return Chunk("SHPPOLYS", p);
}

// An object of a shape file: RBOBJECT { OBJHEAD1 } with the shape number at +56 and its name at +60 (how the update finds the shape an EXTOBJNM names, FUN_004bf1d0)
static Bytes Object(std::string const &name, int32_t shapeNumber)
{
	Bytes p(56, 0);

	AddInt(&p, shapeNumber);
	AddText(&p, name);

	while (p.size() % 4)
	{
		p.push_back(0);
	}

	return Chunk("RBOBJECT", Chunk("OBJHEAD1", p));
}

// The level's shape: header (objectCount objects), SHPEXTFL { SHPEXTFN (rescale, version 3, fileName), [EXTOBJNM], BMPLSTST, RIFFNAME }, old data, an old SHPPRPRO, a chunk of its own
static Bytes LevelShape(double rescale, std::vector<std::string> const &objects, std::string const &fileName = "C:\\shapes\\Pulse.RIF", std::string const &objectName = "", bool terminated = true)
{
	int32_t const box[6] = { 10, -10, 10, -10, 10, -10 };
	Bytes fn;

	AddDouble(&fn, rescale);
	AddInt(&fn, 3);

	if (terminated)
	{
		AddText(&fn, fileName);
	}
	else
	{
		fn.insert(fn.end(), fileName.begin(), fileName.end());
	}

	std::vector<Bytes> ext = { Chunk("SHPEXTFN", fn) };

	if (!objectName.empty())
	{
		Bytes on;

		AddInt(&on, 0);
		AddText(&on, objectName);
		ext.push_back(Chunk("EXTOBJNM", on));
	}

	ext.push_back(Blob("BMPLSTST", 11));
	ext.push_back(Blob("RIFFNAME", 12));

	Bytes const body = Join({ Header(1, 1, 17.0f, box, 5, objects), Chunk("SHPEXTFL", Join(ext)), Points({ 1, 1, 1 }), Blob("SHPCENTR", 0), Blob("SHPPRPRO", 13), Blob("SHPMORPH", 14), Blob("MYCHUNK1", 15) });

	return Chunk("REBSHAPE", body);
}

// A shape file: REBINFF2 { REBENVDT { [ENVSDSCL], RIFFNAME }, REBSHAPE x shapes }; each shape two points (100, -50, 7) and (-3, 3, 1001), a polygon chunk, a morph, a SHPPRPRO
static Bytes ShapeFile(int shapes, bool withScale, double scale, std::vector<std::string> const &objects = {}, int32_t pointsInHeader = 2, int32_t bigPoint = 100, bool fragments = false)
{
	std::vector<Bytes> env;

	if (withScale)
	{
		Bytes s;

		AddDouble(&s, scale);
		env.push_back(Chunk("ENVSDSCL", s));
	}

	env.push_back(Blob("RIFFNAME", 99));

	std::vector<Bytes> top = { Chunk("REBENVDT", Join(env)) };
	int32_t const box[6] = { bigPoint, -3, 3, -50, 1001, 1 };

	for (int i = 0; i < shapes; i++)
	{
		std::vector<Bytes> parts = { Header(pointsInHeader, 1, 1004.5f, box, 42, objects), Points({ bigPoint, -50, 7, -3, 3, 1001 }), Polygons(21), Blob("SHPMORPH", 22), Blob("SHPPRPRO", 23) };

		if (fragments)
		{
			Bytes sub;

			AddInt(&sub, 0);
			AddInt(&sub, 1);
			AddInt(&sub, 1);
			AddInt(&sub, 0);
			AddFloat(&sub, 8.0f);

			for (int32_t v : { 4, -4, 4, -4, 4, -4 })
			{
				AddInt(&sub, v);
			}

			Bytes loc;

			AddInt(&loc, 10);
			AddInt(&loc, -20);
			AddInt(&loc, 30);
			AddInt(&loc, 0);
			AddInt(&loc, 0);
			AddInt(&loc, 0);

			Bytes const subshape = Chunk("SUBSHAPE", Join({ Chunk("SUBSHPHD", sub), Points({ 2, -3, 5 }), Chunk("FRAGLOCN", loc) }));

			parts.push_back(Chunk("SHPFRAGS", subshape));
		}

		top.push_back(Chunk("REBSHAPE", Join(parts)));
	}

	for (std::string const &one : objects)
	{
		top.push_back(Object(one, 7));   // every shape here is number 7: an object is tied to the FIRST of them
	}

	return Chunk("REBINFF2", Join(top));
}

// The children of a whole chunk (name, start of payload, size)
struct Part
{
	std::string name;
	size_t at;
	size_t size;
};

static std::vector<Part> PartsOf(Bytes const &chunk, size_t from, size_t to)
{
	std::vector<Part> out;

	for (size_t at = from; at + 12 <= to; )
	{
		int32_t const size = IntAt(chunk, at + 8);

		if (size < 12 || at + (size_t)size > to)
		{
			out.push_back({ "BROKEN", at, 0 });
			break;
		}

		out.push_back({ std::string(chunk.data() + at, 8), at, (size_t)size });
		at += (size_t)size;
	}

	return out;
}

static Part Find(std::vector<Part> const &parts, char const *name)
{
	for (Part const &p : parts)
	{
		if (p.name == name)
		{
			return p;
		}
	}

	return { "", 0, 0 };
}

static int Count(std::vector<Part> const &parts, char const *name)
{
	int n = 0;

	for (Part const &p : parts)
	{
		n += p.name == name ? 1 : 0;
	}

	return n;
}

static void TestFileName()
{
	std::string name;

	Check(ShapeUpdate_FileName(LevelShape(1.0, { "a" }), &name) && name == "Pulse", "the file's name is the SHPEXTFN name's last part, .RIF taken off (got \"" + name + "\")");
	Check(ShapeUpdate_FileName(LevelShape(1.0, { "a" }, "x:\\a/b/Smart.rif"), &name) && name == "Smart", "after the last ':', '/' or '\\' (FUN_00567e40)");
	Check(!ShapeUpdate_FileName(LevelShape(1.0, { "a" }, "C:\\shapes\\"), &name), "a name with nothing after its last folder gives none");
}

static void TestKeptScale()
{
	Bytes out;
	std::string why;
	double before = 0, after = 0;
	ShapeUpdateResult const r = ShapeUpdate_Build(LevelShape(1.5, { "pulse01" }), ShapeFile(1, true, 0.75), false, &out, &why, &before, &after);

	Check(r == ShapeUpdateResult::Done, std::string("the load's update of a shape objects use: ") + ShapeUpdateWords(r) + " " + why);
	Check(before == 1.5 && after == 1.5, "objects use it and it isn't forced: the shape's own rescale is kept, the file's ENVSDSCL is not taken (0x51348b)");

	std::vector<Part> const parts = PartsOf(out, 12, out.size());
	Part const head = Find(parts, "SHPHEAD1");
	Part const raw = Find(parts, "SHPRAWVT");
	Part const centre = Find(parts, "SHPCENTR");

	Check(IntAt(out, 8) == (int32_t)out.size() && Count(parts, "BROKEN") == 0, "the new shape's sizes all fit");
	Check(raw.size == 12 + 24 && IntAt(out, raw.at + 12) == 150 && IntAt(out, raw.at + 16) == -75 && IntAt(out, raw.at + 20) == 10 && IntAt(out, raw.at + 24) == -4 && IntAt(out, raw.at + 28) == 4 && IntAt(out, raw.at + 32) == 1501,
		"the file's points x 1.5, cut toward 0 (100 -50 7 / -3 3 1001 -> 150 -75 10 / -4 4 1501)");
	Check(IntAt(out, head.at + 12 + 24) == 2 && IntAt(out, head.at + 12 + 28) == 1 && fabs(FloatAt(out, head.at + 12 + 32) - 1506.75f) < 0.01f, "the header's counts are its chunks' (2 points, one 36-byte polygon), its radius x 1.5");
	Check(IntAt(out, head.at + 12 + 36) == 150 && IntAt(out, head.at + 12 + 40) == -4 && IntAt(out, head.at + 12 + 52) == 1501, "and its box x 1.5");
	Check(IntAt(out, head.at + 12 + 20) == 7 && IntAt(out, head.at + 12 + 60) == 5 && IntAt(out, head.at + 12 + 64) == 1, "the header keeps the level's number, version and object list");
	Check(centre.size == 28 && IntAt(out, centre.at + 12) == 73 && IntAt(out, centre.at + 16) == -35 && IntAt(out, centre.at + 20) == 751, "the centre is worked out again, (max + min) / 2 cut toward 0 ((150 - 4) / 2, (4 - 75) / 2, (1501 + 1) / 2)");
	Check(FloatAt(out, centre.at + 24) > 750.0f, "and the radius about it is the farthest point's distance");
	Check(Count(parts, "SHPPRPRO") == 0, "SHPPRPRO is deleted and not copied back (FUN_0050eb20, OP-138b)");
	Check(Count(parts, "SHPMORPH") == 1 && IntAt(out, Find(parts, "SHPMORPH").at + 12) == 22, "SHPMORPH is the file's");
	Check(Count(parts, "SHPPOLYS") == 1 && IntAt(out, Find(parts, "SHPPOLYS").at + 12) == 21, "the polygons are the file's, as they are");
	Check(Count(parts, "MYCHUNK1") == 1, "a chunk of the level's own that the update doesn't name is kept");

	Part const ext = Find(parts, "SHPEXTFL");
	std::vector<Part> const extParts = PartsOf(out, ext.at + 12, ext.at + ext.size);
	Part const fn = Find(extParts, "SHPEXTFN");

	Check(DoubleAt(out, fn.at + 12) == 1.5 && IntAt(out, fn.at + 20) == 42, "the SHPEXTFN holds the rescale and the file shape's version (OP-138e)");
	Check(Count(extParts, "BMPLSTST") == 0 && Count(extParts, "BMNAMEXT") == 1 && Count(extParts, "RIFFNAME") == 1 && IntAt(out, Find(extParts, "RIFFNAME").at + 12) == 99,
		"the file's environment has no texture names: the shape's list goes and a BMNAMEXT is made; its RIFFNAME is made again from the file's (fidelity-1 / -2, 0x514617, 0x5146f0)");
}

static void TestForcedAndUnused()
{
	Bytes out;
	std::string why;
	double before = 0, after = 0;

	Check(ShapeUpdate_Build(LevelShape(1.5, { "pulse01" }), ShapeFile(1, false, 0), true, &out, &why, &before, &after) == ShapeUpdateResult::Done && after == 1.0,
		"forced (Take scale) with no ENVSDSCL in the file: the rescale is 1.0 (0x5134a7)");
	Check(ShapeUpdate_Build(LevelShape(1.5, { "pulse01" }), ShapeFile(1, true, 0.75), true, &out, &why, &before, &after) == ShapeUpdateResult::Done && after == 0.75,
		"forced with an ENVSDSCL: the file's scale");
	Check(ShapeUpdate_Build(LevelShape(1.5, {}), ShapeFile(1, true, 2.0), false, &out, &why, &before, &after) == ShapeUpdateResult::Done && after == 2.0,
		"not forced, no object uses the shape: the file's ENVSDSCL");
	Check(ShapeUpdate_Build(LevelShape(1.5, {}), ShapeFile(1, false, 0), false, &out, &why, &before, &after) == ShapeUpdateResult::Done && after == 1.5,
		"not forced, no object, no ENVSDSCL: the shape's own rescale is kept");
}

static void TestFragments()
{
	Bytes out;
	std::string why;

	Check(ShapeUpdate_Build(LevelShape(2.0, { "a" }), ShapeFile(1, false, 0, {}, 2, 100, true), false, &out, &why) == ShapeUpdateResult::Done, "a shape with fragments is updated: " + why);

	std::vector<Part> const parts = PartsOf(out, 12, out.size());
	Part const frags = Find(parts, "SHPFRAGS");
	std::vector<Part> const subs = PartsOf(out, frags.at + 12, frags.at + frags.size);
	Part const sub = Find(subs, "SUBSHAPE");
	std::vector<Part> const inside = PartsOf(out, sub.at + 12, sub.at + sub.size);
	Part const raw = Find(inside, "SHPRAWVT"), loc = Find(inside, "FRAGLOCN"), hd = Find(inside, "SUBSHPHD");

	Check(raw.size == 24 && IntAt(out, raw.at + 12) == 4 && IntAt(out, raw.at + 16) == -6 && IntAt(out, raw.at + 20) == 10, "each fragment's points x the rescale (FUN_00430210 on the SUBSHAPE)");
	Check(IntAt(out, loc.at + 12) == 20 && IntAt(out, loc.at + 16) == -40 && IntAt(out, loc.at + 20) == 60, "its FRAGLOCN place x the rescale (0x513991-0x5139b2)");
	Check(fabs(FloatAt(out, hd.at + 12 + 16) - 16.0f) < 0.001f && IntAt(out, hd.at + 12 + 20) == 8, "its header's radius and box x the rescale");
}

static void TestNameOnly()
{
	Bytes out;
	std::string why;
	ShapeUpdateResult const r = ShapeUpdate_Build(LevelShape(1.0, { "a" }), ShapeFile(2, false, 0), false, &out, &why);

	Check(r == ShapeUpdateResult::NameOnly, "a file of two shapes with no EXTOBJNM: only the file name (0x51328e)");

	std::vector<Part> const parts = PartsOf(out, 12, out.size());
	Part const ext = Find(parts, "SHPEXTFL");
	std::vector<Part> const extParts = PartsOf(out, ext.at + 12, ext.at + ext.size);

	Check(Count(extParts, "RIFFNAME") == 1 && IntAt(out, Find(extParts, "RIFFNAME").at + 12) == 99, "the SHPEXTFL's RIFFNAME is the file's");
	Check(Count(parts, "SHPRAWVT") == 1 && IntAt(out, Find(parts, "SHPRAWVT").at + 12) == 1, "and nothing else changes");
}

static void TestObjectName()
{
	Bytes out;
	std::string why;

	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }, "Pulse.rif", "gun"), ShapeFile(2, false, 0, { "other", "gun" }), false, &out, &why) == ShapeUpdateResult::Done,
		"EXTOBJNM: the file's shape whose header lists that object is taken (0x513190)");
	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }, "Pulse.rif", "nothere"), ShapeFile(2, false, 0, { "gun" }), false, &out, &why) == ShapeUpdateResult::NoShape && out.empty(),
		"REFUSED: an EXTOBJNM no shape of the file lists: nothing changes");
	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }, "Pulse.rif", "gun"), ShapeFile(1, false, 0, { "gun" }, 2, 100, false), false, &out, &why) == ShapeUpdateResult::Done,
		"one shape listing it is found too");
}

static void TestRefusals()
{
	Bytes out;
	std::string why;
	double const nan = std::numeric_limits<double>::quiet_NaN();
	double const endless = std::numeric_limits<double>::infinity();

	for (double bad : { nan, endless, 0.0, -1.0, 2000000.0 })
	{
		Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }), ShapeFile(1, true, bad), true, &out, &why) == ShapeUpdateResult::BadScale && out.empty(),
			"REFUSED: an ENVSDSCL of " + std::to_string(bad) + " (not a real number above 0 and at most 1,000,000)");
	}

	Check(ShapeUpdate_Build(LevelShape(nan, { "a" }), ShapeFile(1, false, 0), false, &out, &why) == ShapeUpdateResult::BadScale, "REFUSED: a level rescale that is NaN");
	Check(ShapeUpdate_Build(LevelShape(1.5, { "a" }), ShapeFile(1, false, 0, {}, 2, 16000000), false, &out, &why) == ShapeUpdateResult::BadScale && out.empty(),
		"REFUSED: a point pushed farther than 2^24 by the rescale (16,000,000 x 1.5)");

	Bytes notRif = ShapeFile(1, false, 0);

	memcpy(notRif.data(), "REBCRIF1", 8);
	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }), notRif, false, &out, &why) == ShapeUpdateResult::BadFile, "REFUSED: a file that isn't a REBINFF2 (a packed one is not read here)");

	Bytes lying = ShapeFile(1, false, 0);

	lying[12 + 8 + 3] = 0x7f;   // the environment's size (its top byte) is far past the file's end
	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }), lying, false, &out, &why) == ShapeUpdateResult::BadFile, "REFUSED: a chunk whose size runs past its parent");

	for (int32_t const lie : { 3, -1, 1 + (1 << 30) })
	{
		bool const done = ShapeUpdate_Build(LevelShape(1.0, { "a" }), ShapeFile(1, false, 0, {}, lie), false, &out, &why) == ShapeUpdateResult::Done;
		Part const header = Find(PartsOf(out, 12, out.size()), "SHPHEAD1");

		Check(done && header.size >= 12 + 32 && IntAt(out, header.at + 12 + 24) == 2, "a header point count of " + std::to_string(lie) +
			" is not taken: the new header has its SHPRAWVT's 2 (hostile-input-2 / -3; 1 + 2^30 wrapped a 32-bit check)");
	}

	Bytes listOff;
	int32_t const box[6] = { 1, 0, 1, 0, 1, 0 };
	std::vector<Bytes> shape = { Header(2, 1, 1.0f, box, 1, { "gun" }, 100000), Points({ 1, 1, 1, 2, 2, 2 }) };

	listOff = Chunk("REBINFF2", Join({ Chunk("REBENVDT", Blob("RIFFNAME", 1)), Chunk("REBSHAPE", Join(shape)) }));
	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }, "Pulse.rif", "gun"), listOff, false, &out, &why) == ShapeUpdateResult::NoShape, "a file with no object of the EXTOBJNM's name: nothing changes (its header's name list is not what the update reads, fidelity-6)");

	Bytes inner = LevelShape(1.0, { "a" });
	Bytes plain = Chunk("REBSHAPE", Join({ Header(0, 0, 0.0f, box, 1, {}), Points({}) }));

	Check(ShapeUpdate_Build(plain, ShapeFile(1, false, 0), false, &out, &why) == ShapeUpdateResult::NotOutside, "a shape with no SHPEXTFL is not an outside one: nothing to do");
	Check(ShapeUpdate_Build(LevelShape(1.0, { "a" }, "Pulse.rif", "", false), ShapeFile(1, false, 0), false, &out, &why) == ShapeUpdateResult::BadShape, "REFUSED: a SHPEXTFN whose name has no end");

	Bytes cut(inner.begin(), inner.begin() + 40);

	Check(ShapeUpdate_Build(cut, ShapeFile(1, false, 0), false, &out, &why) == ShapeUpdateResult::BadShape, "REFUSED: a level shape cut short");
	Check(ShapeUpdate_Build(inner, Bytes(), false, &out, &why) == ShapeUpdateResult::BadFile, "REFUSED: an empty file");
}

// A BMPNAMES / BMPLSTST payload: the head word, then each name with its five numbers
static Bytes BitmapList(char const *chunkName, std::vector<std::pair<std::string, int32_t>> const &names, int32_t index = 0, bool endName = true, int32_t countGiven = -1)
{
	Bytes p;

	AddInt(&p, countGiven >= 0 ? countGiven : (int32_t)names.size());

	for (auto const &one : names)
	{
		AddInt(&p, one.second);   // flags
		AddInt(&p, index);
		AddInt(&p, 1);            // version
		AddInt(&p, 0);
		AddInt(&p, 0);
		p.insert(p.end(), one.first.begin(), one.first.end());

		if (endName)
		{
			p.insert(p.end(), 4 - one.first.size() % 4, 0);
		}
	}

	return Chunk(chunkName, p);
}

// The level shape of LevelShape with its SHPEXTFL's BMPLSTST replaced, and a shape file with a BMPNAMES
static Bytes LevelWithList(Bytes const &list, int lists = 1)
{
	int32_t const box[6] = { 10, -10, 10, -10, 10, -10 };
	Bytes fn;

	AddDouble(&fn, 1.0);
	AddInt(&fn, 3);
	AddText(&fn, "Pulse.RIF");

	std::vector<Bytes> ext = { Chunk("SHPEXTFN", fn) };

	for (int i = 0; i < lists; i++)
	{
		ext.push_back(list);
	}

	return Chunk("REBSHAPE", Join({ Header(1, 1, 17.0f, box, 5, { "a" }), Chunk("SHPEXTFL", Join(ext)), Points({ 1, 1, 1 }) }));
}

static Bytes FileWithList(Bytes const &names)
{
	int32_t const box[6] = { 1, 0, 1, 0, 1, 0 };

	return Chunk("REBINFF2", Join({ Chunk("REBENVDT", Join({ names, Blob("RIFFNAME", 1) })), Chunk("REBSHAPE", Join({ Header(1, 1, 1.0f, box, 9, {}), Points({ 1, 2, 3 }) })) }));
}

static Bytes ListAfter(Bytes const &shape)
{
	std::vector<Part> const parts = PartsOf(shape, 12, shape.size());
	Part const ext = Find(parts, "SHPEXTFL");
	std::vector<Part> const extParts = PartsOf(shape, ext.at + 12, ext.at + ext.size);
	Part const list = Find(extParts, "BMPLSTST");

	return Count(extParts, "BMPLSTST") == 1 ? Bytes(shape.begin() + (std::ptrdiff_t)list.at, shape.begin() + (std::ptrdiff_t)(list.at + list.size)) : Bytes();
}

static void TestTextures()
{
	Bytes out;
	std::string why;
	Bytes const same = BitmapList("BMPLSTST", { { "Pulse.bmp", 0x2 } });

	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "PULSE.BMP", 0x2 | 0x100 } })), false, &out, &why) == ShapeUpdateResult::Done &&
		ListAfter(out).size() > 36 && std::string(ListAfter(out).data() + 16 + 20) == "PULSE.BMP" && IntAt(ListAfter(out), 16) == 0x2,
		"the same name (any capitals): the list is REWRITTEN as the merge, the file's entry with the level's flags (0x102 & 0x10ffc7f keeps none of 0x100): fidelity-3");

	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "Pulse.bmp", 0x2 }, { "Other.bmp", 0x2 } })), false, &out, &why) == ShapeUpdateResult::Done,
		"a name more in the file: updated");

	Bytes const list = ListAfter(out);

	Check(list.size() > 16 && (IntAt(list, 12) & 0xffff) == 2 && std::string(list.data() + 16 + 20) == "Pulse.bmp", "and the BMPLSTST is the file's names");

	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "Pulse.bmp", 0x2 | 0x40000000 } })), false, &out, &why) == ShapeUpdateResult::Done &&
		ListAfter(out) == same, "a flag outside 0xf0042 alone is no change");
	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "Pulse.bmp", 0x40 | 0x40000000 } })), false, &out, &why) == ShapeUpdateResult::Done &&
		IntAt(ListAfter(out), 16) == 0, "a flag change in 0xf0042: the merge's flags, ((level ^ file & 0xff3ffd3f) & 0x10ffc7f) ^ level, with the level's 0xf0042 bits cleared (0x2, file 0x40000040 -> 0)");
	Check(ShapeUpdate_Build(LevelWithList(same, 2), FileWithList(BitmapList("BMPNAMES", { { "Pulse.bmp", 0x2 } })), false, &out, &why) == ShapeUpdateResult::Done && !ListAfter(out).empty(),
		"two BMPLSTST in the level: ONE is left");
	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", {})), false, &out, &why) == ShapeUpdateResult::Done && ListAfter(out).empty(),
		"a file whose texture list has no names: the shape's own list GOES (fidelity-2, 0x514617-0x514664)");

	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "Pulse.bmp", 2 } }, 40000)), false, &out, &why) == ShapeUpdateResult::BadFile && out.empty(),
		"REFUSED: a texture number above 32767 (the game's reader's limit)");
	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "Pulseabc", 2 } }, 0, false)), false, &out, &why) == ShapeUpdateResult::BadFile,
		"REFUSED: a texture name with no end inside its chunk");
	Check(ShapeUpdate_Build(LevelWithList(same), FileWithList(BitmapList("BMPNAMES", { { "Pulse.bmp", 2 } }, 0, true, 500)), false, &out, &why) == ShapeUpdateResult::BadFile,
		"REFUSED: a list whose count says more names than it holds");
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestFileName();
	TestKeptScale();
	TestForcedAndUnused();
	TestFragments();
	TestNameOnly();
	TestObjectName();
	TestRefusals();
	TestTextures();

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
