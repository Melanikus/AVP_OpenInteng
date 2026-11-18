// verify_outside_shape: MAKES the level and the outside shape file of acceptance-outside-shapes-open (OP-137a, OP-140; reassess/object-placement.md section 7), and CHECKS a saved copy.
// Written 2026-11-17 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_outside_shape.exe make <a stock level> <the level to make, ending ".made.rif"> <the shape folder> [counts | many | bitmaps]
//      verify_outside_shape.exe check <the same stock level> <a saved copy> [counts | many | bitmaps]
// THE HOSTILE VARIANTS (the 11-17 review of the update, hostile-input-1..4; each is a file a level can name, and the update must stay safe):
//   counts:  the outside header's point count is the real one + 2^30 (x 12 it wraps a 32-bit size_t onto the SHPRAWVT's size) and its polygon count 12345. check: the copy's shape is
//            twice as big and its header holds the counts of its own chunks (SHPRAWVT / 12, SHPPOLYS / 36), never the file's.
//   many:    the level holds 650 more copies of the shape (numbers above every other), all naming the file, whose SHPRAWVT is the doubled points repeated to just under 4 MB: made
//            all at once they would pass the 32-bit editor's memory. check: the update was refused (the level would pass what the game's loader takes): the shape is the stock one.
//   unchanged (check only): the file named is the stock shape, point for point: never written (a data-folder original, OP-137b)
//   bitmaps: the shape's SHPEXTFL holds a BMPLSTST of 65535 names and the file a REBENVDT > BMPNAMES of the same 65535: compared each against each they took minutes. check:
//            the copy's shape is twice as big (the open did not hang).
// THE SHAPE, chosen the same way by both: the stock level's first OUTSIDE shape (a REBSHAPE whose SHPEXTFL holds a SHPEXTFN) that is used by an object (its
// header's count is above 0, so inteng keeps the shape's own rescale, FUN_00512fc0 0x51346c), has a rescale of exactly 1, and has points, each still inside the game's 2^24 when twice as far.
// make: the stock level is copied to the new name with that shape's SHPEXTFN naming the file "OpenOutsideTest.rif" (a name no stock level uses, so the shape file this writes can't change
// any other level a later scenario opens from the same folder), and <the shape folder>\OpenOutsideTest.rif is written: an unpacked REBINFF2 holding ONE shape, the level's own with every
// point and its header's radius and box twice as far out. inteng takes the file's shape by the EXTOBJNM's object (the header then lists that object alone) or, with no EXTOBJNM (most
// stock outside shapes), as the file's only shape (0x513151). Both files are read back.
// check: the copy's shape of that number must still name OpenOutsideTest.rif and have exactly twice the stock shape's points and box: the open brought it up to date from the file (inteng's
// FUN_005149e0 -> FUN_00512fc0(0) at every load), with the rescale 1. Every file is read by the game's own loader, and its bytes are walked as hostile.
// A level file is replaced only when its name ends ".made.rif", and a shape file only when it is named OpenOutsideTest.rif (only this tool makes those). Named verify_ so run-shards builds
// it from the tree it runs (its make-level line too). Gives back 0 only when the files were made and read back (make), or every check passed (check).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_import.h"
#include "shape_update.h"

// what the game's chunk code calls
HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("FAILED verify_outside_shape: the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

using namespace host::shape_chunks;

static char const kTestFile[] = "OpenOutsideTest";   // the outside file's name this tool gives the shape (no stock level names it)
static size_t const kHeadPayload = 68;                // SHPHEAD1 up to the count of the objects that use the shape (their names follow)
static int64_t const kFarthest = 1 << 24;             // the game's farthest place (chunk.hpp CheckPlace)
static size_t const kManyCopies = 650;                // the many variant's extra copies of the shape: the made level stays under the 16 MB a level file may be (OpenWith_TakeLevel), and 651 shapes of
                                                      // 3.9 MB each pass the 2 GB a 32-bit program without /LARGEADDRESSAWARE has (AVP_OpenInteng.vcxproj sets none)
static size_t const kBigFileBytes = 3900000;          // the many variant's SHPRAWVT: just under the update's 4 MB cap on a file (kShapeImportMostBytes)
static int32_t const kBitmapCount = 65535;            // the most a list's head word counts
static std::string variant;                           // "", "counts", "many" or "bitmaps"

// The bitmaps variant's list (avpx bitmap_names.cpp, as shape_update.cpp ReadBitmaps reads it): the head word (the count), then each name: flags, index, version, priority, transparency,
// the name ended by a 0 and padded to four
static Bytes BitmapList()
{
	Bytes list;

	AddInt(&list, kBitmapCount);

	for (int32_t i = 0; i < kBitmapCount; i++)
	{
		char name[16];

		snprintf(name, sizeof(name), "t%05d", (int)i);
		AddInt(&list, 0);
		AddInt(&list, i % 32768);
		AddInt(&list, 0);
		AddInt(&list, 0);
		AddInt(&list, 0);
		list.insert(list.end(), name, name + strlen(name));
		list.push_back(0);
		PadToFour(&list);
	}

	return list;
}

// The chosen shape, as the stock level holds it
struct Chosen
{
	size_t top = 0;              // which of the level's top chunks
	int32_t number = 0;          // its SHPHEAD1 number (+20), which the editor's update keeps
	std::string objectName;      // its EXTOBJNM's object (empty: none; most stock outside shapes have none)
	std::string fileName;        // the SHPEXTFN's file, as the update names it
	std::vector<int32_t> points; // its SHPRAWVT, x, y, z for each point
	int32_t box[6] = {};         // its header's max.x, min.x, max.y, min.y, max.z, min.z
};

static bool Fail(std::string const &why)
{
	printf("FAILED verify_outside_shape: %s\n", why.c_str());
	return false;
}

static int Stop(std::string const &why)
{
	Fail(why);
	return 1;
}

static bool EndsWith(std::string const &text, std::string const &end)
{
	if (text.size() < end.size())
	{
		return false;
	}

	for (size_t i = 0; i < end.size(); i++)
	{
		char a = text[text.size() - end.size() + i];
		char b = end[i];

		if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
		if (a != b) return false;
	}

	return true;
}

// The words from `from` up to their 0, all before `end`; false with no 0 there
static bool TextIn(Bytes const &b, size_t from, size_t end, std::string *text)
{
	for (size_t at = from; at < end && at < b.size(); at++)
	{
		if (b[at] == 0)
		{
			text->assign(b.begin() + (std::ptrdiff_t)from, b.begin() + (std::ptrdiff_t)at);
			return true;
		}
	}

	return false;
}

// The level, unpacked, as the game's own loader reads it
static bool LevelImage(char const *path, Bytes *image, std::vector<Span> *top)
{
	OpenLevelFile level;
	size_t total = 0;

	if (level.Open(path) != LevelOpenResult::Opened)
	{
		return Fail(std::string("the game's loader can't open ") + path);
	}

	if (!level.BuildLevel(*image) || image->size() < kHeadingBytes || !NameIs(*image, 0, "REBINFF2") || !ChunkSizeAt(*image, 0, image->size(), &total) ||
		total != image->size() || !Children(*image, kHeadingBytes, total, top))
	{
		return Fail(std::string("the chunks of ") + path + " can't be walked");
	}

	return true;
}

// The SHPEXTFL, SHPEXTFN and EXTOBJNM of a shape's parts; false when it isn't an outside shape that can be read
static bool OutsideParts(Bytes const &b, std::vector<Span> const &parts, size_t *extfl, std::vector<Span> *extParts, size_t *extfn, size_t *extobjnm)
{
	*extfl = FirstNamed(b, parts, "SHPEXTFL");

	if (*extfl == parts.size() || !Children(b, parts[*extfl].at + kHeadingBytes, parts[*extfl].at + parts[*extfl].size, extParts))
	{
		return false;
	}

	*extfn = FirstNamed(b, *extParts, "SHPEXTFN");
	*extobjnm = FirstNamed(b, *extParts, "EXTOBJNM");
	return *extfn != extParts->size() && (*extParts)[*extfn].size >= kHeadingBytes + 13;
}

// THE SHAPE both modes test: see the top of this file
static bool Choose(Bytes const &b, std::vector<Span> const &top, Chosen *chosen)
{
	std::string passed;   // why each outside shape was passed over, for the failure's words

	for (size_t t = 0; t < top.size(); t++)
	{
		std::vector<Span> parts, extParts;
		size_t extfl = 0, extfn = 0, extobjnm = 0;
		Chosen one;

		if (!NameIs(b, top[t].at, "REBSHAPE") || !Children(b, top[t].at + kHeadingBytes, top[t].at + top[t].size, &parts) ||
			!OutsideParts(b, parts, &extfl, &extParts, &extfn, &extobjnm))
		{
			continue;
		}

		std::vector<char> const chunk(b.begin() + (std::ptrdiff_t)top[t].at, b.begin() + (std::ptrdiff_t)(top[t].at + top[t].size));
		size_t const head = FirstNamed(b, parts, "SHPHEAD1");
		size_t const raw = FirstNamed(b, parts, "SHPRAWVT");
		Span const fn = extParts[extfn];
		double rescale = 0.0;
		char why[160] = "";

		host::ShapeUpdate_FileName(chunk, &one.fileName);
		memcpy(&rescale, b.data() + fn.at + kHeadingBytes, sizeof(rescale));

		size_t const hp = head == parts.size() ? 0 : parts[head].at + kHeadingBytes;
		int32_t const pointCount = hp ? IntAt(b, hp + 24) : 0;

		if (one.fileName.empty())
		{
			snprintf(why, sizeof(why), "its SHPEXTFN names no file");
		}
		else if (extobjnm != extParts.size() && (extParts[extobjnm].size < kHeadingBytes + 5 ||
			!TextIn(b, extParts[extobjnm].at + kHeadingBytes + 4, extParts[extobjnm].at + extParts[extobjnm].size, &one.objectName) || one.objectName.empty()))
		{
			snprintf(why, sizeof(why), "its EXTOBJNM can't be read");
		}
		else if (head == parts.size() || parts[head].size < kHeadingBytes + kHeadPayload || raw == parts.size())
		{
			snprintf(why, sizeof(why), "has no header or no SHPRAWVT");
		}
		else if (rescale != 1.0)
		{
			snprintf(why, sizeof(why), "its rescale is %.9g", rescale);
		}
		else if (IntAt(b, hp + 64) <= 0)
		{
			snprintf(why, sizeof(why), "no object uses it");
		}
		else if (pointCount <= 0 || (size_t)pointCount * 12 != parts[raw].size - kHeadingBytes)
		{
			snprintf(why, sizeof(why), "its point count is %d, its SHPRAWVT %d bytes", (int)pointCount, (int)(parts[raw].size - kHeadingBytes));
		}

		if (why[0])
		{
			if (passed.size() < 900)
			{
				passed += "; " + (one.fileName.empty() ? std::string("?") : one.fileName) + (one.objectName.empty() ? std::string() : " (" + one.objectName + ")") + " " + why;
			}

			continue;
		}

		bool inside = true;

		for (size_t i = 0; i < (size_t)pointCount * 3; i++)
		{
			one.points.push_back(IntAt(b, parts[raw].at + kHeadingBytes + i * 4));
			inside = inside && llabs(2 * (int64_t)one.points.back()) < kFarthest;
		}

		for (int k = 0; k < 6; k++)
		{
			one.box[k] = IntAt(b, hp + 36 + (size_t)k * 4);
			inside = inside && llabs(2 * (int64_t)one.box[k]) < kFarthest;
		}

		if (!inside)
		{
			passed += "; " + one.fileName + " would be farther than 2^24 twice as big";
			continue;
		}

		one.top = t;
		one.number = IntAt(b, hp + 20);
		*chosen = one;
		return true;
	}

	return Fail("the stock level has no outside shape that is used, keeps a rescale of 1 and has points" + passed);
}

// The outside file: an unpacked REBINFF2 with the shape twice as big, its header listing the EXTOBJNM's object alone (none without one)
static Bytes OutsideFile(Bytes const &b, Span const &shape, Chosen const &chosen)
{
	std::vector<Span> parts;
	Bytes payload;

	Children(b, shape.at + kHeadingBytes, shape.at + shape.size, &parts);

	for (Span const &part : parts)
	{
		if (NameIs(b, part.at, "SHPEXTFL"))
		{
			continue;
		}

		if (NameIs(b, part.at, "SHPRAWVT"))
		{
			Bytes raw;
			size_t const repeats = variant == "many" ? kBigFileBytes / (chosen.points.size() * 4) : 1;

			for (size_t r = 0; r < repeats; r++)
			{
				for (int32_t v : chosen.points)
				{
					AddInt(&raw, 2 * v);
				}
			}

			Bytes const made = MakeChunk("SHPRAWVT", raw);
			payload.insert(payload.end(), made.begin(), made.end());
		}
		else if (NameIs(b, part.at, "SHPHEAD1"))
		{
			Bytes head(b.begin() + (std::ptrdiff_t)(part.at + kHeadingBytes), b.begin() + (std::ptrdiff_t)(part.at + kHeadingBytes + kHeadPayload));
			float radius = 0.0f;

			memcpy(&radius, head.data() + 32, sizeof(radius));
			radius *= 2.0f;
			memcpy(head.data() + 32, &radius, sizeof(radius));

			for (int k = 0; k < 6; k++)
			{
				PutIntAt(&head, 36 + (size_t)k * 4, 2 * chosen.box[k]);
			}

			if (variant == "counts")
			{
				PutIntAt(&head, 24, (int32_t)(chosen.points.size() / 3 + (1u << 30)));   // x 12 = the real size + 3 x 2^32: a wrap on Win32
				PutIntAt(&head, 28, 12345);
			}

			if (variant == "many")
			{
				// the header states the big SHPRAWVT's own count, so a reader that checks one against the other (the old update did) still builds the big shape
				PutIntAt(&head, 24, (int32_t)((kBigFileBytes / (chosen.points.size() * 4)) * (chosen.points.size() / 3)));
			}

			PutIntAt(&head, 64, chosen.objectName.empty() ? 0 : 1);

			if (!chosen.objectName.empty())
			{
				head.insert(head.end(), chosen.objectName.begin(), chosen.objectName.end());
				head.push_back(0);
			}

			PadToFour(&head);

			Bytes const made = MakeChunk("SHPHEAD1", head);
			payload.insert(payload.end(), made.begin(), made.end());
		}
		else
		{
			AddCopy(&payload, b, part);
		}
	}

	Bytes body;

	if (variant == "bitmaps")
	{
		Bytes envBody = MakeChunk("BMPNAMES", BitmapList());
		Bytes riffPayload(kTestFile, kTestFile + strlen(kTestFile));

		riffPayload.insert(riffPayload.end(), 4 - strlen(kTestFile) % 4, 0);

		Bytes const riff = MakeChunk("RIFFNAME", riffPayload);

		envBody.insert(envBody.end(), riff.begin(), riff.end());

		Bytes const env = MakeChunk("REBENVDT", envBody);

		body.insert(body.end(), env.begin(), env.end());
	}

	Bytes const shapeChunk = MakeChunk("REBSHAPE", payload);

	body.insert(body.end(), shapeChunk.begin(), shapeChunk.end());
	return MakeChunk("REBINFF2", body);
}

// The level with the chosen shape's SHPEXTFN naming OpenOutsideTest.rif (its folder part, rescale and version kept)
static Bytes Renamed(Bytes const &b, std::vector<Span> const &top, Chosen const &chosen)
{
	Span const shape = top[chosen.top];
	std::vector<Span> parts, extParts;
	size_t extfl = 0, extfn = 0, extobjnm = 0;

	Children(b, shape.at + kHeadingBytes, shape.at + shape.size, &parts);
	OutsideParts(b, parts, &extfl, &extParts, &extfn, &extobjnm);

	Span const fn = extParts[extfn];
	std::string oldName;

	TextIn(b, fn.at + kHeadingBytes + 12, fn.at + fn.size, &oldName);

	size_t const cut = oldName.find_last_of(":/\\");
	std::string const newName = (cut == std::string::npos ? std::string() : oldName.substr(0, cut + 1)) + kTestFile + ".rif";
	Bytes fnPayload(b.begin() + (std::ptrdiff_t)(fn.at + kHeadingBytes), b.begin() + (std::ptrdiff_t)(fn.at + kHeadingBytes + 12));

	fnPayload.insert(fnPayload.end(), newName.begin(), newName.end());
	fnPayload.push_back(0);
	PadToFour(&fnPayload);

	Bytes ext(b.begin() + (std::ptrdiff_t)parts[extfl].at, b.begin() + (std::ptrdiff_t)(parts[extfl].at + kHeadingBytes));

	for (size_t i = 0; i < extParts.size(); i++)
	{
		if (i == extfn)
		{
			Bytes const made = MakeChunk("SHPEXTFN", fnPayload);
			ext.insert(ext.end(), made.begin(), made.end());
		}
		else if (!(variant == "bitmaps" && NameIs(b, extParts[i].at, "BMPLSTST")))
		{
			AddCopy(&ext, b, extParts[i]);
		}
	}

	if (variant == "bitmaps")
	{
		Bytes const list = MakeChunk("BMPLSTST", BitmapList());

		ext.insert(ext.end(), list.begin(), list.end());
	}

	PutIntAt(&ext, 8, (int32_t)ext.size());

	Bytes newShape(b.begin() + (std::ptrdiff_t)shape.at, b.begin() + (std::ptrdiff_t)(shape.at + kHeadingBytes));

	for (size_t i = 0; i < parts.size(); i++)
	{
		if (i == extfl)
		{
			newShape.insert(newShape.end(), ext.begin(), ext.end());
		}
		else
		{
			AddCopy(&newShape, b, parts[i]);
		}
	}

	PutIntAt(&newShape, 8, (int32_t)newShape.size());

	Bytes image(b.begin(), b.begin() + (std::ptrdiff_t)shape.at);

	image.insert(image.end(), newShape.begin(), newShape.end());

	if (variant == "many")
	{
		// the copies, each with a shape number above every other shape's (SHPHEAD1 +20), so no object is tied to them
		int32_t highest = 0;

		for (Span const &t : top)
		{
			std::vector<Span> some;

			if (NameIs(b, t.at, "REBSHAPE") && Children(b, t.at + kHeadingBytes, t.at + t.size, &some))
			{
				size_t const h = FirstNamed(b, some, "SHPHEAD1");

				if (h != some.size() && some[h].size >= kHeadingBytes + kHeadPayload && IntAt(b, some[h].at + kHeadingBytes + 20) > highest)
				{
					highest = IntAt(b, some[h].at + kHeadingBytes + 20);
				}
			}
		}

		std::vector<Span> newParts;

		Children(newShape, kHeadingBytes, newShape.size(), &newParts);

		size_t const h = FirstNamed(newShape, newParts, "SHPHEAD1");

		for (size_t k = 0; k < kManyCopies && h != newParts.size(); k++)
		{
			Bytes copy = newShape;

			PutIntAt(&copy, newParts[h].at + kHeadingBytes + 20, highest + 1 + (int32_t)k);
			image.insert(image.end(), copy.begin(), copy.end());
		}
	}
	image.insert(image.end(), b.begin() + (std::ptrdiff_t)(shape.at + shape.size), b.end());
	PutIntAt(&image, 8, (int32_t)image.size());
	return image;
}

static bool WriteFile(std::string const &path, Bytes const &bytes)
{
	FILE *file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") != 0 || !file)
	{
		return Fail("can't write " + path);
	}

	bool const written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	return (fclose(file) == 0 && written) || Fail("can't write all of " + path);
}

static int Make(char const *stock, std::string const &made, std::string folder)
{
	if (!EndsWith(made, ".made.rif"))
	{
		return Stop("the level to make, \"" + made + "\", must end \".made.rif\" (the only level files this tool replaces)");
	}

	Bytes image;
	std::vector<Span> top;
	Chosen chosen;

	if (!LevelImage(stock, &image, &top) || !Choose(image, top, &chosen))
	{
		return 1;
	}

	Bytes const outside = OutsideFile(image, top[chosen.top], chosen);
	Bytes const renamed = Renamed(image, top, chosen);

	// the level, through the game's loader: it takes the bytes or refuses them
	{
		OpenLevelFile level;

		if (level.Open(stock) != LevelOpenResult::Opened || level.ReplaceLevel(renamed) != LevelOpenResult::Opened)
		{
			return Stop("the game's loader refused the level with the shape's file renamed");
		}

		LevelSaveResult const saved = level.SaveAs(made.c_str());

		if (saved != LevelSaveResult::Saved)
		{
			return Stop("the level couldn't be saved to " + made + " (" + std::to_string((int)saved) + ")");
		}
	}

	// the shape file, in a plain folder (made when it isn't there)
	while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
	{
		folder.pop_back();
	}

	DWORD const attributes = GetFileAttributesA(folder.c_str());

	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		if (!CreateDirectoryA(folder.c_str(), nullptr))
		{
			return Stop("can't make the shape folder " + folder);
		}
	}
	else if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		return Stop("the shape folder " + folder + " is not a plain folder");
	}

	std::string const shapePath = folder + "\\" + kTestFile + ".rif";

	if (!WriteFile(shapePath, outside))
	{
		return 1;
	}

	// read back: the level's shape names the test file, and the shape file is what was made
	Bytes back;
	std::vector<Span> backTop;
	Chosen again;
	FILE *file = nullptr;
	Bytes read(outside.size() + 1, 0);
	size_t got = 0;

	if (fopen_s(&file, shapePath.c_str(), "rb") == 0 && file)
	{
		got = fread(read.data(), 1, read.size(), file);
		fclose(file);
	}

	read.resize(got);

	if (!LevelImage(made.c_str(), &back, &backTop) || !Choose(back, backTop, &again) || again.number != chosen.number || again.fileName != kTestFile || read != outside)
	{
		return Stop("the files made don't read back as they were made");
	}

	printf("ok verify_outside_shape make: %s, shape %d (file %s.rif in the stock level) now names %s.rif; %s holds it with its %d points and box twice as far out\n",
		made.c_str(), chosen.number, chosen.fileName.c_str(), kTestFile, shapePath.c_str(), (int)(chosen.points.size() / 3));
	return 0;
}

// strip-centre (V-30b): the stock level copied with the SHPCENTR of the shape the named object is tied to taken out (the first RBOBJECT of that OBJHEAD1 name, +60; its shape the first
// REBSHAPE whose SHPHEAD1 number, +20, is the object's shape number, +56, as FUN_004bf1d0 ties them). No stock outside shape lacks one, so V-30a's making of it (by putting the level
// together again) is only reached so. The shape's SHPEXTFN is left as it is: no file of that name is in a run's shape folder, so the open changes nothing.
static int StripCentre(char const *stock, std::string const &made, std::string const &objectName)
{
	if (!EndsWith(made, ".made.rif"))
	{
		return Stop("the level to make, \"" + made + "\", must end \".made.rif\"");
	}

	Bytes image;
	std::vector<Span> top;

	if (!LevelImage(stock, &image, &top))
	{
		return 1;
	}

	int32_t number = 0;
	bool found = false;

	for (Span const &t : top)
	{
		std::vector<Span> parts;
		std::string name;

		if (!NameIs(image, t.at, "RBOBJECT") || !Children(image, t.at + kHeadingBytes, t.at + t.size, &parts))
		{
			continue;
		}

		size_t const head = FirstNamed(image, parts, "OBJHEAD1");

		if (head != parts.size() && parts[head].size >= kHeadingBytes + 61 && TextIn(image, parts[head].at + kHeadingBytes + 60, parts[head].at + parts[head].size, &name) && name == objectName)
		{
			number = IntAt(image, parts[head].at + kHeadingBytes + 56);
			found = true;
			break;
		}
	}

	if (!found)
	{
		return Stop("the stock level has no object named " + objectName);
	}

	for (size_t i = 0; i < top.size(); i++)
	{
		std::vector<Span> parts;

		if (!NameIs(image, top[i].at, "REBSHAPE") || !Children(image, top[i].at + kHeadingBytes, top[i].at + top[i].size, &parts))
		{
			continue;
		}

		size_t const head = FirstNamed(image, parts, "SHPHEAD1");

		if (head == parts.size() || parts[head].size < kHeadingBytes + 24 || IntAt(image, parts[head].at + kHeadingBytes + 20) != number)
		{
			continue;
		}

		if (FirstNamed(image, parts, "SHPCENTR") == parts.size())
		{
			return Stop("the shape of " + objectName + " has no SHPCENTR to take out");
		}

		Bytes shape(image.begin() + (std::ptrdiff_t)top[i].at, image.begin() + (std::ptrdiff_t)(top[i].at + kHeadingBytes));

		for (Span const &part : parts)
		{
			if (!NameIs(image, part.at, "SHPCENTR"))
			{
				AddCopy(&shape, image, part);
			}
		}

		PutIntAt(&shape, 8, (int32_t)shape.size());

		Bytes level(image.begin(), image.begin() + (std::ptrdiff_t)top[i].at);

		level.insert(level.end(), shape.begin(), shape.end());
		level.insert(level.end(), image.begin() + (std::ptrdiff_t)(top[i].at + top[i].size), image.end());
		PutIntAt(&level, 8, (int32_t)level.size());

		OpenLevelFile open;

		if (open.Open(stock) != LevelOpenResult::Opened || open.ReplaceLevel(level) != LevelOpenResult::Opened || open.SaveAs(made.c_str()) != LevelSaveResult::Saved)
		{
			return Stop("the game's loader refused the level with the SHPCENTR taken out, or it couldn't be saved");
		}

		printf("ok verify_outside_shape strip-centre: %s, the shape of %s (number %d) has no SHPCENTR\n", made.c_str(), objectName.c_str(), (int)number);
		return 0;
	}

	return Stop("the stock level has no shape numbered " + std::to_string(number));
}

static int Check(char const *stock, char const *copy)
{
	Bytes image, saved;
	std::vector<Span> top, savedTop;
	Chosen chosen;

	if (!LevelImage(stock, &image, &top) || !Choose(image, top, &chosen) || !LevelImage(copy, &saved, &savedTop))
	{
		return 1;
	}

	// the copy's shape of that number
	std::vector<Span> parts;
	size_t found = 0;

	for (Span const &t : savedTop)
	{
		std::vector<Span> some;

		if (!NameIs(saved, t.at, "REBSHAPE") || !Children(saved, t.at + kHeadingBytes, t.at + t.size, &some))
		{
			continue;
		}

		size_t const head = FirstNamed(saved, some, "SHPHEAD1");

		if (head != some.size() && some[head].size >= kHeadingBytes + kHeadPayload && IntAt(saved, some[head].at + kHeadingBytes + 20) == chosen.number)
		{
			std::vector<char> const chunk(saved.begin() + (std::ptrdiff_t)t.at, saved.begin() + (std::ptrdiff_t)(t.at + t.size));
			std::string name;

			if (!host::ShapeUpdate_FileName(chunk, &name) || name != kTestFile)
			{
				return Stop("the copy's shape " + std::to_string(chosen.number) + " doesn't name " + kTestFile + ".rif: it isn't the made level's");
			}

			parts = some;
			found++;
		}
	}

	if (found != 1)
	{
		return Stop("the copy holds " + std::to_string(found) + " shapes numbered " + std::to_string(chosen.number) + ", not one");
	}

	bool good = true;
	size_t const head = FirstNamed(saved, parts, "SHPHEAD1");
	size_t const raw = FirstNamed(saved, parts, "SHPRAWVT");
	size_t const hp = parts[head].at + kHeadingBytes;

	if (variant == "many" || variant == "unchanged")
	{
		// the update must have been refused (many), or the file must never have been written (unchanged: the data-folder original of a level opened by its name): the shape is the
		// stock shape, point for point
		bool asStock = raw != parts.size() && parts[raw].size - kHeadingBytes == chosen.points.size() * 4;

		for (size_t i = 0; asStock && i < chosen.points.size(); i++)
		{
			asStock = IntAt(saved, parts[raw].at + kHeadingBytes + i * 4) == chosen.points[i];
		}

		if (!asStock)
		{
			return Stop("the copy's shape " + std::to_string(chosen.number) + " is not the stock shape: the update was not refused though the level would pass what the game's loader takes");
		}

		if (variant == "many")
		{
			printf("ok the copy's shape %d is the stock shape: %d shapes naming one big file were refused, not made at once\n", chosen.number, (int)kManyCopies + 1);
		}
		else
		{
			printf("ok the file's shape %d is the stock shape: it was never written\n", chosen.number);
		}

		return 0;
	}

	// THE SHPEXTFL (FUN_00512fc0 0x513c8e-0x514850; reassess/op138-fidelity-traces-vasquez.json part A): every RIFFNAME of it goes (the file this tool writes has none to make one from);
	// with the bitmaps variant (an environment whose BMPNAMES has names, and the shape's ONE BMPLSTST rewritten in its place) a new BMNAMEXT and a new BMNAMVER of 0 follow, when it had
	// none. The made level's SHPEXTFL is the stock one with, for bitmaps, its lists replaced by one at the end (Renamed).
	{
		auto const childNames = [](Bytes const &b, std::vector<Span> const &shapeParts, std::vector<std::string> *names, std::vector<Span> *spans)
		{
			size_t const ext = FirstNamed(b, shapeParts, "SHPEXTFL");

			if (ext == shapeParts.size() || !Children(b, shapeParts[ext].at + kHeadingBytes, shapeParts[ext].at + shapeParts[ext].size, spans))
			{
				return false;
			}

			for (Span const &one : *spans)
			{
				names->push_back(std::string(b.data() + one.at, 8));
			}

			return true;
		};
		std::vector<Span> stockParts, stockExt, copyExt;
		std::vector<std::string> stockNames, copyNames, wanted;

		if (!Children(image, top[chosen.top].at + kHeadingBytes, top[chosen.top].at + top[chosen.top].size, &stockParts) || !childNames(image, stockParts, &stockNames, &stockExt) ||
			!childNames(saved, parts, &copyNames, &copyExt))
		{
			return Stop("the stock or the copy's shape has no SHPEXTFL that can be walked");
		}

		bool hadExtData = false, hadVersion = false;

		for (std::string const &n : stockNames)
		{
			if (variant == "bitmaps" && n == "BMPLSTST")
			{
				continue;
			}

			hadExtData = hadExtData || n == "BMNAMEXT";
			hadVersion = hadVersion || n == "BMNAMVER";

			// the bitmaps variant's file names itself (a RIFFNAME in its environment): the shape's go, a new one comes last. The others' files have none: the shape's stay (the named fix:
			// deleting them would leave the shape, which has no EXTOBJNM, nameless)
			if (n != "RIFFNAME" || variant != "bitmaps")
			{
				wanted.push_back(n);
			}
		}

		if (variant == "bitmaps")
		{
			wanted.push_back("BMPLSTST");

			if (!hadExtData)
			{
				wanted.push_back("BMNAMEXT");
			}

			if (!hadVersion)
			{
				wanted.push_back("BMNAMVER");
			}

			wanted.push_back("RIFFNAME");
		}

		auto const joined = [](std::vector<std::string> const &v) { std::string all; for (std::string const &n : v) { all += (all.empty() ? "" : " ") + n; } return all; };

		printf("ok (to read) the stock shape's SHPEXTFL holds: %s\n", joined(stockNames).c_str());

		if (copyNames != wanted)
		{
			good = Fail("the copy's SHPEXTFL holds " + joined(copyNames) + ", not " + joined(wanted) + " as inteng leaves it");
		}
		else
		{
			printf("ok the copy's SHPEXTFL holds %s, as inteng leaves it%s\n", joined(copyNames).c_str(),
				variant == "bitmaps" ? " (the list rewritten in its place, the version set, the shape's RIFFNAME gone and the file's made last)" : " (its RIFFNAME kept: the file gives none)");

			for (size_t i = 0; i < copyNames.size(); i++)
			{
				if (variant == "bitmaps" && copyNames[i] == "RIFFNAME" && std::string(saved.data() + copyExt[i].at + kHeadingBytes) != kTestFile)
				{
					good = Fail("the new RIFFNAME is not the file's own name");
				}

				if (variant == "bitmaps" && copyNames[i] == "BMNAMVER" && IntAt(saved, copyExt[i].at + kHeadingBytes) != 0)
				{
					good = Fail("the BMNAMVER holds " + std::to_string(IntAt(saved, copyExt[i].at + kHeadingBytes)) + ", not the file's version 0 (set_version_num)");
				}

				if (variant == "bitmaps" && copyNames[i] == "BMNAMEXT" && (IntAt(saved, copyExt[i].at + kHeadingBytes) & 1) != 0)
				{
					good = Fail("the BMNAMEXT's fixed-palette bit is set: the level has no fixed palette");
				}
			}
		}
	}

	if (variant == "counts")
	{
		size_t const polys = FirstNamed(saved, parts, "SHPPOLYS");
		int32_t const wantPoints = raw == parts.size() ? 0 : (int32_t)((parts[raw].size - kHeadingBytes) / 12);
		int32_t const wantPolygons = polys == parts.size() ? 0 : (int32_t)((parts[polys].size - kHeadingBytes) / 36);

		if (IntAt(saved, hp + 24) != wantPoints || IntAt(saved, hp + 28) != wantPolygons)
		{
			good = Fail("the copy's shape header says " + std::to_string(IntAt(saved, hp + 24)) + " points and " + std::to_string(IntAt(saved, hp + 28)) + " polygons, its chunks hold " +
				std::to_string(wantPoints) + " and " + std::to_string(wantPolygons) + ": the hostile file's counts went into the level");
		}
		else
		{
			printf("ok the copy's shape header holds its own chunks' counts (%d points, %d polygons), not the file's hostile ones\n", (int)wantPoints, (int)wantPolygons);
		}
	}

	if (raw == parts.size() || parts[raw].size - kHeadingBytes != chosen.points.size() * 4 || IntAt(saved, hp + 24) != (int32_t)(chosen.points.size() / 3))
	{
		good = Fail("the copy's shape has " + std::to_string(IntAt(saved, hp + 24)) + " points, not the stock shape's " + std::to_string(chosen.points.size() / 3));
	}
	else
	{
		size_t doubled = 0, asStock = 0;

		for (size_t i = 0; i < chosen.points.size(); i++)
		{
			int32_t const v = IntAt(saved, parts[raw].at + kHeadingBytes + i * 4);

			doubled += v == 2 * chosen.points[i];
			asStock += v == chosen.points[i];
		}

		if (doubled != chosen.points.size())
		{
			good = Fail("of the copy's " + std::to_string(chosen.points.size()) + " point places, " + std::to_string(doubled) + " are twice the stock shape's and " + std::to_string(asStock) +
				" as the stock shape's: the open didn't bring the outside shape up to date from " + kTestFile + ".rif");
		}
		else
		{
			printf("ok the copy's shape %d: all its %d points are twice as far out, as %s.rif holds them\n", chosen.number,
				(int)(chosen.points.size() / 3), kTestFile);
		}
	}

	for (int k = 0; k < 6; k++)
	{
		int32_t const v = IntAt(saved, hp + 36 + (size_t)k * 4);

		if (v != 2 * chosen.box[k])
		{
			good = Fail("the copy's shape box value " + std::to_string(k) + " is " + std::to_string(v) + ", not twice the stock shape's " + std::to_string(chosen.box[k]));
		}
	}

	if (good)
	{
		printf("ok the copy's shape %d: its header's box is twice the stock shape's\n", chosen.number);
	}

	return good ? 0 : 1;
}

int main(int argc, char **argv)
{
	if ((argc == 5 || argc == 6) && strcmp(argv[1], "make") == 0)
	{
		variant = argc == 6 ? argv[5] : "";

		if (!variant.empty() && variant != "counts" && variant != "many" && variant != "bitmaps")
		{
			return Stop("no variant " + variant);
		}

		return Make(argv[2], argv[3], argv[4]);
	}

	if (argc == 5 && strcmp(argv[1], "strip-centre") == 0)
	{
		return StripCentre(argv[2], argv[3], argv[4]);
	}

	if ((argc == 4 || argc == 5) && strcmp(argv[1], "check") == 0)
	{
		variant = argc == 5 ? argv[4] : "";

		if (!variant.empty() && variant != "counts" && variant != "many" && variant != "bitmaps" && variant != "unchanged")
		{
			return Stop("no variant " + variant);
		}

		return Check(argv[2], argv[3]);
	}

	printf("verify_outside_shape: make <stock level> <new level, ending .made.rif> <shape folder> [counts|many|bitmaps] | check <stock level> <saved copy> [counts|many|bitmaps]\n");
	return 2;
}
