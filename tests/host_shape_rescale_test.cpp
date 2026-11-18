// The test for Rescale (host::ShapeRescale_ByValue, src/host/shape_rescale_edit.cpp, model::RescaleShape) and the module flag the link write sets and clears
// (host::SetModuleObjectFlag, src/host/object_base.cpp). Each is run on a REAL level (the first program argument), the level is saved as a COPY (never the
// opened file) and loaded again with the game's own loader, and the numbers are checked. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_rescale_test.exe <a level .rif> <a folder it may write its copies into (made if it isn't there)>. With no arguments it says so and passes.
//
// RESCALE (references made here from the old numbers with the same truncating product, not read from the code under test):
//  1. on an object whose shape has a SHPEXTFN, one user and a SHPCENTR, a typed size that makes the factor 2 (or 0.5 when 2 would be too big) says Done;
//  2. in the saved copy every point is the old coordinate x the factor, truncated toward zero; the six extents likewise; the radius is the old radius x the factor;
//  3. the first SHPEXTFN's rescale is the old one x the factor; SHPCENTR holds (max + min) / 2 of the new extents (truncated toward zero) and the longest distance
//     of a point from it; polygons, corners and every count are as they were; the game's loader takes the copy; the opened file is byte for byte what it was;
//  4. refusals, each named, each leaving the level's bytes as they were: a typed 0, a negative size, a size above 1,000,000, "not a number", an object that does not
//     exist, a shape with other users (when the level has one), an object whose shape has no SHPEXTFN (when the level has one).
// MODULE FLAG (0x200, OBJECT_FLAG_MODULE_OBJECT):
//  5. on a module whose header has it, "off" takes just that bit out (every other bit as it was), "on" puts it back; the loader takes the copy; a placed object is refused.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <limits>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "shape_import.h"   // shape_chunks (V-30a)
#include "shape_rescale_edit.h"
#include "object_base.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

// The first SHPEXTFN inside the first SHPEXTFL of an object's shape, or null
static Chunk *ExternalNameOf(Object_Chunk *object)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Chunk *const file = shape ? shape->lookup_single_child("SHPEXTFL") : nullptr;
	Chunk_With_Children *const holder = file && !file->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(file) : nullptr;

	if (!holder)
	{
		return nullptr;
	}

	Chunk *const name = holder->lookup_single_child("SHPEXTFN");

	return name && !name->r_u_miscellaneous() ? name : nullptr;
}

static Chunk *CentreOf(Object_Chunk *object)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Chunk *const centre = shape ? shape->lookup_single_child("SHPCENTR") : nullptr;

	return centre && !centre->r_u_miscellaneous() ? centre : nullptr;
}

static double RescaleOf(OpenLevelFile const &open, Object_Chunk *object)
{
	Chunk *const name = ExternalNameOf(object);
	std::vector<unsigned char> bytes;
	double rescale = 0.0;

	if (name && open.PayloadOf(name, &bytes) && bytes.size() >= 12)
	{
		memcpy(&rescale, bytes.data(), 8);
	}

	return rescale;
}

static Object_Chunk *FindObject(OpenLevelFile const &open, uint8_t const *id)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
		{
			return each();
		}
	}

	return nullptr;
}

static bool SameBytes(std::vector<char> const &a, std::vector<char> const &b)
{
	return a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0;
}

static void Rescale(std::string const &levelPath, std::string const &copyFolder, std::vector<char> const &original)
{
	printf("RESCALE\n");

	OpenLevelFile before;

	if (before.Open(levelPath.c_str()) != LevelOpenResult::Opened || !before.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	List<Object_Chunk *> all;

	before.Level()->list_objects(&all);

	uint8_t chosen[8] = { 0 };
	uint8_t shared[8] = { 0 };
	uint8_t noExternal[8] = { 0 };
	bool haveChosen = false, haveShared = false, haveNoExternal = false;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape())
		{
			continue;
		}

		model::ShapeModel probe;
		size_t users = 0;
		std::string why;

		if (host::ShapeEdit_Read(before.Level(), &before, (uint8_t const *)&object->object_data.ID, &probe, &users, &why) != host::ShapeEditResult::Done
			|| probe.points.size() < 4 || probe.polygons.size() < 1)
		{
			continue;
		}

		bool const external = ExternalNameOf(object) != nullptr && RescaleOf(before, object) > 0.0;

		if (!external && !haveNoExternal)
		{
			memcpy(noExternal, &object->object_data.ID, 8);
			haveNoExternal = true;
		}

		if (external && users > 1 && !haveShared)
		{
			memcpy(shared, &object->object_data.ID, 8);
			haveShared = true;
		}

		if (external && users == 1 && CentreOf(object) && !haveChosen)
		{
			memcpy(chosen, &object->object_data.ID, 8);
			haveChosen = true;
		}
	}

	Check(haveChosen, "the level has an object with a one-user shape that has a SHPEXTFN and a SHPCENTR");

	if (!haveChosen)
	{
		return;
	}

	Object_Chunk *const object = FindObject(before, chosen);
	model::ShapeModel shapeBefore;
	std::string why;

	Check(host::ShapeEdit_Read(before.Level(), &before, chosen, &shapeBefore, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read before");

	double const rescaleBefore = RescaleOf(before, object);
	double const denominator = ((100.0 * 1.0) * host::kRescaleHundredth) * rescaleBefore;

	// the factor: 2, or 0.5 when doubling would put a number out of the 32-bit range or the typed size above the most
	int64_t largest = 0;

	for (model::ShapePoint const &p : shapeBefore.points)
	{
		int64_t const biggest = llabs(p.x) > llabs(p.y) ? (llabs(p.x) > llabs(p.z) ? llabs(p.x) : llabs(p.z)) : (llabs(p.y) > llabs(p.z) ? llabs(p.y) : llabs(p.z));

		largest = biggest > largest ? biggest : largest;
	}

	double factorWanted = (largest < (1LL << 29) && denominator * 2.0 <= host::kRescaleMost) ? 2.0 : 0.5;
	double const typed = denominator * factorWanted;

	Check(typed > 0.0 && typed <= host::kRescaleMost, "the size to type is in range: " + std::to_string(typed) + " (the SHPEXTFN's rescale is " + std::to_string(rescaleBefore) + ")");

	Chunk *const centreChunk = CentreOf(object);
	std::vector<unsigned char> centreBefore;

	Check(before.PayloadOf(centreChunk, &centreBefore) && centreBefore.size() == 16, "SHPCENTR holds 16 bytes (3 ints and a float)");

	std::vector<char> imageBefore;

	Check(before.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	// 4. the refusals first: each says its reason and changes nothing
	{
		struct Bad { double typed; char const *words; };
		Bad const bad[] = { { 0.0, "a typed 0" }, { -5.0, "a negative size" }, { 1000001.0, "a size above 1,000,000" }, { NAN, "not a number" }, { INFINITY, "infinity" } };

		for (Bad const &one : bad)
		{
			double factor = 7.0;
			host::ShapeRescaleResult const refused = host::ShapeRescale_ByValue(&before, before.Level(), chosen, one.typed, 1.0, &factor, &why);
			std::vector<char> now;

			Check(refused == host::ShapeRescaleResult::BadValue && factor == 0.0 && before.BuildLevel(now) && SameBytes(now, imageBefore), std::string(one.words) + " is refused as BadValue and the level is as it was");
		}

		uint8_t none[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		std::vector<char> now;

		Check(host::ShapeRescale_ByValue(&before, before.Level(), none, typed, 1.0, nullptr, &why) == host::ShapeRescaleResult::NoObject && before.BuildLevel(now) && SameBytes(now, imageBefore),
			"an object that does not exist is refused as NoObject");

		if (haveShared)
		{
			Check(host::ShapeRescale_ByValue(&before, before.Level(), shared, 100.0, 1.0, nullptr, &why) == host::ShapeRescaleResult::SharedShape && before.BuildLevel(now) && SameBytes(now, imageBefore),
				"a shape with other users is refused as SharedShape and the level is as it was");
		}
		else
		{
			printf("  --  no shared shape with a SHPEXTFN in this level: the SharedShape refusal is not exercised\n");
		}

		if (haveNoExternal)
		{
			Check(host::ShapeRescale_ByValue(&before, before.Level(), noExternal, 100.0, 1.0, nullptr, &why) == host::ShapeRescaleResult::NoExternal && before.BuildLevel(now) && SameBytes(now, imageBefore),
				"an object whose shape has no SHPEXTFN is refused as NoExternal and the level is as it was");
		}
		else
		{
			printf("  --  every shape of this level has a SHPEXTFN: the NoExternal refusal is not exercised\n");
		}
	}

	// 1. the rescale itself
	double factor = 0.0;
	host::ShapeRescaleResult const result = host::ShapeRescale_ByValue(&before, before.Level(), chosen, typed, 1.0, &factor, &why);

	Check(result == host::ShapeRescaleResult::Done, std::string("Rescale is done") + (result == host::ShapeRescaleResult::Done ? "" : std::string(": ") + host::ShapeRescaleWords(result) + " " + why));

	if (result != host::ShapeRescaleResult::Done)
	{
		return;
	}

	Check(fabs(factor - factorWanted) <= factorWanted * 1e-12, "the factor used is the one wanted: " + std::to_string(factor));

	std::string const copyPath = CopyPathIn(copyFolder, "rescale-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	std::vector<char> originalAfter;

	Check(ReadWholeFile(levelPath, &originalAfter) && originalAfter == original, "the opened file is byte for byte what it was");

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (!after.Level())
	{
		return;
	}

	model::ShapeModel shapeAfter;
	Object_Chunk *const objectAfter = FindObject(after, chosen);

	Check(objectAfter && host::ShapeEdit_Read(after.Level(), &after, chosen, &shapeAfter, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read after the reload");

	if (!objectAfter)
	{
		return;
	}

	// 2. the numbers, from the old ones with the truncating product made here
	auto expected = [factor](int32_t old) -> int32_t { return (int32_t)((double)old * factor); };
	bool pointsRight = shapeAfter.points.size() == shapeBefore.points.size();

	for (size_t at = 0; pointsRight && at < shapeBefore.points.size(); at++)
	{
		pointsRight = shapeAfter.points[at].x == expected(shapeBefore.points[at].x) && shapeAfter.points[at].y == expected(shapeBefore.points[at].y)
			&& shapeAfter.points[at].z == expected(shapeBefore.points[at].z);
	}

	Check(pointsRight, "every point is the old coordinate x the factor, truncated toward zero: " + std::to_string(shapeAfter.points.size()) + " points");

	Check(shapeAfter.extents.maxX == expected(shapeBefore.extents.maxX) && shapeAfter.extents.minX == expected(shapeBefore.extents.minX)
		&& shapeAfter.extents.maxY == expected(shapeBefore.extents.maxY) && shapeAfter.extents.minY == expected(shapeBefore.extents.minY)
		&& shapeAfter.extents.maxZ == expected(shapeBefore.extents.maxZ) && shapeAfter.extents.minZ == expected(shapeBefore.extents.minZ), "the six extents are the old ones x the factor, truncated");

	Check(fabs((double)shapeAfter.radius - (double)shapeBefore.radius * factor) <= fabs((double)shapeBefore.radius * factor) * 1e-6 + 1e-3,
		"the radius is the old radius x the factor: " + std::to_string(shapeAfter.radius));

	// 3. the SHPEXTFN's rescale, SHPCENTR, and what is unchanged
	double const rescaleAfter = RescaleOf(after, objectAfter);

	Check(fabs(rescaleAfter - rescaleBefore * factor) <= fabs(rescaleBefore * factor) * 1e-12, "the first SHPEXTFN's rescale is the old one x the factor: " + std::to_string(rescaleAfter) + " (was " + std::to_string(rescaleBefore) + ")");

	std::vector<unsigned char> centreAfter;
	int32_t mid[3];
	float aboutCentre = 0.0f;

	Check(after.PayloadOf(CentreOf(objectAfter), &centreAfter) && centreAfter.size() == 16, "SHPCENTR is still 16 bytes");

	if (centreAfter.size() == 16)
	{
		memcpy(mid, centreAfter.data(), 12);
		memcpy(&aboutCentre, centreAfter.data() + 12, 4);

		Check(mid[0] == (int32_t)(((int64_t)shapeAfter.extents.maxX + shapeAfter.extents.minX) / 2) && mid[1] == (int32_t)(((int64_t)shapeAfter.extents.maxY + shapeAfter.extents.minY) / 2)
			&& mid[2] == (int32_t)(((int64_t)shapeAfter.extents.maxZ + shapeAfter.extents.minZ) / 2), "SHPCENTR's centre is (max + min) / 2 of the new extents, truncated toward zero");

		double farthest = 0.0;

		for (model::ShapePoint const &p : shapeAfter.points)
		{
			double const dx = (double)p.x - mid[0], dy = (double)p.y - mid[1], dz = (double)p.z - mid[2];
			double const away = sqrt(dx * dx + dy * dy + dz * dz);

			farthest = away > farthest ? away : farthest;
		}

		// FUN_0050f570 makes the radius about the centre (the longest distance of a point from the centre), and Rescale puts it in SHPCENTR. But the copy is written by the game's own chunk library, and its writer
		// (Shape_Centre_Chunk::fill_data_block, chunk_shape.cpp) writes the shape's HEADER radius (the radius about the origin: cs->radius) in that place, not the radius about the centre it read: so a saved copy
		// holds one of the two, the header's when the centre is not the origin. Found when the runner began giving this test its level (it had passed by testing nothing): Invasion, Sulaco and Temple.
		bool const aboutTheCentre = fabs((double)aboutCentre - farthest) <= farthest * 1e-4 + 1.0;
		bool const theHeaders = fabs((double)aboutCentre - (double)shapeAfter.radius) <= fabs((double)shapeAfter.radius) * 1e-4 + 1.0;

		Check(aboutTheCentre || theHeaders, "SHPCENTR's radius in the copy is the longest distance of a point from the centre (" + std::to_string(farthest) + ") or, as the game's own writer puts it, the header's radius (" + std::to_string(shapeAfter.radius) + "): " + std::to_string(aboutCentre));
		printf("  (the copy's SHPCENTR radius is %s)\n", aboutTheCentre ? "the radius about the centre" : "the header's radius: the game's writer, Shape_Centre_Chunk::fill_data_block, writes cs->radius there");
	}

	bool same = shapeAfter.polygons.size() == shapeBefore.polygons.size() && shapeAfter.polygonNormals.size() == shapeBefore.polygonNormals.size()
		&& shapeAfter.pointNormals.size() == shapeBefore.pointNormals.size();

	for (size_t at = 0; same && at < shapeBefore.polygons.size(); at++)
	{
		same = memcmp(&shapeAfter.polygons[at], &shapeBefore.polygons[at], sizeof(model::ShapePolygon)) == 0;
	}

	Check(same, "the polygons, their corners and every count are as they were: " + std::to_string(shapeAfter.polygons.size()) + " polygons");

	std::vector<char> imageAfter;

	Check(after.BuildLevel(imageAfter) && imageAfter.size() == imageBefore.size(), "the level is the same size as before: " + std::to_string(imageBefore.size()) + " bytes");

	DeleteFileA(copyPath.c_str());
}

// The header word of an object's OBJHEAD1 (its flags are the first 4 bytes of the payload), through the edit store
static bool HeaderFlags(OpenLevelFile const &open, Object_Chunk *object, uint32_t *flags)
{
	Chunk *const header = object ? object->get_header() : nullptr;
	std::vector<unsigned char> payload;

	if (!header || !open.PayloadOf(header, &payload) || payload.size() < 4)
	{
		return false;
	}

	memcpy(flags, payload.data(), 4);
	return true;
}

static void ModuleFlag(std::string const &levelPath, std::string const &copyFolder)
{
	printf("MODULE FLAG 0x200\n");

	OpenLevelFile before;

	if (before.Open(levelPath.c_str()) != LevelOpenResult::Opened || !before.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	List<Object_Chunk *> all;

	before.Level()->list_objects(&all);

	uint8_t module[8] = { 0 };
	uint8_t placed[8] = { 0 };
	uint32_t flagsBefore = 0;
	bool haveModule = false, havePlaced = false;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		uint32_t flags = 0;

		if (!each() || !HeaderFlags(before, each(), &flags))
		{
			continue;
		}

		if (!haveModule && (flags & 0x200) && !(flags & 0x400))
		{
			memcpy(module, &each()->object_data.ID, 8);
			flagsBefore = flags;
			haveModule = true;
		}

		if (!havePlaced && (flags & 0x400))
		{
			memcpy(placed, &each()->object_data.ID, 8);
			havePlaced = true;
		}
	}

	Check(haveModule, "the level has a module whose header holds the module flag 0x200");

	if (!haveModule)
	{
		return;
	}

	Check(host::SetModuleObjectFlag(&before, module, false) == host::BaseObjectResult::Done, "taking the flag out says Done");

	uint32_t flagsOff = 0;

	Check(HeaderFlags(before, FindObject(before, module), &flagsOff) && flagsOff == (flagsBefore & ~0x200u), "the header word is what it was with just bit 0x200 gone");
	Check(host::SetModuleObjectFlag(&before, module, false) == host::BaseObjectResult::Done, "taking it out again changes nothing and says Done");

	std::string const copyPath = CopyPathIn(copyFolder, "modflag-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile after;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy with the flag out");

	if (after.Level())
	{
		uint32_t flagsAfter = 0;

		Check(HeaderFlags(after, FindObject(after, module), &flagsAfter) && flagsAfter == flagsOff, "the copy holds the header word with the flag out");
	}

	Check(host::SetModuleObjectFlag(&before, module, true) == host::BaseObjectResult::Done, "putting the flag in says Done");

	uint32_t flagsOn = 0;

	Check(HeaderFlags(before, FindObject(before, module), &flagsOn) && flagsOn == flagsBefore, "the header word is what it was at the start");

	if (havePlaced)
	{
		Check(host::SetModuleObjectFlag(&before, placed, true) == host::BaseObjectResult::IsPlaced, "a placed object is refused as IsPlaced");
	}

	uint8_t none[8] = { 0 };

	Check(host::SetModuleObjectFlag(&before, none, true) == host::BaseObjectResult::NoName, "an all-0 ID is refused as NoName");

	DeleteFileA(copyPath.c_str());
}

// THE '=' AND '-' KEYS (OP-14, FUN_004d9af0 0x4dc2c3-0x4dc308: the current placed shape's +0x14 with 10/7, then with 0.7; host::ShapeRescale_ByFactor): the points, extents and radius
// times each factor in turn (each integer cut toward zero), the first SHPEXTFN's rescale double times both, saved as a copy and read back by the game's loader; a factor that is not
// a number above 0, a shared shape and a shape with no SHPEXTFN are refused and change nothing. Added 2026-11-17 by Drake (red: ShapeRescale_ByFactor did not exist).
static void KeyScale(std::string const &levelPath, std::string const &copyFolder)
{
	printf("THE '=' AND '-' KEYS\n");

	OpenLevelFile before;

	if (before.Open(levelPath.c_str()) != LevelOpenResult::Opened || !before.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	List<Object_Chunk *> all;

	before.Level()->list_objects(&all);

	uint8_t chosen[8] = { 0 };
	uint8_t shared[8] = { 0 };
	uint8_t noExternal[8] = { 0 };
	bool haveChosen = false, haveShared = false, haveNoExternal = false;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		model::ShapeModel probe;
		size_t users = 0;
		std::string why;

		if (!object || !object->get_assoc_shape() || host::ShapeEdit_Read(before.Level(), &before, (uint8_t const *)&object->object_data.ID, &probe, &users, &why) != host::ShapeEditResult::Done
			|| probe.points.size() < 4 || probe.polygons.size() < 1)
		{
			continue;
		}

		bool const external = ExternalNameOf(object) != nullptr && RescaleOf(before, object) > 0.0;

		if (!external && !haveNoExternal)
		{
			memcpy(noExternal, &object->object_data.ID, 8);
			haveNoExternal = true;
		}

		if (external && users > 1 && !haveShared)
		{
			memcpy(shared, &object->object_data.ID, 8);
			haveShared = true;
		}

		if (external && users == 1 && !haveChosen)
		{
			memcpy(chosen, &object->object_data.ID, 8);
			haveChosen = true;
		}
	}

	Check(haveChosen, "the level has an object with a one-user shape that has a SHPEXTFN");

	if (!haveChosen)
	{
		return;
	}

	{
		Object_Chunk *const named = FindObject(before, chosen);

		printf("  --  the object scaled: %s\n", named && named->object_data.o_name ? named->object_data.o_name : "(no name)");
	}

	Check(host::kScaleKeyUp == 10.0 / 7.0 && host::kScaleKeyDown == 0.7, "the two factors are inteng's doubles (10/7 for '=', 0.7 for '-')");

	model::ShapeModel shapeBefore;
	std::string why;

	Check(host::ShapeEdit_Read(before.Level(), &before, chosen, &shapeBefore, nullptr, &why) == host::ShapeEditResult::Done, "the shape is read before");

	double const rescaleBefore = RescaleOf(before, FindObject(before, chosen));
	std::vector<char> imageBefore;

	Check(before.BuildLevel(imageBefore), "the level as the next save would write it is made (before)");

	// 0. refusals: nothing changes
	{
		std::vector<char> now;
		double const bad[] = { 0.0, -0.7, NAN, INFINITY };

		for (double const one : bad)
		{
			Check(host::ShapeRescale_ByFactor(&before, before.Level(), chosen, one, &why) == host::ShapeRescaleResult::BadValue && before.BuildLevel(now) && SameBytes(now, imageBefore),
				"the factor " + std::to_string(one) + " is refused as BadValue and the level is as it was");
		}

		if (haveShared)
		{
			Check(host::ShapeRescale_ByFactor(&before, before.Level(), shared, host::kScaleKeyUp, &why) == host::ShapeRescaleResult::SharedShape && before.BuildLevel(now) && SameBytes(now, imageBefore),
				"an object whose shape other objects use too is refused as SharedShape and the level is as it was");
		}

		if (haveNoExternal)
		{
			Check(host::ShapeRescale_ByFactor(&before, before.Level(), noExternal, host::kScaleKeyUp, &why) == host::ShapeRescaleResult::NoExternal && before.BuildLevel(now) && SameBytes(now, imageBefore),
				"an object whose shape has no SHPEXTFN is refused as NoExternal and the level is as it was");
		}
	}

	// 1. '=' then '-'
	host::ShapeRescaleResult const up = host::ShapeRescale_ByFactor(&before, before.Level(), chosen, host::kScaleKeyUp, &why);
	host::ShapeRescaleResult const down = host::ShapeRescale_ByFactor(&before, before.Level(), chosen, host::kScaleKeyDown, &why);

	Check(up == host::ShapeRescaleResult::Done && down == host::ShapeRescaleResult::Done, std::string("'=' and then '-' are done") + (up == host::ShapeRescaleResult::Done && down == host::ShapeRescaleResult::Done ? "" : ": " + why));

	if (up != host::ShapeRescaleResult::Done || down != host::ShapeRescaleResult::Done)
	{
		return;
	}

	std::string const copyPath = CopyPathIn(copyFolder, "scale-keys-test-copy.rif");

	CreateDirectoryA(copyFolder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());
	Check(before.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY: " + copyPath);

	OpenLevelFile after;
	model::ShapeModel shapeAfter;

	Check(after.Open(copyPath.c_str()) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the copy");

	if (!after.Level() || host::ShapeEdit_Read(after.Level(), &after, chosen, &shapeAfter, nullptr, &why) != host::ShapeEditResult::Done)
	{
		Check(false, "the shape is read after the reload");
		return;
	}

	auto const twice = [](int32_t v) { return (int32_t)((double)(int32_t)((double)v * host::kScaleKeyUp) * host::kScaleKeyDown); };
	bool pointsRight = shapeAfter.points.size() == shapeBefore.points.size();

	for (size_t i = 0; pointsRight && i < shapeBefore.points.size(); i++)
	{
		pointsRight = shapeAfter.points[i].x == twice(shapeBefore.points[i].x) && shapeAfter.points[i].y == twice(shapeBefore.points[i].y) && shapeAfter.points[i].z == twice(shapeBefore.points[i].z);
	}

	Check(pointsRight, "every point is the old coordinate x 10/7, cut toward zero, then x 0.7, cut toward zero: " + std::to_string(shapeAfter.points.size()) + " points");
	Check(shapeAfter.extents.maxX == twice(shapeBefore.extents.maxX) && shapeAfter.extents.minZ == twice(shapeBefore.extents.minZ), "the extents the same way");

	double const rescaleAfter = RescaleOf(after, FindObject(after, chosen));
	double const wanted = rescaleBefore * host::kScaleKeyUp * host::kScaleKeyDown;

	Check(fabs(rescaleAfter - wanted) <= fabs(wanted) * 1e-12, "the first SHPEXTFN's rescale is the old one x 10/7 x 0.7: " + std::to_string(rescaleAfter) + " (was " + std::to_string(rescaleBefore) + ")");
}

// V-30a (object-placement.md; Vasquez 11-17): inteng's rebuild after a scale (FUN_0050f570, 0x50fce0-0x50fd05) MAKES a SHPCENTR when the shape has none. No stock outside shape lacks one,
// so the test takes the chunk out of a one-user outside shape first (the level put together again and handed to the game's loader), then scales it by 2: the shape holds a SHPCENTR
// again, its centre (max + min) / 2 of the new extents.
static void CentreMade(std::string const &levelPath)
{
	printf("V-30a: A SHPCENTR MADE WHEN THE SHAPE HAS NONE\n");

	using namespace host::shape_chunks;

	OpenLevelFile level;

	if (level.Open(levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "V-30a: the game's loader takes the level");
		return;
	}

	List<Object_Chunk *> all;
	uint8_t chosen[8] = { 0 };
	int32_t number = -1;
	bool have = false;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done() && !have; each.next())
	{
		Object_Chunk *const object = each();
		model::ShapeModel probe;
		size_t users = 0;
		std::string why;

		if (object && object->get_assoc_shape() && ExternalNameOf(object) && RescaleOf(level, object) > 0.0 && CentreOf(object) &&
			host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &probe, &users, &why) == host::ShapeEditResult::Done && users == 1 &&
			probe.points.size() >= 4)
		{
			memcpy(chosen, &object->object_data.ID, 8);
			number = probe.number;
			have = true;
		}
	}

	Check(have, "V-30a: the level has a one-user outside shape to work on");

	if (!have)
	{
		return;
	}

	// the level again with that shape's SHPCENTR left out
	Bytes image, out;
	size_t total = 0;
	std::vector<Span> top;
	bool cut = false;

	if (level.BuildLevel(image) && image.size() >= kHeadingBytes && ChunkSizeAt(image, 0, image.size(), &total) && Children(image, kHeadingBytes, total, &top))
	{
		out.assign(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);

		for (Span const &t : top)
		{
			std::vector<Span> inside;
			bool ours = false;

			if (NameIs(image, t.at, "REBSHAPE") && Children(image, t.at + kHeadingBytes, t.at + t.size, &inside))
			{
				for (Span const &c : inside)
				{
					ours = ours || (NameIs(image, c.at, "SHPHEAD1") && c.size >= kHeadingBytes + 24 && IntAt(image, c.at + kHeadingBytes + 20) == number);
				}
			}

			if (!ours)
			{
				out.insert(out.end(), image.begin() + (std::ptrdiff_t)t.at, image.begin() + (std::ptrdiff_t)(t.at + t.size));
				continue;
			}

			size_t const start = out.size();

			out.insert(out.end(), image.begin() + (std::ptrdiff_t)t.at, image.begin() + (std::ptrdiff_t)(t.at + kHeadingBytes));

			for (Span const &c : inside)
			{
				if (NameIs(image, c.at, "SHPCENTR"))
				{
					cut = true;
					continue;
				}

				out.insert(out.end(), image.begin() + (std::ptrdiff_t)c.at, image.begin() + (std::ptrdiff_t)(c.at + c.size));
			}

			PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
		}

		PutIntAt(&out, 8, (int32_t)out.size());
	}

	Check(cut && level.ReplaceLevel(out) == LevelOpenResult::Opened, "V-30a: the shape's SHPCENTR is taken out and the game's loader takes the level");

	Object_Chunk *object = FindObject(level, chosen);

	Check(object && !CentreOf(object), "V-30a: the shape has no SHPCENTR now");

	std::string why;
	host::ShapeRescaleResult const result = host::ShapeRescale_ByFactor(&level, level.Level(), chosen, 2.0, &why);

	Check(result == host::ShapeRescaleResult::Done, std::string("V-30a: the shape is scaled by 2: ") + host::ShapeRescaleWords(result) + (why.empty() ? "" : " (" + why + ")"));

	object = FindObject(level, chosen);

	model::ShapeModel shape;
	size_t users = 0;
	std::vector<unsigned char> centre;
	bool const read = object && host::ShapeEdit_Read(level.Level(), &level, chosen, &shape, &users, &why) == host::ShapeEditResult::Done;
	bool const made = object && CentreOf(object) && level.PayloadOf(CentreOf(object), &centre) && centre.size() == 16;
	int32_t mid[3] = { 0, 0, 0 };

	if (made)
	{
		memcpy(mid, centre.data(), 12);
	}

	Check(read && made, "V-30a: the shape holds a SHPCENTR again (16 bytes)");
	Check(read && made && mid[0] == (int32_t)(((int64_t)shape.extents.maxX + shape.extents.minX) / 2) && mid[1] == (int32_t)(((int64_t)shape.extents.maxY + shape.extents.minY) / 2) &&
		mid[2] == (int32_t)(((int64_t)shape.extents.maxZ + shape.extents.minZ) / 2), "V-30a: its centre is (max + min) / 2 of the new extents");
}

// OP-102: the "Object scale" line's number (FUN_004e3600 0x4e39d4-0x4e39f6) comes from ShapeRescale_Current: the first SHPEXTFN's rescale double AS IT IS, the one Rescale and the
// '=' / '-' keys multiply. Read on a one-user outside shape; doubled by ShapeRescale_ByFactor it reads doubled; a NaN and a negative rescale (hostile files) come back as they are,
// as inteng prints whatever the chunk holds; an object with no outside file gives false (no line), and so do missing arguments.
static void CurrentRead(std::string const &levelPath)
{
	printf("OP-102: THE OBJECT SCALE LINE'S RESCALE\n");

	OpenLevelFile level;

	if (level.Open(levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "OP-102: the game's loader takes the level");
		return;
	}

	List<Object_Chunk *> all;
	uint8_t outside[8] = { 0 };
	uint8_t plain[8] = { 0 };
	bool haveOutside = false, havePlain = false;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		model::ShapeModel probe;
		size_t users = 0;
		std::string why;

		if (!object || !object->get_assoc_shape())
		{
			continue;
		}

		if (!ExternalNameOf(object) && !havePlain)
		{
			memcpy(plain, &object->object_data.ID, 8);
			havePlain = true;
		}
		else if (ExternalNameOf(object) && !haveOutside && RescaleOf(level, object) > 0.0 &&
			host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &probe, &users, &why) == host::ShapeEditResult::Done && users == 1)
		{
			memcpy(outside, &object->object_data.ID, 8);
			haveOutside = true;
		}
	}

	Check(haveOutside && havePlain, "OP-102: the level has a one-user outside shape and a shape with no outside file");

	if (!haveOutside || !havePlain)
	{
		return;
	}

	double got = -1.0;
	double const own = RescaleOf(level, FindObject(level, outside));

	Check(host::ShapeRescale_Current(&level, level.Level(), outside, &got) && got == own, "OP-102: the outside shape's rescale is read as its SHPEXTFN holds it (" + std::to_string(own) + ")");

	double untouched = 12.5;

	Check(!host::ShapeRescale_Current(&level, level.Level(), plain, &untouched) && untouched == 12.5, "OP-102: a shape with no outside file gives no rescale (inteng draws no line)");
	Check(!host::ShapeRescale_Current(nullptr, level.Level(), outside, &got) && !host::ShapeRescale_Current(&level, nullptr, outside, &got) &&
		!host::ShapeRescale_Current(&level, level.Level(), nullptr, &got) && !host::ShapeRescale_Current(&level, level.Level(), outside, nullptr), "OP-102: missing arguments give false");

	std::string why;

	Check(host::ShapeRescale_ByFactor(&level, level.Level(), outside, 2.0, &why) == host::ShapeRescaleResult::Done && host::ShapeRescale_Current(&level, level.Level(), outside, &got) &&
		got == own * 2.0, "OP-102: after a scale by 2 (the keys' path) the line's rescale is twice what it was: it reads the live chunk");

	// hostile rescales, written into the SHPEXTFN's first 8 bytes as a file could hold them
	double const hostile[2] = { std::numeric_limits<double>::quiet_NaN(), -3.25 };

	for (double const &bad : hostile)
	{
		Chunk *const name = ExternalNameOf(FindObject(level, outside));
		std::vector<unsigned char> bytes;

		if (!name || !level.PayloadOf(name, &bytes) || bytes.size() < 8)
		{
			Check(false, "OP-102: the SHPEXTFN's bytes can be read for the hostile rescale");
			continue;
		}

		memcpy(bytes.data(), &bad, 8);
		got = 0.0;

		bool const read = level.SetPayload(name, bytes) && host::ShapeRescale_Current(&level, level.Level(), outside, &got);
		bool const same = memcmp(&got, &bad, 8) == 0;   // the very bytes: a NaN compares with nothing, and /fp:fast drops the x != x test anyway

		Check(read && same, std::string("OP-102: a hostile rescale (") + (&bad == &hostile[0] ? "NaN" : "-3.25") + ") comes back as it is, as inteng prints whatever the chunk holds");
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_shape_rescale_test: give a level .rif and a folder for its copies; with neither there is nothing to run.\n");
		printf("passed host_shape_rescale_test (0 ok)\n");
		return 0;
	}

	std::string const levelPath = argv[1];
	std::string const copyFolder = argv[2];
	std::vector<char> original;

	if (!ReadWholeFile(levelPath, &original))
	{
		printf("  FAILED  the level can be read: %s\n", levelPath.c_str());
		return 1;
	}

	Rescale(levelPath, copyFolder, original);
	KeyScale(levelPath, copyFolder);
	ModuleFlag(levelPath, copyFolder);
	CentreMade(levelPath);
	CurrentRead(levelPath);

	std::vector<char> originalEnd;

	Check(ReadWholeFile(levelPath, &originalEnd) && originalEnd == original, "at the end the opened file is still byte for byte what it was");

	if (checksFailed)
	{
		printf("FAILED host_shape_rescale_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_shape_rescale_test\n");
	return 0;
}
