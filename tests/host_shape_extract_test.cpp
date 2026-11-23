// The test for src/host/shape_extract.h (Option C): a level's outside shape made back into the shape file it came from. Written 2026-11-23 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE ROUND TRIP is the proof: a shape file laid out as the old tools' package lays one out (RIFVERIN, the shape, its object, its environment data with
// ENDTHEAD) is imported (ShapeImport_Build, the Quick-create maker's own step), the level's shape is made back into a file (ShapeExtract_File), and that
// file is imported again: the two level shapes are the same, byte for byte. So the extracted file places exactly what the package's file placed. Then what
// the extracted file holds, field by field (shape_extract.h says why each), and what it refuses (Tao 3: a stranger's level). It reads no file. The program
// gives back a non-zero result if any check failed. Each check's words say what it proves.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <string>
#include <vector>

#include "huffman.hpp"
#include "mem3dc.h"
#include "shape_extract.h"
#include "shape_import.h"
#include "shape_extract_fixtures.h"   // Filled, Add, PackageFile, LevelOf, PlacingObject (the maps both Option C tests build)

// the game's memory code (our_mem.cpp, for the decoder ShapeExtract_ScanMap uses) calls these: here they only say what happened
void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

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

// A point list (SHPRAWVT: three whole numbers a point) moved and turned the way a placement moves and turns its shape in the world: each point turned
// by the quaternion, then the place added. Only for the test to say what a WORLD copy of the points would be, so it can tell one from the shape's own.
static Bytes InTheWorld(Bytes const &points, int32_t const place[3], float const turn[4])
{
	Bytes out = points;
	double const x = turn[0], y = turn[1], z = turn[2], w = turn[3];

	for (size_t at = 0; at + 12 <= points.size(); at += 12)
	{
		double const px = IntAt(points, at), py = IntAt(points, at + 4), pz = IntAt(points, at + 8);
		// the rotation matrix of a unit quaternion, applied to the point
		double const rx = (1 - 2 * (y * y + z * z)) * px + 2 * (x * y - z * w) * py + 2 * (x * z + y * w) * pz;
		double const ry = 2 * (x * y + z * w) * px + (1 - 2 * (x * x + z * z)) * py + 2 * (y * z - x * w) * pz;
		double const rz = 2 * (x * z - y * w) * px + 2 * (y * z + x * w) * py + (1 - 2 * (x * x + y * y)) * pz;
		PutIntAt(&out, at, (int32_t)rx + place[0]);
		PutIntAt(&out, at + 4, (int32_t)ry + place[1]);
		PutIntAt(&out, at + 8, (int32_t)rz + place[2]);
	}

	return out;
}

// The payload of the first chunk named `id` among a shape's parts (the shape chunk starting at `shapeAt` in `bytes`), or an empty one
static Bytes PartOf(Bytes const &bytes, size_t shapeAt, size_t shapeSize, char const *id)
{
	std::vector<Span> parts;

	if (!Children(bytes, shapeAt + kHeadingBytes, shapeAt + shapeSize, &parts))
	{
		return Bytes();
	}

	size_t const i = FirstNamed(bytes, parts, id);
	return i == parts.size() ? Bytes() : Bytes(bytes.begin() + (std::ptrdiff_t)(parts[i].at + kHeadingBytes), bytes.begin() + (std::ptrdiff_t)(parts[i].at + parts[i].size));
}

// The first shape chunk (REBSHAPE) in a file: where it starts and its size, or false
static bool FirstShape(Bytes const &file, size_t *at, size_t *size)
{
	std::vector<Span> top;

	if (!Children(file, kHeadingBytes, file.size(), &top))
	{
		return false;
	}

	size_t const i = FirstNamed(file, top, "REBSHAPE");

	if (i == top.size())
	{
		return false;
	}

	*at = top[i].at;
	*size = top[i].size;
	return true;
}

// The level shape as a stock level holds it: a SHPFRAGS of the level's own after its header, the old editor's edit count at the header's +60 (1287), and
// what was in its memory in the names' pad bytes (RIFFNAME "Pack\0frg", the picture's name's pad byte)
static Bytes AsAStockLevelHoldsIt(Bytes const &shape)
{
	std::vector<Span> parts;
	Children(shape, kHeadingBytes, shape.size(), &parts);
	Bytes out(shape.begin(), shape.begin() + (std::ptrdiff_t)kHeadingBytes);

	for (size_t i = 0; i < parts.size(); i++)
	{
		AddCopy(&out, shape, parts[i]);

		if (i == 0)
		{
			Add(&out, MakeChunk("SHPFRAGS", Filled(20, 30)));
		}
	}

	PutIntAt(&out, 8, (int32_t)out.size());

	std::vector<Span> p, in;
	Children(out, kHeadingBytes, out.size(), &p);
	PutIntAt(&out, p[FirstNamed(out, p, "SHPHEAD1")].at + kHeadingBytes + 60, 1287);
	size_t const ext = FirstNamed(out, p, "SHPEXTFL");
	Children(out, p[ext].at + kHeadingBytes, p[ext].at + p[ext].size, &in);
	size_t const name = FirstNamed(out, in, "RIFFNAME"), list = FirstNamed(out, in, "BMPLSTST");
	memcpy(out.data() + in[name].at + kHeadingBytes + 5, "frg", 3);
	out[in[list].at + kHeadingBytes + 4 + 20 + 11] = 'Z';
	PutIntAt(&out, in[list].at + kHeadingBytes + 4, 0x0c);   // the picture's flags without 0x100, as a stock level's copy has them
	return out;
}

int main()
{
	printf("shape extract\n");

	Bytes const package = PackageFile();
	Bytes placedShape;
	std::string why;
	ShapeImportResult const imported = ShapeImport_Build(package, "medbox01.rif", 12, &placedShape, &why);
	Check(imported == ShapeImportResult::Done, "the package-like file imports (ShapeImport_Build) " + why);

	// the level as a stock one holds it: the imported shape, with its own SHPFRAGS, an edit count at +60 and leftover bytes in the names' pads
	Bytes const inLevel = AsAStockLevelHoldsIt(placedShape);
	Bytes const level = LevelOf({ inLevel });
	std::vector<ShapeExtractPlaced> placed;
	bool const listed = ShapeExtract_Placed(level, &placed);
	Check(listed && placed.size() == 1 && placed[0].fileName == "medbox01.rif" && placed[0].rescale == 1.0, "the level's one outside shape is found, named medbox01.rif, rescale 1");

	Bytes file;
	std::string name;
	ShapeExtractResult const made = placed.empty() ? ShapeExtractResult::BadShape : ShapeExtract_File(level, placed[0], &file, &name, &why);
	Check(made == ShapeExtractResult::Done && name == "medbox01.rif", std::string("its file is made: ") + ShapeExtractWords(made) + " " + name);

	// THE ROUND TRIP: the extracted file imports to the very shape the package's file imported to (the level's own SHPFRAGS aside: an import never copies one)
	Bytes again;
	ShapeImportResult const reimported = ShapeImport_Build(file, name, 12, &again, &why);
	Check(reimported == ShapeImportResult::Done && again == placedShape, "the extracted file imports to the same level shape as the package's, byte for byte (" +
		std::to_string(again.size()) + " and " + std::to_string(placedShape.size()) + " bytes)");

	// what the file holds
	std::vector<Span> top, parts, env;
	bool const walks = Children(file, kHeadingBytes, file.size(), &top) && top.size() == 3 && NameIs(file, top[0].at, "RIFVERIN") && NameIs(file, top[1].at, "REBSHAPE") &&
		NameIs(file, top[2].at, "REBENVDT") && IntAt(file, top[0].at + kHeadingBytes) == 0;
	Check(walks, "the file is REBINFF2 holding RIFVERIN 0, the shape and its environment data, in that order");

	if (walks)
	{
		Children(file, top[1].at + kHeadingBytes, top[1].at + top[1].size, &parts);
		Children(file, top[2].at + kHeadingBytes, top[2].at + top[2].size, &env);
		size_t const head = FirstNamed(file, parts, "SHPHEAD1");
		size_t const hp = head == parts.size() ? 0 : parts[head].at + kHeadingBytes;
		bool lockEmpty = true;

		for (size_t i = 0; head != parts.size() && i < 16; i++)
		{
			lockEmpty = lockEmpty && file[hp + 4 + i] == 0;
		}

		Check(head == 0 && IntAt(file, hp) == 0 && lockEmpty && IntAt(file, hp + 20) == 0 && IntAt(file, hp + 60) == 3 && IntAt(file, hp + 64) == 0,
			"its header: flags 0 (the level's 0x1000 taken off), no lock user, number 0, the version 3, no object using it");
		Check(FirstNamed(file, parts, "SHPEXTFL") == parts.size() && FirstNamed(file, parts, "SHPFRAGS") == parts.size(),
			"the shape holds no outside file's data and not the level's own fragment list");
		bool const envRight = env.size() == 4 && NameIs(file, env[0].at, "RIFFNAME") && NameIs(file, env[1].at, "BMPNAMES") && NameIs(file, env[2].at, "BMNAMEXT") &&
			NameIs(file, env[3].at, "BMNAMVER");
		Check(envRight, "its environment data: RIFFNAME, BMPNAMES (the level's BMPLSTST), BMNAMEXT, BMNAMVER (no ENDTHEAD: a level doesn't keep one)");
	}

	// LOCAL SPACE (the owner: "do they hold their local space coordinates? this is like, super important"). In a level, a shape is stored once, in its OWN
	// space, and each object placing it carries its own place and turn (OBJHEAD1). So the extracted file's points, normals and centre must be the package
	// file's, byte for byte, never a placed copy's world points. Here the level's one object stands at (1000, -2000, 3000) turned by about 74 degrees round
	// the x axis, so a world copy would differ from the shape's own in every point; the stock measure is the same (467 of 467 rescale-1 copies' points equal the
	// package's, with 2981 placing objects moved and 2726 turned: AvP-docs review/placed-shapes-vs-shape_rifs.md).
	{
		int32_t const place[3] = { 1000, -2000, 3000 };
		float const turn[4] = { 0.6f, 0.0f, 0.0f, 0.8f };
		Bytes placedLevel;
		{
			Bytes whole;
			Add(&whole, MakeChunk("ENVRDATA", Filled(12, 20)));
			Add(&whole, inLevel);
			Add(&whole, PlacingObject(12, place, turn));   // (12: the shape's number, as ShapeImport_Build was given it above)
			placedLevel = MakeChunk("REBINFF2", whole);
		}

		std::vector<ShapeExtractPlaced> got;
		ShapeExtract_Placed(placedLevel, &got);
		Bytes f;
		std::string n;
		ShapeExtractResult const r = got.empty() ? ShapeExtractResult::BadShape : ShapeExtract_File(placedLevel, got[0], &f, &n, nullptr);
		size_t pa = 0, ps = 0, fa = 0, fsz = 0;
		bool const both = r == ShapeExtractResult::Done && FirstShape(package, &pa, &ps) && FirstShape(f, &fa, &fsz);
		Check(both, std::string("a shape placed by a moved and turned object is extracted: ") + ShapeExtractWords(r));

		if (both)
		{
			Bytes const own = PartOf(package, pa, ps, "SHPRAWVT"), world = InTheWorld(own, place, turn);
			Check(!own.empty() && world != own, "(the test can tell: the object's place and turn would move every one of the shape's points)");

			for (char const *id : { "SHPRAWVT", "SHPVNORM", "SHPPNORM", "SHPCENTR" })
			{
				Bytes const fromPackage = PartOf(package, pa, ps, id), extracted = PartOf(f, fa, fsz, id);
				Check(!fromPackage.empty() && extracted == fromPackage, std::string("LOCAL SPACE: the extracted ") + id + " is the package file's, byte for byte (" +
					std::to_string(extracted.size()) + " bytes), not moved or turned by the object placing it");
			}

			Check(PartOf(f, fa, fsz, "SHPRAWVT") != world, "LOCAL SPACE: the extracted points are not the placed object's world points");
		}
	}

	// REFUSED (Tao 3)
	{
		// a rescaled copy: the SHPEXTFN's rescale made 2.0
		Bytes scaled = inLevel;
		std::vector<Span> p, in;
		Children(scaled, kHeadingBytes, scaled.size(), &p);
		size_t const ext = FirstNamed(scaled, p, "SHPEXTFL");
		Children(scaled, p[ext].at + kHeadingBytes, p[ext].at + p[ext].size, &in);
		double const two = 2.0;
		memcpy(scaled.data() + in[0].at + kHeadingBytes, &two, sizeof(two));
		Bytes const lv = LevelOf({ scaled });
		std::vector<ShapeExtractPlaced> got;
		ShapeExtract_Placed(lv, &got);
		Bytes f;
		std::string n;
		ShapeExtractResult const r = got.empty() ? ShapeExtractResult::Done : ShapeExtract_File(lv, got[0], &f, &n, nullptr);
		Check(r == ShapeExtractResult::Rescaled && f.empty(), std::string("a rescaled copy is refused: ") + ShapeExtractWords(r));
	}

	// NAMES (the owner's rule: the name the game uses, made plain): a letter that isn't a letter, digit, '_' or '-' becomes '_', so no name can reach another
	// folder; a device name or one not ending .rif is refused
	struct { char const *given; char const *made; } const names[] = {
		{ "spaceship bulb.rif", "spaceship_bulb.rif" }, { "..\\x.rif", "___x.rif" }, { "x/y.rif", "x_y.rif" }, { "MedBox01.RIF", "medbox01.rif" },
		{ "COM1.rif", nullptr }, { "medbox01.txt", nullptr }, { "nul.rif", nullptr } };

	for (auto const &one : names)
	{
		Bytes named;
		ShapeImport_Build(package, one.given, 12, &named, nullptr);   // (a name the import itself takes as given; only the extract judges it here)
		Bytes const lv = LevelOf({ named });
		std::vector<ShapeExtractPlaced> got;
		ShapeExtract_Placed(lv, &got);
		Bytes f;
		std::string n;
		ShapeExtractResult const r = got.empty() ? ShapeExtractResult::BadShape : ShapeExtract_File(lv, got[0], &f, &n, nullptr);
		bool const right = one.made ? (r == ShapeExtractResult::Done && n == one.made) : (r == ShapeExtractResult::BadName && f.empty());
		Check(!named.empty() && right, std::string("the name \"") + one.given + "\" " + (one.made ? std::string("is made \"") + one.made + "\"" : "is refused") + ": " +
			ShapeExtractWords(r) + " " + n);
	}

	{
		// a shape with no outside file's data: not an outside shape, so it isn't listed, and a direct ask says so
		Bytes plain(kHeadingBytes, 0);
		memcpy(plain.data(), "REBSHAPE", 8);
		std::vector<Span> p;
		Children(inLevel, kHeadingBytes, inLevel.size(), &p);

		for (Span const &s : p)
		{
			if (!NameIs(inLevel, s.at, "SHPEXTFL"))
			{
				AddCopy(&plain, inLevel, s);
			}
		}

		PutIntAt(&plain, 8, (int32_t)plain.size());
		Bytes const lv = LevelOf({ plain });
		std::vector<ShapeExtractPlaced> got;
		bool const ok = ShapeExtract_Placed(lv, &got);
		ShapeExtractPlaced direct;
		direct.at = kHeadingBytes + 24;   // after ENVRDATA (12 + 12 bytes)
		direct.size = plain.size();
		Bytes f;
		std::string n;
		ShapeExtractResult const r = ShapeExtract_File(lv, direct, &f, &n, nullptr);
		Check(ok && got.empty() && r == ShapeExtractResult::NotOutside, std::string("a shape that came from no file is not listed, and is refused when asked: ") + ShapeExtractWords(r));
	}

	{
		// a lying size: a child chunk that says it runs past its shape: the level isn't listed at all, and the shape is refused
		Bytes lying = inLevel;
		PutIntAt(&lying, kHeadingBytes + 8, (int32_t)(lying.size() * 4));
		Bytes const lv = LevelOf({ lying });
		std::vector<ShapeExtractPlaced> got;
		bool const ok = ShapeExtract_Placed(lv, &got);
		ShapeExtractPlaced direct;
		direct.at = kHeadingBytes + 24;
		direct.size = lying.size();
		Bytes f;
		std::string n;
		ShapeExtractResult const r = ShapeExtract_File(lv, direct, &f, &n, nullptr);
		Check(!ok && got.empty() && r == ShapeExtractResult::BadShape && f.empty(), "a chunk whose size runs past its parent: the level's list refused, the shape refused");

		// a shape asked for at a place that isn't one
		ShapeExtractPlaced wrong;
		wrong.at = 3;
		wrong.size = 40;
		Check(ShapeExtract_File(level, wrong, &f, &n, nullptr) == ShapeExtractResult::BadShape, "a shape asked for at a place that isn't a whole REBSHAPE is refused");
	}

	// A PART OF A FILE (Bishop 2's ruling A, 11-23, the owner's rule "call them by whatever game name we found them with"): a copy whose SHPEXTFL names an
	// object (EXTOBJNM) is that object of the file, which the game itself calls "<object>@<RIFFNAME>" (chunk_shape.cpp:1914); made plain, <object>_<file>.rif,
	// so no part ever takes the whole file's name (a fragment file's part named missfrg.rif, where Rebellion's missfrg.rif is 19 shapes). A copy with no
	// EXTOBJNM keeps the file's name, and the same bytes under either are one shape.
	{
		auto variantOf = [&](Bytes const &shape, int seed)
		{
			Bytes v = shape;
			std::vector<Span> p;
			Children(v, kHeadingBytes, v.size(), &p);
			v[p[FirstNamed(v, p, "SHPPOLYS")].at + kHeadingBytes] = (char)(0x60 + seed);
			return v;
		};

		Bytes const part = AsAPart(inLevel, "ductcove01");
		Bytes const partLevel = LevelOf({ part });
		std::vector<ShapeExtractPlaced> got;
		ShapeExtract_Placed(partLevel, &got);
		Bytes f;
		std::string n;
		ShapeExtractResult const r = got.empty() ? ShapeExtractResult::BadShape : ShapeExtract_File(partLevel, got[0], &f, &n, nullptr);
		Check(r == ShapeExtractResult::Done && n == "ductcove01_medbox01.rif" && f == file,
			"a part (EXTOBJNM ductcove01 of medbox01.rif) is named by the game's own name made plain, \"ductcove01_medbox01.rif\" (it is \"" + n + "\"), its bytes the file's");

		// two different parts of one file in one level: each its own name, neither the file's
		std::vector<ShapeExtractLevel> one(1);
		one[0] = { "Sulaco.RIF", LevelOf({ AsAPart(variantOf(inLevel, 1), "Object01"), AsAPart(variantOf(inLevel, 2), "Object02") }), "" };
		std::vector<ShapeExtractShape> shapes;
		std::vector<ShapeExtractSkipped> skipped;
		std::vector<std::string> refused;
		ShapeExtract_Scan(one, &shapes, &skipped, &refused);
		Check(shapes.size() == 2 && shapes[0].fileName == "object01_medbox01.rif" && shapes[1].fileName == "object02_medbox01.rif",
			"two parts of one file: \"object01_medbox01.rif\" and \"object02_medbox01.rif\", neither \"medbox01.rif\" (" + (shapes.size() == 2 ? shapes[0].fileName + ", " +
			shapes[1].fileName : std::to_string(shapes.size()) + " shapes") + ")");

		// a part, and another level's plain copy of OTHER bytes: two names of their own, no conflict between them
		std::vector<ShapeExtractLevel> two(2);
		two[0] = { "Sulaco.RIF", LevelOf({ AsAPart(variantOf(inLevel, 3), "ductcove01") }), "" };
		two[1] = { "Derelict.RIF", LevelOf({ inLevel }), "" };
		ShapeExtract_Scan(two, &shapes, &skipped, &refused);
		Check(shapes.size() == 2 && shapes[0].fileName == "ductcove01_medbox01.rif" && shapes[1].fileName == "medbox01.rif" && shapes[1].conflictOf.empty(),
			"a part and another level's whole-file copy of other bytes: \"ductcove01_medbox01.rif\" and \"medbox01.rif\", no conflict name (" +
			(shapes.size() == 2 ? shapes[0].fileName + ", " + shapes[1].fileName : std::to_string(shapes.size()) + " shapes") + ")");

		// THE SAME BYTES under a part's name and the file's: one shape, by the FILE's name (a one-shape file whose only object is named, ndrum.rif's "drum"),
		// whichever level comes first
		for (int order = 0; order < 2; order++)
		{
			std::vector<ShapeExtractLevel> same(2);
			same[order] = { "Hangar.rif", LevelOf({ AsAPart(inLevel, "drum") }), "" };
			same[1 - order] = { "Derelict.RIF", LevelOf({ inLevel }), "" };
			ShapeExtract_Scan(same, &shapes, &skipped, &refused);
			Check(shapes.size() == 1 && shapes[0].fileName == "medbox01.rif" && shapes[0].levels.size() == 2,
				std::string("the same bytes as a named part and as the whole file are one shape, \"medbox01.rif\", both levels listed (") + (order ? "the whole file" : "the part") +
				" first: " + (shapes.size() == 1 ? shapes[0].fileName : std::to_string(shapes.size()) + " shapes") + ")");
		}
	}

	// THE SCAN (several levels at once) and THE OWNER'S NAMING RULE
	{
		Check(ShapeExtract_Sha256(Bytes{ 'a', 'b', 'c' }) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 of \"abc\" is the standard's own answer");

		// the same shape as another level holds it: other leftover bytes in its names' pads, another edit count at +60: the file must come out the same
		Bytes otherCopy = inLevel;
		{
			std::vector<Span> p, in;
			Children(otherCopy, kHeadingBytes, otherCopy.size(), &p);
			PutIntAt(&otherCopy, p[FirstNamed(otherCopy, p, "SHPHEAD1")].at + kHeadingBytes + 60, 99);
			size_t const ext = FirstNamed(otherCopy, p, "SHPEXTFL");
			Children(otherCopy, p[ext].at + kHeadingBytes, p[ext].at + p[ext].size, &in);
			memcpy(otherCopy.data() + in[FirstNamed(otherCopy, in, "RIFFNAME")].at + kHeadingBytes + 5, "xyz", 3);
		}

		// another shape under the same name: one polygon byte changed (as an edited copy's polygons are)
		auto variant = [&](int seed)
		{
			Bytes v = inLevel;
			std::vector<Span> p;
			Children(v, kHeadingBytes, v.size(), &p);
			v[p[FirstNamed(v, p, "SHPPOLYS")].at + kHeadingBytes] = (char)(0x40 + seed);
			return v;
		};

		Bytes scaled = inLevel;
		{
			std::vector<Span> p, in;
			Children(scaled, kHeadingBytes, scaled.size(), &p);
			size_t const ext = FirstNamed(scaled, p, "SHPEXTFL");
			Children(scaled, p[ext].at + kHeadingBytes, p[ext].at + p[ext].size, &in);
			double const half = 0.5;
			memcpy(scaled.data() + in[FirstNamed(scaled, in, "SHPEXTFN")].at + kHeadingBytes, &half, sizeof(half));
		}

		Bytes broken = LevelOf({ inLevel });
		PutIntAt(&broken, kHeadingBytes + 24 + 8, (int32_t)(broken.size() * 4));   // the shape's chunk (after ENVRDATA's 24 bytes) says it runs past the level

		std::vector<ShapeExtractLevel> levels(5);
		levels[0] = { "Derelict.RIF", LevelOf({ inLevel }), "aa" };
		levels[1] = { "E3demo.rif", LevelOf({ otherCopy, scaled }), "bb" };
		levels[2] = { "Genshd1.RIF", LevelOf({ variant(1), variant(2) }), "cc" };
		levels[3] = { "Broken.rif", broken, "dd" };
		levels[4] = { "Hangar.rif", LevelOf({ inLevel, variant(1) }), "ee" };
		std::vector<ShapeExtractShape> shapes;
		std::vector<ShapeExtractSkipped> skipped;
		std::vector<std::string> refused;
		ShapeExtract_Scan(levels, &shapes, &skipped, &refused);

		Check(shapes.size() == 3, "three shape files from five levels: medbox01 once, and two conflicts (" + std::to_string(shapes.size()) + ")");

		if (shapes.size() == 3)
		{
			ShapeExtractShape const &a = shapes[0], &b = shapes[1], &c = shapes[2];
			Check(a.fileName == "medbox01.rif" && a.conflictOf.empty() && a.file == file && a.sha256 == ShapeExtract_Sha256(file),
				"DETERMINISTIC: the first is medbox01.rif, the very file ShapeExtract_File made above, its SHA-256 the file's");
			Check(a.levels == std::vector<std::string>({ "Derelict.RIF", "E3demo.rif", "Hangar.rif" }) && a.levelSha256 == std::vector<std::string>({ "aa", "bb", "ee" }),
				"the SAME BYTES are one shape: Derelict's copy, E3demo's (other leftover bytes and edit count) and Hangar's, each level listed with its SHA-256");
			Check(b.fileName == "medbox01_genshd1.rif" && b.conflictOf == "medbox01.rif" && c.fileName == "medbox01_genshd1_2.rif" && c.conflictOf == "medbox01.rif",
				"a CONFLICT is <name>_<level>, then _2: \"" + b.fileName + "\", \"" + c.fileName + "\"");
			Check(b.levels == std::vector<std::string>({ "Genshd1.RIF", "Hangar.rif" }) && c.levels == std::vector<std::string>({ "Genshd1.RIF" }),
				"Hangar's copy of the first conflict's bytes is listed with it, not made again");
			Check(b.sha256 != a.sha256 && c.sha256 != b.sha256 && b.sha256 == ShapeExtract_Sha256(b.file), "different bytes, different SHA-256s");
			// each copy is named by its level and where its REBSHAPE starts there, so a level's objects can be credited to the very variant they place
			size_t const second = kHeadingBytes + 24 + inLevel.size();   // Hangar's second shape: after ENVRDATA (24 bytes) and the first shape
			Check(a.copies.size() == 3 && a.copies[2] == std::make_pair(std::string("Hangar.rif"), kHeadingBytes + 24) && b.copies.size() == 2 &&
				b.copies[1] == std::make_pair(std::string("Hangar.rif"), second), "each copy is listed with its level and its shape's place there");
		}

		Check(skipped.size() == 1 && skipped[0].level == "E3demo.rif" && skipped[0].result == ShapeExtractResult::Rescaled, "the rescaled copy is skipped, and says why");
		Check(refused == std::vector<std::string>({ "Broken.rif" }), "a level whose chunks can't be walked is left out whole");

		// the same levels again give the same files under the same names, and the same SHA-256s
		std::vector<ShapeExtractShape> again;
		ShapeExtract_Scan(levels, &again, &skipped, &refused);
		bool same = again.size() == shapes.size();

		for (size_t i = 0; same && i < again.size(); i++)
		{
			same = again[i].fileName == shapes[i].fileName && again[i].sha256 == shapes[i].sha256 && again[i].file == shapes[i].file;
		}

		Check(same, "DETERMINISTIC: a second scan of the same levels gives the same files, names and SHA-256s");

		// a long level name (a stranger's) still gives a plain name the right size
		std::vector<ShapeExtractLevel> two(2);
		two[0] = { "a.rif", LevelOf({ inLevel }), "" };
		two[1] = { std::string(200, 'L') + ".RIF", LevelOf({ variant(3) }), "" };
		ShapeExtract_Scan(two, &shapes, &skipped, &refused);
		Check(shapes.size() == 2 && shapes[1].fileName.size() <= 63 + 4 && shapes[1].fileName.compare(0, 9, "medbox01_") == 0,
			"a stranger's long level name gives a conflict name of at most 63 letters and .rif (" + (shapes.size() == 2 ? shapes[1].fileName : std::string("none")) + ")");
	}

	// THE KINDS (wishlist 9): what the game makes of each object placing an outside shape, read as avpx's loader reads its AVPSTRAT
	{
		int32_t const place[3] = { 0, 0, 0 };
		float const turn[4] = { 0, 0, 0, 1 };

		// an object placing shape 12 with the given AVPSTRAT payload after its index and spare (none: no OBJPRJDT), and a PLOBJLIT if asked
		auto object = [&](std::vector<int32_t> const &strategy, bool light)
		{
			Bytes o = PlacingObject(12, place, turn);   // RBOBJECT holding OBJHEAD1
			Bytes inside(o.begin() + (std::ptrdiff_t)kHeadingBytes, o.end());

			if (!strategy.empty())
			{
				Bytes payload;
				AddInt(&payload, 0);   // index
				AddInt(&payload, 0);   // spare

				for (int32_t n : strategy)
				{
					AddInt(&payload, n);
				}

				Add(&inside, MakeChunk("OBJPRJDT", MakeChunk("AVPSTRAT", payload)));
			}

			if (light)
			{
				Add(&inside, MakeChunk("PLOBJLIT", Filled(60, 40)));
			}

			return MakeChunk("RBOBJECT", inside);
		};

		struct { std::vector<int32_t> strategy; bool light; char const *kind; } const cases[] = {
			{ { 62, 0, 0, 4, 0, 50, 2 }, false, "health" },                         // kind 62: Type (4: health) and ExtraData after the 12-byte head
			{ { 62, 0, 0, 2, 0, 50, 2 }, false, "weapon: pulse rifle" },
			{ { 62, 0, 0, 2, 22, 50, 2 }, false, "weapon: marine pistol" },
			{ { 62, 9, 9, 3, 2, 50, 2 }, false, "ammo: smartgun" },                 // (the head's own Type and ExtraData, 9 and 9, are not kind 62's)
			{ { 62, 0, 0, 0, 0, 50, 2 }, true, "placed light" },
			{ { 62, 0, 0, 9, 0, 50, 2 }, false, "video screen" },
			{ { 53, 5, 0 }, false, "armour" },                                      // kind 53: Type and ExtraData in the head
			{ { 75, 0, 0 }, false, "fan" },
			{ {}, false, "prop (no strategy)" },
			{ { 62, 0, 0, 4 }, false, "strategy too short" },                       // (Tao 3: a kind 62 cut short says so, and nothing past it is read)
		};

		for (auto const &one : cases)
		{
			Bytes whole;
			Add(&whole, inLevel);
			Add(&whole, object(one.strategy, one.light));
			Bytes const lv = MakeChunk("REBINFF2", whole);
			std::vector<ShapeExtractPlacement> got;
			bool const read = ShapeExtract_Placements(lv, &got);
			Check(read && got.size() == 1 && got[0].fileName == "medbox01.rif" && got[0].shapeAt == kHeadingBytes && got[0].kind == one.kind, std::string("an object placing medbox01 is \"") + one.kind +
				"\" (" + (got.size() == 1 ? got[0].kind : std::to_string(got.size()) + " found") + ")");
		}

		// an object placing a shape that isn't an outside one, or no shape the level has, is no placement of an outside shape
		Bytes whole;
		Add(&whole, inLevel);
		Bytes const stray = PlacingObject(13, place, turn);
		Add(&whole, stray);
		std::vector<ShapeExtractPlacement> got;
		Check(ShapeExtract_Placements(MakeChunk("REBINFF2", whole), &got) && got.empty(), "an object whose shape number no outside shape has is not counted");
	}

	// ONE MAP, AS ITS FILE HOLDS IT (the call per map of the owner's rule): plain or packed, the same shapes; anything that isn't a map refused, with why
	{
		int32_t const place[3] = { 0, 0, 0 };
		float const turn[4] = { 0, 0, 0, 1 };
		Bytes whole;
		Add(&whole, MakeChunk("ENVRDATA", Filled(12, 20)));
		Add(&whole, inLevel);
		Add(&whole, PlacingObject(12, place, turn));
		Bytes const plain = MakeChunk("REBINFF2", whole);

		ShapeExtractMap const a = ShapeExtract_ScanMap("Derelict.RIF", plain);
		Check(a.scanned && a.why.empty() && a.shapes.size() == 1 && a.shapes[0].fileName == "medbox01.rif" && a.shapes[0].file == file && a.sha256 == ShapeExtract_Sha256(plain) &&
			a.placements.size() == 1 && a.placements[0].kind == "prop (no strategy)", "a plain map: its one shape, the file ShapeExtract_File made, the map's SHA-256 and its placing object");

		// the same map PACKED as the old tools pack a level (REBCRIF1, the game's own HuffmanCompression): the same shape, byte for byte
		HuffmanPackage *const package = HuffmanCompression((unsigned char *)const_cast<char *>(plain.data()), (int)plain.size());
		Bytes packed;

		if (package)
		{
			packed.assign((char const *)package, (char const *)package + sizeof(HuffmanPackage) + package->CompressedDataSize);
			DeallocMem(package);
		}

		ShapeExtractMap const b = ShapeExtract_ScanMap("Derelict.RIF", packed);
		Check(!packed.empty() && b.scanned && b.shapes.size() == 1 && b.shapes[0].file == a.shapes[0].file && b.shapes[0].sha256 == a.shapes[0].sha256 && b.sha256 == ShapeExtract_Sha256(packed),
			"the same map PACKED (REBCRIF1): unpacked by the game's decoder, the same shape and SHA-256; the map's own SHA-256 is the packed file's (" + b.why + ")");

		ShapeExtractMap const again = ShapeExtract_ScanMap("Derelict.RIF", packed);
		Check(again.scanned && again.shapes.size() == 1 && again.shapes[0].sha256 == b.shapes[0].sha256 && again.sha256 == b.sha256, "DETERMINISTIC: the same bytes scanned again give the same");

		// REFUSED (Tao 3), each with its own words, and each still gives the bytes' SHA-256 (so the list keeps it and it isn't tried at every start)
		Bytes cut(packed.begin(), packed.begin() + (std::ptrdiff_t)(packed.size() / 2));
		Bytes huge = packed;

		if (huge.size() >= sizeof(HuffmanPackage))
		{
			PutIntAt(&huge, offsetof(HuffmanPackage, UncompressedDataSize), 0x7fffffff);
		}

		Bytes longer = packed;
		longer.insert(longer.end(), 4, 0);

		// a code table that doesn't fill the code space (every code length count 0): the game's decoder refuses it
		Bytes badCode = packed;

		for (size_t n = 0; badCode.size() >= sizeof(HuffmanPackage) && n < MAX_DEPTH; n++)
		{
			PutIntAt(&badCode, offsetof(HuffmanPackage, CodelengthCount) + 4 * n, 0);
		}

		Bytes broken = plain;
		PutIntAt(&broken, kHeadingBytes + 24 + 8, (int32_t)(broken.size() * 4));   // the shape's chunk says it runs past the map
		Bytes const junk = Filled(400, 50), tooBig(kShapeExtractMostMapBytes + 1, 0);

		// (the words' start is checked: the length refusal goes on with the two numbers)
		struct { Bytes const *bytes; char const *words; char const *what; } const refusals[] = {
			{ &cut, "packed, but the file isn't its header and the ", "a packed map cut to half its bytes (the game's decoder alone would make bytes of the missing end)" },
			{ &longer, "packed, but the file isn't its header and the ", "a packed map with 4 bytes added" },
			{ &badCode, "packed, but its packing doesn't decode safely and completely", "a packed map whose code table doesn't fill the code space" },
			{ &huge, "packed, but the size it says it unpacks to is not a map's", "a packed map that says it unpacks to 2 GB" },
			{ &broken, "its chunks can't be walked: a size runs past its parent", "a plain map whose shape's size runs past it" },
			{ &junk, "not a map: it is neither a plain (REBINFF2) nor a packed (REBCRIF1) level file", "400 bytes that are no map" },
			{ &tooBig, "the file is larger than 16 MB", "a file of 16 MB and 1 byte" } };

		for (auto const &one : refusals)
		{
			ShapeExtractMap const r = ShapeExtract_ScanMap("x.rif", *one.bytes);
			Check(!r.scanned && r.why.compare(0, strlen(one.words), one.words) == 0 && r.shapes.empty() && r.placements.empty() && r.sha256 == ShapeExtract_Sha256(*one.bytes),
				std::string(one.what) + " is refused: \"" + r.why + "\", and its SHA-256 still given");
		}
	}

	// THE WORK A HOSTILE MAP MAY ASK FOR (Vasquez's MUST, 11-23: the scan runs at every start on the editor's own thread). A map AT the limits (1,024 outside
	// shapes, all one name and each other bytes of one size: the worst case for matching and for conflict names; 16,384 objects placing them) is scanned in
	// seconds; one shape or one object over is refused in words, its SHA-256 still given. (The numbers are written out, not shape_extract.h's, so this
	// test also builds against the code before the limits, for its red.)
	{
		int32_t const place[3] = { 0, 0, 0 };
		float const turn[4] = { 0, 0, 0, 1 };

		auto mapOf = [&](size_t shapes, size_t objects)
		{
			Bytes whole;
			Add(&whole, MakeChunk("ENVRDATA", Filled(12, 20)));
			std::vector<Span> p;
			Children(inLevel, kHeadingBytes, inLevel.size(), &p);
			size_t const polys = p[FirstNamed(inLevel, p, "SHPPOLYS")].at + kHeadingBytes;

			for (size_t i = 0; i < shapes; i++)
			{
				Bytes one = inLevel;
				one[polys] = (char)(i & 0xff);
				one[polys + 1] = (char)((i >> 8) & 0xff);
				Add(&whole, one);
			}

			Bytes const object = PlacingObject(12, place, turn);

			for (size_t i = 0; i < objects; i++)
			{
				Add(&whole, object);
			}

			return MakeChunk("REBINFF2", whole);
		};

		Bytes const atLimits = mapOf(1024, 16384);
		auto const start = std::chrono::steady_clock::now();
		ShapeExtractMap const big = ShapeExtract_ScanMap("Big.rif", atLimits);
		long long const ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		Check(big.scanned && big.shapes.size() == 1024 && big.placements.size() == 16384 && ms < 5000,
			"a map AT the limits (1,024 outside shapes of one name, all other bytes; 16,384 objects) is scanned in " + std::to_string(ms) + " ms (under 5 s): " +
			std::to_string(big.shapes.size()) + " shapes, " + std::to_string(big.placements.size()) + " placements " + big.why);
		Check(big.shapes.size() == 1024 && big.shapes[1023].fileName == "medbox01_big_1023.rif", "its conflicts named to the last: " +
			(big.shapes.size() == 1024 ? big.shapes[1023].fileName : std::string("-")));

		Bytes const tooManyShapes = mapOf(1025, 1), tooManyObjects = mapOf(1, 16385);
		ShapeExtractMap const a = ShapeExtract_ScanMap("x.rif", tooManyShapes), b = ShapeExtract_ScanMap("x.rif", tooManyObjects);
		Check(!a.scanned && a.why.find("1025 outside shapes, more than the 1024") != std::string::npos && a.sha256 == ShapeExtract_Sha256(tooManyShapes),
			"1,025 outside shapes: REFUSED (\"" + a.why + "\"), its SHA-256 given so it isn't tried again");
		Check(!b.scanned && b.why.find("16385 objects, more than the 16384") != std::string::npos && b.sha256 == ShapeExtract_Sha256(tooManyObjects),
			"16,385 objects: REFUSED (\"" + b.why + "\")");
	}

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
