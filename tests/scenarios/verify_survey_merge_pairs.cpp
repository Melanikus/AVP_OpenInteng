// verify_survey_merge_pairs.cpp: SURVEY_MERGE_PAIRS, a survey and not a check (the "verify_" in the file name is only so that the scenario tools'
// verifier build, AvP-docs\tools\po\build-verifier.sh, builds it). Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng, after a dev lost
// ~40 minutes clicking about for a shape whose triangles Merge can join. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_survey_merge_pairs.exe <a folder of level files>
// For every .rif in the folder (by name, A to Z), it prints:
//   <level> row=<row> object=<name> poly=<a> poly=<b> state=<free|paired|taken> order=<any|this>
//     one line for each pair of triangles of an object's shape that the editor's Merge would join, where the object's shape has merge data
//     (SHPMRGDT). The two words "pick-poly <row> <a>" and "pick-poly <row> <b>" in Texture Map select the pair, in that order.
//     state=free: neither triangle is paired yet, so Merge selected polygons (0x119) joins them ("1 pairs made").
//     state=paired: they are already each other's partners in the merge data, so Unmerge selected polygons (0x11a) parts them.
//     state=taken: one of them is paired with some other triangle, so Merge leaves them (unmerge that pair first).
//     order=any: the editor's test takes the two either way round; order=this: only picked in this order (the test is not the same both ways).
//   <level> counts: objects-with-SHPMRGDT=<n> merge-pairs=<n> joinable-pairs=<n> free=<n> paired=<n> taken=<n> [unreadable=<n>] [cut=<n>]
//     merge-pairs: the pairs the merge data already holds (each counted once); unreadable: objects with merge data the editor's Merge
//     would refuse (the shape not readable, or the merge data not one number for each polygon); cut: objects whose search hit its bound.
//   <level> not opened: <why>   when the editor's own open of the level refuses it.
// It gives back 0 when every level was opened, 1 otherwise, 2 for a bad call.
//
// WHY THE NUMBERS ARE THE EDITOR'S OWN. Nothing here re-makes a rule; it links the editor's code and calls it:
// - THE ROWS are LevelObjects_Read's (src/host/level_objects.cpp), on the level opened by the editor's own OpenLevelFile: the very list the
//   harness's pick-poly takes its row from (editor_main.cpp, kHarnessPickPoly: everyObject[row].id).
// - THE OBJECT a row stands for is found by its ID, the first object with that ID in the level's order, as Merge finds it (PolyEdit_Merge's
//   ObjectWithId in poly_edit.cpp). A later row with the same ID (two objects of one name) acts on that same first object, so it is not listed.
// - THE POLYGON NUMBERS are the file's, from host::ShapeEdit_Read, the numbers Merge works on; the merge data is read as PolyEdit_Merge reads it
//   (the shape's first SHPMRGDT, through OpenLevelFile::PayloadOf, one number for each polygon or Merge refuses it).
// - JOINABLE is host::PolyEdit_Joinable(shape, a, b, false), as PolyEdit_MergeInModel calls it for a picked first and b second (fake merge data
//   off, the default). The only thing done here is to choose WHICH pairs to ask it about: pairs of triangles that share at least one corner
//   number. Joinable needs exactly two shared corner numbers, so no pair it could take is left out; that choice only saves time.
//
// THE RULES (a level's bytes are hostile): the editor's own open and shape reader check every size and count; here the folder's files are
// at most kMostLevels, a level's listed pairs at most kMostLinesPerLevel, an object's calls of the test at most kMostTestsPerObject (an object
// that hits it is counted as cut), and an object's name is printed only as plain letters, digits and _ . - (any other byte as %XX), at most
// 64 bytes. Nothing is ever written to a level: the open is read only, and nothing is saved.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_objects.h"
#include "object_list.h"
#include "poly_edit.h"
#include "shape_edit.h"

// ---- Stand-ins for the parts of the game the chunk code calls (as tests/host_poly_edit_test.cpp has them): files are read by Windows' own

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("# the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("# FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

// level_objects.cpp's LevelObjects_Hold asks the game side where it loaded the level from. This survey never calls it (it hands its own open to
// LevelObjects_Use), so the answer is always "no level".
bool LevelView_FilePath(char *path, size_t size)
{
	if (path && size)
	{
		path[0] = '\0';
	}

	return false;
}

// The bounds (hostile input)
static size_t const kMostLevels = 4096;               // level files read from the folder
static size_t const kMostLinesPerLevel = 1000000;     // pair lines printed for one level
static size_t const kMostTestsPerObject = 50000000;   // calls of PolyEdit_Joinable for one object's shape
static size_t const kMostNameBytes = 64;              // an object's name, as the game keeps it (OBJECT_NAME_BYTES)

// Takes the level's open away from the lookups (LevelObjects_Use(nullptr)) on every way out of a level, before the open itself goes
struct UseGuard
{
	~UseGuard() { LevelObjects_Use(nullptr); }
};

// A name for the output: plain letters, digits and _ . - as they are, every other byte as %XX, at most `most` bytes read and never past a 0
static std::string Plain(char const *text, size_t most)
{
	std::string out;

	if (!text)
	{
		return out;
	}

	for (size_t i = 0; i < most && text[i] != '\0'; i++)
	{
		unsigned char const c = (unsigned char)text[i];

		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')
		{
			out += (char)c;
		}
		else
		{
			char hex[4];

			snprintf(hex, sizeof(hex), "%%%02X", c);
			out += hex;
		}
	}

	return out;
}

static char const *OpenWords(LevelOpenResult result)
{
	switch (result)
	{
	case LevelOpenResult::Opened: return "opened";
	case LevelOpenResult::NoPath: return "no path";
	case LevelOpenResult::CantRead: return "the file could not be read whole, or is bigger than any level the game reads";
	case LevelOpenResult::LoaderRefused: return "the game's own loader refuses it";
	case LevelOpenResult::ChangedWhileOpening: return "the file changed while it was being opened";
	case LevelOpenResult::DoesntPair: return "the loaded chunks do not pair with the file's own";
	case LevelOpenResult::TooManyChunks: return "more chunks than the editor takes";
	case LevelOpenResult::NothingOpen: return "nothing open";
	}

	return "unknown answer";
}

// The first object of the level with this ID, in the level's own order: the one PolyEdit_Merge (and ShapeEdit_Read) find for it
static Object_Chunk *FirstWithId(File_Chunk *level, uint8_t const *id)
{
	List<Object_Chunk *> found;

	level->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && memcmp(&object->object_data.ID, id, host::kObjectIdBytes) == 0)
		{
			return object;
		}
	}

	return nullptr;
}

struct LevelCounts
{
	size_t withMerge = 0, mergePairs = 0, joinable = 0, freePairs = 0, paired = 0, taken = 0, unreadable = 0, cut = 0, lines = 0;
};

// One object's shape: its merge pairs counted, its joinable pairs printed
static void SurveyObject(char const *level, size_t row, std::string const &name, model::ShapeModel const &shape, std::vector<int32_t> const &merge, LevelCounts *counts)
{
	size_t const polygons = shape.polygons.size();

	for (size_t p = 0; p < polygons; p++)
	{
		int32_t const q = merge[p];

		if (q >= 0 && (size_t)q < polygons && (size_t)q > p && merge[(size_t)q] == (int32_t)p)
		{
			counts->mergePairs++;
		}
	}

	// which triangles use each point (a speed choice only: Joinable needs two shared corner numbers, so a pair it takes shares a point)
	std::vector<std::vector<int32_t>> users(shape.points.size());

	for (size_t p = 0; p < polygons; p++)
	{
		model::ShapePolygon const &one = shape.polygons[p];

		if (one.cornerCount != 3)
		{
			continue;
		}

		for (int c = 0; c < 3; c++)
		{
			int32_t const point = one.corners[c];

			if (point >= 0 && (size_t)point < users.size() && (users[(size_t)point].empty() || users[(size_t)point].back() != (int32_t)p))
			{
				users[(size_t)point].push_back((int32_t)p);
			}
		}
	}

	std::vector<int32_t> lastAskedFor(polygons, -1);   // the triangle a candidate was last gathered for, so each pair is asked about once
	size_t tests = 0;

	for (size_t a = 0; a < polygons; a++)
	{
		model::ShapePolygon const &one = shape.polygons[a];

		if (one.cornerCount != 3)
		{
			continue;
		}

		std::vector<int32_t> candidates;

		for (int c = 0; c < 3; c++)
		{
			int32_t const point = one.corners[c];

			if (point < 0 || (size_t)point >= users.size())
			{
				continue;
			}

			for (int32_t const b : users[(size_t)point])
			{
				if ((size_t)b > a && lastAskedFor[(size_t)b] != (int32_t)a)
				{
					lastAskedFor[(size_t)b] = (int32_t)a;
					candidates.push_back(b);
				}
			}
		}

		std::sort(candidates.begin(), candidates.end());

		for (int32_t const b : candidates)
		{
			if (tests + 2 > kMostTestsPerObject)
			{
				counts->cut++;
				return;
			}

			tests += 2;

			// as Merge asks it: the polygon picked first, then the one picked after it (PolyEdit_MergeInModel)
			bool const forward = host::PolyEdit_Joinable(shape, (int32_t)a, b, false, nullptr);
			bool const backward = host::PolyEdit_Joinable(shape, b, (int32_t)a, false, nullptr);

			if (!forward && !backward)
			{
				continue;
			}

			int32_t const first = forward ? (int32_t)a : b;
			int32_t const second = forward ? b : (int32_t)a;
			char const *state = "taken";

			if (merge[(size_t)first] == second && merge[(size_t)second] == first)
			{
				state = "paired";
				counts->paired++;
			}
			else if (merge[(size_t)first] == -1 && merge[(size_t)second] == -1)
			{
				state = "free";
				counts->freePairs++;
			}
			else
			{
				counts->taken++;
			}

			counts->joinable++;

			if (counts->lines < kMostLinesPerLevel)
			{
				printf("%s row=%zu object=%s poly=%d poly=%d state=%s order=%s\n", level, row, name.c_str(), first, second, state, forward && backward ? "any" : "this");
				counts->lines++;
			}
			else if (counts->lines == kMostLinesPerLevel)
			{
				printf("%s more pairs than %zu: the rest are counted, not listed\n", level, kMostLinesPerLevel);
				counts->lines++;
			}
		}
	}
}

// One level: false when the editor's open refuses it
static bool SurveyLevel(std::string const &path, std::string const &levelName)
{
	OpenLevelFile open;
	LevelOpenResult const opened = open.Open(path.c_str());

	if (opened != LevelOpenResult::Opened || !open.Level())
	{
		printf("%s not opened: %s\n", levelName.c_str(), OpenWords(opened));
		return false;
	}

	LevelObjects_Use(&open);
	UseGuard letGo;

	std::vector<host::LevelObject> rows;

	if (!LevelObjects_Read(&rows))
	{
		printf("%s not opened: the object list could not be read\n", levelName.c_str());
		return false;
	}

	LevelCounts counts;
	std::vector<std::string> ids;   // the IDs of the rows listed so far: a later row with one of them acts on the same object

	for (size_t row = 0; row < rows.size() && row < host::kObjectListMost; row++)
	{
		uint8_t const *const id = rows[row].id;
		std::string const idBytes((char const *)id, host::kObjectIdBytes);

		if (std::find(ids.begin(), ids.end(), idBytes) != ids.end())
		{
			continue;
		}

		ids.push_back(idBytes);

		Object_Chunk *const object = FirstWithId(open.Level(), id);
		Shape_Chunk *const shapeChunk = object ? object->get_assoc_shape() : nullptr;
		Chunk *const mergeChunk = shapeChunk ? shapeChunk->lookup_single_child("SHPMRGDT") : nullptr;

		if (!mergeChunk)
		{
			continue;   // no shape, or a shape with no merge data (a virtual object has neither)
		}

		counts.withMerge++;

		model::ShapeModel shape;
		std::string why;
		std::vector<unsigned char> bytes;

		if (host::ShapeEdit_Read(open.Level(), &open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done || !open.PayloadOf(mergeChunk, &bytes) ||
			bytes.size() != shape.polygons.size() * 4)
		{
			counts.unreadable++;
			continue;
		}

		std::vector<int32_t> merge(shape.polygons.size(), -1);

		if (!bytes.empty())
		{
			memcpy(merge.data(), bytes.data(), bytes.size());
		}

		SurveyObject(levelName.c_str(), row, Plain(rows[row].name, kMostNameBytes), shape, merge, &counts);
	}

	printf("%s counts: objects-with-SHPMRGDT=%zu merge-pairs=%zu joinable-pairs=%zu free=%zu paired=%zu taken=%zu", levelName.c_str(), counts.withMerge, counts.mergePairs,
		counts.joinable, counts.freePairs, counts.paired, counts.taken);

	if (counts.unreadable)
	{
		printf(" unreadable=%zu", counts.unreadable);
	}

	if (counts.cut)
	{
		printf(" cut=%zu", counts.cut);
	}

	printf("\n");
	fflush(stdout);
	return true;
}

static bool EndsWithRif(std::string const &name)
{
	return name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ".rif") == 0;
}

int main(int argc, char **argv)
{
	if (argc != 2 || !argv[1] || !argv[1][0])
	{
		printf("usage: verify_survey_merge_pairs.exe <a folder of level files>\n");
		return 2;
	}

	std::string folder = argv[1];

	while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
	{
		folder.pop_back();
	}

	if (folder.empty() || folder.size() > MAX_PATH - 16)
	{
		printf("survey_merge_pairs: the folder's path is empty or too long\n");
		return 2;
	}

	// the folder's level files, plain files only, by name A to Z (letters of either case alike), so two runs list them alike
	std::vector<std::string> names;
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		printf("survey_merge_pairs: the folder could not be read\n");
		return 2;
	}

	do
	{
		std::string const name = found.cFileName;

		if (!(found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) && EndsWithRif(name) && folder.size() + 1 + name.size() < MAX_PATH)
		{
			if (names.size() >= kMostLevels)
			{
				printf("survey_merge_pairs: more than %zu level files: the rest are left out\n", kMostLevels);
				break;
			}

			names.push_back(name);
		}
	} while (FindNextFileA(search, &found));

	FindClose(search);

	std::sort(names.begin(), names.end(), [](std::string const &x, std::string const &y) { return _stricmp(x.c_str(), y.c_str()) < 0; });

	bool allOpened = true;

	for (std::string const &name : names)
	{
		allOpened = SurveyLevel(folder + "\\" + name, Plain(name.c_str(), MAX_PATH)) && allOpened;
	}

	printf("survey_merge_pairs: %zu level files\n", names.size());
	return allOpened ? 0 : 1;
}
